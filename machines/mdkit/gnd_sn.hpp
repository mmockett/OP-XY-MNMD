#pragma once
// GND-SN: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// A sine in the manner of the Machinedrum's GND-SN, written from scratch in
// fixed point and fitted to it (reference/fit_gnd.py). The controls:
//   PTCH  DEC   RAMP  RDEC
// PTCH is an octave every 16 steps, 43 Hz at 0 to 10.6 kHz at 127. RAMP adds Hz
// whatever PTCH is: the start is 690 Hz x (RAMP / 16)^2 above the pitch (43.5 kHz
// at 127), and the excess falls at RDEC's rate (a 2.7 ms time constant at 0, 1 s at 64,
// held from 96). It is never capped: as on the Machinedrum, the phase wraps, so
// a start past half the sample rate aliases, and RAMP 127 opens near 630 Hz. A
// one-pole after the sine is the Machinedrum's roll-off, measured from its level:
// -1.2 dB at 5.5 kHz, -3 at 11 kHz, -5 at 20 kHz. DEC is the level's decay, from
// 2 ms at 0 to held from 112.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace gnd_sn {
struct State {
    u32 phase;
    i32 level;      // amplitude envelope, Q31
    i32 ramp;       // RAMP's envelope, Q31
    i32 smooth;     // the roll-off's state
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, RAMP, RDEC };

// Pitch: 43.07 Hz * 2^(PTCH / 16), in Q16 octaves from middle C.
const i32 base_low = -170574, base_span = 520192;
// DEC and RDEC as -60 dB times, Q16 log2 samples at the controls 0, 16, ..., 127.
const i32 dec_times[9] = {423363, 527565, 625869, 726600, 865075, 1003000, 1142700, 1311375, 1311375};
const i32 rdec_times[9] = {631500, 768000, 909000, 1048000, 1189000, 1310000, 1310000, 1310000, 1310000};
// RAMP: the start's excess. Its unit is 690 Hz at middle C (Q16 octaves from the
// key), and (RAMP / 16)^2 is kept as an eighth of itself, for md::scale (Q28, 0 .. 7.88).
const i32 ramp_unit = 91691;
const i32 ramp_held = 0x60c00000;   // RDEC 96 and up: 14 s or more to fall by e, held here
const i32 excess_top = 0x7e020000;
// The roll-off: the share of the difference taken per sample.
const i32 smoothing = 0x5c28f5c3;
// The level: the Machinedrum's -26.6 dB RMS, 2 dB up as the TRX machines are.
const i32 level = 0xaa58606;

// RAMP's full excess as half an increment, kept below a whole cycle a sample
// (a high key reaches that, and stays there).
inline i32 excess_half(u32 key, i32 excess) {
    const u32 eighth = md::scale(md::transpose(key, ramp_unit), excess);
    return eighth > 0x1fffffffu ? 0x7fffffff : static_cast<i32>(eighth << 2);
}

struct Block {
    i32 base, decay_step, excess, ramp_step;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        decay_step = md::decay(md::table9(dec_times, p[DEC]));
        excess = multiply(md::squared(p[RAMP]), excess_top);
        ramp_step = p[RDEC] < ramp_held ? md::decay(md::table9(rdec_times, p[RDEC])) : md::one;
    }
};
}

// Whether a sound is still to come however quiet it is now: the envelope still
// open (-102 dB). A pitch RAMP holds at half the sample rate, or at the sample
// rate, can sample a sine at its zeros, silent until the ramp falls away from it.
inline bool waiting(const State& voice) { return multiply(voice.level, level) >= (1 << 14); }
inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.phase = 0;
    voice.level = voice.ramp = voice.smooth = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.phase = 0;
        voice.level = voice.ramp = md::one;
        voice.smooth = 0;
        voice.fresh = 0;
    }
    const u32 base = md::transpose(pitch[0], b.base);
    const i32 ramp_next = multiply(voice.ramp, md::per_block(b.ramp_step));
    // The excess runs apart from the pitch: their sum may pass a whole cycle a
    // sample, where the phase wraps as it should.
    const i32 half = excess_half(pitch[0], b.excess);
    efm::Ramp excess;
    excess.plan(static_cast<u32>(multiply(half, voice.ramp)) << 1, static_cast<u32>(multiply(half, ramp_next)) << 1);
    voice.ramp = ramp_next;

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        voice.smooth += multiply(multiply(md::sine(voice.phase), multiply(voice.level, level)) - voice.smooth, smoothing);
        output[i] = md::finish(voice.smooth);
        voice.phase += base + excess.step();
        voice.level = multiply(voice.level, b.decay_step);
    }
}
}
}
