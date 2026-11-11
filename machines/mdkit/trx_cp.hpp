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

}
struct Block {
    i32 gap, looseness, highpass, bright, burst_step, burst_block, room_level, room_step, room_block, room_frequency, room_pole;
    void plan(const i32* p) {
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
        burst_block = md::per_block(burst_step);
        room_block = md::per_block(room_step);
    }
};

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
    u32 stale;        // the block's figures want working out, whatever the controls read
    i32 seen[8];      // the controls they were worked out from
    Block block;
};

// Whether a sound is still to come however quiet it is now: a hand.
inline bool waiting(const State& voice) { return voice.fired < hands; }
inline void trigger(State& voice, Common&) { voice.fresh = 1; }
// The block's stages, a loop each: out of line, so each has a hardware loop and all the
// registers to itself, and on locals, as the buffers might otherwise be the voice.
// The room, in a line across the block: it leaves the noise for the hands.
__attribute__((noinline)) inline void run_room(State& voice, const Block& b, i32 room, i32 room_slope, i32* white, i32* mixed) {
    u32 seed = voice.seed;
    md::Svf space = voice.space;
    i32 air1 = voice.air1, air2 = voice.air2;
    const i32 frequency = b.room_frequency, pole = b.room_pole;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 n = md::noise_eighth(seed);
        white[i] = n;
        space.step(n, frequency, room_damping);
        md::smooth(air2, md::smooth(air1, space.band, pole), pole);
        mixed[i] = multiply(air2, room) * 4;
        room += room_slope;
    }
    voice.seed = seed;
    voice.space = space;
    voice.air1 = air1;
    voice.air2 = air2;
}
__attribute__((noinline)) inline void run_hands(State& voice, i32 highpass, i32 bright, i32 burst, i32 burst_slope, const i32* white, i32* mixed) {
    i32 low = voice.low, b1 = voice.bright, b2 = voice.brighter;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        md::smooth(low, white[i], highpass);
        md::smooth(b2, md::smooth(b1, white[i] - low, bright), bright);
        mixed[i] += multiply(b2 * 2, burst);
        burst += burst_slope;
    }
    voice.low = low;
    voice.bright = b1;
    voice.brighter = b2;
}
__attribute__((noinline)) inline void run_finish(const i32* mixed, opxy::Output output) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = md::finish(mixed[i] * 3);
}
// Until the last hand has started: each starts on its beat, nudged by CLPY; the room opens with the last.
__attribute__((noinline)) inline void run_starting(State& voice, const Block& b, opxy::Output output) {
    u32 seed = voice.seed, clock = voice.clock, fired = voice.fired, next = voice.next;
    i32 burst = voice.burst, room = voice.room, low = voice.low, b1 = voice.bright, b2 = voice.brighter, air1 = voice.air1, air2 = voice.air2;
    md::Svf space = voice.space;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 white = md::noise_eighth(seed);
        if (fired < trx_cp::hands && clock >= next) {
            burst = md::one;
            if (++fired == trx_cp::hands) room = b.room_level;
            // white is +-1/8: times 4 and the looseness, up to half a gap either way.
            // The Machinedrum's gaps are uneven, the second an eighth longer and the third an eighth shorter.
            const i32 even = fired == 2 ? b.gap + b.gap / 8 : fired == 3 ? b.gap - b.gap / 8 : b.gap;
            next = clock + static_cast<u32>(even + multiply(multiply(white * 4, b.looseness), b.gap));
        }
        ++clock;
        md::smooth(low, white, b.highpass);
        md::smooth(b2, md::smooth(b1, white - low, b.bright), b.bright);
        space.step(white, b.room_frequency, room_damping);
        md::smooth(air2, md::smooth(air1, space.band, b.room_pole), b.room_pole);
        output[i] = md::finish((multiply(b2 * 2, burst) + multiply(air2, room) * 4) * 3);
        burst = multiply(burst, b.burst_step);
        room = multiply(room, b.room_step);
    }
    voice.seed = seed; voice.clock = clock; voice.fired = fired; voice.next = next;
    voice.burst = burst; voice.room = room; voice.low = low; voice.bright = b1; voice.brighter = b2;
    voice.air1 = air1; voice.air2 = air2; voice.space = space;
}

inline void reset(State& voice, Common& shared, u32 owner) {
    voice.seed = 0x41c64e6du * owner;
    voice.stale = 1;
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
    // Only when a control has moved.
    u32 moved = voice.stale;
    for (unsigned c = 0; c < 8; ++c) moved |= static_cast<u32>(parameters[c] ^ voice.seen[c]);
    if (moved) {
        voice.stale = 0;
        for (unsigned c = 0; c < 8; ++c) voice.seen[c] = parameters[c];
        voice.block.plan(parameters);
    }
    const Block& b = voice.block;
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
        const i32 burst_next = multiply(voice.burst, b.burst_block);
        const i32 room_next = multiply(voice.room, b.room_block);
        i32 burst = voice.burst, room = voice.room;
        const i32 burst_slope = (burst_next - burst) / 64, room_slope = (room_next - room) / 64;
        const bool clapping = burst >= (1 << 14) || burst_next >= (1 << 14);
        voice.burst = burst_next;
        voice.room = room_next;
        i32 white[opxy::block_frames], mixed[opxy::block_frames];
        run_room(voice, b, room, room_slope, white, mixed);
        if (clapping) run_hands(voice, b.highpass, b.bright, burst, burst_slope, white, mixed);
        run_finish(mixed, output);
        return;
    }
    run_starting(voice, b, output);
}
}
}
