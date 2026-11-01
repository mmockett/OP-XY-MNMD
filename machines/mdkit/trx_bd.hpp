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
// A hit is a fixed click, then one sine. The click is synthesised to the measures
// below: a short lobe down, a rise to its peak at 3.6 ms, and a tail under the
// sine that fades with a 6 ms time constant. The sine starts rising from nothing
// at 6.3 ms, its first half cycle small and smaller the later it peaks, so the
// click stands 13 to 17 dB over what follows it for several milliseconds, as on
// the Machinedrum, before the body arrives. Its pitch is the base set by PTCH,
// plus a thump of about 35 Hz falling linearly to nothing by 11 to 27 ms, plus
// RAMP's sweep, which falls exponentially at RDEC's rate. The pitch is the
// Machinedrum's at middle C; other keys move all of it.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace trx_bd {
namespace {
using md::multiply;

enum { PTCH, DEC, RAMP, RDEC, STRT, NOIS, HARM, CLIP };

// Base pitch: 30.6 Hz * 2^(1.98 PTCH), in Q16 octaves from middle C.
const i32 base_low = -203017, base_span = 129696;
// The thump: its depth in Q28 fractions of middle C, and how much it falls per
// sample, at PTCH 0, 16, ..., 127 (24 Hz over 27 ms, to 36 Hz over 11 ms). All but
// its end falls before the sine starts sounding: fitted to the waveform from 6.3 ms.
const i32 thump_depth[9] = {24624282, 35910411, 37962435, 36936423, 41040470, 43092494, 41040470, 32832376, 36936423};
const i32 thump_fall[9] = {20681, 42858, 50637, 52348, 66473, 75166, 77552, 62042, 76142};
// RAMP adds up to 730 Hz (Q28 of middle C), as the cube of the control.
const i32 ramp_most = 749001179;
// RDEC: the sweep's time constant, 4 ms .. 1.4 s, as a -60 dB time in Q16 log2 samples.
const i32 ramp_short = 675028, ramp_long = 1223518;
// DEC: -60 dB in 46 ms .. 14 s, as Q16 log2 samples at DEC 0, 16, ..., 127.
const i32 decay_times[9] = {719996, 736891, 814514, 858952, 915571, 1001160, 1081271, 1177324, 1260640};
// Under it a quiet tail from -20 dB, falling 60 dB in 240 ms: what short DECs leave.
const i32 tail_start = 214748365, tail_time = 876190;
// The click, in tenths of a percent of the Machinedrum's full scale: its levels
// are lifted 3.6 dB, as the body's, and held at half here. It is one cycle of a
// 136 Hz sine, crossing zero upward at 1.79 ms, peaking at 24.9% at 3.6 ms; its
// first lobe is cut off at -21.1%, reached in 12 samples; after the second crossing
// it slides to the tail's -7.8% by 6.7 ms.
const i32 tenth = 1632088;
const u32 soft_rise = 12, soft_zero = 79, soft_end = 241, click_length = 297;   // samples
const u32 soft_increment = 13256071;                 // 2^32 / 324
const i32 soft_peak = 249 * tenth, soft_floor = -211 * tenth, soft_tail = -78 * tenth;
// STRT moves it to a harder click, square-edged and 1.56 x as high: down to -24.8%
// in 20 samples, easing to -12.8% by 3 ms; an edge up to 38.6% in 24 samples, drooping
// to 21.8% at 6.2 ms; a fast edge down to its tail, -5%. That high, it meets the
// SDK's ceiling and is held there.
const u32 hard_rise = 20, hard_up = 132, hard_top = 156, hard_down = 272, hard_tail = 276;   // samples
const i32 hard_low = -248 * tenth, hard_ease = -128 * tenth, hard_peak = 386 * tenth, hard_droop = 218 * tenth, hard_after = -50 * tenth;
inline i32 click_soft(u32 age) {
    if (age >= soft_end) return soft_tail / static_cast<i32>(click_length - soft_end) * static_cast<i32>(age - soft_end);
    const i32 cycle = multiply(md::sine((age - soft_zero) * soft_increment), soft_peak);
    const i32 floor = age < soft_rise ? soft_floor / static_cast<i32>(soft_rise) * static_cast<i32>(age) : soft_floor;
    return cycle > floor ? cycle : floor;
}
inline i32 click_hard(u32 age) {
    if (age < hard_rise) return hard_low / static_cast<i32>(hard_rise) * static_cast<i32>(age);
    if (age < hard_up) return hard_low + (hard_ease - hard_low) / static_cast<i32>(hard_up - hard_rise) * static_cast<i32>(age - hard_rise);
    if (age < hard_top) return hard_ease + (hard_peak - hard_ease) / static_cast<i32>(hard_top - hard_up) * static_cast<i32>(age - hard_up);
    if (age < hard_down) return hard_peak + (hard_droop - hard_peak) / static_cast<i32>(hard_down - hard_top) * static_cast<i32>(age - hard_top);
    if (age < hard_tail) return hard_droop + (hard_after - hard_droop) / static_cast<i32>(hard_tail - hard_down) * static_cast<i32>(age - hard_down);
    return hard_after;
}
const i32 after_time = 713262;                       // its tail: a 6.2 ms time constant
const u32 body_start = 278;                          // the sine starts at 6.3 ms
const u32 opening = 1764;                            // and by 40 ms the tail has gone
const i32 first_gain = 0x43333333, first_time = 717756;   // its first half cycle: 1.05 x, falling with a 6.5 ms time constant,
const i32 first_least = 0x26666666;                  // to 0.3 x at the least, for low keys
// NOIS: a burst falling 60 dB in 5 .. 120 ms, louder and longer with the control;
// its level follows (p + p^2) / 2, as measured.
const i32 burst_short = 510174, burst_long = 810654;
const i32 burst_most = 0x29000000;
const i32 burst_tone = 0x50000000;
// The settled body peaks at an eighth of full scale, 3.6 dB over the Machinedrum's.
const i32 body_level = 0x10000000;
// CLIP: as measured, no gain, a threshold falling from the body's height at CLIP 64 to
// a quarter of it at 127, into md_dsp's high-pass: the 3rd harmonic -22 dB at CLIP 80, -10 at 127.
const i32 clip_thresholds[9] = {body_level, body_level, body_level, body_level, body_level,
                                0x0d99999a, 0x0c7ae148, 0x08f5c28f, 0x04000000};
const i32 clip_onset = 0x46000000, clip_stretch = 0x11a7b960;   // fades in from CLIP 70; 1 / (1 - 0.547), Q27
const i32 clip_unity = 0x04000000;                              // a gain of one, as md_dsp counts it
// HARM's pulse at full control: harmonics about 45 dB under the fundamental.
const i32 harm_most = 0x04000000;

}
struct Block {
    i32 depth, fall, ramp, ramp_block, decay_step, decay_block, noise, burst_step, burst_block, harm, pulse_on, pulse_off, tail_step, tail_block, first_step, after_step;
    u32 base;
    md::Clip clip;   // md_dsp.hpp: fitted on this machine; the sine before its envelope, as measured
    i32 strike;      // STRT: how far from the soft click to the hard, 1 - (1 - p)^3
    void plan(const i32* p, u32 middle) {
        base = md::transpose(middle, md::span(p[PTCH], base_low, base_low + base_span));
        depth = md::table9(thump_depth, p[PTCH]);
        fall = md::table9(thump_fall, p[PTCH]);
        const i32 r = md::positive(p[RAMP]);
        ramp = multiply(multiply(multiply(r, r), r), ramp_most);
        ramp_block = md::per_block(md::decay(md::span(p[RDEC], ramp_short, ramp_long)));
        decay_step = md::decay(md::table9(decay_times, p[DEC]));
        decay_block = md::per_block(decay_step);
        const i32 n = md::positive(p[NOIS]);
        noise = multiply(opxy::q31::half(n) + opxy::q31::half(multiply(n, n)), burst_most);
        burst_step = md::decay(md::span(p[NOIS], burst_short, burst_long));
        burst_block = md::per_block(burst_step);
        harm = multiply(md::squared(p[HARM]), harm_most);
        pulse_on = opxy::q31::half(harm);
        pulse_off = opxy::q31::half(-(harm >> 3));
        tail_step = md::decay(tail_time);
        tail_block = md::per_block(tail_step);
        first_step = md::decay(first_time);
        after_step = md::decay(after_time);
        clip.prepare(p[CLIP], body_level, clip_onset, clip_stretch);
        clip.drive = clip_unity;
        clip.threshold = md::table9(clip_thresholds, p[CLIP]);
        const i32 q = md::one - md::positive(p[STRT]);
        strike = md::one - multiply(multiply(q, q), q);
    }
};

