#pragma once
// EFM-SD: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM snare in the manner of the Machinedrum's EFM-SD, written from scratch
// in fixed point and fitted to it (reference/fit_efm_sd.py). The controls:
//   PTCH  DEC   NOIS  NDEC
//   MOD   MFRQ  MDEC  HPF
// A carrier from 122 Hz to 680 Hz, frequency-modulated as EFM-BD's
// (md_efm.hpp) by a modulator fed back on itself at full strength, so MOD
// spreads the tone into a rough band around its pitch rather than clean
// sidebands. NOIS adds a bright noise (white, through two 2.5 kHz high-passes)
// in proportion, with NDEC its decay; both pass a one-pole high-pass that HPF
// sweeps from off to 3.3 kHz. The tone starts just past its zero crossing,
// falling, at any pitch: the first sample is 0.07 of its peak, as the Machinedrum's.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace efm_sd {
struct State {
    efm::Pair fm;
    i32 level;      // the tone's envelope, Q31
    i32 tail;       // the quiet tail under it, Q31
    i32 depth;      // MDEC's envelope, Q31
    i32 hiss;       // the noise's envelope, Q31
    i32 low1, low2; // the noise's high-passes
    i32 low;        // HPF's state
    u32 seed;
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, NOIS, NDEC, MOD, MFRQ, MDEC, HPF };

// Pitch: 121.9 Hz * 2^(2.48 PTCH), in Q16 octaves from middle C.
const i32 base_low = -72208, base_span = 162491;
// NDEC: the noise's -60 dB time, 25 ms .. 1.7 s, Q16 log2 samples (md::table9).
const i32 ndec_times[9] = {662344, 706782, 748218, 793416, 844140, 901600, 957974, 1011122, 1061292};
// DEC: EFM-BD's times, but shorter at the bottom: 22 ms at 0, 37 ms at 16.
const i32 dec_times[9] = {650000, 698000, 793416, 854102, 934567, 1011122, 1087794, 1170568, 1250679};
// The noise's colour: two one-pole high-passes at 2.5 kHz.
const i32 colour = 643515395;
// The quiet tail: from -36 dB, falling 60 dB in 250 ms.
const i32 tail_start = 34035589, tail_time = 880050;
// The levels: the Machinedrum's tone (as EFM-BD's) and noise, 2 dB up as the TRX machines are.
const i32 body_level = 0x1ae147ae;
const i32 noise_level = 0x2b333333;

struct Block {
    i32 base, decay_step, noise, noise_step, deviation, ratio, depth_step, hpf;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        decay_step = md::decay(md::table9(dec_times, p[DEC]));
        noise = multiply(md::positive(p[NOIS]), noise_level);
        noise_step = md::decay(md::table9(ndec_times, p[NDEC]));
        deviation = efm::sixteenths(p[MOD]);
        ratio = efm::ratio(p[MFRQ]);
        depth_step = md::decay(md::table9(efm::mdec_times, p[MDEC]));
        hpf = md::table9(efm::hpf_coefficients, p[HPF]);
    }
};
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.fm.reset();
    voice.level = voice.tail = voice.depth = voice.hiss = 0;
    voice.low1 = voice.low2 = voice.low = 0;
    voice.seed = 0x2545f491u;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.fm.reset(efm::fed_start);
        voice.level = voice.hiss = voice.depth = md::one;
        voice.tail = tail_start;
        voice.low1 = voice.low2 = voice.low = 0;
        voice.seed = 0x2545f491u;
        voice.fresh = 0;
    }
    const i32 tail_step = md::decay(tail_time);
    const i32 fb = efm::feedback(md::one);
    i32 flat = 0;
    efm::Sweep sweep;
    sweep.plan(md::transpose(pitch[0], b.base), 0, flat, md::one, b.ratio, b.deviation, voice.depth, b.depth_step);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = voice.fm.run(sweep.carrier.step(), static_cast<i32>(sweep.deviation.step()), sweep.modulator.step(), fb);
        const i32 held = voice.level > voice.tail ? voice.level : voice.tail;
        const i32 white = md::noise_eighth(voice.seed);
        voice.low1 += multiply(white - voice.low1, colour);
        const i32 once = white - voice.low1;
        voice.low2 += multiply(once - voice.low2, colour);
        const i32 hiss = multiply(once - voice.low2, multiply(voice.hiss, b.noise));
        const i32 mix = hiss * 4 - multiply(s, multiply(held, body_level));
        voice.low += multiply(mix - voice.low, b.hpf);
        output[i] = md::finish(mix - voice.low);
        voice.level = multiply(voice.level, b.decay_step);
        voice.tail = multiply(voice.tail, tail_step);
        voice.hiss = multiply(voice.hiss, b.noise_step);
    }
}
}
}
