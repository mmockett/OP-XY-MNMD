#pragma once
// TRX-XT and TRX-XC: the sound, modules any engine can hold (docs/VOICES.md).
// The tom and the conga play the same model with their own constants, fitted
// to the Machinedrum's own (reference/fit_tom.py). Their page:
//   PTCH  DEC   RAMP  RDEC
//   DAMP  DIST  DTYP
// As measured: a short downward click (52 samples on the tom, 29 on the conga),
// then one sine at 72.5 Hz x 2^(1.99 PTCH) (the conga 86.5 Hz, three semitones
// up), plus RAMP's extra Hz (up to 356 or 366 Hz, whatever PTCH is) falling
// exponentially at RDEC's rate, the whole decaying exponentially at DEC's.
// DAMP lowers the body linearly to half and deepens the click to twice. DIST
// clips the sine, before its envelope, at a threshold that falls to a third
// (the conga's to 0.42) into a 30 Hz high-pass; DTYP raises that high-pass to
// 260 Hz, thinning the fundamental under the clipped edges. The pitch is the Machinedrum's at middle
// C; other keys move all of it.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace trx_tom {
namespace {
using md::multiply;

enum { PTCH, DEC, RAMP, RDEC, DAMP, DIST, DTYP };

}
struct Model {
    i32 base_low, base_span;    // the base pitch, Q16 octaves from middle C
    i32 decay[9];               // DEC: -60 dB times, Q16 log2 samples, at 0, 16, ..., 127
    i32 sweep[9];               // RDEC: the sweep's -60 dB time, likewise
    i32 ramp[9];                // RAMP: the extra Hz at its start, Q28 of middle C
    i32 clip[9];                // DIST: the clip's threshold, Q31 of the sine's peak
    u32 click_length;           // samples
    u32 click_increment;        // half a cycle over the click's length
    i32 click;                  // the click's depth at DAMP 0, Q31 of the body's level
};
const Model tom = {
    -121376, 130286,
    {758939, 801504, 862490, 922198, 982847, 1042560, 1103779, 1162517, 1219987},
    {843548, 855788, 869879, 886143, 906552, 932009, 966603, 1019394, 1146463},
    {0, 43401027, 87622878, 131742125, 175348358, 219775414, 263792059, 308424321, 365061123},
    {0x7fffffff, 2064888123, 1988410785, 1731841652, 1533916891, 1301505241, 1130254552, 879418667, 727960559},
    52, 41297762, 0x2b851eb8,
};
const Model conga = {
    -104632, 129696,
    {762508, 785888, 829744, 872773, 917283, 961426, 1005175, 1049485, 1090971},
    {836777, 848781, 862939, 879812, 898951, 925776, 959773, 1016723, 1145993},
    {0, 46684320, 94907684, 141386798, 188276324, 233626806, 281439758, 330586548, 375321413},
    {0x7fffffff, 2126221434, 2105376125, 1979247602, 1867377085, 1561806289, 1342177280, 1066858864, 894784853},
    29, 74051160, 0x3916872b,
};
// DTYP: DIST's high-pass from 30 Hz to 260 Hz, one-pole coefficients at 0, 16, ..., 127.
const i32 highpass[9] = {9159348, 12203775, 15243867, 19795892, 24338198, 37907018, 51389120, 65215758, 78095387};
// The body peaks at 0.28 of full scale, as the Machinedrum's (-11 dB); half of it here.
const i32 body_level = 0x11db4000;

struct Block {
    i32 ramp, ramp_block, decay_step, body, click, threshold, highpass, mix, trim;
    u32 base;
    void plan(const i32* p, u32 middle, const Model& m) {
        base = md::transpose(middle, md::span(p[PTCH], m.base_low, m.base_low + m.base_span));
        ramp = md::table9(m.ramp, p[RAMP]);
        ramp_block = md::per_block(md::decay(md::table9(m.sweep, p[RDEC])));
        decay_step = md::decay(md::table9(m.decay, p[DEC]));
        const i32 d = md::positive(p[DAMP]);
        body = multiply(md::one - opxy::q31::half(d), body_level);            // 1 .. 1/2
        click = multiply(m.click, body_level);
        click += multiply(click, d);                                          // 1 .. 2 x
        threshold = opxy::q31::half(md::table9(m.clip, p[DIST]));             // of the sine at half scale
        highpass = md::table9(trx_tom::highpass, p[DTYP]);
        const u32 a = static_cast<u32>(md::positive(p[DIST]));
        mix = a < 0x08000000u ? static_cast<i32>(a * 16) : md::one;           // clean into clipped over the first 1/16
        trim = md::span(p[DIST], md::one, 0x66666666);                        // the clipped edges' overshoot: 1 .. 0.8
    }
};

