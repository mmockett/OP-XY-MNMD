#pragma once
// TRX-SD: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Snare: a Machinedrum TRX-SD-style snare, written from scratch in fixed
// point and fitted to the Machinedrum's own (reference/fit_sd.py). Top row on
// the encoders, bottom row as their alternates:
//   PTCH  DEC   BUMP  BENV
//   SNAP  TONE  TUNE  CLIP
// The body is one sine, 341 Hz at the default PTCH 48, with a small pitch bump (BUMP,
// BENV). On the Machinedrum PTCH barely moves it (339 .. 345 Hz); here it goes further, five
// semitones either way, finely around the default. As on the Machinedrum, TONE adds the wires: a 6.98 kHz
// sine ring-modulated by high-passed noise, so a strong line with noise 10 dB
// under it either side and a notch around it, swelling in over 5 to 10 ms and decaying
// with the body (DEC). SNAP adds a 2 .. 7 kHz crack that holds 15 ms. TUNE has no measurable
// effect on the Machinedrum; here it tunes the wires half an octave either way.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace trx_sd {
namespace {
using md::multiply;

enum { PTCH, DEC, BUMP, BENV, SNAP, TONE, TUNE, CLIP };

const i32 body_home = 25123;                          // 341 Hz at middle C at PTCH 48, Q16 octaves
const i32 ptch_home = 811647360;                     // PTCH 48
const i32 ptch_up = 1726141916, ptch_down = 1420470959;     // the rest of PTCH's way either side to 0 .. 1: * 2, * 4
const i32 ptch_reach = 27307;                        // five semitones
const i32 ptch_line = 0x33333333, ptch_cube = 0x4ccccccd;   // 0.4 in a line near the default, 0.6 as a cube
const i32 bump_most = 123123481;                     // 120 Hz (Q28 of middle C), as BUMP squared
const i32 bump_short = 671808, bump_long = 832989;   // its time constant 4 .. 22 ms, as -60 dB times
const i32 decay_short = 791506, decay_long = 1066696;// body and wires: -60 dB in 98 ms .. 1.8 s
const u32 wires_increment = 679793010;               // 6.98 kHz
const i32 wires_tune = md::octave / 2;               // TUNE's reach either way
const i32 wires_highpass = 305964574;                  // the modulating noise from 1 kHz
const i32 wires_carrier = 0x40000000;                // the line, 10 dB over the noise either side
const i32 swell_rate = 9739155;                      // the wires' rise: squared, 5 ms
const i32 wires_most = 0x0e000000;                  // about 13 dB under the body's peak at full TONE
const u32 snap_hold = 662;                           // 15 ms
const i32 snap_time = 837854;                        // then -60 dB in 160 ms
const i32 snap_highpass = 764911434, snap_lowpass = 843229609;   // 2.5 .. 3.5 kHz corners, two poles each side: 2 .. 7 kHz at -6 dB
const i32 snap_most = 0x7fffffff;
const i32 body_level = 0x10000000;                   // the body peaks at an eighth of full scale

// PTCH to the body's pitch: exactly body_home at the default, then fine, then wide.
inline i32 body_pitch(i32 control) {
    const i32 away = md::positive(control) - ptch_home;
    const i32 x = away >= 0 ? multiply(away, ptch_up) * 2 : multiply(-away, ptch_down) * 4;
    const i32 curve = multiply(x, ptch_line) + multiply(multiply(multiply(x, x), x), ptch_cube);
    const i32 moved = multiply(curve, ptch_reach);
    return away >= 0 ? body_home + moved : body_home - moved;
}
}
struct Block {
    i32 body, bump, bump_block, decay_block, swell_block, wires, snap, snap_step;
    u32 wires_increment, base;
    md::Clip clip;
    void plan(const i32* p, u32 middle) {
        body = body_pitch(p[PTCH]);
        bump = multiply(md::squared(p[BUMP]), bump_most);
        bump_block = md::per_block(md::decay(md::span(p[BENV], bump_short, bump_long)));
        decay_block = md::per_block(md::decay(md::span(p[DEC], decay_short, decay_long)));
        swell_block = md::per_block(md::one - swell_rate);
        wires = multiply(md::positive(p[TONE]), wires_most);   // linear in TONE, as measured
        snap = multiply(md::positive(p[SNAP]), snap_most);
        wires_increment = md::transpose(trx_sd::wires_increment, md::span(p[TUNE], -wires_tune, wires_tune));
        clip.prepare(p[CLIP], body_level, 0x63000000, 0x247ae148);   // from about 100: 1 / (1 - 0.773)
        snap_step = md::decay(snap_time);
        base = md::transpose(middle, body);
    }
};
struct State {
    u32 phase;        // the body's sine
    u32 wires_phase;  // the wires' 6.98 kHz sine
    i32 body;         // the body's level, Q31
    i32 wires;        // the wires' level, Q31
    i32 swell;        // the wires' 4 ms rise, Q31
    i32 bump;         // BUMP's pitch envelope, Q31
    i32 snap;         // SNAP's level, Q31
    u32 snap_age;     // samples of SNAP's hold still to run
    u32 seed;
    i32 wires_low;    // the low part of the wires' noise, taken away to high-pass it
    i32 snap_low, snap_low2;    // SNAP's high-pass states
    i32 snap_high, snap_high2;  // SNAP's low-pass states
    i32 highpass;     // CLIP's high-pass state
    i32 body_out, wires_out, snap_out;   // the gains each block ended on, which the next starts from
    u32 carrying;     // they are set: a note's first block starts from its envelope
    u32 fresh;
    u32 stale;        // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];      // the controls they were worked out from
    Block block;
};

