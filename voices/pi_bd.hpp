#pragma once
// PI-BD: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_modal.hpp"

// A Machinedrum PI-BD-style kick: a struck membrane, written from scratch in
// fixed point and fitted to the Machinedrum's own (reference/fit_pi_bd.py).
// Its page:
//   PTCH  DEC   HARD  HAMR
//   TENS  DAMP
// As measured: two membrane modes, the fundamental from 28.9 Hz (PTCH 0) to
// 113 Hz (127) and the second at 1.595 times it, both starting higher while
// the head is stretched by the strike (TENS, up to 2.7 times) and falling back
// with a 27 ms time constant. DEC sets the fundamental's ring, -60 dB in 31 ms
// per step; the second mode dies in about half a second. The strike opens it
// with a burst of noise, even from about 1 to 6 kHz around a centre near
// 2 kHz, which HAMR (a heavier beater) lowers and softens: the head's upper
// modes, too many and too close to hear as a pitch. (A single 2 kHz sine here
// was 8 dB over the Machinedrum's in its band and beeped.) HARD and DAMP have no measurable effect on the
// Machinedrum's (OS 1.63), so none here either.
namespace md {
namespace pi_bd {
struct State {
    modal::Mode body, second;
    i32 bend;        // the strike's stretch still to relax, Q31
    modal::Mode strike;   // the strike's burst: its level, and its centre as an increment
    i32 high, low1, low2; // the burst's band: a high-pass pole and two low-pass
    u32 seed;
    u32 hold;        // blocks of the burst still held at full level
    u32 age;         // blocks since the strike, through the body's swell
    u32 fresh;
};
namespace {
using md::multiply;

enum { PTCH, DEC, HARD, HAMR, TENS, DAMP };

// The fundamental: 28.9 Hz * 2^(1.97 PTCH), Q16 octaves from middle C.
const i32 base_low = -208299, base_span = 129106;
const i32 second_ratio = 44142;    // 1.595, Q16 octaves
// DEC: the fundamental falls 60 dB in 0.2 s at DEC 0, then 31.3 ms per step;
// the second mode in 0.2 .. 1.1 s. Q16 log2 samples at DEC 0, 16, ..., 127.
const i32 body_times[9] = {858952, 945586, 1011122, 1049458, 1076658, 1097755, 1114994, 1129568, 1141625};
const i32 second_times[9] = {858952, 917117, 954597, 959618, 968926, 980062, 1002205, 1011122, 1020989};
// TENS: the strike's stretch, the ratio above the settled pitch at the start
// (Q28, less one), at TENS 0, 16, ..., 127; it relaxes 60 dB in 186 ms.
const i32 stretch[9] = {26843546, 37580964, 67108864, 147639501, 198642237, 279172874, 346281738, 407607296, 464043520};
const i32 relax_time = 852349;
// Lower and higher kicks stretch less (PTCH 0, 16, ..., 127, Q31), the very
// highest not at all, and no strike starts above 157 Hz (at middle C): the
// largest start ratio, less one, Q28.
const i32 stretch_share[9] = {1181116006, 1460288881, 1760936591, 1954210119, 2147483647, 2147483647, 2147483647,
                              1073741824, 0};
const i32 stretch_cap[9] = {1189847125, 959369443, 765318145, 601936138, 464376223, 348557287, 251043225, 168941002,
                            103795593};
// The second mode starts at 0.7 of the fundamental.
const i32 second_level = 0x5999999a;
// The strike's burst: the head's upper modes, a band of noise centred near
// 2.2 kHz (PTCH 64, TENS 40, HAMR 64), held for 4 ms, then falling 60 dB in
// 15 ms. The band's edges are an octave and an eighth below the centre (one
// high-pass pole) and an octave and a fifth above it (two low-pass poles). PTCH, TENS and HAMR move it, in Q16 octaves at 0, 16, ..., 127;
// it is shorter the higher PTCH is, and HAMR quietens it by 3 dB. The centre is Q16
// octaves from middle C.
const i32 burst_centre = 200109;
const i32 burst_ptch[9] = {-34786, -30000, -24824, -15000, 0, 13512, 30000, 45325, 53926};
const i32 burst_tens[9] = {-58547, -34786, -7586, 6989, 19614, 25346, 25346, 25346, 25346};
const i32 burst_hamr[9] = {13512, 13512, 13512, 6000, 0, -6000, -15813, -30000, -34786};
// It falls 60 dB in 60 ms (PTCH 0) .. 9 ms (127), Q16 log2 samples.
const i32 burst_times[9] = {745118, 717918, 694157, 679582, 662344, 650257, 592948, 565748, 565748};
const u32 burst_hold = 3;   // blocks
const i32 burst_level = 0x2c000000;
const i32 band_below = -98304, band_above = 26214;   // Q16 octaves from the centre
const i32 burst_quiet_hamr = 0x5a9df7ac;   // 3 dB down at HAMR 127
// The body swells in under the burst, over its first 6 blocks (9 ms).
const u32 swell_blocks = 6;
const i32 swell_step = md::one / 6;
// The body peaks 2 dB over the Machinedrum's (reference/pi_measure.py, LEVEL_DB).
const i32 body_level = 0x12a00000;
const i32 low_level = 0x73333333, high_level = 0x60000000;   // PTCH 0 and 127: 0.9 and 2.5 dB down

// A one-pole coefficient (Q31) for a corner at `increment` (of 2^32 a sample):
// 1 - e^-w, w = 2 pi f / rate, by its series to w^3.
inline i32 corner(u32 increment) {
    const i32 eighth = multiply(static_cast<i32>(increment >> 1), 0x6487ed51);   // w / 8: pi / 4, Q31
    if (eighth >= 0x0fffffff) return md::one;
    const i32 w = eighth * 8;
    const i32 c = multiply(w, md::one - w / 2 + multiply(w, w) / 6);
    return c < 0 ? md::one : c;
}

struct Block {
    i32 base, body_step, second_step, stretch, burst_tune, burst_amount, burst_step, level;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        body_step = md::decay(md::table9(body_times, p[DEC]));
        second_step = md::decay(md::table9(second_times, p[DEC]));
        stretch = multiply(md::table9(md::pi_bd::stretch, p[TENS]), md::table9(stretch_share, p[PTCH]));
        const i32 cap = md::table9(stretch_cap, p[PTCH]);
        if (stretch > cap) stretch = cap;
        burst_tune = burst_centre + md::table9(burst_ptch, p[PTCH]) + md::table9(burst_tens, p[TENS])
                     + md::table9(burst_hamr, p[HAMR]);
        burst_amount = multiply(burst_level, md::span(md::positive(p[HAMR] - 0x40000000) * 2, md::one, burst_quiet_hamr));
        burst_step = md::decay(md::table9(burst_times, p[PTCH]));
        level = multiply(body_level, md::span(p[PTCH], low_level, high_level));
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.body.phase = voice.second.phase = 0;
    voice.body.increment = voice.second.increment = 0;
    voice.body.level = voice.second.level = 0;
    voice.strike.phase = voice.strike.increment = 0;
    voice.strike.level = 0;
    voice.high = voice.low1 = voice.low2 = 0;
    voice.seed = 0x2545f491u * (owner + 1);
    voice.bend = 0;
    voice.hold = 0;
    voice.age = swell_blocks;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    const u32 settled = md::transpose(pitch[0], b.base);
    const u32 upper = md::transpose(pitch[0], b.base + second_ratio);
    if (voice.fresh) {
        voice.fresh = 0;
        voice.body.phase = voice.second.phase = 0;
        voice.body.level = md::one;
        voice.second.level = second_level;
        voice.bend = md::one;
        voice.strike.phase = 0;
        voice.strike.level = md::one;
        voice.hold = burst_hold;
        voice.age = 0;
        const i32 ratio = 0x10000000 + b.stretch;
        voice.body.increment = md::scale(settled, ratio);
        voice.second.increment = md::scale(upper, ratio);
    }
    // The stretch relaxes along a straight line within the block.
    voice.bend = multiply(voice.bend, md::per_block(md::decay(relax_time)));
    const i32 ratio = 0x10000000 + multiply(b.stretch, voice.bend);

