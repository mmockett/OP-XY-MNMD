#pragma once
// EFM-BD: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM kick in the manner of the Machinedrum's EFM-BD, written from scratch in
// fixed point and fitted to it (reference/fit_efm_bd.py). The controls:
//   PTCH  DEC   RAMP  RDEC
//   MOD   MFRQ  MDEC  MFB
// One carrier, frequency-modulated by one modulator (md_efm.hpp). PTCH sets the
// pitch, 25.7 Hz to 401 Hz in four octaves. RAMP multiplies it at the start by
// RAMP / 16, so 16 is flat, more falls from above and less rises from below,
// and the multiplier settles to 1 exponentially at RDEC's rate. DEC is the
// level's decay, over a quiet tail from -20 dB; MDEC the modulation's.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace efm_bd {
namespace {
using md::multiply;

enum { PTCH, DEC, RAMP, RDEC, MOD, MFRQ, MDEC, MFB };

// Pitch: 25.7 Hz * 2^(3.96 PTCH), in Q16 octaves from middle C.
const i32 base_low = -219392, base_span = 259769;
// The quiet tail: from -20 dB, falling 60 dB in 250 ms.
const i32 tail_start = 214748365, tail_time = 880050;
// The level: the Machinedrum's -15.5 dB peak, 2 dB up as the TRX machines are.
const i32 body_level = 0x1ae147ae;

}
struct Block {
    i32 ramp_from, ramp_block, decay_step, deviation, ratio, depth_block, fb, tail_step;
    u32 base;
    void plan(const i32* p, u32 middle) {
        base = md::transpose(middle, md::span(p[PTCH], base_low, base_low + base_span));
        ramp_from = efm::sixteenths(p[RAMP]) - 0x10000000;   // the start's multiple, less 1
        ramp_block = md::per_block(md::decay(md::table9(efm::rdec_times, p[RDEC])));
        decay_step = md::decay(md::table9(efm::dec_times, p[DEC]));
        deviation = efm::sixteenths(p[MOD]);
        ratio = efm::ratio(p[MFRQ]);
        depth_block = md::per_block(md::decay(md::table9(efm::mdec_times, p[MDEC])));
        fb = efm::feedback(p[MFB]);
        tail_step = md::decay(tail_time);
    }
};

struct State {
    efm::Pair fm;
    i32 level;      // amplitude envelope, Q31
    i32 tail;       // the quiet tail under it, Q31
    i32 ramp;       // RAMP's envelope, Q31
    i32 depth;      // MDEC's envelope, Q31
    u32 fresh;      // a note has started and render has not yet run
    u32 stale;        // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];      // the controls they were worked out from
    Block block;
};

// The level over the tone, out of line and on locals. body_level is far under the output's ceiling.
__attribute__((noinline)) inline void run_level(State& voice, i32 step, i32 tail_step, opxy::Output x) {
    i32 level = voice.level, tail = voice.tail;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 held = level > tail ? level : tail;
        x[i] = -multiply(x[i], multiply(held, body_level));
        level = multiply(level, step);
        tail = multiply(tail, tail_step);
    }
    voice.level = level;
    voice.tail = tail;
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.fm.reset();
    voice.level = voice.tail = voice.ramp = voice.depth = 0;
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
        voice.fm.reset();
        voice.level = md::one;
        voice.tail = tail_start;
        voice.ramp = md::one;
        voice.depth = md::one;
        voice.fresh = 0;
    }
    efm::Sweep sweep;
    sweep.plan(b.base, b.ramp_from, voice.ramp, b.ramp_block, b.ratio, b.deviation, voice.depth, b.depth_block);
    efm::run_modulator_swept(voice.fm, sweep.modulator, b.fb, output);
    efm::run_carrier_swept(voice.fm, sweep.carrier, sweep.deviation, output);
    run_level(voice, b.decay_step, b.tail_step, output);
}
}
}
