#pragma once
// FM+STAT, FM+PAR and FM+DYN: the sounds, modules any engine can hold
// (docs/VOICES.md). Written from scratch in fixed point and fitted to the
// Monomachine's own behaviour (reference/fit_fmp.py, reference/README.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

// What the Monomachine's FM+ machines were found to be: one sine carrier at
// the note, frequency-modulated by two (STAT, DYN) or three (PAR) sine
// modulators in parallel, each at a ratio of the carrier.
//
// - True FM: each modulator moves the carrier's frequency by index x its own
//   frequency, so a modulator with a DC part (from feedback) shifts the
//   carrier's pitch, as the original's does.
// - Depth (2VOL, 1VOL, and an ENV control up to its middle) is an index of
//   (pi/2)(v/64)^2 radians up to v = 64, then a straight line to about 14 at 127.
// - An ENV control above its middle is a decay: the index falls exponentially
//   from pi/2 (on FM+PAR from the depth curve's own value at v), with a time
//   constant from about 20 s at 72 to 6 ms at 127; it does not follow the key.
// - Ratios: STAT and PAR step through 24 listed ratios, 1/64 to 4; DYN's are
//   continuous, 1FRQ = v/64 and 2FRQ = (v/64)^2, with 127 taken as 128.
// - TONE is a low-pass on the modulators, so it darkens high ratios first and
//   high notes more than low ones; DYN has a fixed one. TUNE is +-2 semitones.
namespace md_mmpar {} namespace md = md_mmpar; namespace md_mmpar {
namespace fmp {
using md::multiply;

// The listed ratios, Q28, in the order the sound steps through them. (The
// Monomachine's LCD shows 5/32 five steps later than the sound plays it.)
static const i32 ratios[24] = {
    4194304, 8388608, 16777216, 25165824, 33554432, 41943040, 50331648, 67108864,
    83886080, 100663296, 117440512, 134217728, 167772160, 201326592, 234881024, 268435456,
    335544320, 402653184, 469762048, 536870912, 671088640, 805306368, 939524096, 1073741824,
};
// A stepped control's step, as the SDK quantizes an enum (Q31 to Q15, then n steps).
inline u32 step(i32 parameter, u32 steps) {
    const u32 q15 = static_cast<u32>(md::positive(parameter)) >> 16;
    const u32 s = (q15 * steps) >> 15;
    return s < steps ? s : steps - 1;
}
inline i32 ratio(i32 parameter) { return ratios[step(parameter, 24)]; }
// DYN: 1FRQ = v / 64 (Q28), 127 as 128; 2FRQ its square.
inline i32 dyn_ratio1(i32 parameter) {
    const i32 p = md::positive(parameter);
    return p >= 0x7f7f7f7f ? 0x20000000 : multiply(p, 532676608);   // 127 * 2^22
}
inline i32 dyn_ratio2(i32 parameter) {
    const i32 r = dyn_ratio1(parameter);
    return multiply(r, r) * 8;
}

// 17 points, interpolated at a position in Q16 (0 .. 16).
inline i32 table17(const i32* points, i32 position_q16) {
    const u32 p = static_cast<u32>(md::clamp(position_q16, 0, 16 * 65536 - 1));
    const u32 index = p >> 16;
    const i32 fraction = static_cast<i32>((p & 0xffffu) << 15);
    return points[index] + multiply(points[index + 1] - points[index], fraction);
}
inline i32 steps_of(i32 parameter) { return multiply(md::positive(parameter), 8323072); }   // v, Q16 (127 << 16)

// The depth curve: the index at v = 0, 8, ..., 128, in radians (Q24).
static const i32 depth_curve[17] = {
    0, 335544, 1677722, 3858760, 6878659, 10737418, 15435039, 21139292, 26508001,
    51002737, 77342966, 102844334, 129184563, 155189248, 180019528, 207030845, 233203302,
};
const i32 depth_slope = 12779;      // per step past 128, Q24 radians / 256 (DYN's 1VEN reaches there)
// The index at v steps (Q16, any sign or size): negative steps invert the modulator.
inline i32 depth_at(i32 steps_q16) {
    const i32 s = steps_q16 < 0 ? -steps_q16 : steps_q16;
    i32 d;
    if (s < 128 * 65536) d = table17(depth_curve, static_cast<i32>(static_cast<u32>(s) >> 3));
    else d = depth_curve[16] + static_cast<i32>(static_cast<u32>(md::clamp(s - 128 * 65536, 0, 192 * 65536)) >> 8) * depth_slope;
    return steps_q16 < 0 ? -d : d;
}
inline i32 depth(i32 parameter) { return depth_at(steps_of(parameter)); }
const i32 quarter_turn = 26353589;   // pi / 2, Q24: where STAT's and DYN's decays start
const i32 middle = 1082196484;       // 64 of 0..127, Q31
inline bool decays(i32 parameter) { return parameter > middle; }

// A decay's time constant as log2 samples (Q16) at v = 64, 68, ..., 128.
// The ENV controls': 72 about 22 s, 80 1.5 s, 96 95 ms, 124 7.6 ms.
static const i32 env_times[17] = {
    1881945, 1484869, 1303375, 1155408, 1050086, 965924, 897288, 836665, 788566,
    745118, 704388, 660434, 626986, 604084, 580323, 549762, 524207,
};
// DYN's 1FEN: 72 5.4 s, 80 2 s, 96 0.25 s, 112 15 ms; 1VEN's run a little longer.
static const i32 fen_times[17] = {
    1881945, 1332700, 1170568, 1124005, 1076658, 1032219, 990024, 935624, 880050,
    802427, 727880, 662344, 614046, 600516, 584721, 510174, 396340,
};
static const i32 ven_times[17] = {
    1881945, 1332700, 1180530, 1129568, 1085669, 1033720, 977399, 945586, 917117,
    843586, 778050, 717918, 658484, 614046, 565748, 489076, 396340,
};
// One block's multiplier for that decay at control v > 64: e^(-64 / tau), as
// (1 - 2^-log2 tau)^64.
inline i32 decay_per_block(const i32* times, i32 parameter) {
    const i32 above = steps_of(parameter) - 64 * 65536;   // 4 steps a point
    const i32 log2_tau = table17(times, above <= 0 ? 0 : static_cast<i32>(static_cast<u32>(above) >> 2));
    const u32 e = static_cast<u32>(47 * 65536 - log2_tau);   // 16 + 31 - log2 tau, never negative
    const u32 shift = (e >> 16) - 16;                        // whole octaves of 2^(31 - log2 tau): 2 .. 23
    const i32 mantissa = exp2_table[(e >> 8) & 255u];         // 2^fraction, Q30
    const i32 loss = shift >= 30 ? md::one : static_cast<i32>(static_cast<u32>(mantissa) >> (30 - shift));
    return md::per_block(md::one - loss);
}

// log2 of a phase increment, Q16 (for the filters' tables).
inline i32 log2_of(u32 x) {
    if (!x) return 0;
    i32 whole = 31;
    while (!(x & 0x80000000u)) { x <<= 1; --whole; }
    // x = 2^31 (1 + f): log2(1 + f) ~ f + 0.34 f (1 - f), within 0.005 octave.
    const u32 f = (x & 0x7fffffffu) >> 15;                    // Q16
    const u32 bend = (f * (65536u - f)) >> 16;                // f (1 - f), Q16
    return whole * 65536 + static_cast<i32>(f + ((bend * 22282u) >> 16));
}

// TONE's low-pass on a modulator: its gain (Q31) by TONE (rows at v = 0, 16,
// ..., 112, 127) and the modulator's frequency (columns at octaves of 65.4 Hz,
// 65 Hz to 16.7 kHz), as measured to 4186 Hz and closing above. DYN's fixed
// filter is its own row, near TONE 70.
static const i32 tone_gain[9][9] = {
    {322122547, 107374182, 21474836, 0, 0, 0, 0, 0, 0},
    {1181116006, 429496730, 171798692, 42949673, 0, 0, 0, 0, 0},
    {1932735283, 1331439862, 858993459, 322122547, 150323855, 42949673, 0, 0, 0},
    {0x7fffffff, 1889785610, 1825361101, 1073741824, 493921239, 171798692, 21474836, 0, 0},
    {0x7fffffff, 0x7fffffff, 0x7fffffff, 1825361101, 1288490189, 493921239, 27917287, 0, 0},
    {0x7fffffff, 0x7fffffff, 0x7fffffff, 2040109466, 1932735283, 1395864371, 644245094, 107374182, 0},
    {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 1825361101, 1073741824, 322122547, 0},
    {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 1889785610, 1288490189, 429496730, 0},
    {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 1975684956, 1331439862, 536870912, 0},
};
static const i32 dyn_gain[9] = {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 1868310774, 1760936591, 214748365, 21474836, 0};
const i32 lowest_log2 = 1481346;   // log2_of(6369751), 65.4 Hz, Q16

inline i32 along(const i32* row, i32 octaves_q16) {
    const u32 p = static_cast<u32>(md::clamp(octaves_q16, 0, 8 * 65536 - 1));
    const u32 i = p >> 16;
    const i32 f = static_cast<i32>((p & 0xffffu) << 15);
    return row[i] + multiply(row[i + 1] - row[i], f);
}
inline i32 tone_filter(i32 tone, u32 increment) {
    const i32 octaves = log2_of(increment) - lowest_log2;
    const u32 position = static_cast<u32>(md::positive(tone)) >> 3;   // 0 .. 2^28: 8 rows apart
    const u32 row = position >> 25;
    if (row >= 8) return along(tone_gain[8], octaves);
    const i32 f = static_cast<i32>((position & 0x1ffffffu) << 6);
    const i32 a = along(tone_gain[row], octaves), b = along(tone_gain[row + 1], octaves);
    return a + multiply(b - a, f);
}
inline i32 dyn_filter(u32 increment) { return along(dyn_gain, log2_of(increment) - lowest_log2); }

// TUNE: +-2 semitones, (v - 64) / 32 of a semitone, in Q16 octaves.
inline i32 tune(i32 parameter) { return multiply(md::positive(parameter), 21675) - 10923; }

// A modulator's swing of the carrier's increment for an index (Q24 radians):
// index x its own increment, the frequency deviation of FM.
inline i32 swing(i32 index, u32 increment) {
    const i32 inc = static_cast<i32>(increment > 0x7fffffffu ? 0x7fffffffu : increment);
    return md::gain(multiply(inc, md::positive(index)) , 7) * (index < 0 ? -1 : 1);
}

// The feedback (1FB, 2FB): a modulator's sine of its phase plus feedback x
// its own output, solved by two steps from the last sample's, as an analog-like
// instantaneous loop: up to 32 it only adds harmonics; from about 48 the wave
// tips over and its DC part shifts the carrier's pitch up, as the original's
// does; past 64 it turns rough. Radians at v = 0, 16, ..., 112, 127 (Q28 cycles).
static const i32 feedback_curve[9] = {0, 21361415, 33323807, 43577286, 46995113, 59811962, 85445659, 111079357, 136713055};
inline i32 feedback(i32 parameter) { return md::table9(feedback_curve, parameter); }

// One modulator.
struct Op {
    u32 phase, increment;
    i32 swing;        // the carrier's swing at the end of the last block
    i32 envelope;     // its decay, Q31 of the start
    i32 history[1];   // its last output, for feedback
};
// The phases a note starts from, fitted on FM+STAT at ratio 1 (where the
// sidebands fold onto each other and the phases set the harmonics): each
// modulator 3.4 radians in, the carrier 4.6 radians plus 0.967 x the indices
// it starts with (the deviation it would have gathered from there).
const u32 start_phase = 2324121937u;
inline void start(Op& op) { op.phase = start_phase; op.envelope = md::one; op.history[0] = 0; }
inline u32 carrier_start(i32 indices) {   // the starting indices' sum, Q24 radians
    return 3144400268u + static_cast<u32>(indices) * 39u + static_cast<u32>(multiply(indices, 839614007));
}

// Where a modulator's index goes this block from its depth or ENV control:
// up to the middle, the depth curve; above, the decay from `peak`.
inline i32 target(Op& op, i32 control, i32 peak, bool fresh) {
    if (!decays(control)) return depth(control);
    if (!fresh) op.envelope = multiply(op.envelope, decay_per_block(env_times, control));
    return multiply(peak, op.envelope);
}

// A modulator's sample: its sine, with feedback (Q28 cycles) when given.
inline i32 run(Op& op) {
    const i32 y = md::sine(op.phase);
    op.phase += op.increment;
    return y;
}
inline i32 run(Op& op, i32 fb) {
    const i32 first = md::sine(op.phase + (static_cast<u32>(multiply(op.history[0], fb)) << 4));
    const i32 y = md::sine(op.phase + (static_cast<u32>(multiply(first, fb)) << 4));
    op.history[0] = y;
    op.phase += op.increment;
    return y;
}

// The carrier's level: a quarter of full scale, as the other Monomachine engines.
inline i32 level(i32 carrier) { return opxy::q31::half(opxy::q31::half(carrier)); }

// The carrier's swings move in a straight line through each block.
struct Ramp {
    i32 now, step;
    void plan(i32& last, i32 next, bool fresh) {
        if (fresh) last = next;
        now = last;
        step = (opxy::q31::half(next) - opxy::q31::half(last)) / 32;   // halves: no overflow; a constant divisor
        last = next;
    }
    i32 at(i32 y) { const i32 s = multiply(y, now); now += step; return s; }
};

// The carrier's instantaneous increment, its own plus the swings, held within
// +-`limit` (or its own, if higher): the original's deviation seems to
// saturate, which keeps the widest settings' aliasing mostly under 8 kHz.
const i32 limit = 0x24000000;   // 6.2 kHz
inline u32 increment(u32 carrier, i32 swings) {
    const i32 own = static_cast<i32>(carrier > 0x7fffffffu ? 0x7fffffffu : carrier);
    const i32 most = own > limit ? own : limit;   // a carrier above the limit keeps its own pitch
    return static_cast<u32>(md::clamp(md::add(own, swings), -most, most));
}
}

// ---------------------------------------------------------------- FM+STAT
// 1FRQ 1FIN 1ENV 1FB / 2FRQ 2VOL TONE TUNE. M1 at a listed ratio, detuned by
// 1FIN, with feedback; its depth is 1ENV (static to 64, a decay above). M2 at a
// listed ratio with a static depth, 2VOL. TONE filters both.
namespace fmpstat {
struct State {
    u32 carrier;
    fmp::Op m1, m2;
    u32 fresh;
};
namespace {
enum { FRQ1, FIN1, ENV1, FB1, FRQ2, VOL2, TONE, TUNE };
}
inline void trigger(State& v, Common&) { v.fresh = 1; }
inline void reset(State& v, Common& shared, u32) {
    v.carrier = 0;
    fmp::start(v.m1);
    fmp::start(v.m2);
    v.m1.swing = v.m2.swing = 0;
    trigger(v, shared);
}

// 1FIN moves M1 by (v - 64) / 256 of the carrier's frequency (Q28).
inline i32 fine(i32 parameter) { return multiply(md::positive(parameter), 133169152) - 67108864; }

inline void render(State& v, Common&, opxy::Output out, const i32* p, opxy::Pitch pitch) {
    const bool fresh = v.fresh != 0;
    if (fresh) {
        v.carrier = 0;
        fmp::start(v.m1);
        fmp::start(v.m2);
        v.fresh = 0;
    }
    const u32 carrier = md::transpose(pitch[0], fmp::tune(p[TUNE]));
    const i32 r1 = fmp::ratio(p[FRQ1]) + fine(p[FIN1]);
    v.m1.increment = md::scale(carrier, r1 > 0 ? r1 : 0);
    v.m2.increment = md::scale(carrier, fmp::ratio(p[FRQ2]));
    const i32 i1 = multiply(fmp::target(v.m1, p[ENV1], fmp::quarter_turn, fresh), fmp::tone_filter(p[TONE], v.m1.increment));
    const i32 i2 = multiply(fmp::depth(p[VOL2]), fmp::tone_filter(p[TONE], v.m2.increment));
    if (fresh) v.carrier = fmp::carrier_start(i1 + i2);
    fmp::Ramp s1, s2;
    s1.plan(v.m1.swing, fmp::swing(i1, v.m1.increment), fresh);
    s2.plan(v.m2.swing, fmp::swing(i2, v.m2.increment), fresh);
    const i32 fb = fmp::feedback(p[FB1]);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 y1 = fmp::run(v.m1, fb), y2 = fmp::run(v.m2);
        out[i] = md::finish(fmp::level(md::sine(v.carrier)));
        v.carrier += fmp::increment(carrier, md::add(s1.at(y1), s2.at(y2)));
    }
}
}

