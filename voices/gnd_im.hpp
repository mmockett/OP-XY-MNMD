#pragma once
// GND-IM: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

// An impulse in the manner of the Machinedrum's GND-IM, written from scratch in
// fixed point and fitted to it (reference/fit_gnd.py). The controls:
//   UP    UVAL  DOWN  DVAL
// Two steps below zero, then back to it: the first lasts UP's length at UVAL's
// depth, the second DOWN's at DVAL's. A length is its control in samples to 64,
// then 64 + (v - 64)^2 (exact at every step measured): 92 ms at 127, so UP 127 is
// heard as a second hit where the step ends. A depth goes as its control squared.
// Then the Machinedrum's output as measured from a long step (error -26 dB): a
// one-pole roll-off, two high-passes (16, 11.5 Hz) and a resonant one (8 Hz, Q 1.1),
// which swing past zero and leave the hit's low tail to 200 ms.
namespace md {
namespace gnd_im {
struct State {
    u32 count;      // samples since the hit
    i32 smooth;     // the roll-off's state
    i32 low1, low2; // the two high-passes' states
    i32 low3, band; // the resonant high-pass's
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { UP, UVAL, DOWN, DVAL };

// The output: the roll-off's share per sample, then the high-passes'.
const i32 smoothing = 0x6147ae14;
const i32 highpass1 = 4861300, highpass2 = 3523900;
const i32 resonant = 2463400, damping = 1968600000;
// A settled filter below this (-114 dB) is put to rest, so the voice ends at zero.
const i32 settled = 1 << 12;
// The depth at 127: the Machinedrum's 0.2615, 2 dB up as the TRX machines are.
const i32 depth = 0x2a238536;

// A length control as samples, 0 .. 4033.
inline u32 samples(i32 control) {
    const u32 v = static_cast<u32>(md::positive(control)) >> 24;
    return v <= 64 ? v : 64 + (v - 64) * (v - 64);
}
inline bool small(i32 x) { return x < settled && x > -settled; }
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.count = 0xffffffffu;
    voice.smooth = voice.low1 = voice.low2 = voice.low3 = voice.band = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch) {
    if (voice.fresh) {
        voice.count = 0;
        voice.smooth = voice.low1 = voice.low2 = voice.low3 = voice.band = 0;
        voice.fresh = 0;
    }
    const u32 up = samples(parameters[UP]), down = up + samples(parameters[DOWN]);
    const i32 first = -multiply(md::squared(parameters[UVAL]), depth);
    const i32 second = -multiply(md::squared(parameters[DVAL]), depth);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 x = voice.count < up ? first : voice.count < down ? second : 0;
        if (voice.count < down) ++voice.count;
        voice.smooth += multiply(x - voice.smooth, smoothing);
        voice.low1 += multiply(voice.smooth - voice.low1, highpass1);
        const i32 high1 = voice.smooth - voice.low1;
        voice.low2 += multiply(high1 - voice.low2, highpass2);
        const i32 high = high1 - voice.low2 - voice.low3 - multiply(voice.band, damping);
        voice.band += multiply(high, resonant);
        voice.low3 += multiply(voice.band, resonant);
        output[i] = md::finish(high);
    }
    // Every stage settled, it rests, and the voice ends at zero.
    if (voice.count >= down && small(voice.smooth) && small(voice.smooth - voice.low1) && small(voice.low2) &&
        small(voice.low3) && small(voice.band) && small(output[opxy::block_frames - 1]))
        voice.smooth = voice.low1 = voice.low2 = voice.low3 = voice.band = 0;
}
}
}
