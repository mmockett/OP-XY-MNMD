#pragma once
// GND-NOIS: the sound, a module any engine can hold (docs/VOICES.md), fitted to
// the Monomachine (reference/fit_mgnd.py):
//   ST    RED   STON  -     /  -     -     -     TUNE
// White noise, dulled above 7 kHz as the original's is. RED fades it into red
// noise (a 1-pole low-pass near 170 Hz), loudest at its middle as on the
// original. ST raises the level up to +2.9 dB while STON is off; with STON on
// ST does nothing heard in mono. TUNE has no effect on the original's noise.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

namespace md_mmnois {} namespace md = md_mmnois; namespace md_mmnois {
namespace gnd_nois {
struct State {
    u32 seed;
    i32 top[2];    // the roll-off above 7 kHz: two 1-pole low-passes
    i32 red;       // the red noise's 1-pole state
};
namespace {
enum { ST, RED, STON, TUNE = 7 };
// The white and red parts' levels at RED 0, 16, ..., 112, 127 (Q29, 0..4),
// fitted to the original's octave bands (reference/fit_mgnd.py): the mix is
// loudest near 64, as the original's.
const i32 white_of[9] = {0x0a7ef9db, 0x09a1cac1, 0x08bc6a7f, 0x07ef9db2, 0x07439581, 0x05581062, 0x0374bc6a, 0x01916873, 0x00000000};
const i32 red_of[9] = {0x00b55ef2, 0x02ba9d1f, 0x04978d50, 0x05e76c8b, 0x0800d1b7, 0x068f5c29, 0x059d883c, 0x046f3f53, 0x032617c2};
// ST: the gain over the noise while STON is off, 1 .. 1.40 (Q30), at ST 0, 16, ..., 127.
const i32 st_gain[9] = {0x40000000, 0x40000000, 0x41cac083, 0x44189375, 0x475c28f6, 0x4a7ef9db, 0x4fced917, 0x548b4396, 0x59581062};
const i32 top_pole = 0x570a3d71;         // 1 - e^(-2 pi 8000 / 44100), Q31, twice
const i32 red_pole = 0x031022c0;         // 1 - e^(-2 pi 170 / 44100), Q31
const i32 out_level = 0x10000000;        // Q28, then x 4 (reference/fit_mgnd.py)
}

inline void trigger(State&, Common&) {}
inline void reset(State& voice, Common&, u32 owner) {
    voice.seed = 0x2545f491u * (owner + 1);
    voice.top[0] = voice.top[1] = 0;
    voice.red = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* p, opxy::Pitch) {
    const i32 white = md::table9(white_of, p[RED]);
    const i32 red = md::table9(red_of, p[RED]);
    const i32 st = p[STON] >= 0x40000000 ? 0x40000000 : md::table9(st_gain, p[ST]);
    const i32 level = md::multiply(out_level, st) * 2;                 // Q28
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 n = md::noise_eighth(voice.seed);
        voice.top[0] += md::multiply(n - voice.top[0], top_pole);
        voice.top[1] += md::multiply(voice.top[0] - voice.top[1], top_pole);
        const i32 w = voice.top[1];                                   // within an eighth of full scale
        voice.red += md::multiply(w - voice.red, red_pole);
        // The red part is about 13 dB quieter than the white before its level: x 8 here.
        const i32 mixed = md::multiply(w, white) * 4 + md::multiply(voice.red, red) * 32;
        output[i] = md::finish(md::gain(md::multiply(mixed, level) * 8, 2));
    }
}
}
}