// ---------------------------------------------------------------- FM+PAR
// 1FRQ 1ENV 2FRQ 2ENV / 3FRQ 3ENV TONE TUNE. Three modulators at listed
// ratios into one carrier, each with its own ENV: static to 64, above it a
// decay from the depth curve's own value. TONE filters all three.
namespace fmppar {
struct State {
    u32 carrier;
    fmp::Op m[3];
    u32 fresh;
};
namespace {
enum { FRQ1, ENV1, FRQ2, ENV2, FRQ3, ENV3, TONE, TUNE };
}
inline void trigger(State& v, Common&) { v.fresh = 1; }
inline void reset(State& v, Common& shared, u32) {
    v.carrier = 0;
    for (unsigned k = 0; k < 3; ++k) { fmp::start(v.m[k]); v.m[k].swing = 0; }
    trigger(v, shared);
}

inline void render(State& v, Common&, opxy::Output out, const i32* p, opxy::Pitch pitch) {
    const bool fresh = v.fresh != 0;
    if (fresh) {
        v.carrier = 0;
        for (unsigned k = 0; k < 3; ++k) fmp::start(v.m[k]);
        v.fresh = 0;
    }
    const u32 carrier = md::transpose(pitch[0], fmp::tune(p[TUNE]));
    fmp::Ramp s[3];
    i32 sum = 0;
    for (unsigned k = 0; k < 3; ++k) {
        const i32 control = p[2 * k + 1];
        fmp::Op& m = v.m[k];
        m.increment = md::scale(carrier, fmp::ratio(p[2 * k]));
        const i32 index = multiply(fmp::target(m, control, fmp::depth(control), fresh), fmp::tone_filter(p[TONE], m.increment));
        s[k].plan(m.swing, fmp::swing(index, m.increment), fresh);
        sum += index;
    }
    if (fresh) v.carrier = fmp::carrier_start(sum);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        i32 swings = 0;
        for (unsigned k = 0; k < 3; ++k) swings = md::add(swings, s[k].at(fmp::run(v.m[k])));
        out[i] = md::finish(fmp::level(md::sine(v.carrier)));
        v.carrier += fmp::increment(carrier, swings);
    }
}
}

