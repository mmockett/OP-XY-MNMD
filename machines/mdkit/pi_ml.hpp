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
// highest PTCH does not bend. Two things sound with the modes and ring as long
// as they do: the knock of the bar's frame, a low tone near 130 Hz whatever
// PTCH is and different every hit, 16 dB under the peak at 5 to 20 ms; and a
// haze above 2 kHz, many faint modes as noise, falling about 9 dB an octave,
// 10 dB louder for every 64 steps of PTCH.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace pi_ml {
const unsigned mode_count = 7;
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
// The knock: 110 to 155 Hz (Q16 octaves from middle C); its level (Q31).
// It rings 0.4 octaves shorter than the modes.
const i32 knock_shorter = 26214;
const i32 knock_low = -81922, knock_span = 32425, knock_level = 0x30000000;
// The haze: above 1.5 kHz, two poles at 2 kHz; its level by PTCH at 0, 16, ..., 127 (Q31).
const i32 haze_under = 0x18a13c35, haze_over = 0x1fbcce9a;
const i32 haze_levels[9] = {203728187, 271675904, 362285640, 483115666, 644245094, 859114641, 1145647786, 1527745877, 2037281870};
// The loudest mode peaks 2 dB over the Machinedrum's (reference/pi_measure.py, LEVEL_DB).
const i32 body_level = 0x08800000;
// Past the strike's first blocks the modes under 900 Hz run at a quarter of the rate (modal::Low).
const u32 low_after = 32;   // blocks: 46 ms, so its short ring keeps the whole strike

}
struct Block {
    u32 settled[mode_count];   // the modes' increments at rest
    i32 base, fall, knock_fall, relax_fall, stretch, lower, upper_level, knock_amount, haze;
    void plan(const i32* p, u32 middle) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        for (unsigned k = 0; k < mode_count; ++k) settled[k] = md::transpose(middle, base + ratios[k]);
        const i32 time = md::table9(decay_times, p[DEC]);
        fall = md::per_block(md::decay(time));   // the same for the modes and the haze, so once
        knock_fall = md::per_block(md::decay(time - knock_shorter));
        stretch = multiply(md::table9(pi_ml::stretch, p[TENS]), md::table9(stretch_share, p[PTCH]));
        relax_fall = md::per_block(md::decay(md::table9(relax_times, p[TENS])));
        lower = multiply(md::table9(hard_lower, p[HARD]), body_level);
        upper_level = multiply(md::table9(hard_upper, p[HARD]), body_level);
        knock_amount = multiply(knock_level, lower);
        // A soft strike leaves little of it: HARD's fall counts twice.
        haze = multiply(multiply(md::table9(haze_levels, p[PTCH]), lower), md::table9(hard_lower, p[HARD]));
    }
};
struct State {
    modal::Mode modes[mode_count];
    modal::Mode knock;
    i32 knock_offset; // its pitch this hit, Q16 octaves from middle C
    i32 haze;        // the haze's envelope, Q31
    i32 under, low1, low2;   // the haze: a high-pass pole and two low-pass
    i32 bend;        // the strike's stretch still to relax, Q31
    u32 seed;
    u32 fresh;
    u32 stale;       // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];     // the controls they were worked out from
    Block block;
    u32 seen_ratio;  // the ratio the targets were worked out from
    u32 targets[mode_count];
    u32 age;         // blocks since the strike, up to low_after
    u32 low_set;     // the modes in low_end
    i32 low_end;     // the low modes' value where the last block ended
};

