#pragma once
// Modal synthesis for the physical-model machines (MD P-I): a struck body rings
// as a sum of decaying sines, its modes, each with its own frequency, level
// and decay. The P-I machines' models come down to how the strike sets each
// mode's level (where and how hard it is hit) and how the body sets the
// modes' frequencies and decays (size, tension, damping).
//
// Cheap enough for many modes: each mode reads the sine table without
// interpolation (1024 steps: its error is near -60 dB, under the decays'
// noise), multiplies it by its level in 16 bits (a plain integer product, no
// Q31 multiply), and its level and pitch move in straight lines across a
// block, planned once per block from a per-sample decay.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace modal {
struct Mode {
    u32 phase;
    u32 increment;   // phase increment at the block's start
    i32 level;       // Q31 amplitude at the block's start, never negative
};

// The sine table's top 16 bits, -32768..32767, which is floor(sine / 65536): the
// upper half of each little-endian word (the Blackfin's and every host's), read
// directly.
typedef short __attribute__((__may_alias__)) half_word;
inline i32 sine16(u32 phase) {
    return reinterpret_cast<const half_word*>(sine_table)[(phase >> 22) * 2 + 1];
}
// The same top halves as a table to step through, at base + (phase >> 22) * 2. The sample loops
// read the word whose address the sample before worked out, so the Blackfin moves each index to
// an address register a few instructions ahead of its load, not just before it, where the move
// would hold the load 4 cycles. (Written out in each loop: through a helper, GCC schedules it worse.)
inline const half_word* sine16_base() { return reinterpret_cast<const half_word*>(sine_table) + 1; }

// A mode whose level is under one step (Q16, 0..65535) all through a block adds exactly nothing:
// its loop is skipped, its phase and level kept moving, so if a control lifts it again it
// comes back where it would have been. The cases refuse anything higher: at 2 or 3 steps
// (-90 to -84 dBFS a mode) the many modes' sum went from the cymbals', PI-BD's and PI-SD's tails.
const i32 quiet = 1;

// The sample loops, out of line so each gets a hardware loop and a whole set of registers
// whatever it is called from. The level is Q16 with 8 bits more while it ramps, so it lands
// within a step of its end. Nearly always it fits a signed 16 bits, and the plain 16-by-16
// product is a single cycle against the 32-bit multiply's five.
__attribute__((noinline)) inline void sound(u32 phase, u32 increment, i32 level, i32 slope, i32* out) {
    const half_word* const base = sine16_base();
    const half_word* at = base + (phase >> 22) * 2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = *at;
        phase += increment;
        at = base + (phase >> 22) * 2;
        out[i] += s * static_cast<short>(static_cast<u32>(level) >> 8);
        level += slope;
    }
}
// A long ring's level hardly moves in a block (under 0.4%, 58 dB under the mode, in steps
// that small at the block's rate): held, a fifth fewer cycles, and exact when it doesn't move at all.
__attribute__((noinline)) inline void sound_held(u32 phase, u32 increment, i32 level, i32* out) {
    const short held = static_cast<short>(level);
    const half_word* const base = sine16_base();
    const half_word* at = base + (phase >> 22) * 2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = *at;
        phase += increment;
        at = base + (phase >> 22) * 2;
        out[i] += s * held;
    }
}
__attribute__((noinline)) inline void sound_wide(u32 phase, u32 increment, i32 level, i32 slope, i32* out) {
    const half_word* const base = sine16_base();
    const half_word* at = base + (phase >> 22) * 2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = *at;
        phase += increment;
        at = base + (phase >> 22) * 2;
        out[i] += s * static_cast<i32>(static_cast<u32>(level) >> 8);
        level += slope;
    }
}
__attribute__((noinline)) inline void sound_glide(u32 phase, u32 increment, i32 bend, i32 level, i32 slope, i32* out) {
    const half_word* const base = sine16_base();
    const half_word* at = base + (phase >> 22) * 2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = *at;
        phase += increment;
        at = base + (phase >> 22) * 2;
        increment += static_cast<u32>(bend);
        out[i] += s * static_cast<short>(static_cast<u32>(level) >> 8);
        level += slope;
    }
}
__attribute__((noinline)) inline void sound_glide_wide(u32 phase, u32 increment, i32 bend, i32 level, i32 slope, i32* out) {
    const half_word* const base = sine16_base();
    const half_word* at = base + (phase >> 22) * 2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = *at;
        phase += increment;
        at = base + (phase >> 22) * 2;
        increment += static_cast<u32>(bend);
        out[i] += s * static_cast<i32>(static_cast<u32>(level) >> 8);
        level += slope;
    }
}

