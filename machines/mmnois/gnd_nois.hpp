#pragma once
// GND-NOIS: the sound, a module any engine can hold (docs/VOICES.md), fitted to
// the Monomachine (reference/fit_mgnd.py):
//   ST    RED   STON  -     /  -     RES   -     TUNE
// The original's four unnamed slots do nothing measurable (`slots`); RES, in
// one of them, is this machine's own.
// White noise under a top that darkens as the key rises, its level held: the
// corner of two 1-poles falls from 7.2 kHz at C1 to 3.9 kHz at C7, so the
// centroid falls 0.9 octave, as the original's (`top`). RED fades it into red
// noise, loudest at its middle as on the original. The red part (a 1-pole
// low-pass that RED lowers) passes a two-pole high-pass an octave under the
// played note, so the key, and TUNE's +-2 semitones, move it from a rumble at
// C1 to a hiss at C7, as on the original (`key`). ST raises the level up to
// +2.9 dB while STON is off; with STON on ST does nothing heard in mono.
// RES, at 0 the original, moves the high-pass's corner up to the note, raises
// its Q from 0.71 to 24 and turns its output into its band, and feeds the
// white part through it too: the noise narrows to a band, then a whistle, at
// the note, so the key and TUNE play a pitch. Its level falls 2 dB over RES at
// middle C, and the red part's fall across the keyboard flattens as RES rises.
// In stereo (render_stereo), with STON on, ST is the width: a second noise
// through the same chain is added to the left and taken from the right, each
// side holding the mono level.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

