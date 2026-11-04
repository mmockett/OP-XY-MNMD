#pragma once
// GND-NOIS: the sound, a module any engine can hold (docs/VOICES.md), fitted to
// the Monomachine (reference/fit_mgnd.py):
//   ST    RED   STON  -     /  -     -     -     TUNE
// White noise, dulled above 7 kHz as the original's is. RED fades it into red
// noise, loudest at its middle as on the original. The red part (a 1-pole
// low-pass that RED lowers) passes a two-pole high-pass an octave under the
// played note, so the key, and TUNE's +-2 semitones, move it from a rumble at
// C1 to a hiss at C7, as on the original (reference/fit_mgnd.py, `key`); the
// white part ignores both. ST raises the level up to +2.9 dB while STON is
// off; with STON on ST does nothing heard in mono.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

namespace md_mmnois {} namespace md = md_mmnois; namespace md_mmnois {
namespace gnd_nois {
struct State {
    u32 seed;
    i32 top[2];    // the roll-off above 7 kHz: two 1-pole low-passes
    i32 red;       // the red noise's 1-pole state
    i32 hp[2];     // the red part's high-pass: a Chamberlin filter's low and band
};
namespace {
enum { ST, RED, STON, TUNE = 7 };
// The white and red parts' levels at RED 0, 16, ..., 112, 127 (Q29, 0..4),
// fitted to the original's octave bands (reference/fit_mgnd.py): the mix is
// loudest near 64, as the original's. The red part's 1-pole sits near 280 Hz
// to RED 64, then falls to 15 Hz at 127: the original's slope steepens from
// about 3 dB an octave at RED 64 to 6 at 127. Fitted to its bands from 31 Hz
// to 6 kHz at every C from C1 to C8, then the levels set to its at middle C.
const i32 white_of[9] = {0x0a7ef9db, 0x09a1cac1, 0x08bc6a7f, 0x07ef9db2, 0x07439581, 0x05581062, 0x0374bc6a, 0x01916873, 0x005208df};
const i32 red_of[9] = {0x00000000, 0x003b1313, 0x007c8f03, 0x00b31112, 0x00ed9f5d, 0x00e02bf3, 0x00ed5471, 0x017f65a7, 0x04f38178};
// 1 - e^(-2 pi fc / 44100), Q31, for fc 374 (RED 0 and 16), 282 (to 64), 233, 160, 62 and 15 Hz.
const i32 red_pole_of[9] = {0x06a3f0a8, 0x06a3f0a8, 0x0508599b, 0x0508599b, 0x0508599b, 0x042df6af, 0x02e0bdf0, 0x01201996, 0x0045f478};
// ST: the gain over the noise while STON is off, 1 .. 1.40 (Q30), at ST 0, 16, ..., 127.
const i32 st_gain[9] = {0x40000000, 0x40000000, 0x41cac083, 0x44189375, 0x475c28f6, 0x4a7ef9db, 0x4fced917, 0x548b4396, 0x59581062};
const i32 top_pole = 0x570a3d71;         // 1 - e^(-2 pi 8000 / 44100), Q31, twice
const i32 out_level = 0x10000000;        // Q28, then x 4 (reference/fit_mgnd.py)
const i32 tune_centre = 1082196480;      // 64/127 in Q31; TUNE +-2 semitones, as GND-SIN's
const i32 hp_damping = 0x5a82799a;       // half of sqrt 2: Butterworth
const i32 hp_most = 0x30000000;          // the high-pass's half coefficient, capped near 5.4 kHz
}

inline void trigger(State&, Common&) {}
inline void reset(State& voice, Common&, u32 owner) {
    voice.seed = 0x2545f491u * (owner + 1);
    voice.top[0] = voice.top[1] = 0;
    voice.red = 0;
    voice.hp[0] = voice.hp[1] = 0;
}

namespace {
// The block's samples, with the state on locals. A nonzero seed never returns to 0,
// so md::noise's check is needed only before the first draw.
__attribute__((noinline)) inline void samples(opxy::Output output, State& voice, i32 white, i32 red, i32 red_pole, i32 level, i32 hp) {
    u32 seed = voice.seed ? voice.seed : 0x9e3779b9u;
    i32 top0 = voice.top[0], top1 = voice.top[1], low = voice.red;
    i32 hp_low = voice.hp[0], hp_band = voice.hp[1];
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        const i32 n = static_cast<i32>(seed >> 3) - 0x10000000;   // md::noise_eighth
        top0 += md::multiply(n - top0, top_pole);
        top1 += md::multiply(top0 - top1, top_pole);
        low += md::multiply(top1 - low, red_pole);                // within an eighth of full scale
        // The high-pass (md::Svf's high output, its states on locals).
        hp_low += md::multiply(hp, hp_band) * 2;
        const i32 high = low - hp_low - md::multiply(hp_damping, hp_band) * 2;
        hp_band += md::multiply(hp, high) * 2;
        // The red part is far quieter than the white before its level: x 64 here.
        const i32 mixed = md::multiply(top1, white) * 4 + md::multiply(high, red) * 256;
        // md::finish(md::gain(md::multiply(mixed, level) * 8, 2)): the same clamp in one step.
        i32 v = static_cast<i32>(static_cast<u32>(md::multiply(mixed, level)) << 3);
        v = v > 0x10000000 ? 0x10000000 : v;
        v = v < -0x10000000 ? -0x10000000 : v;
        output[i] = static_cast<i32>(static_cast<u32>(v) << 2);
    }
    voice.seed = seed;
    voice.top[0] = top0; voice.top[1] = top1;
    voice.red = low;
    voice.hp[0] = hp_low; voice.hp[1] = hp_band;
}
}

inline void render(State& voice, Common&, opxy::Output output, const i32* p, opxy::Pitch pitch) {
    // The high-pass sits at half the note: sin(pi fc / fs) ~ pi increment / 2^33, Q31.
    const u32 note = md::transpose(pitch[0], md::multiply(p[TUNE] - tune_centre, 21675));
    const i32 hp = md::clamp(md::multiply(static_cast<i32>(note >> 1), 0x6487ed51) * 2, 0, hp_most);
    const i32 white = md::table9(white_of, p[RED]);
    const i32 red = md::table9(red_of, p[RED]);
    const i32 red_pole = md::table9(red_pole_of, p[RED]);
    const i32 st = p[STON] >= 0x40000000 ? 0x40000000 : md::table9(st_gain, p[ST]);
    const i32 level = md::multiply(out_level, st) * 2;                 // Q28
    samples(output, voice, white, red, red_pole, level, hp);
}
}
}
