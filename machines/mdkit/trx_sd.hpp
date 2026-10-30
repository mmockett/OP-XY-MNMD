#pragma once
// TRX-SD: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Snare: a Machinedrum TRX-SD-style snare, written from scratch in fixed
// point and fitted to the Machinedrum's own (reference/fit_sd.py). Top row on
// the encoders, bottom row as their alternates:
//   PTCH  DEC   BUMP  BENV
//   SNAP  TONE  TUNE  CLIP
// As on the Machinedrum: the body is one sine near 342 Hz, which PTCH barely
// moves, with a small pitch bump (BUMP, BENV). TONE adds the wires: a 6.98 kHz
// sine ring-modulated by high-passed noise, so a strong line with noise 10 dB
// under it either side and a notch around it, swelling in over 5 to 10 ms and decaying
// with the body (DEC). SNAP adds a 2 .. 7 kHz crack that holds 15 ms. TUNE has no measurable
// effect on the Machinedrum; here it tunes the wires half an octave either way.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace trx_sd {
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
    u32 fresh;
};
namespace {
using md::multiply;

enum { PTCH, DEC, BUMP, BENV, SNAP, TONE, TUNE, CLIP };

const i32 body_low = 24496, body_high = 26155;       // 339 .. 345 Hz at middle C, Q16 octaves
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

struct Block {
    i32 body, bump, bump_step, decay_step, wires, snap;
    u32 wires_increment;
    md::Clip clip;
    explicit Block(const i32* p) {
        body = md::span(p[PTCH], body_low, body_high);
        bump = multiply(md::squared(p[BUMP]), bump_most);
        bump_step = md::decay(md::span(p[BENV], bump_short, bump_long));
        decay_step = md::decay(md::span(p[DEC], decay_short, decay_long));
        wires = multiply(md::positive(p[TONE]), wires_most);   // linear in TONE, as measured
        snap = multiply(md::positive(p[SNAP]), snap_most);
        wires_increment = md::transpose(trx_sd::wires_increment, md::span(p[TUNE], -wires_tune, wires_tune));
        clip.prepare(p[CLIP], body_level, 0x63000000, 0x247ae148);   // from about 100: 1 / (1 - 0.773)
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase = voice.wires_phase = 0;
    voice.body = voice.wires = voice.swell = voice.bump = voice.snap = 0;
    voice.snap_age = 0;
    voice.seed = 0x2545f491u * owner;
    voice.wires_low = voice.snap_low = voice.snap_low2 = voice.snap_high = voice.snap_high2 = voice.highpass = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.phase = 0;
        voice.body = voice.wires = voice.bump = voice.snap = md::one;
        voice.swell = 0;
        voice.snap_age = snap_hold;
        voice.highpass = 0;
        voice.snap_low = voice.snap_low2 = voice.snap_high = voice.snap_high2 = 0;
        voice.fresh = 0;
    }
    const i32 snap_step = md::decay(snap_time);

    // The body's pitch plus the bump, in Hz above it; linear between block ends.
    const u32 base = md::transpose(pitch[0], b.body);
    const i32 bump_next = multiply(voice.bump, md::per_block(b.bump_step));
    const u32 now = base + md::scale(pitch[0], multiply(voice.bump, b.bump));
    const u32 next = base + md::scale(pitch[0], multiply(bump_next, b.bump));
    u32 increment = now;
    const i32 slope = (static_cast<i32>(next) - static_cast<i32>(now)) / 64;
    voice.bump = bump_next;

    // The three gains, envelope and level together, in straight lines across the
    // block: the body's, the wires' (with their swell) and SNAP's.
    const i32 decay_block = md::per_block(b.decay_step);
    const i32 body_next = multiply(voice.body, decay_block), wires_next = multiply(voice.wires, decay_block);
    const i32 swell_next = md::one - multiply(md::one - voice.swell, md::per_block(md::one - swell_rate));
    i32 snap_next = voice.snap;
    if (voice.snap_age >= opxy::block_frames) {
        voice.snap_age -= opxy::block_frames;
    } else {
        // The hold ends in this block, or has: the rest of it falls.
        for (unsigned i = voice.snap_age; i < opxy::block_frames; ++i) snap_next = multiply(snap_next, snap_step);
        voice.snap_age = 0;
    }
    i32 body_gain = multiply(voice.body, body_level);
    const i32 body_slope = (multiply(body_next, body_level) - body_gain) / 64;
    i32 wires_gain = multiply(multiply(voice.wires, multiply(voice.swell, voice.swell)), b.wires);
    const i32 wires_slope = (multiply(multiply(wires_next, multiply(swell_next, swell_next)), b.wires) - wires_gain) / 64;
    i32 snap_gain = multiply(voice.snap, b.snap);
    const i32 snap_end = multiply(snap_next, b.snap);
    const i32 snap_slope = (snap_end - snap_gain) / 64;
    voice.body = body_next;
    voice.wires = wires_next;
    voice.swell = swell_next;
    voice.snap = snap_next;
    // SNAP's filters run only while it can be heard (-102 dB).
    const bool snapping = snap_gain >= (1 << 14) || snap_end >= (1 << 14);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 body = multiply(md::sine(voice.phase), body_gain);
        voice.phase += increment;
        increment += static_cast<u32>(slope);

        // The wires: a 6.98 kHz sine ring-modulated by a carrier plus high-passed noise.
        const i32 white = md::noise_eighth(voice.seed);
        md::smooth(voice.wires_low, white, wires_highpass);
        const i32 ring = multiply(md::sine(voice.wires_phase), wires_carrier + md::gain(white - voice.wires_low, 1));
        voice.wires_phase += b.wires_increment;
        i32 mixed = body + multiply(ring, wires_gain);

        // SNAP: 1.2 .. 7 kHz of the same noise, held 10 ms, then falling.
        if (snapping) {
            md::smooth(voice.snap_low, white, snap_highpass);
            md::smooth(voice.snap_low2, white - voice.snap_low, snap_highpass);
            md::smooth(voice.snap_high, white - voice.snap_low - voice.snap_low2, snap_lowpass);
            md::smooth(voice.snap_high2, voice.snap_high, snap_lowpass);
            mixed += multiply(md::gain(voice.snap_high2, 3), snap_gain);
        }
        output[i] = md::finish(md::gain(b.clip.run(mixed, voice.highpass), 1));
        body_gain += body_slope;
        wires_gain += wires_slope;
        snap_gain += snap_slope;
    }
}
}
}