// One block of a mode at a steady pitch: `start` and `end`, Q16, its level there.
inline void sound_steady(const Mode& mode, i32 start, i32 end, i32* out) {
    if (start < 32768 && end < 32768) {
        const i32 moved = end - start;
        if (moved <= 0 && -moved <= start >> 8) sound_held(mode.phase, mode.increment, (start + end) >> 1, out);
        else sound(mode.phase, mode.increment, start * 256, moved * 4, out);
    } else {
        sound_wide(mode.phase, mode.increment, start * 256, (end - start) * 4, out);
    }
}

// One block of a mode, added to `out`: its level falls by `fall` over the block (a
// per_block(step), the same for every mode that rings as long, so a voice works it out
// once). What it adds is scaled by `amount` (Q31, not negative) at the block's start, moving
// to `amount_next` at its end (a mode swelling in), and the pitch glides in a straight line
// to `target`.
inline void ring_glide_fall(Mode& mode, i32 fall, i32 amount, i32 amount_next, u32 target, i32* out) {
    const i32 next = multiply(mode.level, fall);
    // The level as Q16 (0..65535), so a 16-bit sine times it is the Q31 product.
    const i32 start = static_cast<i32>(static_cast<u32>(multiply(mode.level, amount)) >> 15);
    const i32 end = static_cast<i32>(static_cast<u32>(multiply(next, amount_next)) >> 15);
    const i32 bend = (static_cast<i32>(target) - static_cast<i32>(mode.increment)) / 64;
    if (start >= quiet || end >= quiet) {
        if (!bend) sound_steady(mode, start, end, out);
        else if (start < 32768 && end < 32768) sound_glide(mode.phase, mode.increment, bend, start * 256, (end - start) * 4, out);
        else sound_glide_wide(mode.phase, mode.increment, bend, start * 256, (end - start) * 4, out);
    }
    // A block's phase and pitch as the samples would have moved them (the bend's triangle: 63 * 64 / 2).
    mode.phase += (mode.increment << 6) + static_cast<u32>(bend) * 2016u;
    mode.increment = target;
    mode.level = next;
}
inline void ring_glide(Mode& mode, i32 step, i32 amount, i32 amount_next, u32 target, i32* out) {
    ring_glide_fall(mode, per_block(step), amount, amount_next, target, out);
}
inline void ring_glide(Mode& mode, i32 step, i32 amount, u32 target, i32* out) {
    ring_glide(mode, step, amount, amount, target, out);
}

// The same at a steady pitch.
inline void ring_fall(Mode& mode, i32 fall, i32 amount, i32 amount_next, i32* out) {
    const i32 next = multiply(mode.level, fall);
    const i32 start = static_cast<i32>(static_cast<u32>(multiply(mode.level, amount)) >> 15);
    const i32 end = static_cast<i32>(static_cast<u32>(multiply(next, amount_next)) >> 15);
    if (start >= quiet || end >= quiet) sound_steady(mode, start, end, out);
    mode.phase += mode.increment << 6;
    mode.level = next;
}
inline void ring(Mode& mode, i32 step, i32 amount, i32 amount_next, i32* out) {
    ring_fall(mode, per_block(step), amount, amount_next, out);
}
inline void ring(Mode& mode, i32 step, i32 amount, i32* out) { ring(mode, step, amount, amount, out); }

