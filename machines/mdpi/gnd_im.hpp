#pragma once
// GND-IM: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

// An impulse in the manner of the Machinedrum's GND-IM, written from scratch in
// fixed point and fitted to it (reference/fit_gnd.py). The controls:
//   UP    UVAL  DOWN  DVAL
// Two steps below zero, then back to it, through a 35 Hz high-pass (the
// Machinedrum's DC blocking): the first lasts UP samples at UVAL's depth, the
// second DOWN samples at DVAL's; a depth goes as its control squared, to a
// quarter of full scale at 127.
namespace md {
namespace gnd_im {
struct State {
    u32 count;      // samples since the hit
    i32 low;        // the high-pass's state
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { UP, UVAL, DOWN, DVAL };

// The high-pass: one pole at 35 Hz.
const i32 highpass = 10682104;
// The depth at 127: the Machinedrum's 0.252, 2 dB up as the TRX machines are.
const i32 depth = 0x28a47ed0;

// A length control as samples, 0 .. 128.
inline u32 samples(i32 control) { return static_cast<u32>(md::positive(control)) >> 24; }
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.count = 0xffffffffu;
    voice.low = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch) {
    if (voice.fresh) {
        voice.count = 0;
        voice.low = 0;
        voice.fresh = 0;
    }
    const u32 up = samples(parameters[UP]), down = up + samples(parameters[DOWN]);
    const i32 first = -multiply(md::squared(parameters[UVAL]), depth);
    const i32 second = -multiply(md::squared(parameters[DVAL]), depth);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 x = voice.count < up ? first : voice.count < down ? second : 0;
        if (voice.count < down) ++voice.count;
        voice.low += multiply(x - voice.low, highpass);
        output[i] = md::finish(x - voice.low);
    }
}
}
}
