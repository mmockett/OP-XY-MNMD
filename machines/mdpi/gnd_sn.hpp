#pragma once
// GND-SN: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// A sine in the manner of the Machinedrum's GND-SN, written from scratch in
// fixed point and fitted to it (reference/fit_gnd.py). The controls:
//   PTCH  DEC   RAMP  RDEC
// PTCH is an octave every 16 steps, 43 Hz at 0 to 10.6 kHz at 127. RAMP starts
// the pitch (RAMP / 16)^2 times the pitch above it, an octave at 16, and the
// excess falls at RDEC's rate (a 3 ms time constant at 0, none to speak of from
// 80). DEC is the level's decay, from 2 ms at 0 to held from 112.
namespace md_mdpi {} namespace md = md_mdpi; namespace md_mdpi {
namespace gnd_sn {
struct State {
    u32 phase;
    i32 level;      // amplitude envelope, Q31
    i32 ramp;       // RAMP's envelope, Q31
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, RAMP, RDEC };

// Pitch: 43.07 Hz * 2^(PTCH / 16), in Q16 octaves from middle C.
const i32 base_low = -170574, base_span = 520192;
// DEC and RDEC as -60 dB times, Q16 log2 samples at the controls 0, 16, ..., 127.
const i32 dec_times[9] = {423363, 527565, 625869, 745144, 865075, 945684, 1148846, 1311375, 1311375};
const i32 rdec_times[9] = {634384, 773492, 914263, 1060173, 1193744, 1335800, 1335800, 1335800, 1335800};
// RAMP: the start's excess, (RAMP / 16)^2 times the pitch, Q24 (0 .. 63).
const i32 excess_top = 1056964608;
// The level: the Machinedrum's -26.6 dB RMS, 2 dB up as the TRX machines are.
const i32 level = 0xaa58606;

// The pitch times 1 + excess (Q24, 0 .. 63), capped below half the sample rate.
inline u32 raised(u32 base, i32 excess) {
    const i32 ratio = 0x02000000 + excess * 2;                    // Q25, 1 .. 127
    if (ratio < 0x10000000) return md::scale(base, ratio * 8);    // under 8: Q28
    const u32 eighth = md::scale(base, ratio);                    // base * ratio / 8
    return eighth > 0x0fffffffu ? 0x7fffffffu : eighth << 3;
}

struct Block {
    i32 base, decay_step, excess, ramp_step;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        decay_step = md::decay(md::table9(dec_times, p[DEC]));
        excess = multiply(md::squared(p[RAMP]), excess_top);
        ramp_step = md::decay(md::table9(rdec_times, p[RDEC]));
    }
};
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.phase = 0;
    voice.level = voice.ramp = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.phase = 0;
        voice.level = voice.ramp = md::one;
        voice.fresh = 0;
    }
    const u32 base = md::transpose(pitch[0], b.base);
    const i32 ramp_next = multiply(voice.ramp, md::per_block(b.ramp_step));
    efm::Ramp increment;
    increment.plan(raised(base, multiply(b.excess, voice.ramp)), raised(base, multiply(b.excess, ramp_next)));
    voice.ramp = ramp_next;

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        output[i] = md::finish(multiply(md::sine(voice.phase), multiply(voice.level, level)));
        voice.phase += increment.step();
        voice.level = multiply(voice.level, b.decay_step);
    }
}
}
}