// The haze, added: out of line, so the loop has a hardware loop and all the registers to itself.
__attribute__((noinline)) inline void add_haze(State& voice, i32 level, i32 slope, i32* mixed) {
    u32 seed = modal::live_seed(voice.seed);
    i32 under = voice.under, low1 = voice.low1, low2 = voice.low2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 white = modal::noise_eighth_on(seed);
        md::smooth(under, white, haze_under);
        md::smooth(low1, white - under, haze_over);
        md::smooth(low2, low1, haze_over);
        mixed[i] += multiply(low2, level) * 64;
        level += slope;
    }
    voice.seed = seed;
    voice.under = under;
    voice.low1 = low1;
    voice.low2 = low2;
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
    voice.knock.phase = voice.knock.increment = 0;
    voice.knock.level = 0;
    voice.knock_offset = 0;
    voice.haze = voice.under = voice.low1 = voice.low2 = 0;
    voice.bend = 0;
    voice.seed = 0x68e31da4u * owner;
    voice.stale = 1;
    voice.seen_ratio = 0;
    voice.age = 0;
    voice.low_set = 0;
    voice.low_end = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    // Only when a control or the key has moved.
    u32 moved = voice.stale | (pitch[0] ^ voice.seen_pitch);
    for (unsigned c = 0; c < 8; ++c) moved |= static_cast<u32>(parameters[c] ^ voice.seen[c]);
    if (moved) {
        voice.stale = 0;
        voice.seen_pitch = pitch[0];
        for (unsigned c = 0; c < 8; ++c) voice.seen[c] = parameters[c];
        voice.block.plan(parameters, pitch[0]);
    }
    const Block& b = voice.block;
    const bool fresh = voice.fresh != 0;
    if (fresh) {
        voice.fresh = 0;
        for (unsigned k = 0; k < mode_count; ++k) {
            md::noise(voice.seed);
            const i32 vary = vary_low + multiply(static_cast<i32>(voice.seed >> 1), vary_span);
            voice.modes[k].level = multiply(levels[k], vary);
            md::noise(voice.seed);
            voice.modes[k].phase = voice.seed;   // and its own phase
            voice.modes[k].increment = md::scale(b.settled[k], 0x10000000 + b.stretch);
        }
        md::noise(voice.seed);
        voice.knock_offset = knock_low + multiply(static_cast<i32>(voice.seed >> 1), knock_span);
        voice.knock.phase = voice.seed << 8;
        voice.knock.level = md::one;
        voice.haze = md::one;
        voice.bend = md::one;
        voice.age = 0;
    }
    if (moved || fresh) voice.knock.increment = md::transpose(pitch[0], voice.knock_offset);
    voice.bend = multiply(voice.bend, b.relax_fall);
    const i32 ratio = 0x10000000 + multiply(b.stretch, voice.bend);
    if (moved || static_cast<u32>(ratio) != voice.seen_ratio) {
        voice.seen_ratio = static_cast<u32>(ratio);
        for (unsigned k = 0; k < mode_count; ++k) voice.targets[k] = md::scale(b.settled[k], ratio);
    }
    const i32 fall = b.fall;
    i32 mixed[opxy::block_frames];
    for (unsigned i = 0; i != opxy::block_frames; ++i) mixed[i] = 0;
    // A mode stays at the full rate while its pitch is over the limit.
    u32 low_set = 0;
    if (voice.age < low_after) {
        ++voice.age;
    } else {
        for (unsigned k = 0; k < mode_count; ++k)
            if (voice.modes[k].increment < modal::low_limit && voice.targets[k] < modal::low_limit) low_set |= 1u << k;
        if (voice.knock.increment < modal::low_limit) low_set |= 1u << mode_count;
    }
    if (!low_set) {
        voice.low_set = 0;
        for (unsigned k = 0; k < mode_count; ++k) {
            modal::Mode& mode = voice.modes[k];
            if (mode.level < 0x00010000) continue;
            const i32 amount = k < upper ? b.lower : b.upper_level;
            modal::ring_glide_fall(mode, fall, amount, amount, voice.targets[k], mixed);
        }
        if (voice.knock.level >= 0x00010000) modal::ring_fall(voice.knock, b.knock_fall, b.knock_amount, b.knock_amount, mixed);
    } else {
        // A pitch or a control can move a mode across the limit: the line then starts afresh.
        const bool opening = voice.low_set != low_set;
        voice.low_set = low_set;
        modal::Low low;
        for (unsigned i = 0; i <= modal::low_frames; ++i) low.points[i] = 0;
        if (!opening) low.points[0] = voice.low_end;
        for (unsigned k = 0; k < mode_count; ++k) {
            modal::Mode& mode = voice.modes[k];
            if (mode.level < 0x00010000) continue;
            const i32 amount = k < upper ? b.lower : b.upper_level;
            if (low_set >> k & 1u) modal::ring_glide_low(mode, fall, amount, voice.targets[k], low, opening);
            else modal::ring_glide_fall(mode, fall, amount, amount, voice.targets[k], mixed);
        }
        if (voice.knock.level >= 0x00010000) {
            if (low_set >> mode_count & 1u) modal::ring_fall_low(voice.knock, b.knock_fall, b.knock_amount, low, opening);
            else modal::ring_fall(voice.knock, b.knock_fall, b.knock_amount, b.knock_amount, mixed);
        }
        voice.low_end = low.points[modal::low_frames];
        modal::spread_low(low.points, mixed);
    }
    if (voice.haze >= 0x00010000) {
        const i32 next = multiply(voice.haze, fall);
        i32 level = multiply(voice.haze, b.haze);
        const i32 slope = (multiply(next, b.haze) - level) / 64;
        voice.haze = next;
        add_haze(voice, level, slope, mixed);
    }
    modal::finish_doubled(mixed, output);
}
}
}
