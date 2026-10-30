#pragma once
// EFM-CY: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM cymbal in the manner of the Machinedrum's EFM-CY, written from scratch
// in fixed point and fitted to it (reference/fit_efm_cy.py). The controls:
//   PTCH  DEC   FB    HPF
//   MOD   MFRQ  MDEC  -
// Six sines at the Machinedrum's ratios, 1, 1.41, 1.80 and 2.70 at full level
// and 3.50 and 4.94 at -11 dB, the lowest 244 Hz to 1.36 kHz over PTCH, each
// their sum fed back into each one's phase by FB (past FB 80 a buzz). One
// modulator at MFRQ / 16 of the lowest sweeps the four loud ones, at the same
// index for each, MOD's deviation four times EFM-BD's: the cymbal's wash. MDEC takes up
// to 40% of it away. DEC is the level's decay; HPF is EFM-SD's high-pass.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace efm_cy {
const unsigned partials = 6;
struct State {
    u32 phase[partials];
    i32 last[1];          // the partials' previous sum, for feedback
    u32 modulator;
    i32 level;            // amplitude envelope, Q31
    i32 depth;            // MDEC's envelope, Q31
    i32 low;              // HPF's state
    u32 fresh;            // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, FB, HPF, MOD, MFRQ, MDEC };

// Pitch: 244 Hz * 2^(2.48 PTCH), in Q16 octaves from middle C, and the partials' ratios (Q28) and levels.
const i32 base_low = -6595, base_span = 162371;
const i32 ratios[partials] = {0x10000000, 0x168f5c29, 0x1ccccccd, 0x2b3b645a, 0x380c49ba, 0x4f0a3d71};
const i32 weak_weight = 0x23d70a3d;   // the last two, -11 dB
// DEC: EFM-BD's times, but 17 ms at 0 and 37 ms at 16 (as EFM-RS's).
const i32 dec_times[9] = {625880, 699411, 793416, 854102, 934567, 1011122, 1087794, 1170568, 1250679};
// FB: 0.55 of MFB's feedback on EFM-BD; MOD: four times EFM-BD's deviation,
// of which MDEC takes 40% away.
const i32 fb_scale = 0x46666666;
const i32 lasting = 0x4ccccccd;
// Each partial's level: the Machinedrum's, 2 dB up as the TRX machines are,
// times four for the quarters the loop sums.
const i32 partial_level = 0x19a9294c;

// A sine without interpolation: cheap, and its -60 dB error is lost in a cymbal.
inline i32 rough_sine(u32 phase) { return sine_table[phase >> 22]; }

struct Block {
    i32 base, decay_step, fb, deviation, ratio, depth_step, hpf;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        decay_step = md::decay(md::table9(dec_times, p[DEC]));
        fb = multiply(efm::feedback(p[FB]), fb_scale);
        deviation = efm::sixteenths(p[MOD]);
        ratio = efm::ratio(p[MFRQ]);
        depth_step = md::decay(md::table9(efm::mdec_times, p[MDEC]));
        hpf = md::table9(efm::hpf_coefficients, p[HPF]);
    }
};
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    for (unsigned k = 0; k < partials; ++k) voice.phase[k] = 0;
    voice.last[0] = 0;
    voice.modulator = 0;
    voice.level = voice.depth = voice.low = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        for (unsigned k = 0; k < partials; ++k) voice.phase[k] = efm::start_phase;
        voice.last[0] = 0;
        voice.modulator = efm::start_phase;
        voice.level = voice.depth = md::one;
        voice.low = 0;
        voice.fresh = 0;
    }
    const u32 base = md::transpose(pitch[0], b.base);
    // The deviation in multiples of each partial (Q28), held over the block; the loop swings four times it.
    const i32 swing = multiply(b.deviation, lasting + multiply(md::one - lasting, voice.depth));
    voice.depth = multiply(voice.depth, md::per_block(b.depth_step));
    u32 increment[partials];
    i32 deviation[4];
    for (unsigned k = 0; k < partials; ++k) increment[k] = md::scale(base, ratios[k]);
    for (unsigned k = 0; k < 4; ++k) deviation[k] = static_cast<i32>(md::scale(increment[k], swing) >> 1);
    const u32 modulation = md::scale(base, b.ratio);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 m = rough_sine(voice.modulator);
        voice.modulator += modulation;
        // FB: the partials' sum, fed back into every one's phase.
        const u32 fed = static_cast<u32>(multiply(b.fb, voice.last[0])) << 3;
        // Each partial a quarter of full scale, without a signed shift: biased, shifted, unbiased.
        u32 main = 0, weak = 0;
        for (unsigned k = 0; k < 4; ++k) {
            main += (static_cast<u32>(rough_sine(voice.phase[k] + fed)) + 0x80000000u) >> 2;
            voice.phase[k] += increment[k] + (static_cast<u32>(multiply(deviation[k], m)) << 3);
        }
        for (unsigned k = 4; k < partials; ++k) {
            weak += (static_cast<u32>(rough_sine(voice.phase[k] + fed)) + 0x80000000u) >> 2;
            voice.phase[k] += increment[k];
        }
        const i32 sum = static_cast<i32>(main - 0x80000000u) + multiply(static_cast<i32>(weak - 0x40000000u), weak_weight);
        voice.last[0] = sum;
        const i32 x = multiply(sum, voice.level);
        voice.low += multiply(x - voice.low, b.hpf);
        output[i] = md::finish(multiply(x - voice.low, partial_level));
        voice.level = multiply(voice.level, b.decay_step);
    }
}
}
}
