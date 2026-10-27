#pragma once
// PI-ML: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_modal.hpp"

// A Machinedrum PI-ML-style struck bar, written from scratch in fixed point and
// fitted to the Machinedrum's own (reference/fit_pi_ml.py). Its page:
//   PTCH  DEC   HARD  TENS
// As measured: seven inharmonic modes at 0.58, 0.69, 0.74, 1, 1.11, 1.97 and
// 2.07 times a pitch from 372 Hz (PTCH 0) to 1.47 kHz (127), two octaves. Each
// hit strikes them a little differently, so their balance varies hit to hit
// by a few dB, as the Machinedrum's does, and their phases too. DEC sets the ring, -60 dB in 30 ms
// to 1.8 s. HARD is the strike's force: soft strikes are much quieter and
// leave mostly the upper pair. TENS bends every mode up at the strike, to
// twice the pitch, relaxing in 20 ms (TENS 64) to 120 ms (TENS 127); the
// highest PTCH does not bend.
namespace md {
namespace pi_ml {
const unsigned mode_count = 7;
struct State {
    modal::Mode modes[mode_count];
    i32 bend;        // the strike's stretch still to relax, Q31
    u32 seed;
    u32 fresh;
};
namespace {
using md::multiply;

enum { PTCH, DEC, HARD, TENS };

// The pitch, 372 Hz * 2^(1.986 PTCH), Q16 octaves from middle C; the modes above it and their levels.
const i32 base_low = 33278, base_span = 130154;
const i32 ratios[mode_count] = {-51340, -35221, -28214, 0, 10037, 64107, 68789};
const i32 levels[mode_count] = {1076291389, 1076291389, 1705806895, 2147483647, 761955951, 1207618800, 539423504};
const unsigned upper = 5;   // the modes from here on are the upper pair
// HARD: the lower modes' level and the upper pair's at HARD 0, 16, ..., 127 (Q31).
const i32 hard_lower[9] = {310405827, 1004453568, 1686280685, 2147483647, 2147483647, 2147483647, 2147483647,
                           2147483647, 2147483647};
const i32 hard_upper[9] = {719332803, 2147483647, 2147483647, 1806882308, 1520301996, 1520301996, 1520301996,
                           1520301996, 1354970580};
// DEC: -60 dB in 30 ms .. 1.8 s, Q16 log2 samples at DEC 0, 16, ..., 127.
const i32 decay_times[9] = {679582, 772318, 825229, 883758, 926822, 962824, 1001160, 1035928, 1066696};
// TENS: the strike's stretch (the start ratio less one, Q28) and how fast it
// relaxes (-60 dB, Q16 log2 samples), at TENS 0, 16, ..., 127.
const i32 stretch[9] = {0, 40265318, 96636764, 147639501, 233538847, 268435456, 268435456, 268435456, 268435456};
const i32 relax_times[9] = {862311, 889511, 841213, 841213, 823975, 855788, 902725, 951433, 993383};
// The stretch's share by PTCH (Q31 at PTCH 0, 16, ..., 127): none at the top.
const i32 stretch_share[9] = {1288490189, 1503238554, 1717986918, 1932735283, 2147483647, 2147483647, 1717986918,
                              858993459, 0};
// Each hit's balance: every mode's level times 0.55 .. 1, at random (-5 .. 0 dB).
const i32 vary_low = 0x46666666, vary_span = 0x39999999;
// The loudest mode peaks 2 dB over the Machinedrum's (reference/pi_measure.py, LEVEL_DB).
const i32 body_level = 0x08800000;

struct Block {
    i32 base, step, stretch, relax, lower, upper_level;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        step = md::decay(md::table9(decay_times, p[DEC]));
        stretch = multiply(md::table9(pi_ml::stretch, p[TENS]), md::table9(stretch_share, p[PTCH]));
        relax = md::decay(md::table9(relax_times, p[TENS]));
        lower = multiply(md::table9(hard_lower, p[HARD]), body_level);
        upper_level = multiply(md::table9(hard_upper, p[HARD]), body_level);
    }
};
}

inline void trigger(State& voice, Common& shared) {
    voice.fresh = 1;
    voice.seed += 0x9e3779b9u * ++shared.strikes;   // each hit its own balance
}
inline void reset(State& voice, Common& shared, u32 owner) {
    for (unsigned k = 0; k < mode_count; ++k) {
        voice.modes[k].phase = voice.modes[k].increment = 0;
        voice.modes[k].level = 0;
    }
    voice.bend = 0;
    voice.seed = 0x68e31da4u * owner;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.fresh = 0;
        for (unsigned k = 0; k < mode_count; ++k) {
            md::noise(voice.seed);
            const i32 vary = vary_low + multiply(static_cast<i32>(voice.seed >> 1), vary_span);
            voice.modes[k].level = multiply(levels[k], vary);
            md::noise(voice.seed);
            voice.modes[k].phase = voice.seed;   // and its own phase
            voice.modes[k].increment = md::scale(md::transpose(pitch[0], b.base + ratios[k]), 0x10000000 + b.stretch);
        }
        voice.bend = md::one;
    }
    voice.bend = multiply(voice.bend, md::per_block(b.relax));
    const i32 ratio = 0x10000000 + multiply(b.stretch, voice.bend);
    i32 mixed[opxy::block_frames];
    for (unsigned i = 0; i != opxy::block_frames; ++i) mixed[i] = 0;
    for (unsigned k = 0; k < mode_count; ++k) {
        modal::Mode& mode = voice.modes[k];
        if (mode.level < 0x00010000) continue;
        const u32 target = md::scale(md::transpose(pitch[0], b.base + ratios[k]), ratio);
        modal::ring_glide(mode, b.step, k < upper ? b.lower : b.upper_level, target, mixed);
    }
    for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = md::finish(md::gain(mixed[i], 1));
}
}
}
