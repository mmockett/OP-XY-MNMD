#pragma once
// EFM-CY: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM cymbal in the manner of the Machinedrum's EFM-CY, written from scratch
// in fixed point and fitted to it (reference/fit_efm_cy.py). The controls:
//   PTCH  DEC   FB    HPF
//   MOD   MFRQ  MDEC  -
// Four sines at the Machinedrum's ratios, 1, 1.41, 1.80 and 2.70, the lowest
// 244 Hz to 1.36 kHz over PTCH, each with its own modulator at MFRQ / 16 + 0.5
// of itself, moving its phase by an index that MOD sets, 0.55 radian at MOD 0
// to 9.8 at 127, whatever MFRQ and PTCH. So even at MOD 0 each sine has two
// sidebands 11 dB down (twelve lines in all, at 1, 3.5 and 5.5 times the four at
// MFRQ 64, measured), and as MOD rises those take over from the sines, the
// cymbal's wash. MDEC is the whole index's decay, slow: -60 dB in 2 s (MDEC 0)
// to 76 s (127). The sum is fed back into each sine's phase by FB (past FB 80
// a buzz). DEC is the level's decay; HPF is two one-pole high-passes in cascade
// (580 Hz down 30 dB at 127, 8.6 kHz down 0.7).
namespace md {
namespace efm_cy {
const unsigned partials = 4;
struct State {
    u32 phase[partials];
    u32 modulator[partials];
    i32 last;             // the partials' previous sum, for feedback
    i32 level;            // amplitude envelope, Q31
    i32 depth;            // MDEC's envelope, Q31
    i32 low1, low2;       // HPF's states
    i32 before;           // the last sample before the output's soft top
    u32 fresh;            // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, FB, HPF, MOD, MFRQ, MDEC };

// Pitch: 244 Hz * 2^(2.48 PTCH), in Q16 octaves from middle C, and the partials' ratios (Q28).
const i32 base_low = -6595, base_span = 162371;
const i32 ratios[partials] = {0x10000000, 0x168f5c29, 0x1ccccccd, 0x2b3b645a};
// DEC: EFM-BD's times, but 17 ms at 0 and 37 ms at 16 (as EFM-RS's).
const i32 dec_times[9] = {625880, 699411, 793416, 854102, 934567, 1011122, 1087794, 1170568, 1250679};
// MOD: the index, 0.55 .. 9.75 radians, as beta / 2 pi * 2^29: run through a
// multiply and shifted up 3, it is a phase.
const i32 index_low = 46995113, index_span = 786100067;
// MDEC: the index's fall per block, its time constant 0.29, 0.45, 0.72, 1.13,
// 1.8, 2.9, 4.7, 7.3 and 11 s (longer than md::decay reaches).
const i32 mdec_steps[9] = {2136736994, 2140558027, 2143155135, 2144725657, 2145752243, 2146408983, 2146820557, 2147056726, 2147200327};
// FB: 0.6 of MFB's feedback on EFM-BD.
const i32 fb_scale = 0x4ccccccd;
// HPF: the two poles' coefficient, measured line by line (within 0.06 dB).
const i32 hpf_coefficients[9] = {0, 125627793, 223123551, 320834057, 417470821, 515181327, 611603343, 708240107, 799508162};
const i32 top_fall = 0x13333333;   // 0.15
// Each sine's level: the Machinedrum's, 2 dB up as the TRX machines are, times
// four for the quarters the loop sums.
const i32 partial_level = 0x33f00000;

// A sine without interpolation: cheap, and its -60 dB error is lost in a cymbal.
inline i32 rough_sine(u32 phase) { return sine_table[phase >> 22]; }

struct Block {
    i32 base, decay_step, fb, index, ratio, depth_step, hpf;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        decay_step = md::decay(md::table9(dec_times, p[DEC]));
        fb = multiply(efm::feedback(p[FB]), fb_scale);
        index = md::span(p[MOD], index_low, index_low + index_span);
        // MFRQ / 16 + 0.5 of each sine, halved to fit Q28.
        ratio = (efm::sixteenths(p[MFRQ]) + 0x08000000) / 2;
        depth_step = md::table9(mdec_steps, p[MDEC]);
        hpf = md::table9(hpf_coefficients, p[HPF]);
    }
};
}

// With no feedback (FB's default, and every kit's) each partial is on its own: a loop each,
// out of line, so each has a hardware loop and all the registers to itself.
__attribute__((noinline)) inline void add_partial(u32 phase, u32 increment, u32 modulator, u32 modulation, i32 index, u32* total) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const u32 moved = static_cast<u32>(multiply(index, rough_sine(modulator))) << 3;
        total[i] += (static_cast<u32>(rough_sine(phase + moved)) + 0x80000000u) >> 2;
        modulator += modulation;
        phase += increment;
    }
}

