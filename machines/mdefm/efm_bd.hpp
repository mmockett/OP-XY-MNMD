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
namespace md {
namespace efm_bd {
struct State {
    efm::Pair fm;
    i32 level;      // amplitude envelope, Q31
    i32 tail;       // the quiet tail under it, Q31
    i32 ramp;       // RAMP's envelope, Q31
    i32 depth;      // MDEC's envelope, Q31
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, RAMP, RDEC, MOD, MFRQ, MDEC, MFB };

// Pitch: 25.7 Hz * 2^(3.96 PTCH), in Q16 octaves from middle C.
const i32 base_low = -219392, base_span = 259769;
// The quiet tail: from -20 dB, falling 60 dB in 250 ms.
const i32 tail_start = 214748365, tail_time = 880050;
// The level: the Machinedrum's -15.5 dB peak, 2 dB up as the TRX machines are.
const i32 body_level = 0x1ae147ae;

struct Block {
    i32 base, ramp_from, ramp_step, decay_step, deviation, ratio, depth_step, fb;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        ramp_from = efm::sixteenths(p[RAMP]) - 0x10000000;   // the start's multiple, less 1
        ramp_step = md::decay(md::table9(efm::rdec_times, p[RDEC]));
        decay_step = md::decay(md::table9(efm::dec_times, p[DEC]));
        deviation = efm::sixteenths(p[MOD]);
        ratio = efm::ratio(p[MFRQ]);
        depth_step = md::decay(md::table9(efm::mdec_times, p[MDEC]));
        fb = efm::feedback(p[MFB]);
    }
};
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.fm.reset();
    voice.level = voice.tail = voice.ramp = voice.depth = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.fm.reset();
        voice.level = md::one;
        voice.tail = tail_start;
        voice.ramp = md::one;
        voice.depth = md::one;
        voice.fresh = 0;
    }
    const i32 tail_step = md::decay(tail_time);
    efm::Sweep sweep;
    sweep.plan(md::transpose(pitch[0], b.base), b.ramp_from, voice.ramp, b.ramp_step, b.ratio, b.deviation, voice.depth,
               b.depth_step);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = voice.fm.run(sweep.carrier.step(), static_cast<i32>(sweep.deviation.step()), sweep.modulator.step(), b.fb);
        const i32 held = voice.level > voice.tail ? voice.level : voice.tail;
        output[i] = md::finish(-multiply(s, multiply(held, body_level)));
        voice.level = multiply(voice.level, b.decay_step);
        voice.tail = multiply(voice.tail, tail_step);
    }
}
}
}