struct State {
    u32 phase;
    i32 level;      // amplitude envelope, Q31
    i32 tail;       // the quiet tail under it, Q31
    i32 ramp;       // RAMP's sweep envelope, Q31
    i32 thump;      // the built-in drop still to fall, Q28 of middle C
    i32 burst;      // NOIS's burst envelope, Q31
    i32 after;      // the click now, and its tail once the table has run, Q31 at half level
    u32 age;        // samples since the note, counted through the opening only
    i32 gate;       // the sine's first half cycle's gain, Q31; one after it
    u32 halfway;    // the sine's first half cycle is over
    u32 seed;       // noise
    i32 tone;       // one-pole state for the burst
    i32 highpass;   // CLIP's high-pass state
    i32 burst_out;  // the burst's gain the last block ended on, which the next starts from
    u32 carrying;   // it is set: the settled loop's first block starts from the envelope
    u32 fresh;      // a note has started and render has not yet run
    u32 stale;        // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];      // the controls they were worked out from
    Block block;
};

// The body's envelope: DEC's level over the quiet tail.
inline i32 held(i32 level, i32 tail) { return level > tail ? level : tail; }
// The block's stages once settled, a loop each: out of line, so each has a hardware loop and
// all the registers to itself, and on locals, as the buffers might otherwise be the voice.
// HARM: a narrow pulse on each cycle, under the sine at its full height.
__attribute__((noinline)) inline void run_body(State& voice, u32 increment, i32 slope, i32 pulse_on, i32 pulse_off, i32 gain, i32 gain_slope, i32* x) {
    u32 phase = voice.phase;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 sine = md::sine(phase);
        const i32 pulse = phase < 0x20000000u ? pulse_on : pulse_off;
        phase += increment;
        increment += static_cast<u32>(slope);
        x[i] = multiply(multiply(pulse - opxy::q31::half(sine), body_level) * 2, gain);
        gain += gain_slope;
    }
    voice.phase = phase;
}
// CLIP shapes the sine at its full height, before the envelope: the shape holds as it falls.
__attribute__((noinline)) inline void run_clipped(State& voice, u32 increment, i32 slope, const Block& b, i32 gain, i32 gain_slope, i32* x) {
    u32 phase = voice.phase;
    i32 highpass = voice.highpass;
    const md::Clip clip = b.clip;
    const i32 pulse_on = b.pulse_on, pulse_off = b.pulse_off;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 sine = md::sine(phase);
        const i32 pulse = phase < 0x20000000u ? pulse_on : pulse_off;
        phase += increment;
        increment += static_cast<u32>(slope);
        x[i] = multiply(clip.run(multiply(pulse - opxy::q31::half(sine), body_level) * 2, highpass), gain);
        gain += gain_slope;
    }
    voice.phase = phase;
    voice.highpass = highpass;
}
__attribute__((noinline)) inline void run_burst(State& voice, i32 gain, i32 gain_slope, i32* x) {
    u32 seed = voice.seed;
    i32 tone = voice.tone;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        x[i] += multiply(md::smooth(tone, opxy::q31::half(md::noise(seed)), burst_tone), gain) * 2;
        gain += gain_slope;
    }
    voice.seed = seed;
    voice.tone = tone;
}
// Doubling then stopping at half scale is stopping at a quarter, then doubling.
__attribute__((noinline)) inline void run_finish(opxy::Output x) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) x[i] = md::clamp(x[i], -0x20000000, 0x20000000) * 2;
}
// The opening: the click, the first half cycle and the body starting.
__attribute__((noinline)) inline void run_opening(State& voice, const Block& b, u32 increment, i32 slope, opxy::Output output) {
    u32 phase = voice.phase, age = voice.age, seed = voice.seed, halfway = voice.halfway;
    i32 gate = voice.gate, level = voice.level, tail = voice.tail, burst_level = voice.burst, tone = voice.tone, after = voice.after, highpass = voice.highpass;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        i32 mixed = 0;
        if (age >= body_start) {
            const i32 sine = md::sine(phase);
            // HARM: a narrow pulse on each cycle, a little buzz of upper harmonics.
            const i32 pulse = phase < 0x20000000u ? b.pulse_on : b.pulse_off;
            phase += increment;
            // The body: DEC's level over the quiet tail. The Machinedrum's sine starts upwards,
            // and its first half cycle is small; at the turn it is passing nothing, so it can jump.
            if (!halfway) {
                if (phase < 0x80000000u) {
                    halfway = 1;
                    gate = md::one;
                } else {
                    gate = multiply(gate, b.first_step);
                    if (gate < first_least) gate = first_least;
                }
            }
            mixed = multiply(b.clip.run(multiply(pulse - opxy::q31::half(sine), body_level) * 2, highpass), multiply(held(level, tail), gate));
        }
        increment += static_cast<u32>(slope);
        mixed += multiply(multiply(md::smooth(tone, opxy::q31::half(md::noise(seed)), burst_tone), multiply(burst_level, b.noise)), body_level) * 2;
        if (age < click_length) {
            const i32 soft = click_soft(age);
            after = soft + multiply(click_hard(age) - soft, b.strike);
        } else {
            after = multiply(after, b.after_step);
        }
        mixed = md::add(mixed, after);
        if (age < opening) ++age;
        output[i] = md::finish(md::gain(mixed, 1));
        level = multiply(level, b.decay_step);
        tail = multiply(tail, b.tail_step);
        burst_level = multiply(burst_level, b.burst_step);
    }
    voice.phase = phase; voice.age = age; voice.seed = seed; voice.halfway = halfway; voice.gate = gate;
    voice.level = level; voice.tail = tail; voice.burst = burst_level; voice.tone = tone; voice.after = after; voice.highpass = highpass;
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase = 0;
    voice.age = opening;
    voice.gate = md::one;
    voice.halfway = 1;
    voice.after = 0;
    voice.seed = 0x1234567u * owner;
    voice.stale = 1;
    voice.carrying = 0;
    voice.level = voice.tail = voice.ramp = voice.burst = voice.thump = 0;
    voice.tone = voice.highpass = 0;
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
    if (voice.fresh) {
        // A half turn on, where the sine (subtracted) starts rising.
        voice.phase = 0x80000000u;
        voice.highpass = 0;
        voice.age = 0;
        voice.gate = first_gain;
        voice.halfway = 0;
        voice.after = 0;
        voice.level = md::one;
        voice.tail = tail_start;
        voice.ramp = md::one;
        voice.burst = md::one;
        voice.thump = b.depth;
        voice.tone = 0;
        voice.carrying = 0;
        voice.fresh = 0;
    }
    // The pitch in Hz above the base, as Q28 of middle C, now and at the block's
    // end: the thump falls in a straight line, the sweep exponentially. Between
    // the two the increment moves in a straight line.
    const u32 base = b.base;
    const i32 thump_next = voice.thump > b.fall * 64 ? voice.thump - b.fall * 64 : 0;
    const i32 ramp_next = multiply(voice.ramp, b.ramp_block);
    const u32 now = base + md::scale(pitch[0], voice.thump + multiply(voice.ramp, b.ramp));
    const u32 next = base + md::scale(pitch[0], thump_next + multiply(ramp_next, b.ramp));
    const i32 slope = (static_cast<i32>(next) - static_cast<i32>(now)) / 64;
    voice.thump = thump_next;
    voice.ramp = ramp_next;

    if (voice.age >= opening && voice.halfway) {
        // Settled: the click is over and its tail gone. The body's gain and the
        // burst's, envelopes and levels together, in straight lines across the block.
        const i32 level_next = multiply(voice.level, b.decay_block);
        const i32 tail_next = multiply(voice.tail, b.tail_block);
        const i32 burst_next = multiply(voice.burst, b.burst_block);
        i32 body_gain = held(voice.level, voice.tail);
        const i32 body_slope = (held(level_next, tail_next) - body_gain) / 64;
        // From where the last block's ended, so NOIS moved glides.
        i32 burst_gain = voice.carrying ? voice.burst_out : multiply(multiply(voice.burst, b.noise), body_level);
        const i32 burst_end = voice.burst_out = multiply(multiply(burst_next, b.noise), body_level);
        voice.carrying = 1;
        const i32 burst_slope = (burst_end - burst_gain) / 64;
        // The burst's noise runs only while it can be heard (-102 dB).
        const bool bursting = burst_gain >= (1 << 14) || burst_end >= (1 << 14);
        voice.level = level_next;
        voice.tail = tail_next;
        voice.burst = burst_next;
        if (b.clip.amount) run_clipped(voice, now, slope, b, body_gain, body_slope, output);
        else run_body(voice, now, slope, b.pulse_on, b.pulse_off, body_gain, body_slope, output);
        if (bursting) run_burst(voice, burst_gain, burst_slope, output);
        run_finish(output);
        return;
    }
    run_opening(voice, b, now, slope, output);
}
}
}
