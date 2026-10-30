#pragma once
// EFM-RS: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM rimshot in the manner of the Machinedrum's EFM-RS, written from
// scratch in fixed point and fitted to it (reference/fit_efm_rs.py). The controls:
//   PTCH  DEC   MOD   HPF
//   SNAR  SPTC  SDEC  SMOD
// Two of EFM-SD's fed-back FM tones (md_efm.hpp), gentler, each with its
// modulator at four times its pitch and no modulation envelope: the rim, 244 Hz to 3.8 kHz
// (PTCH, DEC, MOD), and the snare under it, 122 Hz to 680 Hz as EFM-SD's
// (SPTC, SDEC, SMOD), in proportion to SNAR. Both decay as EFM-BD's body does,
// shorter at the bottom, and pass EFM-SD's high-pass (HPF).
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace efm_rs {
struct State {
    efm::Pair rim, snare;
    i32 rim_level, snare_level;   // envelopes, Q31
    i32 low;                      // HPF's state
    u32 fresh;                    // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, MOD, HPF, SNAR, SPTC, SDEC, SMOD };

// Pitches in Q16 octaves from middle C: the rim 243.9 Hz * 2^(3.97 PTCH),
// the snare 121.9 Hz * 2^(2.48 SPTC).
const i32 rim_low = -6633, rim_span = 260094;
const i32 snare_low = -72208, snare_span = 162491;
// DEC and SDEC: EFM-BD's times, but 17 ms at 0 and 37 ms at 16.
const i32 dec_times[9] = {625880, 699411, 793416, 854102, 934567, 1011122, 1087794, 1170568, 1250679};
// Both modulators at four times their carriers; MOD swings a sixth as far
// as EFM-SD's, SMOD three quarters.
const i32 ratio = 0x40000000;
const i32 rim_scale = 0x17000000, snare_scale = 0x60000000;
// The level of each: the Machinedrum's, 2 dB up as the TRX machines are.
const i32 body_level = 0x19919b6d;

struct Block {
    i32 rim, snare, rim_step, snare_step, rim_depth, snare_depth, snare_level, hpf;
    explicit Block(const i32* p) {
        rim = md::span(p[PTCH], rim_low, rim_low + rim_span);
        snare = md::span(p[SPTC], snare_low, snare_low + snare_span);
        rim_step = md::decay(md::table9(dec_times, p[DEC]));
        snare_step = md::decay(md::table9(dec_times, p[SDEC]));
        rim_depth = multiply(efm::sixteenths(p[MOD]), rim_scale);
        snare_depth = multiply(efm::sixteenths(p[SMOD]), snare_scale);
        snare_level = multiply(md::positive(p[SNAR]), body_level);
        hpf = md::table9(efm::hpf_coefficients, p[HPF]);
    }
};
}

// One of the two, under its falling level, taken from `out`: one loop each, out of line, so
// each has a hardware loop and all the registers to itself. Their pitches don't move in a block.
__attribute__((noinline)) inline void take_pair(efm::Pair& held_pair, u32 increment, efm::Ramp deviation, u32 modulation, i32 fb,
                                                i32& held_level, i32 step, i32 scale, i32* out) {
    efm::Pair pair = held_pair;
    i32 level = held_level;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] -= multiply(pair.run(increment, static_cast<i32>(deviation.step()), modulation, fb), multiply(level, scale));
        level = multiply(level, step);
    }
    held_pair = pair;
    held_level = level;
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.rim.reset();
    voice.snare.reset();
    voice.rim_level = voice.snare_level = voice.low = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.rim.reset();
        voice.snare.reset();
        voice.rim_level = voice.snare_level = md::one;
        voice.low = 0;
        voice.fresh = 0;
    }
    const i32 fb = efm::feedback(md::one);
    i32 flat = 0, held = md::one;
    efm::Sweep rim, snare;
    rim.plan(md::transpose(pitch[0], b.rim), 0, flat, md::one, ratio, b.rim_depth, held, md::one);
    snare.plan(md::transpose(pitch[0], b.snare), 0, flat, md::one, ratio, b.snare_depth, held, md::one);

    i32 mixed[opxy::block_frames];
    for (unsigned i = 0; i != opxy::block_frames; ++i) mixed[i] = 0;
    take_pair(voice.rim, rim.carrier.value, rim.deviation, rim.modulator.value, fb, voice.rim_level, b.rim_step, body_level, mixed);
    take_pair(voice.snare, snare.carrier.value, snare.deviation, snare.modulator.value, fb, voice.snare_level, b.snare_step, b.snare_level, mixed);
    i32 low = voice.low;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        low += multiply(mixed[i] - low, b.hpf);
        output[i] = md::finish(mixed[i] - low);
    }
    voice.low = low;
}
}
}
