// amp_curve.h: stock OS 1.2.1's amp envelope as times and per-frame factors,
// for machine screens that follow it. Closed-form fits, no tables; integer
// only, no divides, and no 64-bit multiplies or variable 64-bit shifts (on the
// Blackfin those call libgcc's __muldi3, __ashldi3 and __lshrdi3, which the
// macOS and Linux toolchains don't carry), so it needs no arithmetic runtime.
// Against the emulated stage machine, every index 0..127:
// attack time within 1.4%, decay and release time constants within 0.4%,
// decay and release stage times within 2.3% (amp_curve_test.cpp).
//
// Use: copy into the machine's folder and include it inside your own
// namespace, once per translation unit:
//     namespace mymachine { namespace amp {
//     #include "amp_curve.h"
//     } }
// Everything is static inline and it includes nothing, so several machines
// can link together without clashing.
//
// Directions (stock reads only word >> 24, 0..127, of each Q31 word):
//   attack   larger word -> LONGER  (concave rise, 1 block .. ~6 min)
//   decay    larger word -> LONGER  (exponential fall to sustain + 1%)
//   sustain  larger word -> HIGHER  (linear gain, word / 2^31)
//   release  larger word -> SHORTER (inverted: same curve as decay, mirrored;
//            a preset's release 32767 is stock's shortest release, ~6 ms)
// Release fades from the current level to -66 dB (0.0005). Times are rounded
// up to stock's 64-sample blocks at 44.1 kHz, as stock leaves a stage only at
// a block boundary. A negative word reads as 0; check known() first.
#pragma once

static_assert(sizeof(unsigned) == 4 && sizeof(unsigned long long) == 8, "amp_curve.h needs 32-bit unsigned");

namespace amp_curve_detail {
typedef unsigned u32;
typedef int i32;
typedef unsigned long long u64;
typedef long long i64;

static inline u32 top7(i32 word) { return word < 0 ? 0u : u32(word) >> 24; }

// a * b in full, from 16-bit halves: 32-bit multiplies only.
static inline u64 mul32(u32 a, u32 b) {
    const u32 al = a & 0xffffu, ah = a >> 16, bl = b & 0xffffu, bh = b >> 16;
    const u64 mid = u64(al * bh) + ah * bl;
    return (u64(ah * bh) << 32) + (mid << 16) + al * bl;
}
// a * b modulo 2^64, as a 64-bit multiply gives it.
static inline u64 mul64(u64 a, u32 b) { return (mul32(u32(a >> 32), b) << 32) + mul32(u32(a), b); }
// m * 2^n for -32 <= n <= 45 and m < 2^18, as 64-bit shifts give it.
static inline u64 shifted(u32 m, i32 n) {
    if (n >= 32) return u64(m << (n - 32)) << 32;
    if (n > 0) return (u64(m >> (32 - n)) << 32) | u32(m << n);
    return n > -32 ? u64(m >> -n) : 0u;
}
// floor(a * f / 2^16) for 0 <= f < 2^16.
static inline i32 mul_q16(i32 a, u32 f) { return (a >> 16) * i32(f) + i32((u32(a & 0xffff) * f) >> 16); }

// 2^x, x in Q16 (clamped to -32..46), result in Q16.
static inline u64 exp2_q16(i64 x) {
    if (x <= -(i64(32) << 16)) return 0;
    if (x >= i64(46) << 16) x = (i64(46) << 16) - 1;
    const u64 ux = u64(x + (i64(32) << 16));
    const i32 n = i32(ux >> 16) - 32;
    const u32 f = u32(ux & 0xffffu);   // 2^f - 1 as a quartic in f, exact at the ends, error 5e-6
    // Each product stays under 2^32: the bracket under f is at most 65533.
    const u32 m = 65536u + ((f * (45416u + ((f * (15831u + ((f * (3392u + ((f * 897u) >> 16))) >> 16))) >> 16))) >> 16);
    return shifted(m, n);
}

// log2(v) in Q16 for v > 0 (v == 0 reads as 1).
static inline i32 log2_q16(u32 v) {
    if (!v) v = 1;
    i32 n = 0;
    while ((v >> n) > 1u) ++n;
    const u32 f = (n >= 16 ? v >> (n - 16) : v << (16 - n)) - 65536u;   // mantissa - 1, Q16
    // log2(1 + f) as a quartic in f, exact at the ends, error 1.5e-4.
    const i32 p = mul_q16(94246 + mul_q16(-44222 + mul_q16(20775 + mul_q16(-5263, f), f), f), f);
    return (n << 16) + p;
}

// Microseconds rounded up to whole 64-sample blocks (1451.247 us at 44.1 kHz).
static inline u32 ceil_blocks_us(u32 us) {
    const u64 blocks = (mul32(us, 46243u) + ((u64(1) << 26) - 1)) >> 26;
    return u32(mul32(u32(blocks), 1486077u) >> 10);
}

// One-pole time constant (1/e) of coefficient index j = 0..127, us.
// Fit: 5.66 ms * j + 0.26 ms * 2^(j / 8.7); j = 0 is stock's fastest, 0.763 ms.
static inline u32 tau_us(u32 j) {
    if (!j) return 763u;
    return 5660u * j + u32(mul32(u32(exp2_q16(i32(j) * 7533)), 260u) >> 16);
}

// 2^-(c * dt / t) in Q15 (32768 = 1.0), with log2(c) in Q16: no divide.
static inline u32 neg_pow2_ratio_q15(u32 dt_us, u32 t_us, i32 log2c_q16) {
    if (!dt_us) return 32768u;
    if (!t_us) return 0;
    const i64 e = i64(log2_q16(dt_us)) - log2_q16(t_us) + log2c_q16;   // log2(c dt / t)
    if (e >= (i64(5) << 16)) return 0;                                 // 2^-32 or less
    const u64 x = exp2_q16(e);                                         // c dt / t, Q16
    return u32((exp2_q16(-i64(x)) + 1) >> 1);
}

// Continuous note-on to 0.99 for attack index i >= 2, us: 2^(7.8945 + 0.1261 i) samples.
static inline u32 attack_raw_us(u32 i) {
    return u32(mul64(exp2_q16(517375 + i32(i) * 8264), 1486077u) >> 32);
}
}  // namespace amp_curve_detail

