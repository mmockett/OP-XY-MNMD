#pragma once
// TRX-CL: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Claves: a Machinedrum TRX-CL-style clave, written from scratch in fixed
// point and fitted to the Machinedrum's own (reference/fit_cl.py). Its page:
//   PTCH  DEC   DUAL  ENH
//   TUNE  CLIC
// As measured: a pure sine at 942 Hz + 2188 Hz x PTCH, rising in 1.5 ms
// and falling 60 dB in 58 ms .. 2.3 s (DEC). ENH strikes harder: up to 4 dB
// louder, with an extra 1.5 to 4 dB at the top of the attack, gone 2 ms later. DUAL strikes twice: a first,
// 8 dB quieter strike, then the main one up to 15 ms later. TUNE adds a
// second tone on PTCH's scale, opposite in phase (22 dB under the first at PTCH
// 0, 13 dB at 64), none at TUNE 0. CLIC has no measurable effect on the
// Machinedrum; here it adds a millisecond's click of noise above 3 kHz. The
// Machinedrum's claves peak 7 dB under its kicks; here they are lifted to
// stand with them. The key moves the tones.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace trx_cl {
namespace {
using md::multiply;

enum { PTCH, DEC, DUAL, ENH, TUNE, CLIC };

const i32 tone_low = 483413569, tone_high = 1605632802;   // 942 .. 3130 Hz, Q28 of middle C, halved
const i32 decay_times[9] = {741913, 786554, 830483, 875001, 919139, 963452, 1007557, 1051515, 1090772};
const i32 dual_delay[9] = {0, 33, 66, 143, 220, 353, 485, 576, 662};   // samples, at DUAL 0, 16, ..., 127
const i32 first_level = 0x1c000000;                      // the first strike, 8 dB under the main (at half)
const i32 enh_level[9] = {1370660400, 1485695697, 1610385552, 1715654626, 1827805019, 1913946816, 2004148350, 2076891154, 0x7fffffff};
const i32 snap_least = 0x18525460, snap_most = 0x48f5c28f;   // ENH's extra at the start: 0.19 .. 0.57 x the strike
const i32 snap_time = 396000;                            // from the top of the attack, gone in 1.5 ms
const i32 rise_step = 0x01f07c1f;                        // the attack: 1.5 ms
const i32 second_low = 0x0a1cac08, second_high = 0x30000000;   // the second tone's level by PTCH, 0.08 .. 0.37
const u32 click_length = 44;                             // 1 ms
const i32 click_highpass = 746927805;                    // 3 kHz
const i32 click_most = 0x28000000;
// The tone peaks near -10 dB of full scale (the Machinedrum's -18, lifted 8 dB); half here.
const i32 tone_level = 0x18000000;

}
struct Block {
    i32 ratio, second_ratio, decay_step, level, snap, second, click, snap_step;
    u32 delay, increment, second_increment;
    void plan(const i32* p, u32 middle) {
        ratio = md::span(p[PTCH], tone_low, tone_high);
        second_ratio = md::span(p[TUNE], tone_low, tone_high);
        decay_step = md::decay(md::table9(decay_times, p[DEC]));
        level = multiply(md::table9(enh_level, p[ENH]), tone_level);
        snap = md::span(p[ENH], snap_least, snap_most);
        // The second tone: none at TUNE 0, in over its first sixteenth.
        const u32 t = static_cast<u32>(md::positive(p[TUNE]));
        const i32 fade = t < 0x08000000u ? static_cast<i32>(t * 16) : md::one;
        second = multiply(md::span(p[PTCH], second_low, second_high), fade);
        click = multiply(md::squared(p[CLIC]), click_most);
        delay = static_cast<u32>(md::table9(dual_delay, p[DUAL]));
        increment = md::scale(middle, ratio) << 1;
        second_increment = md::scale(middle, second_ratio) << 1;
        snap_step = md::decay(snap_time);
    }
};

struct State {
    u32 phase, second;   // the two tones
    i32 level;           // the main strike's envelope, Q31
    i32 first;           // the first strike's (DUAL), Q31
    i32 snap;            // ENH's extra, Q31
    i32 rise;            // the attack, Q31
    u32 wait;            // samples until the main strike (DUAL)
    u32 click;           // samples of CLIC's click still to run
    i32 hiss;            // the click's high-pass state
    u32 seed;
    u32 fresh;
    u32 stale;        // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];      // the controls they were worked out from
    Block block;
};

