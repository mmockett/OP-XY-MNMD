#pragma once
// TRX-BD: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Kick: a Machinedrum TRX-BD-style kick, written from scratch in fixed point
// and fitted to the Machinedrum's own behaviour (reference/fit_bd.py). The
// eight controls follow the TRX-BD page, top row on the encoders and the
// bottom row as their alternates:
//   PTCH  DEC   RAMP  RDEC
//   STRT  NOIS  HARM  CLIP
// The sound is one sine. Its pitch is the base set by PTCH, plus a fixed thump
// that falls linearly in Hz over the first 15 to 20 ms, plus RAMP's sweep, which
// falls exponentially at RDEC's rate. The pitch is the Machinedrum's at middle
// C; other keys move all of it.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace trx_bd {
struct State {
    u32 phase;
    i32 level;      // amplitude envelope, Q31
    i32 tail;       // the quiet tail under it, Q31
    i32 boost;      // the opening's extra level, Q31
    i32 ramp;       // RAMP's sweep envelope, Q31
    i32 thump;      // the built-in drop still to fall, Q28 of middle C
    i32 burst;      // NOIS's burst envelope, Q31
    u32 click_phase;// the opening click's cycle
    u32 age;        // samples since the note, counted through the opening only
    i32 gate;       // the body's fade in after the click, Q31
    u32 seed;       // noise
    i32 tone;       // one-pole state for the burst
    i32 highpass;   // CLIP's high-pass state
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, RAMP, RDEC, STRT, NOIS, HARM, CLIP };

// Base pitch: 30.6 Hz * 2^(1.98 PTCH), in Q16 octaves from middle C.
const i32 base_low = -203017, base_span = 129696;
// The thump: its depth in Q28 fractions of middle C, and how much it falls per
// sample, at PTCH 0, 16, ..., 127 (about 150 Hz over 20 ms, to 142 Hz over 15 ms).
const i32 thump_depth[9] = {154212161, 154006955, 156674630, 162933407, 168986978,
                            170628625, 170628625, 159957923, 145696120};
const i32 thump_fall[9] = {174844, 170352, 177636, 199710, 225406, 241821, 257942, 241811, 220251};
// RAMP adds up to 730 Hz (Q28 of middle C), as the cube of the control.
const i32 ramp_most = 749001179;
// RDEC: the sweep's time constant, 4 ms .. 1.4 s, as a -60 dB time in Q16 log2 samples.
const i32 ramp_short = 675028, ramp_long = 1223518;
// DEC: -60 dB in 46 ms .. 14 s, as Q16 log2 samples at DEC 0, 16, ..., 127.
const i32 decay_times[9] = {719996, 736891, 814514, 858952, 915571, 1001160, 1081271, 1177324, 1260640};
// Under it a quiet tail from -20 dB, falling 60 dB in 240 ms: what short DECs leave.
const i32 tail_start = 214748365, tail_time = 876190;
// The opening, as the Machinedrum's: a fixed click, one downward cycle of 200 Hz
// over the first 5 ms at 1.2 x the settled body whatever DEC is; the body fades
// in over the next 2 ms, then starts 1.6 x as loud and settles with an 8.5 ms
// time constant (-60 dB in 59 ms).
const u32 click_cycle = 220, body_in = 88;           // samples
const u32 click_increment = 19522579;                // 2^32 / 220
const i32 click_level = 644245094;                   // 1.2 x, at a quarter: STRT's 1.57 x of it stays under one
const i32 boost_start = 1288490189, boost_time = 743076;
// NOIS: a burst falling 60 dB in 5 .. 120 ms, louder and longer with the control;
// its level follows (p + p^2) / 2, as measured.
const i32 burst_short = 510174, burst_long = 810654;
const i32 burst_most = 0x29000000;
const i32 burst_tone = 0x50000000;
// The settled body peaks at an eighth of full scale; the opening reaches twice
// that and CLIP doubles it, up to the SDK's half-scale ceiling.
const i32 body_level = 0x10000000;
// HARM's pulse at full control: harmonics about 45 dB under the fundamental.
const i32 harm_most = 0x04000000;
// STRT raises the click by up to 1.57 x, quickly at first: 1 - (1 - p)^2.
const i32 strt_most = 0x66666666;    // 0.8: 1.57 x once the body's opening is counted

struct Block {
    i32 base, depth, fall, ramp, ramp_step, decay_step, noise, burst_step, harm;
    md::Clip clip;   // md_dsp.hpp: fitted on this machine; the click is not clipped
    i32 click;
    u32 start_phase;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        depth = md::table9(thump_depth, p[PTCH]);
        fall = md::table9(thump_fall, p[PTCH]);
        const i32 r = md::positive(p[RAMP]);
        ramp = multiply(multiply(multiply(r, r), r), ramp_most);
        ramp_step = md::decay(md::span(p[RDEC], ramp_short, ramp_long));
        decay_step = md::decay(md::table9(decay_times, p[DEC]));
        const i32 n = md::positive(p[NOIS]);
        noise = multiply(opxy::q31::half(n) + opxy::q31::half(multiply(n, n)), burst_most);
        burst_step = md::decay(md::span(p[NOIS], burst_short, burst_long));
        harm = multiply(md::squared(p[HARM]), harm_most);
        clip.prepare(p[CLIP], body_level);
        const i32 q = md::one - md::positive(p[STRT]);
        click = click_level + multiply(multiply(click_level, md::one - multiply(q, q)), strt_most);
        start_phase = static_cast<u32>(md::positive(p[STRT])) >> 3;   // the click starts up to 1/16 cycle in
    }
};
}

