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

}
struct Block {
    i32 rim_step, snare_step, snare_level, hpf, fb;
    efm::Sweep rim, snare;   // nothing in them moves within a note
    void plan(const i32* p, u32 middle) {
        rim_step = md::decay(md::table9(dec_times, p[DEC]));
        snare_step = md::decay(md::table9(dec_times, p[SDEC]));
        const i32 rim_depth = multiply(efm::sixteenths(p[MOD]), rim_scale);
        const i32 snare_depth = multiply(efm::sixteenths(p[SMOD]), snare_scale);
        snare_level = multiply(md::positive(p[SNAR]), body_level);
        hpf = md::table9(efm::hpf_coefficients, p[HPF]);
        fb = efm::feedback(md::one);
        const i32 flat_block = md::per_block(md::one);
        i32 flat = 0, held = md::one;
        rim.plan(md::transpose(middle, md::span(p[PTCH], rim_low, rim_low + rim_span)), 0, flat, flat_block, ratio, rim_depth, held, flat_block);
        snare.plan(md::transpose(middle, md::span(p[SPTC], snare_low, snare_low + snare_span)), 0, flat, flat_block, ratio, snare_depth, held, flat_block);
    }
};

struct State {
    efm::Pair rim, snare;
    i32 rim_level, snare_level;   // envelopes, Q31
    i32 low;                      // HPF's state
    u32 fresh;                    // a note has started and render has not yet run
    u32 stale;        // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];      // the controls they were worked out from
    Block block;
};

// Each of the two under its falling level: a stage each, out of line, so each has a hardware
// loop and all the registers to itself. Their pitches don't move in a block.
template <bool First>
__attribute__((noinline)) inline void take_pair(efm::Pair& pair, u32 increment, efm::Ramp deviation, u32 modulation, i32 fb, i32& held_level, i32 step, i32 scale, i32* out, i32* y) {
    efm::run_modulator(pair, modulation, fb, y);
    efm::run_carrier(pair, increment, deviation, y);
    i32 level = held_level;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 taken = multiply(y[i], multiply(level, scale));
        out[i] = First ? -taken : out[i] - taken;
        level = multiply(level, step);
    }
    held_level = level;
}
__attribute__((noinline)) inline void run_highpass(State& voice, i32 hpf, const i32* mixed, opxy::Output output) {
    i32 low = voice.low;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        low += multiply(mixed[i] - low, hpf);
        output[i] = md::finish(mixed[i] - low);
    }
    voice.low = low;
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.rim.reset();
    voice.snare.reset();
    voice.rim_level = voice.snare_level = voice.low = 0;
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
        voice.rim.reset();
        voice.snare.reset();
        voice.rim_level = voice.snare_level = md::one;
        voice.low = 0;
        voice.fresh = 0;
    }
    i32 mixed[opxy::block_frames], y[opxy::block_frames];
    take_pair<true>(voice.rim, b.rim.carrier.value, b.rim.deviation, b.rim.modulator.value, b.fb, voice.rim_level, b.rim_step, body_level, mixed, y);
    take_pair<false>(voice.snare, b.snare.carrier.value, b.snare.deviation, b.snare.modulator.value, b.fb, voice.snare_level, b.snare_step, b.snare_level, mixed, y);
    run_highpass(voice, b.hpf, mixed, output);
}
}
}
