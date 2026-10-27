#pragma once
// Shared fixed-point DSP for the Machinedrum-style machines. The canonical copy
// lives in lib/; tools/sync_lib.py copies it into each machine folder, because a
// machine may only include its own headers and the SDK's.
//
// Everything is integer: the OP-XY's Blackfin has no floating point, no divide
// instruction and no 64-bit helpers in the module. Signals are Q31 and kept well
// below full scale inside a voice, so sums cannot overflow.
#include <opxy/q31.hpp>
#include "md_tables.hpp"

namespace md_mmpulse {} namespace md = md_mmpulse; namespace md_mmpulse {
using opxy::q31::multiply;
const i32 one = opxy::q31::maximum;
const i32 half_scale = 0x40000000;   // the SDK's output ceiling
const i32 octave = 65536;            // pitch and time offsets are Q16 octaves

inline i32 positive(i32 value) { return value < 0 ? 0 : value; }
inline i32 clamp(i32 value, i32 low, i32 high) { return value < low ? low : value > high ? high : value; }

// A parameter (Q31, 0..1) placed on a span: low + p * (high - low).
inline i32 span(i32 parameter, i32 low, i32 high) { return low + multiply(positive(parameter), high - low); }
// A parameter squared: finer control near zero, for amounts.
inline i32 squared(i32 parameter) { const i32 p = positive(parameter); return multiply(p, p); }

inline i32 add(i32 a, i32 b) {
    const u32 sum = static_cast<u32>(a) + static_cast<u32>(b);
    // Overflow happened when both operands share a sign the sum does not.
    if (~(static_cast<u32>(a) ^ static_cast<u32>(b)) & (static_cast<u32>(a) ^ sum) & 0x80000000u)
        return a < 0 ? -one : one;
    return static_cast<i32>(sum);
}
// value * 2^shift, saturating, for 0 <= shift < 31.
inline i32 gain(i32 value, u32 shift) {
    const i32 limit = one >> shift;
    if (value > limit) return one;
    if (value < -limit) return -one;
    return value * (static_cast<i32>(1) << shift);
}

// A value from a table of 9 measured points at the controls 0, 16, ..., 112, 127
// (Q31 0..1), interpolated linearly. The fitted mappings use these.
inline i32 table9(const i32* points, i32 parameter) {
    const u32 position = static_cast<u32>(positive(parameter)) >> 3;   // 0..2^28: 8 segments
    const u32 index = position >> 25;
    if (index >= 8) return points[8];
    const i32 fraction = static_cast<i32>((position & 0x1ffffffu) << 6);
    return points[index] + multiply(points[index + 1] - points[index], fraction);
}

// A phase increment times a ratio in Q28 (0..8): Hz offsets measured at middle C
// follow the played key this way.
inline u32 scale(u32 increment, i32 ratio) {
    // (increment / 2) * ratio / 2^31 = increment * ratio(real) / 16
    const u32 scaled = static_cast<u32>(multiply(static_cast<i32>(increment >> 1), positive(ratio)));
    return scaled > (0x7fffffffu >> 4) ? 0x7fffffffu : scaled << 4;
}

// Interpolated sine of a 32-bit phase.
inline i32 sine(u32 phase) {
    const u32 index = phase >> 22;
    const i32 fraction = static_cast<i32>((phase & 0x3fffffu) << 9);
    return sine_table[index] + multiply(sine_table[index + 1] - sine_table[index], fraction);
}

// A phase increment moved by a Q16 number of octaves (-16..+16), capped below
// half the sample rate.
inline u32 transpose(u32 increment, i32 octaves) {
    const u32 shifted = static_cast<u32>(octaves + 16 * octave);   // never negative: avoids signed shifts
    const u32 whole = shifted >> 16;
    const u32 index = (shifted >> 8) & 255u;
    const i32 fraction = static_cast<i32>((shifted & 255u) << 23);
    const i32 ratio = exp2_table[index] + multiply(exp2_table[index + 1] - exp2_table[index], fraction);
    // (increment / 2) * ratio(Q30) / 2^31 = increment * ratio / 4.
    const u32 base = static_cast<u32>(multiply(static_cast<i32>(increment >> 1), ratio));
    // base is a quarter of the scaled increment; whole is 16 at no transposition.
    u32 result;
    if (whole >= 14) {
        const u32 up = whole - 14;
        result = up >= 31 || base > (0x7fffffffu >> up) ? 0x7fffffffu : base << up;
    } else {
        result = base >> (14 - whole);
    }
    return result > 0x7fffffffu ? 0x7fffffffu : result;
}

// The per-sample multiplier that falls 60 dB over 2^(log2 samples) samples,
// log2 given in Q16. Shorter than 16 samples clamps there; longer than 24 s too.
inline i32 decay(i32 log2_samples) {
    const i32 position = clamp((log2_samples - decay_low_octave * octave) * 16, 0, decay_steps * octave - 1);
    const u32 p = static_cast<u32>(position);
    const u32 index = p >> 16;
    const i32 fraction = static_cast<i32>((p & 0xffffu) << 15);
    return decay_table[index] + multiply(decay_table[index + 1] - decay_table[index], fraction);
}

// Half the state-variable frequency coefficient for a cutoff of
// 20 Hz * 2^(octaves above 20 Hz), Q16 octaves, 0..9.
inline i32 cutoff(i32 octaves_above_20hz) {
    const i32 position = clamp(octaves_above_20hz * 32, 0, svf_steps * octave - 1);
    const u32 p = static_cast<u32>(position);
    const u32 index = p >> 16;
    const i32 fraction = static_cast<i32>((p & 0xffffu) << 15);
    return svf_table[index] + multiply(svf_table[index + 1] - svf_table[index], fraction);
}

// e^-x for x in Q16 (0 .. 8; beyond 8 it is taken as 8), Q31: for screen curves.
inline i32 exp_neg(i32 x) {
    const u32 p = static_cast<u32>(clamp(x, 0, 8 * 65536 - 1)) * 32;   // Q16 table position
    const u32 index = p >> 16;
    const i32 fraction = static_cast<i32>((p & 0xffffu) << 15);
    return expneg_table[index] + multiply(expneg_table[index + 1] - expneg_table[index], fraction);
}

// Chamberlin state-variable filter. `frequency` is cutoff()'s half coefficient;
// `damping` is half the damping factor (one = 2, no resonance; small = ringing).
// Plain arithmetic, for speed: feed it at most an eighth of full scale and keep
// damping at or above min_damping, and the states stay well inside range.
const i32 min_damping = 0x06000000;
struct Svf {
    i32 low, band, high;
    void reset() { low = band = high = 0; }
    void run(i32 input, i32 frequency, i32 damping) {
        low += multiply(frequency, band) * 2;
        high = input - low - multiply(damping, band) * 2;
        band += multiply(frequency, high) * 2;
    }
};

// A multiplier raised to the 64th power: one block's worth of a per-sample decay.
inline i32 per_block(i32 step) {
    for (unsigned i = 0; i < 6; ++i) step = multiply(step, step);
    return step;
}

// Phase increments for one block of a pitch falling along an envelope, linear
// between the block's ends: two transposes per block instead of one per sample.
struct Glide {
    u32 increment;
    i32 slope;   // added each sample
    void plan(u32 base, i32 depth_now, i32 depth_next) {
        increment = transpose(base, depth_now);
        const u32 next = transpose(base, depth_next);
        slope = (static_cast<i32>(next) - static_cast<i32>(increment)) / 64;
    }
    u32 step() { const u32 now = increment; increment += static_cast<u32>(slope); return now; }
};

// One-pole low-pass, coefficient Q31 (one = no filtering).
inline i32 smooth(i32& state, i32 input, i32 coefficient) {
    state += multiply(input - state, coefficient);
    return state;
}

// White noise, xorshift32. A zero seed would stick at zero.
inline i32 noise(u32& seed) {
    u32 x = seed ? seed : 0x9e3779b9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    seed = x;
    return static_cast<i32>(x);
}

// White noise at an eighth of full scale, without a signed shift: the SVF's feed.
inline i32 noise_eighth(u32& seed) {
    noise(seed);
    return static_cast<i32>(seed >> 3) - 0x10000000;
}

// Cubic soft clip: x - x^3/3 on a full-scale input, peaking at 2/3.
inline i32 soft_clip(i32 x) {
    return x - multiply(multiply(multiply(x, x), x), 0x2aaaaaab);
}
// CLIP: drive by 2^(0..4) into the soft clip, fading from clean into clipped
// over the first eighth of the control so a little is a little warmth. The
// input is expected near quarter scale. Prepare once per block.
struct Drive {
    i32 mantissa, mix;   // the boost's 2^fraction (Q30) and the clipped share (Q31)
    u32 whole;           // the boost's whole doublings, after the first
    void prepare(i32 amount) {
        const u32 a = static_cast<u32>(positive(amount));
        const u32 octaves = a >> 13;                      // Q31 0..1 -> 0..4 octaves in Q16
        whole = 1 + (octaves >> 16);
        mantissa = exp2_table[(octaves >> 8) & 255u];
        mix = a < 0x10000000u ? static_cast<i32>(a * 8) : one;
    }
    i32 run(i32 x) const {
        if (!mix) return gain(x, 1);
        const i32 boosted = gain(x, 1);
        const i32 hot = gain(multiply(x, mantissa), whole + 1);   // x * 2^(fraction) * 2^whole
        return boosted + multiply(soft_clip(hot) - boosted, mix);
    }
};

// CLIP as on the Machinedrum's TRX machines (fitted on TRX-BD, reference/fit_bd.py):
// nothing below `onset`, then gain up to 32 x into a hard clip whose threshold
// falls from `level` to 0.3 of it, then a 30 Hz high-pass, which droops each
// clipped edge. `stretch` is 1 / (1 - onset), Q27. TRX-BD starts at half way,
// TRX-SD near 100. Prepare once per block; `highpass` is the voice's state.
struct Clip {
    i32 amount, drive, threshold;
    void prepare(i32 control, i32 level, i32 onset = 0x40000000, i32 stretch = 0x10000000) {
        const i32 c = positive(control) - onset;
        amount = c > 0 ? md::gain(multiply(c, stretch), 4) : 0;       // onset .. 1 -> 0 .. 1
        drive = 0x04000000 + multiply(amount, 0x7c000000);             // (1 .. 32) / 32
        threshold = multiply(level, one - multiply(multiply(amount, amount), 0x5999999a));
    }
    i32 run(i32 x, i32& highpass) const {
        if (!amount) return x;
        const i32 hot = clamp(md::gain(multiply(x, drive), 5), -threshold, threshold);
        highpass += multiply(hot - highpass, 9170000);                 // 2 pi 30 / 44100
        const i32 clipped = hot - highpass;
        return x + multiply(clipped - x, amount < 0x20000000 ? amount * 4 : one);
    }
};

// State an engine's voices share (the SDK's Shared): closed hats choke open
// ones, as on a drum machine. Every engine carries it, so machines can move
// between engines unchanged.
struct Common {
    u32 closed_hats;   // closed hats started so far
    u32 strikes;       // notes started by machines that vary hit to hit (a seed)
    u32 previous_pitch;// the pitch of the note started last (SID-6581's MSRC = PRCH)
    u32 reserved[1];
};

// The SDK's output contract: never beyond half scale.
inline i32 finish(i32 x) { return clamp(x, -half_scale, half_scale); }
}
