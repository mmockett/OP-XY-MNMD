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
namespace md {
namespace trx_cl {
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
};
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

struct Block {
    i32 ratio, second_ratio, decay_step, level, snap, second, click;
    u32 delay;
    explicit Block(const i32* p) {
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
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase = voice.second = 0;
    voice.level = voice.first = voice.snap = voice.rise = 0;
    voice.wait = voice.click = 0;
    voice.hiss = 0;
    voice.seed = 0x7f4a7c15u * owner;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
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
    const u32 increment = md::scale(pitch[0], b.ratio) << 1;
    const u32 second_increment = md::scale(pitch[0], b.second_ratio) << 1;
    const i32 snap_step = md::decay(snap_time);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        if (voice.wait && !--voice.wait) {
            // DUAL: the main strike, after the first.
            voice.level = md::one;
            voice.snap = md::one;
            voice.rise = 0;
        }
        voice.rise = voice.rise < md::one - rise_step ? voice.rise + rise_step : md::one;
        // At half: the snap rides over the strike.
        const i32 strike = opxy::q31::half(voice.level) + opxy::q31::half(multiply(voice.snap, b.snap));
        const i32 envelope = multiply(strike + voice.first, voice.rise);
        // The tone, and the second opposite in phase.
        const i32 tone = opxy::q31::half(md::sine(voice.phase)) - multiply(opxy::q31::half(md::sine(voice.second)), b.second);
        voice.phase += increment;
        voice.second += second_increment;
        i32 mixed = multiply(multiply(tone, envelope), b.level) * 4;
        if (voice.click) {
            const i32 white = md::noise_eighth(voice.seed);
            md::smooth(voice.hiss, white, click_highpass);
            mixed += multiply(white - voice.hiss, b.click) * 2;
            --voice.click;
        }
        output[i] = md::finish(md::gain(mixed, 1));
        voice.level = multiply(voice.level, b.decay_step);
        voice.first = multiply(voice.first, b.decay_step);
        if (voice.rise == md::one) voice.snap = multiply(voice.snap, snap_step);
    }
}
}
}
