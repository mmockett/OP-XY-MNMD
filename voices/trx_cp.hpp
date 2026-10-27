#pragma once
// TRX-CP: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Clap: a Machinedrum TRX-CP-style clap, written from scratch in fixed point
// and fitted to the Machinedrum's own (reference/fit_cp.py). A train of hands of
// broadband noise, then a tuned room. Top row on the encoders, bottom row as
// alternates:
//   CLPY  TONE  HARD  RICH
//   RATE  ROOM  RSIZ  RTUN
// As measured: four hands of noise from about 0.5 to 8 kHz; HARD brightens them
// (two low-passes from 3 to 7 kHz), TONE only a little (the high-pass from
// 200 Hz to 1 kHz). RICH is how long each hand rings, RATE spreads the train from
// 3 to 25 ms a hand. On the Machinedrum CLPY barely changes the measured sound;
// here it loosens the hands' timing, from machine-tight to human. The room is a
// narrow band from 550 Hz to 3 kHz (RTUN), falling 60 dB in 40 to 500 ms (RSIZ),
// nearly as loud at any ROOM above a quarter. A clap has no pitch: keys play it alike.
namespace md {
namespace trx_cp {
struct State {
    u32 seed;
    i32 burst;       // the current hand's level, Q31
    i32 room;        // the room's level, Q31
    u32 clock;       // samples since the note started
    u32 fired;       // hands sounded so far
    u32 next;        // when the next hand starts
    i32 low;         // the hands' high-pass state
    i32 bright, brighter;   // the hands' two low-passes
    md::Svf space;   // the room's band-pass
    u32 fresh;
};
namespace {
using md::multiply;

enum { CLPY, TONE, HARD, RICH, RATE, ROOM, RSIZ, RTUN };

const i32 highpass_low = 60329286, highpass_high = 285167595;   // 200 Hz .. 1 kHz one-pole
const i32 bright_low = 746927805, bright_high = 1355357198;   // two one-pole low-passes, 3 .. 7 kHz
const i32 ring_short = 614046, ring_long = 890765;              // each hand: -60 dB in 15 .. 280 ms
const u32 hands = 4;
const i32 gap_shortest = 132, gap_range = 970;                  // 3 .. 25 ms, in samples
// The room's -60 dB time at RSIZ 0, 16, ..., 127: 40 .. 500 ms, Q16 log2 samples.
const i32 room_times[9] = {706782, 810654, 854102, 883758, 903390, 919638, 931326, 939735, 945586};
const i32 room_low = 313351, room_high = 473748;                // 550 Hz .. 3 kHz
const i32 room_damping = 0x10000000;                            // a band about a third of an octave wide
const i32 room_most = 0x30000000;

struct Block {
    i32 gap, looseness, highpass, bright, burst_step, room_level, room_step, room_frequency;
    explicit Block(const i32* p) {
        gap = gap_shortest + multiply(md::positive(p[RATE]), gap_range);
        looseness = opxy::q31::half(md::positive(p[CLPY]));             // up to half a gap either way
        highpass = md::span(p[TONE], highpass_low, highpass_high);
        bright = md::span(p[HARD], bright_low, bright_high);
        burst_step = md::decay(md::span(p[RICH], ring_short, ring_long));
        const i32 q = md::one - md::positive(p[ROOM]);                    // 1 - (1 - ROOM)^3
        room_level = multiply(md::one - multiply(multiply(q, q), q), room_most);
        room_step = md::decay(md::table9(room_times, p[RSIZ]));
        room_frequency = md::cutoff(md::span(p[RTUN], room_low, room_high));
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.seed = 0x41c64e6du * owner;
    voice.burst = voice.room = 0;
    voice.clock = 0;
    voice.fired = hands;
    voice.next = 0;
    voice.low = voice.bright = voice.brighter = 0;
    voice.space.reset();
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.clock = 0;
        voice.next = 0;
        voice.fired = 0;
        voice.fresh = 0;
    }
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        // Each hand starts on its beat, nudged by CLPY; the room opens with the last.
        const i32 white = md::noise_eighth(voice.seed);
        if (voice.fired < hands && voice.clock >= voice.next) {
            voice.burst = md::one;
            if (++voice.fired == hands) voice.room = b.room_level;
            // white is +-1/8: times 4 and the looseness, up to half a gap either way.
            voice.next = voice.clock + static_cast<u32>(b.gap + multiply(multiply(white * 4, b.looseness), b.gap));
        }
        ++voice.clock;
        md::smooth(voice.low, white, b.highpass);
        md::smooth(voice.brighter, md::smooth(voice.bright, white - voice.low, b.bright), b.bright);
        voice.space.run(white, b.room_frequency, room_damping);
        const i32 hands = multiply(voice.brighter * 2, voice.burst);
        // The band-pass peaks at 1 / (2 damping): scale back to the input's level.
        const i32 room = multiply(multiply(voice.space.band, room_damping) * 2, voice.room);
        output[i] = md::finish((hands + room) * 3);
        voice.burst = multiply(voice.burst, b.burst_step);
        voice.room = multiply(voice.room, b.room_step);
    }
}
}
}
