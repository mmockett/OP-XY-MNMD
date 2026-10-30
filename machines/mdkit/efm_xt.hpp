#pragma once
// EFM-XT: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM tom in the manner of the Machinedrum's EFM-XT, written from scratch in
// fixed point and fitted to it (reference/fit_efm_xt.py). The controls:
//   PTCH  DEC   RAMP  RDEC
//   MOD   MFRQ  MDEC  CLIC
// EFM-BD's voice (md_efm.hpp), two octaves from 43 Hz to 171 Hz, without the
// kick's quiet tail, and through a 125 Hz
// high-pass, so low settings are thinner and the sidebands carry more of it.
// Its modulator is fed back on itself at full strength, as EFM-SD's: chaotic, so
// MOD also spreads a grit up the band that falls with MDEC, 6 dB an octave from
// about 25 dB under the peak at 3 kHz at MOD 110, as the Machinedrum's.
// CLIC starts the carrier up to 0.19 cycle in, from a zero crossing at 0 to
// near its trough at 127, which the high-pass turns into a click.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace efm_xt {
struct State {
    efm::Pair fm;
    i32 level;      // amplitude envelope, Q31
    i32 ramp;       // RAMP's envelope, Q31
    i32 depth;      // MDEC's envelope, Q31
    i32 low;        // the high-pass's state
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, RAMP, RDEC, MOD, MFRQ, MDEC, CLIC };

// Pitch: 43.1 Hz * 2^(1.99 PTCH), in Q16 octaves from middle C.
const i32 base_low = -170508, base_span = 130246;
// The high-pass: one pole at 125 Hz.
const i32 highpass = 38245571;
// MOD's deviation, 0.85 of EFM-BD's (the fed-back modulator's fundamental is weaker than a plain one).
const i32 depth = 0x6ccccccd;
// CLIC's start, 0.19 cycle at 127.
const i32 click_phase = 0x30a3d70a;
// The level: the Machinedrum's, 2 dB up as the TRX machines are.
const i32 body_level = 0x151b6d1e;

struct Block {
    i32 base, ramp_from, ramp_step, decay_step, deviation, ratio, depth_step;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        ramp_from = efm::sixteenths(p[RAMP]) - 0x10000000;
        ramp_step = md::decay(md::table9(efm::rdec_times, p[RDEC]));
        decay_step = md::decay(md::table9(efm::dec_times, p[DEC]));
        deviation = multiply(efm::sixteenths(p[MOD]), depth);
        ratio = efm::ratio(p[MFRQ]);
        depth_step = md::decay(md::table9(efm::mdec_times, p[MDEC]));
    }
};
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.fm.reset();
    voice.level = voice.ramp = voice.depth = voice.low = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.fm.reset();
        voice.fm.carrier = static_cast<u32>(multiply(md::positive(parameters[CLIC]), click_phase));
        voice.level = md::one;
        voice.ramp = md::one;
        voice.depth = md::one;
        voice.low = 0;
        voice.fresh = 0;
    }
    const i32 fb = efm::feedback(md::one);
    efm::Sweep sweep;
    sweep.plan(md::transpose(pitch[0], b.base), b.ramp_from, voice.ramp, b.ramp_step, b.ratio, b.deviation, voice.depth,
               b.depth_step);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = multiply(voice.fm.run(sweep.carrier.step(), static_cast<i32>(sweep.deviation.step()),
                                            sweep.modulator.step(), fb),
                               multiply(voice.level, body_level));
        // A step too small to take settles the filter, so it ends at zero.
        const i32 step = multiply(s - voice.low, highpass);
        voice.low = step ? voice.low + step : s;
        output[i] = md::finish(voice.low - s);
        voice.level = multiply(voice.level, b.decay_step);
    }
}
}
}