// Whether a sound is still to come however quiet it is now: DUAL's main strike.
inline bool waiting(const State& voice) { return voice.wait != 0; }
inline void trigger(State& voice, Common&) { voice.fresh = 1; }
// The block's stages, a loop each: out of line, so each has a hardware loop and all the
// registers to itself, and on locals, as the buffers might otherwise be the voice.
// The tone, and the second opposite in phase.
__attribute__((noinline)) inline void run_tones(State& voice, u32 increment, u32 second_increment, i32 amount, i32* x) {
    u32 phase = voice.phase, second = voice.second;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        x[i] = opxy::q31::half(md::sine(phase)) - multiply(opxy::q31::half(md::sine(second)), amount);
        phase += increment;
        second += second_increment;
    }
    voice.phase = phase;
    voice.second = second;
}
// The envelope while a strike is still to come or rising.
__attribute__((noinline)) inline void run_rising(State& voice, const Block& b, i32* x) {
    u32 wait = voice.wait;
    i32 level = voice.level, snap = voice.snap, rise = voice.rise, first = voice.first;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        if (wait && !--wait) {
            // DUAL: the main strike, after the first.
            level = md::one;
            snap = md::one;
            rise = 0;
        }
        rise = rise < md::one - rise_step ? rise + rise_step : md::one;
        // At half: the snap rides over the strike.
        const i32 strike = opxy::q31::half(level) + opxy::q31::half(multiply(snap, b.snap));
        x[i] = multiply(multiply(x[i], multiply(strike + first, rise)), b.level) * 4;
        level = multiply(level, b.decay_step);
        first = multiply(first, b.decay_step);
        if (rise == md::one) snap = multiply(snap, b.snap_step);
    }
    voice.wait = wait; voice.level = level; voice.snap = snap; voice.rise = rise; voice.first = first;
}
// And once it has risen, with nothing left to test.
__attribute__((noinline)) inline void run_risen(State& voice, const Block& b, i32* x) {
    i32 level = voice.level, snap = voice.snap, first = voice.first;
    const i32 snap_amount = b.snap, amount = b.level, step = b.decay_step, snap_step = b.snap_step;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 strike = opxy::q31::half(level) + opxy::q31::half(multiply(snap, snap_amount));
        x[i] = multiply(multiply(x[i], multiply(strike + first, md::one)), amount) * 4;
        level = multiply(level, step);
        first = multiply(first, step);
        snap = multiply(snap, snap_step);
    }
    voice.level = level; voice.snap = snap; voice.first = first;
}
// CLIC's click, its first millisecond.
__attribute__((noinline)) inline void run_click(State& voice, i32 amount, i32* x) {
    const u32 count = voice.click < opxy::block_frames ? voice.click : opxy::block_frames;
    u32 seed = voice.seed;
    i32 hiss = voice.hiss;
    for (unsigned i = 0; i != count; ++i) {
        const i32 white = md::noise_eighth(seed);
        md::smooth(hiss, white, click_highpass);
        x[i] += multiply(white - hiss, amount) * 2;
    }
    voice.click -= count;
    voice.seed = seed;
    voice.hiss = hiss;
}
// Doubling then stopping at half scale is stopping at a quarter, then doubling.
__attribute__((noinline)) inline void run_finish(opxy::Output x) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) x[i] = md::clamp(x[i], -0x20000000, 0x20000000) * 2;
}

inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase = voice.second = 0;
    voice.level = voice.first = voice.snap = voice.rise = 0;
    voice.wait = voice.click = 0;
    voice.hiss = 0;
    voice.seed = 0x7f4a7c15u * owner;
    voice.stale = 1;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    // Only when a control or the key has moved.
    u32 moved = voice.stale | (pitch[0] ^ voice.seen_pitch);
    for (unsigned c = 0; c < 8; ++c) moved |= static_cast<u32>(parameters[c] ^ voice.seen[c]);
    if (moved) {
        voice.stale = 0;
        voice.seen_pitch = pitch[0];
        for (unsigned c = 0; c < 8; ++c) voice.seen[c] = parameters[c];
        voice.block.plan(parameters, pitch[0]);
    }
    const Block& b = voice.block;
    if (voice.fresh) {
        voice.phase = voice.second = 0;
        voice.wait = b.delay;
        voice.level = voice.wait ? 0 : md::one;
        voice.first = voice.wait ? first_level : 0;
        voice.snap = md::one;
        voice.rise = 0;
        voice.click = click_length;
        voice.hiss = 0;
        voice.fresh = 0;
    }
    run_tones(voice, b.increment, b.second_increment, b.second, output);
    if (voice.wait || voice.rise != md::one) run_rising(voice, b, output);
    else run_risen(voice, b, output);
    if (voice.click) run_click(voice, b.click, output);
    run_finish(output);
}
}
}