// The sum under the level, through HPF, to the output.
__attribute__((noinline)) inline void finish_block(State& voice, i32 hpf, i32 step, const u32* total, opxy::Output output) {
    i32 level = voice.level, low1 = voice.low1, low2 = voice.low2, before = voice.before;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 sum = static_cast<i32>(total[i] - 0x80000000u);
        // Halved: the high-passes can double an edge, and the sum can reach full scale.
        const i32 x = -multiply(sum, level) / 2;
        // Two poles: the first's output is taken after it moves, the second's before.
        low1 += multiply(x - low1, hpf);
        const i32 y = x - low1 - low2;
        low2 += multiply(y, hpf);
        // The top falls away gently: 1 dB at 8.6 kHz, 3 dB at 17 kHz.
        const i32 soft = y - multiply(y - before, top_fall);
        before = y;
        output[i] = md::finish(multiply(soft, partial_level));
        level = multiply(level, step);
    }
    voice.level = level;
    voice.low1 = low1;
    voice.low2 = low2;
    voice.before = before;
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    for (unsigned k = 0; k < partials; ++k) voice.phase[k] = voice.modulator[k] = 0;
    voice.last = 0;
    voice.level = voice.depth = voice.low1 = voice.low2 = voice.before = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        for (unsigned k = 0; k < partials; ++k) voice.phase[k] = voice.modulator[k] = 0;
        voice.last = 0;
        voice.level = voice.depth = md::one;
        voice.low1 = voice.low2 = voice.before = 0;
        voice.fresh = 0;
    }
    // Gone: the filters' last bits of rounding go too, so it ends at zero.
    if (!voice.level) voice.low1 = voice.low2 = voice.before = 0;
    const u32 base = md::transpose(pitch[0], b.base);
    // The index, held over the block.
    const i32 index = multiply(b.index, voice.depth);
    voice.depth = multiply(voice.depth, b.depth_step);
    u32 increment[partials], modulation[partials];
    for (unsigned k = 0; k < partials; ++k) {
        increment[k] = md::scale(base, ratios[k]);
        modulation[k] = md::scale(increment[k], b.ratio) << 1;
    }

    u32 total[opxy::block_frames];
    if (!b.fb) {
        for (unsigned i = 0; i != opxy::block_frames; ++i) total[i] = 0;
        for (unsigned k = 0; k < partials; ++k) {
            add_partial(voice.phase[k], increment[k], voice.modulator[k], modulation[k], index, total);
            voice.phase[k] += increment[k] << 6;
            voice.modulator[k] += modulation[k] << 6;
        }
        voice.last = static_cast<i32>(total[opxy::block_frames - 1] - 0x80000000u);
    } else {
        for (unsigned i = 0; i != opxy::block_frames; ++i) {
            // FB: the partials' sum, fed back into every one's phase.
            const u32 fed = static_cast<u32>(multiply(b.fb, voice.last)) << 3;
            // Each partial a quarter of full scale, without a signed shift: biased, shifted, unbiased.
            u32 sum = 0;
            for (unsigned k = 0; k < partials; ++k) {
                const u32 moved = static_cast<u32>(multiply(index, rough_sine(voice.modulator[k]))) << 3;
                sum += (static_cast<u32>(rough_sine(voice.phase[k] + fed + moved)) + 0x80000000u) >> 2;
                voice.modulator[k] += modulation[k];
                voice.phase[k] += increment[k];
            }
            total[i] = sum;
            voice.last = static_cast<i32>(sum - 0x80000000u);
        }
    }
    finish_block(voice, b.hpf, b.decay_step, total, output);
}
}
}
