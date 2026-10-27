#pragma once
// EFM-CP: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM clap in the manner of the Machinedrum's EFM-CP, written from scratch in
// fixed point and fitted to it (reference/fit_efm_cp.py). The controls:
//   PTCH  DEC   CLPS  CDEC
//   MOD   MFRQ  MDEC  HPF
// EFM-SD's fed-back FM tone (md_efm.hpp), 122 Hz to 5.4 kHz, under a train of
// claps and then a tail. CLPS sets how many claps, CLPS / 8 rounded (none at
// 0: the first is silent); CDEC their spacing, 1 ms at 0 to 41 ms at 127, each one
// struck at full level and falling 18 dB before the next. The tail then starts
// at full level and falls at DEC's rate, as EFM-BD's body does. HPF is
// EFM-SD's high-pass.
namespace md_mdefm {} namespace md = md_mdefm; namespace md_mdefm {
namespace efm_cp {
struct State {
    efm::Pair fm;
    i32 clap;       // the current clap's envelope, Q31
    i32 level;      // the tail's envelope, Q31
    i32 depth;      // MDEC's envelope, Q31
    i32 low;        // HPF's state
    u32 count;      // samples into the current clap
    u32 claps;      // claps still to come, the current one included
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, CLPS, CDEC, MOD, MFRQ, MDEC, HPF };

// Pitch: 121.9 Hz * 2^(5.46 PTCH), in Q16 octaves from middle C.
const i32 base_low = -72208, base_span = 357574;
// CDEC: the claps' spacing, 1 ms .. 41 ms, as Q16 log2 samples.
const i32 spacings[9] = {358004, 402442, 448346, 491411, 536423, 579418, 624197, 668564, 709116};
// A clap's -60 dB time: 3.3 spacings (18 dB per clap), in Q16 octaves over the spacing.
const i32 clap_time = 113834;
// The level: the Machinedrum's, 2 dB up as the TRX machines are.
const i32 body_level = 0x183126e9;

// 2^(log2 / 65536) samples, for spacings under 2^14.
inline u32 samples(i32 log2) {
    const u32 l = static_cast<u32>(md::positive(log2));
    return static_cast<u32>(exp2_table[(l >> 8) & 255u]) >> (30 - (l >> 16));
}

struct Block {
    i32 base, decay_step, clap_step, deviation, ratio, depth_step, hpf;
    u32 spacing;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        decay_step = md::decay(md::table9(efm::dec_times, p[DEC]));
        const i32 space = md::table9(spacings, p[CDEC]);
        spacing = samples(space);
        clap_step = md::decay(space + clap_time);
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
    voice.clap = voice.level = voice.depth = voice.low = 0;
    voice.count = voice.claps = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        const u32 claps = (static_cast<u32>(md::positive(parameters[CLPS])) + 0x4000000u) >> 27;   // CLPS / 8, rounded
        voice.fm.reset();
        voice.claps = claps ? claps : 1;
        voice.clap = claps ? md::one : 0;
        voice.level = md::one;
        voice.depth = md::one;
        voice.count = 0;
        voice.low = 0;
        voice.fresh = 0;
    }
    const i32 fb = efm::feedback(md::one);
    i32 flat = 0;
    efm::Sweep sweep;
    sweep.plan(md::transpose(pitch[0], b.base), 0, flat, md::one, b.ratio, b.deviation, voice.depth, b.depth_step);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = voice.fm.run(sweep.carrier.step(), static_cast<i32>(sweep.deviation.step()), sweep.modulator.step(), fb);
        i32 envelope;
        if (voice.claps) {
            envelope = voice.clap;
            voice.clap = multiply(voice.clap, b.clap_step);
            if (++voice.count >= b.spacing) {
                voice.count = 0;
                voice.clap = md::one;
                --voice.claps;
            }
        } else {
            envelope = voice.level;
            voice.level = multiply(voice.level, b.decay_step);
        }
        const i32 x = -multiply(s, multiply(envelope, body_level));
        voice.low += multiply(x - voice.low, b.hpf);
        output[i] = md::finish(x - voice.low);
    }
}
}
}
