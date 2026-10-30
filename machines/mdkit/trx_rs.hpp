#pragma once
// TRX-RS: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Rim: a Machinedrum TRX-RS-style rim shot, written from scratch in fixed
// point and fitted to the Machinedrum's own (reference/fit_rs.py). Its page:
//   PTCH  DEC   DIST
// As measured: two parts. The rim is a burst of a 477 Hz sawtooth, high-passed
// at 1.3 kHz and low-passed at 4 kHz, with a line near 2.46 kHz, held 6 ms and
// gone 25 ms later. Its phase runs free, so no two hits are alike in detail:
// here it starts at a random phase. Under it the tone, a sine at 172 Hz +
// 171 Hz x PTCH whose level follows its pitch, rings for DEC's time (-60 dB in
// 92 ms .. 3.8 s). DIST adds alternating spikes at the tone's zero crossings,
// odd harmonics about level to the 7th (16 dB under the tone at full DIST),
// keeping the tone's own level. The Machinedrum's rim shot peaks 10 dB under
// its kicks; here it is lifted 12.5 dB to stand with them, as the kit's clap
// and hats are. The key moves all of it.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace trx_rs {
struct State {
    u32 phase;         // the tone's sine
    u32 saw;           // the rim's sawtooth
    u32 line;          // the rim's 2.46 kHz line
    i32 tone;          // the tone's level, Q31
    i32 rim;           // the rim's level, Q31
    u32 hold;          // samples of the rim's hold still to run
    i32 high, low1, low2;   // the rim's filters
    i32 edge;          // DIST's high-pass state
    u32 seed;
    u32 fresh;
};
namespace {
using md::multiply;

enum { PTCH, DEC, DIST };

const i32 tone_low = 176784799, tone_high = 352133157;     // 172.3 .. 343.2 Hz, Q28 of middle C
const i32 decay_times[9] = {785532, 829842, 874601, 918637, 962981, 1007360, 1052191, 1096576, 1136620};
const i32 saw_octaves = 56786, line_octaves = 211846;      // 477 Hz and 2459 Hz, Q16 octaves from middle C
const i32 rim_highpass = 363090925, rim_lowpass = 932909729;   // 1.3 kHz one pole, 4 kHz two poles
const u32 rim_hold = 265;                                  // 6 ms
const i32 rim_fall = 662344;                               // then -60 dB in 25 ms
const i32 edge_highpass = 747500000;                       // DIST's spikes: the tone's square above 3 kHz
// Levels, lifted 12.5 dB over the Machinedrum's: the tone at 258 Hz starts at
// -18 dB of full scale, rising and falling with its pitch, and the rim's
// burst peaks near -10 dB.
const i32 tone_level = 0x10200000;   // at 258 Hz, half of it; times f / 258 Hz
const i32 saw_level = 0x21500000;
const i32 line_level = 0x00a70000;
const i32 spike_most = 0x73333333;   // spikes 3.6 x the tone's level, as DIST squared

struct Block {
    i32 ratio, amplitude, decay_step, spikes;
    explicit Block(const i32* p) {
        ratio = md::span(p[PTCH], tone_low, tone_high);
        // The level follows the pitch: f / 258 Hz, from 0.67 to 1.33, at half.
        amplitude = multiply(md::span(p[PTCH], 0x2aaaaaab, 0x55555555), tone_level) * 4;   // of a quarter-scale sine
        decay_step = md::decay(md::table9(decay_times, p[DEC]));
        spikes = multiply(md::squared(p[DIST]), spike_most);
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase = voice.saw = voice.line = 0;
    voice.tone = voice.rim = 0;
    voice.hold = 0;
    voice.high = voice.low1 = voice.low2 = voice.edge = 0;
    voice.seed = 0x51ed2701u * owner;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.phase = 0;
        voice.saw = static_cast<u32>(md::noise(voice.seed));
        voice.line = static_cast<u32>(md::noise(voice.seed));
        voice.tone = voice.rim = md::one;
        voice.hold = rim_hold;
        voice.high = voice.low1 = voice.low2 = voice.edge = 0;
        voice.fresh = 0;
    }
    const u32 increment = md::scale(pitch[0], b.ratio);
    const u32 saw_increment = md::transpose(pitch[0], saw_octaves);
    const u32 line_increment = md::transpose(pitch[0], line_octaves);
    const i32 fall = md::decay(rim_fall);
    const bool ringing = voice.rim > 0x00080000;   // the rim, until it is 72 dB down

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        // The tone, and DIST's spikes: its square, high-passed, at each zero crossing.
        const i32 sine = md::sine(voice.phase);
        voice.phase += increment;
        // The spikes peak where the sine crosses zero and are gone 0.2 ms later, so
        // the sum stays in range though they stand 3.6 x over the sine.
        const i32 square = sine < 0 ? -0x08000000 : 0x08000000;
        md::smooth(voice.edge, square, edge_highpass);
        const i32 tone = opxy::q31::half(opxy::q31::half(sine)) + multiply(square - voice.edge, b.spikes) * 8;
        i32 mixed = multiply(tone, multiply(voice.tone, b.amplitude));
        voice.tone = multiply(voice.tone, b.decay_step);

        if (ringing) {
            // The rim: the sawtooth's band and its line.
            const i32 saw = static_cast<i32>(voice.saw >> 3) - 0x10000000;   // an eighth of full scale
            voice.saw += saw_increment;
            md::smooth(voice.high, saw, rim_highpass);
            md::smooth(voice.low1, saw - voice.high, rim_lowpass);
            md::smooth(voice.low2, voice.low1, rim_lowpass);
            const i32 band = multiply(voice.low2, saw_level) * 8 + multiply(md::sine(voice.line), line_level);
            voice.line += line_increment;
            mixed += multiply(band, voice.rim);
            if (voice.hold) --voice.hold;
            else voice.rim = multiply(voice.rim, fall);
        }
        output[i] = md::finish(md::gain(mixed, 1));
    }
}
}
}
