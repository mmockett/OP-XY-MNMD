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
// 1 to 20.5 ms a hand. On the Machinedrum CLPY barely changes the measured sound;
// here it loosens the hands' timing, from machine-tight to human. The room is a
// narrow band from 550 Hz to 3 kHz (RTUN), falling 60 dB in 40 to 500 ms (RSIZ),
// nearly as loud at any ROOM above a quarter, louder the bigger. It falls away as steeply as the
// Machinedrum's, 30 dB an octave above the band and 6 below: a band-pass over two
// low-pass poles a little under its centre. A clap has no pitch: keys play it alike.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
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
    i32 air1, air2;  // the room's two low-pass poles
    u32 fresh;
};
namespace {
using md::multiply;

enum { CLPY, TONE, HARD, RICH, RATE, ROOM, RSIZ, RTUN };

const i32 highpass_low = 60329286, highpass_high = 285167595;   // 200 Hz .. 1 kHz one-pole
const i32 bright_low = 746927805, bright_high = 1355357198;   // two one-pole low-passes, 3 .. 7 kHz
const i32 ring_short = 614046, ring_long = 890765;              // each hand: -60 dB in 15 .. 280 ms
const u32 hands = 4;
const i32 gap_shortest = 40, gap_range = 864;                   // 1 .. 20.5 ms, in samples
// The room's -60 dB time at RSIZ 0, 16, ..., 127: 40 .. 500 ms, Q16 log2 samples.
const i32 room_times[9] = {706782, 810654, 854102, 883758, 903390, 919638, 931326, 939735, 945586};
const i32 room_low = 313351, room_high = 473748;                // 550 Hz .. 3 kHz
const i32 room_damping = 0x0da1cac0;                            // a band about a third of an octave wide
const i32 room_most = 0x48000000;                               // and the mix takes it four times
// RSIZ 0, 16, ..., 127: a bigger room is louder too, by 11 dB, as measured over the hands.
const i32 room_sizes[9] = {605243126, 605243126, 605243126, 605243126, 761955951, 1076291388, 1354970579, 1705806894, 2147483647};

struct Block {
    i32 gap, looseness, highpass, bright, burst_step, room_level, room_step, room_frequency, room_pole;
    explicit Block(const i32* p) {
        gap = gap_shortest + multiply(md::positive(p[RATE]), gap_range);
        looseness = opxy::q31::half(md::positive(p[CLPY]));             // up to half a gap either way
        highpass = md::span(p[TONE], highpass_low, highpass_high);
        bright = md::span(p[HARD], bright_low, bright_high);
        burst_step = md::decay(md::span(p[RICH], ring_short, ring_long));
        const i32 q = md::one - md::positive(p[ROOM]);                    // 1 - (1 - ROOM)^3
        room_level = multiply(multiply(md::one - multiply(multiply(q, q), q), room_most), md::table9(room_sizes, p[RSIZ]));
        room_step = md::decay(md::table9(room_times, p[RSIZ]));
        room_frequency = md::cutoff(md::span(p[RTUN], room_low, room_high));
        room_pole = multiply(room_frequency, 0x74000000);
    }
};
}

// Whether a sound is still to come however quiet it is now: a hand.
inline bool waiting(const State& voice) { return voice.fired < hands; }
inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.seed = 0x41c64e6du * owner;
    voice.burst = voice.room = 0;
    voice.clock = 0;
    voice.fired = hands;
    voice.next = 0;
    voice.low = voice.bright = voice.brighter = 0;
    voice.space.reset();
    voice.air1 = voice.air2 = 0;
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
    if (voice.fired == hands) {
        // Every hand has started: no more timing. The two gains in straight
        // lines across the block, and the hands' filters only while they can
        // be heard (-102 dB).
        const i32 burst_next = multiply(voice.burst, md::per_block(b.burst_step));
        const i32 room_next = multiply(voice.room, md::per_block(b.room_step));
        i32 burst = voice.burst, room = voice.room;
        const i32 burst_slope = (burst_next - burst) / 64, room_slope = (room_next - room) / 64;
        const bool clapping = burst >= (1 << 14) || burst_next >= (1 << 14);
        voice.burst = burst_next;
        voice.room = room_next;
        for (unsigned i = 0; i != opxy::block_frames; ++i) {
            const i32 white = md::noise_eighth(voice.seed);
            i32 mixed = 0;
            if (clapping) {
                md::smooth(voice.low, white, b.highpass);
                md::smooth(voice.brighter, md::smooth(voice.bright, white - voice.low, b.bright), b.bright);
                mixed = multiply(voice.brighter * 2, burst);
            }
            voice.space.run(white, b.room_frequency, room_damping);
            md::smooth(voice.air2, md::smooth(voice.air1, voice.space.band, b.room_pole), b.room_pole);
            mixed += multiply(voice.air2, room) * 4;
            output[i] = md::finish(mixed * 3);
            burst += burst_slope;
            room += room_slope;
        }
        return;
    }
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        // Each hand starts on its beat, nudged by CLPY; the room opens with the last.
        const i32 white = md::noise_eighth(voice.seed);
        if (voice.fired < hands && voice.clock >= voice.next) {
            voice.burst = md::one;
            if (++voice.fired == hands) voice.room = b.room_level;
            // white is +-1/8: times 4 and the looseness, up to half a gap either way.
            // The Machinedrum's gaps are uneven, the second an eighth longer and the third an eighth shorter.
            const i32 even = voice.fired == 2 ? b.gap + b.gap / 8 : voice.fired == 3 ? b.gap - b.gap / 8 : b.gap;
            voice.next = voice.clock + static_cast<u32>(even + multiply(multiply(white * 4, b.looseness), b.gap));
        }
        ++voice.clock;
        md::smooth(voice.low, white, b.highpass);
        md::smooth(voice.brighter, md::smooth(voice.bright, white - voice.low, b.bright), b.bright);
        voice.space.run(white, b.room_frequency, room_damping);
        md::smooth(voice.air2, md::smooth(voice.air1, voice.space.band, b.room_pole), b.room_pole);
        const i32 hands = multiply(voice.brighter * 2, voice.burst);
        const i32 room = multiply(voice.air2, voice.room) * 4;
        output[i] = md::finish((hands + room) * 3);
        voice.burst = multiply(voice.burst, b.burst_step);
        voice.room = multiply(voice.room, b.room_step);
    }
}
}
}