// Low modes at a quarter of the rate. A mode well under the sample rate hardly moves between
// samples, so after the strike's first blocks a voice can sound its low modes on every fourth
// sample, into a `Low`, and draw straight lines between: linear interpolation's images of a
// mode at 1 kHz sit about 40 dB under it, near 10 kHz, and fall 12 dB per octave as it drops.
// The modes' phases and levels end each block exactly where the full rate leaves them.
const unsigned low_frames = opxy::block_frames / 4;
struct Low {
    i32 points[low_frames + 1];   // [0] is the last block's end: the voice keeps it between blocks
};
__attribute__((noinline)) inline void sound_low(u32 phase, u32 increment, i32 level, i32 slope, i32* out) {
    const half_word* const base = sine16_base();
    phase += increment;
    const half_word* at = base + (phase >> 22) * 2;
    for (unsigned i = 0; i != low_frames; ++i) {
        const i32 s = *at;
        phase += increment;
        at = base + (phase >> 22) * 2;
        level += slope;
        out[i] += s * static_cast<short>(static_cast<u32>(level) >> 8);
    }
}
__attribute__((noinline)) inline void sound_low_wide(u32 phase, u32 increment, i32 level, i32 slope, i32* out) {
    const half_word* const base = sine16_base();
    phase += increment;
    const half_word* at = base + (phase >> 22) * 2;
    for (unsigned i = 0; i != low_frames; ++i) {
        const i32 s = *at;
        phase += increment;
        at = base + (phase >> 22) * 2;
        level += slope;
        out[i] += s * static_cast<i32>(static_cast<u32>(level) >> 8);
    }
}
// A mode's block into `low`, as ring_fall into a whole block. On the first block at this rate,
// `opening` also gives it the point where the block starts.
inline void ring_fall_low(Mode& mode, i32 fall, i32 amount, Low& low, bool opening) {
    const i32 next = multiply(mode.level, fall);
    const i32 start = static_cast<i32>(static_cast<u32>(multiply(mode.level, amount)) >> 15);
    const i32 end = static_cast<i32>(static_cast<u32>(multiply(next, amount)) >> 15);
    if (start >= quiet || end >= quiet) {
        if (opening) low.points[0] += sine16(mode.phase) * start;
        if (start < 32768 && end < 32768) sound_low(mode.phase, mode.increment << 2, start * 256, (end - start) * 16, low.points + 1);
        else sound_low_wide(mode.phase, mode.increment << 2, start * 256, (end - start) * 16, low.points + 1);
    }
    mode.phase += mode.increment << 6;
    mode.level = next;
}
// The same for a gliding mode: the phase and the level land as ring_glide_fall's do, and so do
// the points, the phases being those of every fourth sample of the full rate's glide.
__attribute__((noinline)) inline void sound_low_glide(u32 phase, u32 increment, i32 bend, i32 level, i32 slope, i32* out) {
    const half_word* const base = sine16_base();
    phase += increment;
    increment += static_cast<u32>(bend);
    const half_word* at = base + (phase >> 22) * 2;
    for (unsigned i = 0; i != low_frames; ++i) {
        const i32 s = *at;
        phase += increment;
        increment += static_cast<u32>(bend);
        at = base + (phase >> 22) * 2;
        level += slope;
        out[i] += s * static_cast<short>(static_cast<u32>(level) >> 8);
    }
}
__attribute__((noinline)) inline void sound_low_glide_wide(u32 phase, u32 increment, i32 bend, i32 level, i32 slope, i32* out) {
    const half_word* const base = sine16_base();
    phase += increment;
    increment += static_cast<u32>(bend);
    const half_word* at = base + (phase >> 22) * 2;
    for (unsigned i = 0; i != low_frames; ++i) {
        const i32 s = *at;
        phase += increment;
        increment += static_cast<u32>(bend);
        at = base + (phase >> 22) * 2;
        level += slope;
        out[i] += s * static_cast<i32>(static_cast<u32>(level) >> 8);
    }
}
inline void ring_glide_low(Mode& mode, i32 fall, i32 amount, u32 target, Low& low, bool opening) {
    const i32 next = multiply(mode.level, fall);
    const i32 start = static_cast<i32>(static_cast<u32>(multiply(mode.level, amount)) >> 15);
    const i32 end = static_cast<i32>(static_cast<u32>(multiply(next, amount)) >> 15);
    const i32 bend = (static_cast<i32>(target) - static_cast<i32>(mode.increment)) / 64;
    if (start >= quiet || end >= quiet) {
        if (opening) low.points[0] += sine16(mode.phase) * start;
        // Four samples' pitch (the bend's triangle: 3 * 4 / 2), which moves 16 bends per point.
        const u32 quarter = (mode.increment << 2) + static_cast<u32>(bend) * 6u;
        if (start < 32768 && end < 32768) sound_low_glide(mode.phase, quarter, bend * 16, start * 256, (end - start) * 16, low.points + 1);
        else sound_low_glide_wide(mode.phase, quarter, bend * 16, start * 256, (end - start) * 16, low.points + 1);
    }
    mode.phase += (mode.increment << 6) + static_cast<u32>(bend) * 2016u;
    mode.increment = target;
    mode.level = next;
}
// A mode over about 900 Hz (an increment at 44.1 kHz) stays at the full rate.
const u32 low_limit = 87654000u;
// The straight lines, added to the voice's mix.
__attribute__((noinline)) inline void spread_low(const i32* points, i32* out) {
    i32 now = points[0];
    for (unsigned i = 0; i != low_frames; ++i) {
        const i32 next = points[i + 1], step = (next - now) >> 2;
        out[0] += now; now += step;
        out[1] += now; now += step;
        out[2] += now; now += step;
        out[3] += now;
        now = next;
        out += 4;
    }
}