// True when the four words are real (the SDK sends -1 when unavailable).
// Same as opxy::draw::envelope_known.
static inline bool known(int attack, int decay, int sustain, int release) {
    return attack >= 0 && decay >= 0 && sustain >= 0 && release >= 0;
}

// Sustain as linear gain, Q15 (32767 at the top word).
static inline int sustain_q15(int sustain) { return sustain < 0 ? 0 : sustain >> 16; }

// Note-on to peak (0.99), us and ms. Larger word, longer attack.
static inline unsigned attack_us(int attack) {
    using namespace amp_curve_detail;
    const u32 i = top7(attack);
    return i < 2 ? ceil_blocks_us(1) : ceil_blocks_us(attack_raw_us(i));
}
static inline unsigned attack_ms(int attack) {
    return unsigned((amp_curve_detail::mul32(attack_us(attack), 4294967u) + (1u << 31)) >> 32);
}

// Decay time constant (1/e), us and ms. Larger word, longer decay. Index 0 reads as 1.
static inline unsigned decay_tau_us(int decay) {
    using namespace amp_curve_detail;
    const u32 i = top7(decay);
    return tau_us(i ? i : 1u);
}
static inline unsigned decay_tau_ms(int decay) {
    return unsigned((amp_curve_detail::mul32(decay_tau_us(decay), 4294967u) + (1u << 31)) >> 32);
}

// Release time constant (1/e), us and ms. INVERTED: larger word, SHORTER release.
static inline unsigned release_tau_us(int release) {
    using namespace amp_curve_detail;
    return tau_us(127u - top7(release));
}
static inline unsigned release_tau_ms(int release) {
    return unsigned((amp_curve_detail::mul32(release_tau_us(release), 4294967u) + (1u << 31)) >> 32);
}

// Peak to sustain + 1%: tau * -ln(sustain + 0.01), us and ms. 0 when sustain >= 0.99.
static inline unsigned decay_us(int decay, int sustain) {
    using namespace amp_curve_detail;
    const u32 v = u32(sustain_q15(sustain)) + 328u;          // sustain + 0.01, Q15
    if (v >= 32768u) return 0;
    const u32 neg_ln = u32(mul32(u32((15 << 16) - log2_q16(v)), 45426u) >> 16);   // -ln, Q16
    return ceil_blocks_us(u32(mul32(decay_tau_us(decay), neg_ln) >> 16));
}
static inline unsigned decay_ms(int decay, int sustain) {
    return unsigned((amp_curve_detail::mul32(decay_us(decay, sustain), 4294967u) + (1u << 31)) >> 32);
}

// Full level to stock's -66 dB cut-off: tau * ln(2000), us and ms. Larger word, SHORTER.
static inline unsigned release_us(int release) {
    using namespace amp_curve_detail;
    return ceil_blocks_us(u32(mul32(release_tau_us(release), 498133u) >> 16));
}
static inline unsigned release_ms(int release) {
    return unsigned((amp_curve_detail::mul32(release_us(release), 4294967u) + (1u << 31)) >> 32);
}

// Per-frame factors, Q15 (32768 = 1.0); compute when a word changes, not per frame.
// dt_us defaults to one 60 fps frame. From input.elapsed (Q10 s): dt_us = elapsed * 15625 >> 4.
//
// fade_q15(tau, dt) = exp(-dt / tau). Decay: level = sustain + 1% floor of
//   level * decay_frame_q15 >> 15; release: level = level * release_frame_q15 >> 15
//   until it falls under 0.0005 (16 in Q15, so effectively 0).
static inline unsigned fade_q15(unsigned tau_us, unsigned dt_us = 16667u) {
    return amp_curve_detail::neg_pow2_ratio_q15(dt_us, tau_us, 34653);   // log2(log2 e)
}
static inline unsigned decay_frame_q15(int decay, unsigned dt_us = 16667u) {
    return fade_q15(decay_tau_us(decay), dt_us);
}
static inline unsigned release_frame_q15(int release, unsigned dt_us = 16667u) {
    return fade_q15(release_tau_us(release), dt_us);
}

// Attack is concave: L(u) = 2 (1 - 0.505^u) over its time, so per frame
// (2 - L) shrinks by g = 0.505^(dt / T): level = 65536 - ((65536 - level) * g >> 15)
// in Q15, until level >= attack_peak_q15, when stock moves to decay. Past ~10 s
// the per-frame step is a few Q15 steps, so the drawn rise is coarse there.
static const int attack_peak_q15 = 32440;   // 0.99
static inline unsigned attack_frame_q15(int attack, unsigned dt_us = 16667u) {
    using namespace amp_curve_detail;
    const u32 i = top7(attack);
    if (i < 2) return 0;                                                  // peak at once
    return neg_pow2_ratio_q15(dt_us, attack_raw_us(i), -1367);          // log2(-log2 0.505)
}