// The settled body's gain: DEC's level over the quiet tail, with the opening's boost.
inline i32 settled_gain(i32 level, i32 tail, i32 boost) {
    const i32 settled = multiply(level > tail ? level : tail, body_level);
    return settled + multiply(settled, boost);
}
inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase = voice.click_phase = 0;
    voice.age = click_cycle + body_in;
    voice.gate = md::one;
    voice.seed = 0x1234567u * owner;
    voice.level = voice.tail = voice.boost = voice.ramp = voice.burst = voice.thump = 0;
    voice.tone = voice.highpass = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        // STRT starts the click part way into its cycle, and louder: a harder attack.
        voice.phase = 0;
        voice.click_phase = b.start_phase;
        voice.highpass = 0;
        voice.age = 0;
        voice.gate = 0;
        voice.level = md::one;
        voice.tail = tail_start;
        voice.boost = boost_start;
        voice.ramp = md::one;
        voice.burst = md::one;
        voice.thump = b.depth;
        voice.tone = 0;
        voice.fresh = 0;
    }
    const i32 tail_step = md::decay(tail_time);
    const i32 boost_step = md::decay(boost_time);
    const i32 gate_step = md::one / static_cast<i32>(body_in);   // a constant: no runtime division

    // The pitch in Hz above the base, as Q28 of middle C, now and at the block's
    // end: the thump falls in a straight line, the sweep exponentially. Between
    // the two the increment moves in a straight line.
    const u32 base = md::transpose(pitch[0], b.base);
    const i32 thump_next = voice.thump > b.fall * 64 ? voice.thump - b.fall * 64 : 0;
    const i32 ramp_next = multiply(voice.ramp, md::per_block(b.ramp_step));
    const u32 now = base + md::scale(pitch[0], voice.thump + multiply(voice.ramp, b.ramp));
    const u32 next = base + md::scale(pitch[0], thump_next + multiply(ramp_next, b.ramp));
    u32 increment = now;
    const i32 slope = (static_cast<i32>(next) - static_cast<i32>(now)) / 64;
    voice.thump = thump_next;
    voice.ramp = ramp_next;

    if (voice.age >= click_cycle + body_in) {
        // Settled: the click is over and the body faded in. Its gain and the
        // burst's, envelopes and levels together, in straight lines across the block.
        const i32 level_next = multiply(voice.level, md::per_block(b.decay_step));
        const i32 tail_next = multiply(voice.tail, md::per_block(tail_step));
        const i32 boost_next = multiply(voice.boost, md::per_block(boost_step));
        const i32 burst_next = multiply(voice.burst, md::per_block(b.burst_step));
        i32 body_gain = settled_gain(voice.level, voice.tail, voice.boost);
        const i32 body_slope = (settled_gain(level_next, tail_next, boost_next) - body_gain) / 64;
        i32 burst_gain = multiply(multiply(voice.burst, b.noise), body_level);
        const i32 burst_end = multiply(multiply(burst_next, b.noise), body_level);
        const i32 burst_slope = (burst_end - burst_gain) / 64;
        // The burst's noise runs only while it can be heard (-102 dB).
        const bool bursting = burst_gain >= (1 << 14) || burst_end >= (1 << 14);
        voice.level = level_next;
        voice.tail = tail_next;
        voice.boost = boost_next;
        voice.burst = burst_next;
        for (unsigned i = 0; i != opxy::block_frames; ++i) {
            const i32 sine = md::sine(voice.phase);
            const i32 pulse = voice.phase < 0x20000000u ? b.harm : -(b.harm >> 3);
            voice.phase += increment;
            increment += static_cast<u32>(slope);
            i32 mixed = multiply(opxy::q31::half(pulse) - opxy::q31::half(sine), body_gain) * 2;
            if (bursting)
                mixed += multiply(md::smooth(voice.tone, opxy::q31::half(md::noise(voice.seed)), burst_tone), burst_gain) * 2;
            output[i] = md::finish(md::gain(b.clip.run(mixed, voice.highpass), 1));
            body_gain += body_slope;
            burst_gain += burst_slope;
        }
        return;
    }

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 sine = md::sine(voice.phase);
        // HARM: a narrow pulse on each cycle, a little buzz of upper harmonics.
        const i32 pulse = voice.phase < 0x20000000u ? b.harm : -(b.harm >> 3);
        voice.phase += increment;
        increment += static_cast<u32>(slope);

        // The body: DEC's level over the quiet tail, opening loud, faded in after the click.
        const i32 held = voice.level > voice.tail ? voice.level : voice.tail;
        const i32 settled = multiply(multiply(held, body_level), voice.gate);
        const i32 amplitude = settled + multiply(settled, voice.boost);
        // The Machinedrum's sine starts downwards.
        i32 mixed = multiply(opxy::q31::half(pulse) - opxy::q31::half(sine), amplitude) * 2;
        const i32 burst = multiply(md::smooth(voice.tone, opxy::q31::half(md::noise(voice.seed)), burst_tone),
                                   multiply(voice.burst, b.noise));
        mixed += multiply(burst, body_level) * 2;
        mixed = b.clip.run(mixed, voice.highpass);
        if (voice.age < click_cycle) {
            mixed -= multiply(md::sine(voice.click_phase), multiply(body_level, b.click) * 4);
            voice.click_phase += click_increment;
        } else if (voice.age < click_cycle + body_in) {
            voice.gate += gate_step;
        } else {
            voice.gate = md::one;
            voice.boost = multiply(voice.boost, boost_step);
        }
        if (voice.age < click_cycle + body_in) ++voice.age;

        output[i] = md::finish(md::gain(mixed, 1));

        voice.level = multiply(voice.level, b.decay_step);
        voice.tail = multiply(voice.tail, tail_step);
        voice.burst = multiply(voice.burst, b.burst_step);
    }
}
}
}