// md::noise and md::noise_eighth for a sample loop: xorshift32 never reaches zero, so the loop
// takes `live_seed` once and each sample skips md::noise's check for it.
inline u32 live_seed(u32 seed) { return seed ? seed : 0x9e3779b9u; }
inline i32 noise_on(u32& seed) {
    u32 x = seed;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    seed = x;
    return static_cast<i32>(x);
}
inline i32 noise_eighth_on(u32& seed) {
    noise_on(seed);
    return static_cast<i32>(seed >> 3) - 0x10000000;
}

// Modes falling alike share one ramp over their sum: each sounds at its held level, into `plate`.
inline void ring_fall_plate(Mode& mode, i32 fall, i32 amount, i32* plate, i32* out) {
    const i32 next = multiply(mode.level, fall);
    const i32 start = static_cast<i32>(static_cast<u32>(multiply(mode.level, amount)) >> 15);
    const i32 end = static_cast<i32>(static_cast<u32>(multiply(next, amount)) >> 15);
    if (start >= quiet || end >= quiet) {
        if (start < 32768) sound_held(mode.phase, mode.increment, start, plate);
        else sound_wide(mode.phase, mode.increment, start * 256, (end - start) * 4, out);
    }
    mode.phase += mode.increment << 6;
    mode.level = next;
}
__attribute__((noinline)) inline void spread_plate(const i32* plate, i32 fall, i32* out) {
    i32 gain = md::one;
    const i32 slope = (fall - md::one) / 64;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += multiply(plate[i], gain);
        gain += slope;
    }
}

// The voice's mix to its output at twice its level, stopping at the SDK's ceiling: what
// md::finish(md::gain(x, 1)) gives, with no branches in the loop.
__attribute__((noinline)) inline void finish_doubled(const i32* mixed, opxy::Output output) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = md::clamp(mixed[i], -half_scale / 2, half_scale / 2) * 2;
}
}
}
