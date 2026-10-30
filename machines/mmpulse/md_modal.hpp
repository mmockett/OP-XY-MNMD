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

namespace md_mmpulse {} namespace md = md_mmpulse; namespace md_mmpulse {
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
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += static_cast<i32>(static_cast<short>(sine16(phase))) * static_cast<short>(static_cast<u32>(level) >> 8);
        phase += increment;
        level += slope;
    }
}
// A long ring's level hardly moves in a block (under 0.4%, 58 dB under the mode, in steps
// that small at the block's rate): held, a fifth fewer cycles, and exact when it doesn't move at all.
__attribute__((noinline)) inline void sound_held(u32 phase, u32 increment, i32 level, i32* out) {
    const short held = static_cast<short>(level);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += static_cast<i32>(static_cast<short>(sine16(phase))) * held;
        phase += increment;
    }
}
__attribute__((noinline)) inline void sound_wide(u32 phase, u32 increment, i32 level, i32 slope, i32* out) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += sine16(phase) * static_cast<i32>(static_cast<u32>(level) >> 8);
        phase += increment;
        level += slope;
    }
}
__attribute__((noinline)) inline void sound_glide(u32 phase, u32 increment, i32 bend, i32 level, i32 slope, i32* out) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += static_cast<i32>(static_cast<short>(sine16(phase))) * static_cast<short>(static_cast<u32>(level) >> 8);
        phase += increment;
        increment += static_cast<u32>(bend);
        level += slope;
    }
}
__attribute__((noinline)) inline void sound_glide_wide(u32 phase, u32 increment, i32 bend, i32 level, i32 slope, i32* out) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += sine16(phase) * static_cast<i32>(static_cast<u32>(level) >> 8);
        phase += increment;
        increment += static_cast<u32>(bend);
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

// The voice's mix to its output at twice its level, stopping at the SDK's ceiling: what
// md::finish(md::gain(x, 1)) gives, with no branches in the loop.
__attribute__((noinline)) inline void finish_doubled(const i32* mixed, opxy::Output output) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = md::clamp(mixed[i], -half_scale / 2, half_scale / 2) * 2;
}
}
}