// ---------------------------------------------------------------- FM+DYN
// 1FRQ 1FEN 1VOL 1VEN / 2FRQ 2ENV 2FB TUNE. M1 at v/64 of the carrier, its
// ratio swept by 1FEN and its depth (1VOL) moved by 1VEN; M2 at (v/64)^2, with
// an ENV like STAT's 1ENV and feedback, 2FB.
namespace fmpdyn {
struct State {
    u32 carrier;
    fmp::Op m1, m2;
    i32 sweep;   // 1FEN's envelope, Q31
    i32 lift;    // 1VEN's envelope, Q31
    u32 fresh;
};
namespace {
enum { FRQ1, FEN1, VOL1, VEN1, FRQ2, ENV2, FB2, TUNE };
}
inline void trigger(State& v, Common&) { v.fresh = 1; }
inline void reset(State& v, Common& shared, u32) {
    v.carrier = 0;
    fmp::start(v.m1);
    fmp::start(v.m2);
    v.m1.swing = v.m2.swing = 0;
    v.sweep = v.lift = 0;
    trigger(v, shared);
}

// The signed envelopes' distance from 64 (Q31, as a control above the middle).
inline i32 mirrored(i32 parameter) {
    const i32 p = md::positive(parameter);
    if (p >= fmp::middle) return p;
    const u32 below = static_cast<u32>(md::one - p) + 16909320u;   // 128 - v, at most 127 (the table's end)
    return below > 0x7fffffffu ? md::one : static_cast<i32>(below);
}
// Below 16 the original's negative envelopes fade out: at 0 they do nothing
// (Q31 of full, from d = -48 to -64 steps, Q16).
inline i32 taper(i32 d) {
    if (d >= -48 * 65536) return md::one;
    return md::clamp((d + 64 * 65536) * 2048, 0, md::one);   // 2^31 / (16 << 16)
}
// 1VEN: steps of 1VOL added at the start: 4 per step above 64, 2.6 per step below (Q16 steps).
inline i32 lift_steps(i32 parameter) {
    const i32 d = fmp::steps_of(parameter) - 64 * 65536;
    // Above 64: 4 a step to 24 steps out, then 1.5 a step (VEN 104 lifts by 120).
    if (d >= 0) return d <= 24 * 65536 ? d * 4 : 96 * 65536 + multiply(d - 24 * 65536, 0x60000000) * 2;
    return multiply(multiply(d, 0x53333333) * 4, taper(d));   // 2.6 = 4 x 0.65
}
// 1FEN: M1's ratio times (1 + F) at the start, F = 0.00512 (v - 64)^2 above 64
// and a quarter of that, negative, below, to at most -0.9 (Q24).
inline i32 sweep_factor(i32 parameter) {
    const i32 d = fmp::steps_of(parameter) - 64 * 65536;
    const i32 a = static_cast<i32>(static_cast<u32>(d < 0 ? -d : d) >> 8);   // steps from 64, Q8
    const i32 f = multiply(a * a, 1407374884) * 2;                         // 0.00512 steps^2 in Q24: x 1.31072
    if (d >= 0) return f;
    const i32 down = multiply(opxy::q31::half(opxy::q31::half(f)), taper(d));
    return down < 15099494 ? -down : -15099494;   // never past 0.9 down: M1 does not turn back
}

inline u32 times(u32 increment, i32 factor_q24) {
    // increment x factor / 2^24, saturating below 2^31; a negative factor turns M1 backwards.
    const i32 f = factor_q24 < 0 ? -factor_q24 : factor_q24;
    const u32 m = static_cast<u32>(multiply(static_cast<i32>(increment >> 1), f));
    const u32 r = m > (0x7fffffffu >> 8) ? 0x7fffffffu : m << 8;
    return factor_q24 < 0 ? 0u - r : r;
}

inline void render(State& v, Common&, opxy::Output out, const i32* p, opxy::Pitch pitch) {
    const bool fresh = v.fresh != 0;
    if (fresh) {
        v.carrier = 0;
        fmp::start(v.m1);
        fmp::start(v.m2);
        v.sweep = v.lift = md::one;
        v.fresh = 0;
    } else {
        v.sweep = multiply(v.sweep, fmp::decay_per_block(fmp::fen_times, mirrored(p[FEN1])));
        v.lift = multiply(v.lift, fmp::decay_per_block(fmp::ven_times, mirrored(p[VEN1])));
    }
    const u32 carrier = md::transpose(pitch[0], fmp::tune(p[TUNE]));
    const u32 base = md::scale(carrier, fmp::dyn_ratio1(p[FRQ1]));
    v.m1.increment = times(base, (1 << 24) + multiply(sweep_factor(p[FEN1]), v.sweep));
    v.m2.increment = md::scale(carrier, fmp::dyn_ratio2(p[FRQ2]));
    const i32 x1 = fmp::steps_of(p[VOL1]) + multiply(lift_steps(p[VEN1]), v.lift);
    const u32 m1_speed = v.m1.increment > 0x7fffffffu ? 0u - v.m1.increment : v.m1.increment;
    const i32 i1 = multiply(fmp::depth_at(x1), fmp::dyn_filter(m1_speed));
    const i32 i2 = multiply(fmp::target(v.m2, p[ENV2], fmp::quarter_turn, fresh), fmp::dyn_filter(v.m2.increment));
    if (fresh) v.carrier = fmp::carrier_start(i1 + i2);
    fmp::Ramp s1, s2;
    s1.plan(v.m1.swing, fmp::swing(i1, m1_speed), fresh);
    s2.plan(v.m2.swing, fmp::swing(i2, v.m2.increment), fresh);
    const i32 fb = fmp::feedback(p[FB2]);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 y1 = fmp::run(v.m1), y2 = fmp::run(v.m2, fb);
        out[i] = md::finish(fmp::level(md::sine(v.carrier)));
        v.carrier += fmp::increment(carrier, md::add(s1.at(y1), s2.at(y2)));
    }
}
}
}