    i32 mixed[opxy::block_frames];
    for (unsigned i = 0; i != opxy::block_frames; ++i) mixed[i] = 0;
    const i32 swell = voice.age < swell_blocks ? swell_step * static_cast<i32>(voice.age) : md::one;
    const i32 swell_next = voice.age + 1 < swell_blocks ? swell + swell_step : md::one;
    if (voice.age < swell_blocks) ++voice.age;
    const i32 now = multiply(b.level, swell), next = multiply(b.level, swell_next);
    modal::ring_glide(voice.body, b.body_step, now, next, md::scale(settled, ratio), mixed);
    modal::ring_glide(voice.second, b.second_step, now, next, md::scale(upper, ratio), mixed);

    if (voice.strike.level > 0x00100000) {
        const u32 centre = md::transpose(pitch[0], b.burst_tune);
        const i32 high = corner(md::transpose(centre, band_below));
        const i32 low = corner(md::transpose(centre, band_above));
        const i32 next = multiply(voice.strike.level, md::per_block(voice.hold ? md::one : b.burst_step));
        i32 level = multiply(voice.strike.level, b.burst_amount);
        const i32 slope = (multiply(next, b.burst_amount) - level) / 64;
        for (unsigned i = 0; i != opxy::block_frames; ++i) {
            const i32 white = md::noise_eighth(voice.seed);
            md::smooth(voice.high, white, high);
            md::smooth(voice.low1, white - voice.high, low);
            md::smooth(voice.low2, voice.low1, low);
            mixed[i] += multiply(voice.low2, level) * 8;
            level += slope;
        }
        voice.strike.level = next;
        if (voice.hold) --voice.hold;
    }
    modal::finish_doubled(mixed, output);
}
}
}