// The block's stages, a loop each: out of line, so each has a hardware loop and all the
// registers to itself, and on locals, as the buffers might otherwise be the voice.
__attribute__((noinline)) inline void run_body(State& voice, u32 increment, i32 slope, i32 gain, i32 gain_slope, i32* mixed) {
    u32 phase = voice.phase;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        mixed[i] = multiply(md::sine(phase), gain);
        phase += increment;
        increment += static_cast<u32>(slope);
        gain += gain_slope;
    }
    voice.phase = phase;
}
// The wires: a 6.98 kHz sine ring-modulated by a carrier plus high-passed noise.
__attribute__((noinline)) inline void run_wires(State& voice, u32 increment, i32 gain, i32 gain_slope, i32* white, i32* mixed) {
    u32 phase = voice.wires_phase, seed = voice.seed;
    i32 low = voice.wires_low;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 n = md::noise_eighth(seed);
        white[i] = n;
        md::smooth(low, n, wires_highpass);
        // n and low are within an eighth of full scale, so the doubling cannot saturate.
        const i32 ring = multiply(md::sine(phase), wires_carrier + (n - low) * 2);
        phase += increment;
        mixed[i] += multiply(ring, gain);
        gain += gain_slope;
    }
    voice.wires_phase = phase; voice.seed = seed; voice.wires_low = low;
}
// SNAP: 1.2 .. 7 kHz of the same noise, held 10 ms, then falling.
__attribute__((noinline)) inline void run_snap(State& voice, i32 gain, i32 gain_slope, const i32* white, i32* mixed) {
    i32 low = voice.snap_low, low2 = voice.snap_low2, high = voice.snap_high, high2 = voice.snap_high2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 n = white[i];
        md::smooth(low, n, snap_highpass);
        md::smooth(low2, n - low, snap_highpass);
        md::smooth(high, n - low - low2, snap_lowpass);
        md::smooth(high2, high, snap_lowpass);
        mixed[i] += multiply(md::gain(high2, 3), gain);
        gain += gain_slope;
    }
    voice.snap_low = low; voice.snap_low2 = low2; voice.snap_high = high; voice.snap_high2 = high2;
}
// Doubling then stopping at half scale is stopping at a quarter, then doubling.
__attribute__((noinline)) inline void finish_clean(const i32* mixed, opxy::Output output) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = md::clamp(mixed[i], -0x20000000, 0x20000000) * 2;
}
__attribute__((noinline)) inline void finish_clipped(State& voice, const md::Clip& clip, const i32* mixed, opxy::Output output) {
    i32 highpass = voice.highpass;
    const md::Clip c = clip;
    for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = md::finish(md::gain(c.run(mixed[i], highpass), 1));
    voice.highpass = highpass;
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase = voice.wires_phase = 0;
    voice.body = voice.wires = voice.swell = voice.bump = voice.snap = 0;
    voice.snap_age = 0;
    voice.seed = 0x2545f491u * owner;
    voice.carrying = 0;
    voice.stale = 1;
    voice.wires_low = voice.snap_low = voice.snap_low2 = voice.snap_high = voice.snap_high2 = voice.highpass = 0;
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
        voice.phase = 0;
        voice.body = voice.wires = voice.bump = voice.snap = md::one;
        voice.swell = 0;
        voice.snap_age = snap_hold;
        voice.highpass = 0;
        voice.snap_low = voice.snap_low2 = voice.snap_high = voice.snap_high2 = 0;
        voice.carrying = 0;
        voice.fresh = 0;
    }
    const i32 snap_step = b.snap_step;

    // The body's pitch plus the bump, in Hz above it; linear between block ends.
    const u32 base = b.base;
    const i32 bump_next = multiply(voice.bump, b.bump_block);
    const u32 now = base + md::scale(pitch[0], multiply(voice.bump, b.bump));
    const u32 next = base + md::scale(pitch[0], multiply(bump_next, b.bump));
    const i32 slope = (static_cast<i32>(next) - static_cast<i32>(now)) / 64;
    voice.bump = bump_next;

    // The three gains, envelope and level together, in straight lines across the
    // block: the body's, the wires' (with their swell) and SNAP's.
    const i32 decay_block = b.decay_block;
    const i32 body_next = multiply(voice.body, decay_block), wires_next = multiply(voice.wires, decay_block);
    const i32 swell_next = md::one - multiply(md::one - voice.swell, b.swell_block);
    i32 snap_next = voice.snap;
    if (voice.snap_age >= opxy::block_frames) {
        voice.snap_age -= opxy::block_frames;
    } else {
        // The hold ends in this block, or has: the rest of it falls.
        for (unsigned i = voice.snap_age; i < opxy::block_frames; ++i) snap_next = multiply(snap_next, snap_step);
        voice.snap_age = 0;
    }
    // Each from where the last block's ended, so TONE or SNAP moved glides.
    i32 body_gain = voice.carrying ? voice.body_out : multiply(voice.body, body_level);
    voice.body_out = multiply(body_next, body_level);
    const i32 body_slope = (voice.body_out - body_gain) / 64;
    i32 wires_gain = voice.carrying ? voice.wires_out : multiply(multiply(voice.wires, multiply(voice.swell, voice.swell)), b.wires);
    voice.wires_out = multiply(multiply(wires_next, multiply(swell_next, swell_next)), b.wires);
    const i32 wires_slope = (voice.wires_out - wires_gain) / 64;
    i32 snap_gain = voice.carrying ? voice.snap_out : multiply(voice.snap, b.snap);
    const i32 snap_end = voice.snap_out = multiply(snap_next, b.snap);
    const i32 snap_slope = (snap_end - snap_gain) / 64;
    voice.carrying = 1;
    voice.body = body_next;
    voice.wires = wires_next;
    voice.swell = swell_next;
    voice.snap = snap_next;
    // SNAP's filters run only while it can be heard (-102 dB).
    const bool snapping = snap_gain >= (1 << 14) || snap_end >= (1 << 14);

    i32 mixed[opxy::block_frames], white[opxy::block_frames];
    run_body(voice, now, slope, body_gain, body_slope, mixed);
    run_wires(voice, b.wires_increment, wires_gain, wires_slope, white, mixed);
    if (snapping) run_snap(voice, snap_gain, snap_slope, white, mixed);
    if (b.clip.amount) finish_clipped(voice, b.clip, mixed, output);
    else finish_clean(mixed, output);
}
}
}
