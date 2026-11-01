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

}
struct Block {
    i32 amplitude, decay_step, spikes, fall;
    u32 increment, saw_increment, line_increment;
    void plan(const i32* p, u32 middle) {
        increment = md::scale(middle, md::span(p[PTCH], tone_low, tone_high));
        saw_increment = md::transpose(middle, saw_octaves);
        line_increment = md::transpose(middle, line_octaves);
        fall = md::decay(rim_fall);
        // The level follows the pitch: f / 258 Hz, from 0.67 to 1.33, at half.
        amplitude = multiply(md::span(p[PTCH], 0x2aaaaaab, 0x55555555), tone_level) * 4;   // of a quarter-scale sine
        decay_step = md::decay(md::table9(decay_times, p[DEC]));
        spikes = multiply(md::squared(p[DIST]), spike_most);
    }
};

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
    u32 stale;        // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];      // the controls they were worked out from
    Block block;
};

// The block's stages, a loop each: out of line, so each has a hardware loop and all the
// registers to itself, and on locals, as the buffer might otherwise be the voice.
// The tone, and DIST's spikes: its square, high-passed, at each zero crossing.
__attribute__((noinline)) inline void run_tone(State& voice, const Block& b, i32* x) {
    u32 phase = voice.phase;
    i32 edge = voice.edge, level = voice.tone;
    const u32 increment = b.increment;
    const i32 spikes = b.spikes, amplitude = b.amplitude, step = b.decay_step;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 sine = md::sine(phase);
        phase += increment;
        // The spikes peak where the sine crosses zero and are gone 0.2 ms later, so
        // the sum stays in range though they stand 3.6 x over the sine.
        const i32 square = sine < 0 ? -0x08000000 : 0x08000000;
        md::smooth(edge, square, edge_highpass);
        const i32 tone = opxy::q31::half(opxy::q31::half(sine)) + multiply(square - edge, spikes) * 8;
        x[i] = multiply(tone, multiply(level, amplitude));
        level = multiply(level, step);
    }
    voice.phase = phase;
    voice.edge = edge;
    voice.tone = level;
}
// The rim: the sawtooth's band...
__attribute__((noinline)) inline void run_saw(State& voice, u32 increment, i32* band) {
    u32 saw = voice.saw;
    i32 high = voice.high, low1 = voice.low1, low2 = voice.low2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 tooth = static_cast<i32>(saw >> 3) - 0x10000000;   // an eighth of full scale
        saw += increment;
        md::smooth(high, tooth, rim_highpass);
        md::smooth(low1, tooth - high, rim_lowpass);
        md::smooth(low2, low1, rim_lowpass);
        band[i] = multiply(low2, saw_level) * 8;
    }
    voice.saw = saw; voice.high = high; voice.low1 = low1; voice.low2 = low2;
}
// ...and its line.
__attribute__((noinline)) inline void run_rim(State& voice, u32 increment, i32 fall, const i32* band, i32* x) {
    u32 line = voice.line, hold = voice.hold;
    i32 rim = voice.rim;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        x[i] += multiply(band[i] + multiply(md::sine(line), line_level), rim);
        line += increment;
        if (hold) --hold;
        else rim = multiply(rim, fall);
    }
    voice.line = line; voice.hold = hold; voice.rim = rim;
}
// Doubling then stopping at half scale is stopping at a quarter, then doubling.
__attribute__((noinline)) inline void run_finish(opxy::Output x) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) x[i] = md::clamp(x[i], -0x20000000, 0x20000000) * 2;
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase = voice.saw = voice.line = 0;
    voice.tone = voice.rim = 0;
    voice.hold = 0;
    voice.high = voice.low1 = voice.low2 = voice.edge = 0;
    voice.seed = 0x51ed2701u * owner;
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
        voice.phase = 0;
        voice.saw = static_cast<u32>(md::noise(voice.seed));
        voice.line = static_cast<u32>(md::noise(voice.seed));
        voice.tone = voice.rim = md::one;
        voice.hold = rim_hold;
        voice.high = voice.low1 = voice.low2 = voice.edge = 0;
        voice.fresh = 0;
    }
    const bool ringing = voice.rim > 0x00080000;   // the rim, until it is 72 dB down
    run_tone(voice, b, output);
    if (ringing) {
        i32 band[opxy::block_frames];
        run_saw(voice, b.saw_increment, band);
        run_rim(voice, b.line_increment, b.fall, band, output);
    }
    run_finish(output);
}
}
}
