#pragma once
// GND-SIN: the sound, a module any engine can hold (docs/VOICES.md), fitted to
// the Monomachine (reference/fit_mgnd.py). One slot, TUNE, at the page's end:
//   -     -     -     -     /  -     -     -     TUNE
// A pure sine at the played key; TUNE moves it +-2 semitones, as on SuperWave.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

namespace md {
namespace gnd_sin {
struct State {
    u32 phase;
    u32 fresh;
};
namespace {
enum { TUNE = 7 };
const i32 tune_centre = 1082196480;   // 64/127 in Q31
// The Monomachine's sine peaks at 0.183 of its output (reference/fit_mgnd.py);
// this engine's is 0.153 of full scale, 7.6 dB under the fitted 0.366, so it
// plays at the loudness of the other synths (the level pass of 2026-11-04).
const i32 level = 0x13878d48;
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32) {
    voice.phase = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* p, opxy::Pitch pitch) {
    if (voice.fresh) {
        voice.phase = 0;   // starts at zero, as the Monomachine's does: no click
        voice.fresh = 0;
    }
    const u32 increment = md::transpose(pitch[0], md::multiply(p[TUNE] - tune_centre, 21675));
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        output[i] = md::multiply(md::sine(voice.phase), level);
        voice.phase += increment;
    }
}
}
}