namespace md_mmnois {} namespace md = md_mmnois; namespace md_mmnois {
namespace gnd_nois {
// One noise and its filters' states.
struct Noise {
    u32 seed;
    i32 top[2];    // the roll-off above the key's corner: two 1-poles
    i32 red;       // the red noise's 1-pole state
    i32 hp[2];     // the high-pass: a Chamberlin filter's low and band
};
struct State {
    Noise noise;
    Noise side;    // render_stereo's second noise, through the same chain
};
namespace {
enum { ST, RED, STON, RES = 5, TUNE = 7 };
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
// The white top at each C from C0 to C8 (the key, below): 1 - e^(-2 pi fc / 44100),
// Q31, for fc 8481, 7205, 6121, 5683, 5038, 4457, 4039, 3864 and 3716 Hz, fitted to
// the original's centroid at each key (`top`), and the gain holding the level
// (Q30, 0.97 .. 1.54) against the 8 kHz top before.
const i32 top_pole_of[9] = {0x59c46a65, 0x52250847, 0x4a7c85b7, 0x470a6028, 0x418ede20, 0x3c2b5524, 0x3801c439, 0x363076b8, 0x349dcb3d};
const i32 top_gain_of[9] = {0x3de353f8, 0x43f7ced9, 0x4ab020c5, 0x4de353f8, 0x5353f7cf, 0x59374bc7, 0x5e24dd2f, 0x6072b021, 0x627ef9db};
// RES at 0, 16, ..., 112, 127. The corner: pi / 4 x its ratio to the note,
// half at 0 (the original), the note from 16 on.
const i32 corner_of[9] = {0x3243f6a9, 0x6487ed51, 0x6487ed51, 0x6487ed51, 0x6487ed51, 0x6487ed51, 0x6487ed51, 0x6487ed51, 0x6487ed51};
// Half the damping, 1 / 2Q (Q31): Butterworth at 0, then Q 1.1, 1.7, 2.7, 4.1,
// 6.4, 9.9, 15 and 24, rising evenly in octaves. A filter without feedback,
// so it rings but never oscillates on its own.
const i32 damping_of[9] = {0x5a82799a, 0x3a420029, 0x257f9af6, 0x1822eb9c, 0x0f89260a, 0x09fff287, 0x066fbef8, 0x04249842, 0x02aaaaab};
// The Chamberlin filter's peak sits sharp by about 0.26 F d of its frequency;
// taking 1.04 hp^2 damping off its coefficient keeps it within 2 cents of the
// note from C1 to C8 at every Q. Off at RES 0, where the original has no peak.
const i32 fix_of[9] = {0x00000000, 0x428f5c29, 0x428f5c29, 0x428f5c29, 0x428f5c29, 0x428f5c29, 0x428f5c29, 0x428f5c29, 0x428f5c29};
// The white part's share fed through the filter, all of it from 16 on (Q31);
// the filter's output turning from its high-pass into its band (Q31).
const i32 share_of[9] = {0x00000000, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff};
const i32 band_of[9] = {0x00000000, 0x2ccccccd, 0x4ccccccd, 0x63d70a3d, 0x73333333, 0x7ae147ae, 0x7fffffff, 0x7fffffff, 0x7fffffff};
// The white share's and red part's gains into the filter (Q28, 1 at RES 0), set
// on this voice so the level at middle C falls 0, 0.5, 1, 1.5 then 2 dB over
// RES, for white noise (RED 0) and red (RED 127) alike.
const i32 white_res_of[9] = {0x10000000, 0x163a92a3, 0x1cc91d15, 0x1ca5119d, 0x15ad42c4, 0x10a9930c, 0x0cf4f0d8, 0x0a8d4fdf, 0x08b1c433};
const i32 red_res_of[9] = {0x10000000, 0x173c3611, 0x11c50481, 0x0be1b08a, 0x08055326, 0x0612d773, 0x04b78034, 0x03d7dbf5, 0x032b020c};
// The red part's pole in octaves from middle C at each RED step (Q16): below
// it the red part is flat, as white noise is, above it falls 6 dB an octave.
const i32 red_octaves_of[9] = {33786, 33786, 7090, 7090, 7090, -10956, -46493, -136129, -270301};
// log2(1 + 2^u), Q16, for u = -8, -6, ..., 8 (Q16): the red part's level at a
// note against its pole, in octaves.
const i32 softplus_of[9] = {369, 1466, 5732, 21098, 65536, 152170, 267876, 394682, 524657};
// How their gains follow the key, as half the power of 2 per octave (Q31): the
// white part's band grows 3 dB an octave and is held against it from middle C;
// the red part's follows its density at the note (flat below RED's pole, 6 dB
// an octave down above it), from the original's fall at RES 0 to none at 112.
// Set on this voice from C2 to C6.
const i32 white_follow_of[9] = {0x00000000, 0x0458793e, 0x1718fc50, 0x327d566d, 0x420aa64c, 0x44985f07, 0x44bac711, 0x4535a858, 0x4695e9e2};
const i32 red_follow_of[9] = {0x00000000, 0x0ae7d567, 0x104d013b, 0x1b958106, 0x27a5e354, 0x339096bc, 0x3d04816f, 0x43c36113, 0x42ae7d56};
const i32 out_level = 0x10000000;        // Q28, then x 4 (reference/fit_mgnd.py)
// In stereo, the level at ST 0, 16, ..., 127 with STON on: 1 / sqrt(1 + ST^2), Q31.
const i32 even_of[9] = {0x7fffffff, 0x7f02f623, 0x7c2da123, 0x77d9a26e, 0x727c9717, 0x6c8b355b, 0x66666666, 0x60546ee2, 0x5a82799a};
const i32 tune_centre = 1082196480;      // 64/127 in Q31; TUNE +-2 semitones, as GND-SIN's
const i32 hp_most = 0x30000000;          // the high-pass's half coefficient, capped near 5.4 kHz
const i32 middle_c_log2 = 1612525;       // log2_q16(md::kit::middle_c), as computed below

// log2(1 + 2^u), u in Q16 octaves, Q16.
inline i32 softplus(i32 u) {
    if (u >= 8 * md::octave) return u;
    if (u <= -8 * md::octave) return 0;
    return md::table9(softplus_of, static_cast<i32>(static_cast<u32>(u + 8 * md::octave) << 11));
}

// log2 of a phase increment, Q16, within 0.01 of an octave: the leading bit,
// then the mantissa m's log as m + 0.3466 m (1 - m).
inline i32 log2_q16(u32 increment) {
    if (!increment) return 0;
    u32 top = 31;
    while (!(increment >> top)) --top;
    const u32 m = (top >= 16 ? increment >> (top - 16) : increment << (16 - top)) & 0xffffu;
    const u32 bend = (m * (65536u - m)) >> 16;
    return static_cast<i32>((top << 16) + m + ((bend * 22714u) >> 16));
}
}

inline void trigger(State&, Common&) {}
inline void reset(State& voice, Common&, u32 owner) {
    voice.noise = {0x2545f491u * (owner + 1), {0, 0}, 0, {0, 0}};
    voice.side = {0x9e3779b9u * (owner + 1), {0, 0}, 0, {0, 0}};
}

namespace {
// The block's samples, with the state on locals. A nonzero seed never returns to 0,
// so md::noise's check is needed only before the first draw.
__attribute__((noinline)) inline void samples(opxy::Output output, Noise& voice, i32 top_pole, i32 dry, i32 wet, i32 red,
                                              i32 red_pole, i32 level, i32 hp, i32 damping, i32 high_gain, i32 band_gain) {
    u32 seed = voice.seed ? voice.seed : 0x9e3779b9u;
    i32 top0 = voice.top[0], top1 = voice.top[1], low = voice.red;
    i32 hp_low = voice.hp[0], hp_band = voice.hp[1];
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        const i32 n = static_cast<i32>(seed >> 3) - 0x10000000;   // md::noise_eighth
        top0 += md::multiply(n - top0, top_pole);
        top1 += md::multiply(top0 - top1, top_pole);
        low += md::multiply(top1 - low, red_pole);                // within an eighth of full scale
        // The filter (md::Svf, its states on locals) takes a quarter of the red
        // part's level and RES's share of the white part's.
        const i32 in = md::multiply(low, red) * 64 + md::multiply(top1, wet);
        hp_low += md::multiply(hp, hp_band) * 2;
        const i32 high = in - hp_low - md::multiply(damping, hp_band) * 2;
        hp_band += md::multiply(hp, high) * 2;
        // The white part around the filter, and its high-pass turning into its band, back x 4.
        const i32 mixed = md::multiply(top1, dry) * 4 + md::multiply(high, high_gain) * 8 + md::multiply(hp_band, band_gain) * 8;
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

// The block's coefficients, for the mono noise and the stereo side alike.
struct Coefficients {
    i32 top_pole, dry, wet, red, red_pole, level, hp, damping, high_gain, band_gain;
};
inline Coefficients coefficients(const i32* p, opxy::Pitch pitch) {
    const u32 note = md::transpose(pitch[0], md::multiply(p[TUNE] - tune_centre, 21675));
    // The tuned note's octaves from middle C (Q16), and as the key, C0 .. C8 over 0 .. 1 (Q31).
    const i32 octaves = log2_q16(note) - middle_c_log2;
    const i32 key = static_cast<i32>(static_cast<u32>(md::clamp(octaves + 4 * md::octave, 0, 8 * md::octave - 1)) << 12);
    // The filter's coefficient, sin(pi fc / fs) ~ x - x^3 / 6 for x = pi fc / fs (clamped
    // before it is scaled), less the fix for its peak.
    const i32 damping = md::table9(damping_of, p[RES]);
    i32 hp = md::clamp(md::multiply(static_cast<i32>(note >> 1), md::table9(corner_of, p[RES])), 0, hp_most / 4) * 4;
    hp -= md::multiply(md::multiply(md::multiply(hp, hp), hp), 0x15555555);
    hp -= md::multiply(md::multiply(md::multiply(hp, hp), damping), md::table9(fix_of, p[RES])) * 2;
    // The white part: around the filter at RES 0, through it from 16.
    const i32 white = md::multiply(md::table9(white_of, p[RED]), md::table9(top_gain_of, key)) * 2;
    const i32 share = md::multiply(white, md::table9(share_of, p[RES]));
    const u32 white_follow = md::transpose(1u << 26, -md::multiply(md::table9(white_follow_of, p[RES]), octaves));   // Q26
    // The red part's band falls with its density at the note: hold it against
    // middle C's, where it flattens below the pole and falls above it.
    const i32 pole = md::table9(red_octaves_of, p[RED]);
    const i32 tilt = softplus(2 * (octaves - pole)) - softplus(-2 * pole) - octaves;
    const u32 red_follow = md::transpose(1u << 26, md::multiply(md::table9(red_follow_of, p[RES]), tilt));
    const i32 wet = md::gain(md::multiply(md::multiply(share, md::table9(white_res_of, p[RES])), static_cast<i32>(white_follow)), 8);
    const i32 red = md::gain(md::multiply(md::multiply(md::table9(red_of, p[RED]), md::table9(red_res_of, p[RES])), static_cast<i32>(red_follow)), 8);
    // x 8 in the loop: a half is x 4.
    const i32 band = md::multiply(0x40000000, md::table9(band_of, p[RES]));
    const i32 st = p[STON] >= 0x40000000 ? 0x40000000 : md::table9(st_gain, p[ST]);
    const i32 level = md::multiply(out_level, st) * 2;                 // Q28
    return {md::table9(top_pole_of, key), white - share, wet, red, md::table9(red_pole_of, p[RED]),
            level, hp, damping, 0x40000000 - band, band};
}

inline void run(opxy::Output output, Noise& chain, const Coefficients& c) {
    samples(output, chain, c.top_pole, c.dry, c.wet, c.red, c.red_pole, c.level, c.hp, c.damping, c.high_gain, c.band_gain);
}

inline void render(State& voice, Common&, opxy::Output output, const i32* p, opxy::Pitch pitch) {
    run(output, voice.noise, coefficients(p, pitch));
}

// 64 interleaved frames. STON off, or ST at 0, is the mono noise on both sides.
// With STON on, ST is the width: a second noise through the same chain, added to
// the left and taken from the right, two independent noises at full width. The
// level comes down by as much as the side adds, 1 / sqrt(1 + ST^2), so each side
// holds the mono noise's level and the middle dips, by 3 dB at full width.
inline void render_stereo(State& voice, Common&, opxy::Output output, const i32* p, opxy::Pitch pitch) {
    const Coefficients c = coefficients(p, pitch);
    i32 mono[opxy::block_frames];
    run(mono, voice.noise, c);
    const i32 width = p[STON] >= 0x40000000 ? md::positive(p[ST]) : 0;
    if (!width) {
        // The side rests until it opens, so it starts clean.
        voice.side.top[0] = voice.side.top[1] = voice.side.red = 0;
        voice.side.hp[0] = voice.side.hp[1] = 0;
        for (unsigned i = 0; i != opxy::block_frames; ++i) output[2 * i] = output[2 * i + 1] = mono[i];
        return;
    }
    i32 side[opxy::block_frames];
    run(side, voice.side, c);
    const i32 level = md::table9(even_of, width);
    const i32 wide = md::multiply(width, level);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 m = md::multiply(mono[i], level), s = md::multiply(side[i], wide);
        output[2 * i] = md::finish(m + s);
        output[2 * i + 1] = md::finish(m - s);
    }
}
}
}