struct State {
    u32 phase;        // the body's sine
    u32 click_phase;  // the click's half cycle
    u32 age;          // samples of click still to run
    i32 level;        // the body's envelope, Q31
    i32 ramp;         // RAMP's sweep envelope, Q31
    i32 highpass;     // DIST's high-pass state
    u32 fresh;
    const Model* seen_model;
    u32 stale;        // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];      // the controls they were worked out from
    Block block;
};
namespace {

// The stages, a loop each: out of line, so each has a hardware loop and all the registers
// to itself, and on locals, as the buffer might otherwise be the voice.
// The click: one downward half cycle; the body waits for it. Doubling then stopping at half
// scale is stopping at a quarter, then doubling.
__attribute__((noinline)) inline void run_click(State& voice, i32 click, u32 increment, u32 count, opxy::Output output) {
    u32 phase = voice.click_phase;
    for (unsigned i = 0; i != count; ++i) {
        output[i] = md::clamp(-multiply(md::sine(phase), click), -0x20000000, 0x20000000) * 2;
        phase += increment;
    }
    voice.click_phase = phase;
}
// The body. Doubled twice and stopped at half scale is stopped at an eighth, then four times.
__attribute__((noinline)) inline void run_clean(State& voice, const Block& b, u32 increment, i32 slope, u32 count, opxy::Output output) {
    u32 phase = voice.phase;
    i32 level = voice.level;
    const i32 body = b.body, step = b.decay_step;
    for (unsigned i = 0; i != count; ++i) {
        const i32 tone = opxy::q31::half(md::sine(phase));
        output[i] = md::clamp(multiply(tone, multiply(level, body)), -0x10000000, 0x10000000) * 4;
        phase += increment;
        increment += static_cast<u32>(slope);
        level = multiply(level, step);
    }
    voice.phase = phase;
    voice.level = level;
}
// DIST clips the sine itself, before its envelope, as on the Machinedrum:
// a long note stays as distorted as it fades.
__attribute__((noinline)) inline void run_clipped(State& voice, const Block& b, u32 increment, i32 slope, u32 count, opxy::Output output) {
    u32 phase = voice.phase;
    i32 level = voice.level, held = voice.highpass;
    const i32 threshold = b.threshold, highpass = b.highpass, trim = b.trim, mix = b.mix, body = b.body, step = b.decay_step;
    for (unsigned i = 0; i != count; ++i) {
        i32 tone = opxy::q31::half(md::sine(phase));
        const i32 hot = md::clamp(tone, -threshold, threshold);
        held += multiply(hot - held, highpass);
        tone += multiply(multiply(hot - held, trim) - tone, mix);
        output[i] = md::clamp(multiply(tone, multiply(level, body)), -0x10000000, 0x10000000) * 4;
        phase += increment;
        increment += static_cast<u32>(slope);
        level = multiply(level, step);
    }
    voice.phase = phase;
    voice.level = level;
    voice.highpass = held;
}

inline void trigger(State& voice) { voice.fresh = 1; }
inline void reset(State& voice) {
    voice.phase = voice.click_phase = 0;
    voice.age = 0;
    voice.level = voice.ramp = voice.highpass = 0;
    voice.stale = 1;
    trigger(voice);
    voice.fresh = 0;
}

inline void render(State& voice, opxy::Output output, const i32* parameters, opxy::Pitch pitch, const Model& m) {
    // Only when a control or the key has moved.
    u32 moved = voice.stale | (pitch[0] ^ voice.seen_pitch) | static_cast<u32>(voice.seen_model != &m);
    for (unsigned c = 0; c < 8; ++c) moved |= static_cast<u32>(parameters[c] ^ voice.seen[c]);
    if (moved) {
        voice.stale = 0;
        voice.seen_pitch = pitch[0];
        voice.seen_model = &m;
        for (unsigned c = 0; c < 8; ++c) voice.seen[c] = parameters[c];
        voice.block.plan(parameters, pitch[0], m);
    }
    const Block& b = voice.block;
    if (voice.fresh) {
        voice.phase = voice.click_phase = 0;
        voice.age = m.click_length;
        voice.level = voice.ramp = md::one;
        voice.highpass = 0;
        voice.fresh = 0;
    }
    // The pitch in Hz above the base, linear between the block's ends.
    const u32 base = b.base;
    const i32 ramp_next = multiply(voice.ramp, b.ramp_block);
    const u32 now = base + md::scale(pitch[0], multiply(voice.ramp, b.ramp));
    const u32 next = base + md::scale(pitch[0], multiply(ramp_next, b.ramp));
    const i32 slope = (static_cast<i32>(next) - static_cast<i32>(now)) / 64;
    voice.ramp = ramp_next;

    const u32 clicks = voice.age < opxy::block_frames ? voice.age : opxy::block_frames;
    if (clicks) {
        run_click(voice, b.click, m.click_increment, clicks, output);
        voice.age -= clicks;
    }
    const u32 count = opxy::block_frames - clicks;
    if (b.mix) run_clipped(voice, b, now, slope, count, output + clicks);
    else run_clean(voice, b, now, slope, count, output + clicks);
}
}
}

namespace trx_xt {
typedef trx_tom::State State;
inline void reset(State& s, Common&, u32) { trx_tom::reset(s); }
inline void trigger(State& s, Common&) { trx_tom::trigger(s); }
inline void render(State& s, Common&, opxy::Output o, const i32* p, opxy::Pitch pitch) { trx_tom::render(s, o, p, pitch, trx_tom::tom); }
}

namespace trx_xc {
typedef trx_tom::State State;
inline void reset(State& s, Common&, u32) { trx_tom::reset(s); }
inline void trigger(State& s, Common&) { trx_tom::trigger(s); }
inline void render(State& s, Common&, opxy::Output o, const i32* p, opxy::Pitch pitch) { trx_tom::render(s, o, p, pitch, trx_tom::conga); }
}
}
