#pragma once
// GND-NS: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

// White noise in the manner of the Machinedrum's GND-NS, written from scratch
// in fixed point and fitted to it (reference/fit_gnd.py). The control:
//   DEC
// DEC is the level's decay, as GND-SN's: silent at 0, 214 ms at 64, held from 112.
namespace md_mdefm {} namespace md = md_mdefm; namespace md_mdefm {
namespace gnd_ns {
struct State {
    i32 level;      // amplitude envelope, Q31
    u32 seed;
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { DEC };

// DEC as -60 dB times, Q16 log2 samples at the controls 0, 16, ..., 127 (GND-SN's, but silent at 0).
const i32 dec_times[9] = {262144, 527565, 625869, 745144, 865075, 945684, 1148846, 1311375, 1311375};
// The level: the Machinedrum's -19 dB RMS, 2 dB up as the TRX machines are.
const i32 level = 0x1e523b24;
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.level = 0;
    voice.seed = 0x9e3779b9u * owner;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch) {
    const i32 decay_step = md::decay(md::table9(dec_times, parameters[DEC]));
    if (voice.fresh) {
        voice.level = parameters[DEC] > 0 ? md::one : 0;
        voice.fresh = 0;
    }
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        output[i] = md::finish(multiply(md::noise(voice.seed), multiply(voice.level, level)));
        voice.level = multiply(voice.level, decay_step);
    }
}
}
}
