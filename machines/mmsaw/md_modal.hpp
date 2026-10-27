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

namespace md_mmsaw {} namespace md = md_mmsaw; namespace md_mmsaw {
namespace modal {
struct Mode {
    u32 phase;
    u32 increment;   // phase increment at the block's start
    i32 level;       // Q31 amplitude at the block's start, never negative
};

// The sine table's top 16 bits, -32768..32767: floor(sine / 65536), without a
// signed shift.
inline i32 sine16(u32 phase) {
    return static_cast<i32>((static_cast<u32>(sine_table[phase >> 22]) + 0x80000000u) >> 16) - 32768;
}

// One block of a mode, added to `out`: its level falls by `step` per sample
// (md::decay), drawn as a straight line from now to the block's end. What it
// adds is scaled by `amount` (Q31, not negative) at the block's start, moving
// to `amount_next` at its end (a mode swelling in); the pitch glides in a
// straight line to `target`.
inline void ring_glide(Mode& mode, i32 step, i32 amount, i32 amount_next, u32 target, i32* out) {
    const i32 next = multiply(mode.level, per_block(step));
    // The level as Q16 (0..65535), so a 16-bit sine times it is the Q31 product.
    const i32 start = static_cast<i32>(static_cast<u32>(multiply(mode.level, amount)) >> 15);
    const i32 end = static_cast<i32>(static_cast<u32>(multiply(next, amount_next)) >> 15);
    // Kept 8 bits finer while it ramps, so it lands within a step of its end.
    i32 level = start * 256;
    const i32 slope = (end - start) * 4;   // (end - start) * 256 / 64
    const i32 bend = (static_cast<i32>(target) - static_cast<i32>(mode.increment)) / 64;
    u32 phase = mode.phase, increment = mode.increment;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += sine16(phase) * static_cast<i32>(static_cast<u32>(level) >> 8);
        phase += increment;
        increment += static_cast<u32>(bend);
        level += slope;
    }
    mode.phase = phase;
    mode.increment = target;
    mode.level = next;
}
inline void ring_glide(Mode& mode, i32 step, i32 amount, u32 target, i32* out) {
    ring_glide(mode, step, amount, amount, target, out);
}

// The same at a steady pitch.
inline void ring(Mode& mode, i32 step, i32 amount, i32 amount_next, i32* out) {
    const i32 next = multiply(mode.level, per_block(step));
    const i32 start = static_cast<i32>(static_cast<u32>(multiply(mode.level, amount)) >> 15);
    const i32 end = static_cast<i32>(static_cast<u32>(multiply(next, amount_next)) >> 15);
    i32 level = start * 256;
    const i32 slope = (end - start) * 4;
    u32 phase = mode.phase;
    const u32 increment = mode.increment;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += sine16(phase) * static_cast<i32>(static_cast<u32>(level) >> 8);
        phase += increment;
        level += slope;
    }
    mode.phase = phase;
    mode.level = next;
}
inline void ring(Mode& mode, i32 step, i32 amount, i32* out) { ring(mode, step, amount, amount, out); }
}
}
