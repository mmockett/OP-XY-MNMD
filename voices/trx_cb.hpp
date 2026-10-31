#pragma once
// TRX-CB: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Cowbell: a Machinedrum TRX-CB-style cowbell, written from scratch in fixed
// point and fitted to the Machinedrum's own (reference/fit_cb.py). Its page:
//   PTCH  DEC   ENH   DAMP
//   TONE  BUMP
// As measured: a pulse near 1117 Hz, low-passed, beating against a sine near
// 1051 Hz, with a quieter line near 1752 Hz and a quieter still at 3856. The oscillators run free, so the
// beat falls differently on every hit: here the sines start at random phases,
// faded in over 0.7 ms so a hit starts from nothing, as the Machinedrum's does
// (first sample about 0.1 of the peak), and the pulse starts half way through a
// high half, which leaves no low thump under its first cycles. The pulse's edges
// fall between samples, each sample its average, so nothing folds down beneath
// the bell: as on the Machinedrum, nothing stands between 100 and 1000 Hz. TONE
// is the pulse's brightness (its 3rd harmonic from 29 to 11 dB under the
// fundamental, the fundamental kept); ENH brings up the 1051 Hz voice (from 15
// dB under the pulse to level with it at ENH 80), narrows the pulse a little
// (even harmonics) and darkens it; DAMP takes the second voice and the 1752 Hz
// line away and darkens the pulse a little. DEC: -60 dB in 9 ms .. 3.1 s, the
// second voice and the line faster. On the Machinedrum PTCH and
// BUMP have no effect that can be measured; here PTCH tunes the bell a fourth
// either way and BUMP bends the strike up, as much as a fifth, over 10 ms. The
// Machinedrum's cowbell sits 9 dB under its kicks; here it is lifted to stand
// with them. The key moves all of it.
namespace md {
namespace trx_cb {
struct State {
    u32 bell, voice, line, high;   // the pulse, the second voice, the lines
    i32 level;               // the pulse's envelope, Q31
    i32 others;              // the second voice's and the line's, Q31
    i32 bump;                // BUMP's bend, Q31
    i32 low1, low2;          // the pulse's low-pass
    u32 fading;              // samples of the sines' fade in so far
    u32 seed;
    u32 fresh;
};
namespace {
using md::multiply;

enum { PTCH, DEC, ENH, DAMP, TONE, BUMP };

const i32 bell_octaves = 137236, voice_octaves = 131477, line_octaves = 179793;   // 1117, 1051, 1752 Hz from middle C
const i32 high_octaves = 254386;       // 3856 Hz
const i32 high_share = 0x1c28f5c3;     // 13 dB under the 1752 Hz line, moving with it
const i32 tune_reach = 27307;          // PTCH: a fourth either way, Q16 octaves
const i32 bump_most = 38229;           // BUMP: up to a fifth
const i32 bump_time = 600000;          // -60 dB in about 30 ms: the bend is mostly gone in 10
const i32 decay_times[9] = {565748, 645859, 704388, 780249, 845240, 911321, 980977, 1050586, 1118063};
// The second voice and the line fall faster: under the pulse's decay, their own,
// -60 dB in a second up to DEC 96, then 2.2 and 9 s.
const i32 others_times[9] = {1011122, 1011122, 1011122, 1011122, 1011122, 1011122, 1011122, 1085669, 1218866};
// The second voice, against the pulse's fundamental: by ENH (at TONE 64, DAMP 0),
// TONE (at half, 1 at TONE 64) and DAMP.
const i32 voice_enh[9] = {381882595, 539423504, 761955951, 1207618800, 1705806895, 2147483647, 1913946816, 1520301996, 1207618800};
const i32 voice_tone[9] = {1516743514, 1516743514, 1351785578, 1204745645, 1073741824, 956987256, 852933856, 677498806, 677498806};
const i32 voice_damp[9] = {2147483647, 1705806895, 1207618800, 854928639, 428479319, 303340128, 214748365, 214748365, 191394682};
// The 1752 Hz line, likewise (its TONE table at full scale).
const i32 line_enh[9] = {340353221, 270352174, 170580690, 170580690, 152030200, 152030200, 170580690, 135497058, 135497058};
const i32 line_tone[9] = {2147483647, 2147483647, 1804587480, 1705806895, 1354970580, 1296372000, 1076291389, 959245710, 854928639};
const i32 line_damp[9] = {2147483647, 1354970580, 1076291389, 1076291389, 959245710, 761955951, 539423504, 85492864, 76195595};
// The pulse's two-pole low-pass, from 530 Hz to 8.5 kHz in nine steps of half an
// octave, and the gain that keeps its fundamental where it was (Q27).
const i32 lowpass[9] = {156283539, 218201741, 302719832, 416263988, 565400186, 755127855, 985830599, 1248998593, 1506435671};
const i32 keep[9] = {729638111, 430307488, 281456802, 207436555, 170627877, 152323713, 143221445, 138695083, 136543589};
// Where TONE, ENH and DAMP put the low-pass: Q31 positions on those tables.
const i32 tone_low = 0x18a3d70a, tone_span = 0x575c28f6;   // TONE 0 .. 127: 0.77 .. 3.5 octaves above 530 Hz, over 4
const i32 enh_darkens = 0x1bd70a3d, damp_darkens = 0x1999999a;   // ENH: -0.87 octave, DAMP: -0.8
// Levels: the pulse's fundamental at -14.3 dB of full scale (the Machinedrum's
// -23.3 lifted 9 dB), half of it here; the pulse is an eighth of full scale.
const i32 bell_level = 0x670a3d71;   // 0.8, with the low-pass's own loss
const i32 voice_ref = 0x147ae148;    // the second voice's level at its table's 1
const i32 line_ref = 0x2083126f;     // the line's
const u32 fade_in = 32;              // the sines' fade in, samples: a power of two
const u32 fade_shift = 5;
const i32 pulse_level = 0x10000000;

// 1 / step for the pulse's edges, without a division: the step moved up to [0.5, 1)
// (Q31, by `shift`), then its reciprocal, Q29, from a line and two Newton steps.
struct Reciprocal {
    i32 inverse;
    u32 shift;
    explicit Reciprocal(u32 step) {
        shift = 0;
        step |= 1u;   // never zero
        while (step < 0x40000000u) { step <<= 1; ++shift; }
        const i32 m = static_cast<i32>(step >= 0x80000000u ? 0x7fffffffu : step);
        i32 x = 0x5d413ccd - (m >> 1);   // 2.914 - 2m, Q29
        x = multiply(x, 0x40000000 - multiply(m, x)) * 4;
        x = multiply(x, 0x40000000 - multiply(m, x)) * 4;
        inverse = x;
    }
    // How far `part` is through the step, Q30 (part <= step).
    i32 of(u32 part) const { return multiply(static_cast<i32>(part << shift), inverse) * 2; }
};

struct Block {
    i32 tune, bump, decay_step, others_step, duty, cutoff, keep, voice, line;
    explicit Block(const i32* p) {
        tune = md::span(p[PTCH], -tune_reach, tune_reach);
        bump = multiply(md::squared(p[BUMP]), bump_most);
        const i32 time = md::table9(decay_times, p[DEC]);
        decay_step = md::decay(time);
        others_step = multiply(decay_step, md::decay(md::table9(others_times, p[DEC])));
        const i32 e = md::positive(p[ENH]), d = md::positive(p[DAMP]);
        duty = 0x40000000 - multiply(e, 0x051eb852);              // ENH: 50% .. 46% of the cycle, Q31 of half a turn
        i32 position = tone_low + multiply(md::positive(p[TONE]), tone_span) - multiply(e, enh_darkens) - multiply(d, damp_darkens);
        position = md::clamp(position, 0, md::one);
        cutoff = md::table9(lowpass, position);
        keep = md::table9(trx_cb::keep, position);
        voice = multiply(multiply(multiply(md::table9(voice_enh, p[ENH]), md::table9(voice_tone, p[TONE])) * 2,
                                  md::table9(voice_damp, p[DAMP])), voice_ref);
        line = multiply(multiply(multiply(md::table9(line_enh, p[ENH]), md::table9(line_tone, p[TONE])),
                                 md::table9(line_damp, p[DAMP])), line_ref);
    }
};
}

// The block's stages, a loop each: out of line, so each has a hardware loop and all the
// registers to itself.
// The pulse, each sample its average (an edge inside it gives the part-way value), through its low-pass.
__attribute__((noinline)) inline void make_bell(State& voice, u32 step, u32 edge, const Reciprocal& reciprocal, i32 cutoff, i32 keep, i32* bell) {
    u32 from = voice.bell;
    i32 low1 = voice.low1, low2 = voice.low2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const u32 to = from + step;
        i32 pulse;
        if (to < from) pulse = pulse_level - multiply(pulse_level, reciprocal.of(0u - from)) * 4;
        else if (from < edge && to >= edge) pulse = multiply(pulse_level, reciprocal.of(edge - from)) * 4 - pulse_level;
        else pulse = from < edge ? pulse_level : -pulse_level;
        from = to;
        md::smooth(low1, pulse, cutoff);
        md::smooth(low2, low1, cutoff);
        bell[i] = multiply(md::gain(multiply(low2, keep), 4), bell_level);
    }
    voice.bell = from;
    voice.low1 = low1;
    voice.low2 = low2;
}
__attribute__((noinline)) inline void add_sine(u32 phase, u32 step, i32 amount, i32* out) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += multiply(md::sine(phase), amount);
        phase += step;
    }
}
// md::finish(md::gain(mixed, 1)), with no branches in the loop.
__attribute__((noinline)) inline void mix_out(State& voice, i32 step, i32 others_step, const i32* bell, const i32* others, opxy::Output output) {
    i32 level = voice.level, sines = voice.others;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 mixed = multiply(bell[i], level) + multiply(others[i], sines);
        output[i] = md::clamp(mixed, -half_scale / 2, half_scale / 2) * 2;
        level = multiply(level, step);
        sines = multiply(sines, others_step);
    }
    voice.level = level;
    voice.others = sines;
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.bell = voice.voice = voice.line = voice.high = 0;
    voice.level = voice.others = voice.bump = 0;
    voice.low1 = voice.low2 = 0;
    voice.fading = fade_in;
    voice.seed = 0x2b6e1f3du * owner;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.bell = static_cast<u32>(b.duty);   // half way through the high half: half the pulse's push
        voice.fading = 0;
        voice.voice = static_cast<u32>(md::noise(voice.seed));
        voice.line = static_cast<u32>(md::noise(voice.seed));
        voice.high = static_cast<u32>(md::noise(voice.seed));
        voice.level = voice.others = voice.bump = md::one;
        voice.low1 = voice.low2 = 0;
        voice.fresh = 0;
    }
    // One bend for the block: the oscillators move together.
    const i32 bend = b.tune + multiply(voice.bump, b.bump);
    voice.bump = multiply(voice.bump, md::per_block(md::decay(bump_time)));
    const u32 bell_step = md::transpose(pitch[0], bell_octaves + bend);
    const u32 voice_step = md::transpose(pitch[0], voice_octaves + bend);
    const u32 line_step = md::transpose(pitch[0], line_octaves + bend);
    const u32 high_step = md::transpose(pitch[0], high_octaves + bend);
    const i32 high = multiply(b.line, high_share);
    const u32 edge = static_cast<u32>(b.duty) * 2;   // where the high half ends
    const Reciprocal reciprocal(bell_step);

    i32 bell[opxy::block_frames], others[opxy::block_frames];
    make_bell(voice, bell_step, edge, reciprocal, b.cutoff, b.keep, bell);
    for (unsigned i = 0; i != opxy::block_frames; ++i) others[i] = 0;
    add_sine(voice.voice, voice_step, b.voice, others);
    add_sine(voice.line, line_step, b.line, others);
    add_sine(voice.high, high_step, high, others);
    voice.voice += voice_step << 6;
    voice.line += line_step << 6;
    voice.high += high_step << 6;
    if (voice.fading < fade_in) {   // a note's first block
        for (unsigned i = 0; i != opxy::block_frames; ++i) {
            i32 sines = voice.others;
            if (voice.fading < fade_in) {
                sines = (sines >> fade_shift) * static_cast<i32>(voice.fading);
                ++voice.fading;
            }
            const i32 mixed = multiply(bell[i], voice.level) + multiply(others[i], sines);
            output[i] = md::clamp(mixed, -half_scale / 2, half_scale / 2) * 2;
            voice.level = multiply(voice.level, b.decay_step);
            voice.others = multiply(voice.others, b.others_step);
        }
    } else {
        mix_out(voice, b.decay_step, b.others_step, bell, others, output);
    }
}
}
}
