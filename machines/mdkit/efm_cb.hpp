#pragma once
// EFM-CB: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM cowbell in the manner of the Machinedrum's EFM-CB, written from scratch
// in fixed point and fitted to it (reference/fit_efm_cb.py). The controls:
//   PTCH  DEC   SNAP  FB
//   MOD   MFRQ  MDEC  -
// Two sines at equal level, the second 1.48 times the first (172 Hz to 960 Hz
// over PTCH), each frequency-modulated by its own modulator at MFRQ / 16 of it
// (md_efm.hpp) with half EFM-BD's deviation, MDEC its envelope. FB feeds each
// sine back into its own phase one sample late, adding harmonics, and past
// FB 80 a buzz at the top of the band, as the Machinedrum's does. DEC is the
// level's decay; SNAP takes up to 78% of it away over the first 50 ms. Both
// sines start at a zero crossing, falling: the first sample is 0.06 of the peak,
// which comes a quarter cycle later, as the Machinedrum's.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace efm_cb {
namespace {
using md::multiply;

enum { PTCH, DEC, SNAP, FB, MOD, MFRQ, MDEC };

// Pitch: 172 Hz * 2^(2.48 PTCH), in Q16 octaves from middle C; the second tone 1.48 times it, Q28.
const i32 base_low = -39656, base_span = 162570;
const i32 fifth = 0x17b020c5;
// DEC: EFM-BD's times, but 17 ms at 0 and 37 ms at 16 (as EFM-RS's).
const i32 dec_times[9] = {625880, 699411, 793416, 854102, 934567, 1011122, 1087794, 1170568, 1250679};
// SNAP: the share of the level that falls away, and how fast (-60 dB in 94 ms).
const i32 snap_shares[9] = {0, 64424509, 236223201, 450971566, 622770258, 944892805, 1181116006, 1395864371, 1675037245};
const i32 snap_time = 787511;
// FB: 0.38 of MFB's feedback on EFM-BD (2.4 radians at 127); MOD: half EFM-BD's deviation.
const i32 fb_scale = 0x3020c49c;
const i32 depth_scale = 0x40000000;
// The level of each tone: the Machinedrum's, 2 dB up as the TRX machines are.
const i32 tone_level = 0xe5ca14c;

}
struct Block {
    i32 decay_step, share, deviation, fb, snap_step;
    efm::Flat low, high;
    void plan(const i32* p, u32 middle) {
        const u32 base = md::transpose(middle, md::span(p[PTCH], base_low, base_low + base_span));
        decay_step = md::decay(md::table9(dec_times, p[DEC]));
        share = md::table9(snap_shares, p[SNAP]);
        deviation = multiply(efm::sixteenths(p[MOD]), depth_scale);
        const i32 ratio = efm::ratio(p[MFRQ]), depth_step = md::decay(md::table9(efm::mdec_times, p[MDEC]));
        low.prepare(base, ratio, depth_step);
        high.prepare(md::scale(base, fifth), ratio, depth_step);
        snap_step = md::decay(snap_time);
        fb = multiply(efm::feedback(p[FB]), fb_scale);
    }
};

struct State {
    efm::Fed low, high;
    i32 level;      // amplitude envelope, Q31
    i32 snap;       // SNAP's fast envelope, Q31
    i32 depth;      // MDEC's envelope, Q31
    u32 fresh;      // a note has started and render has not yet run
    u32 stale;        // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];      // the controls they were worked out from
    Block block;
};

// The level less SNAP's share of it, which falls away at the fast rate.
__attribute__((noinline)) inline void run_held(State& voice, i32 share, i32 step, i32 snap_step, i32* held) {
    i32 level = voice.level, snap = voice.snap;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        held[i] = multiply(level - multiply(multiply(level, share), md::one - snap), tone_level);
        level = multiply(level, step);
        snap = multiply(snap, snap_step);
    }
    voice.level = level;
    voice.snap = snap;
}
// Two tones each at most tone_level: they cannot reach the output's ceiling.
__attribute__((noinline)) inline void run_mix(const i32* low, const i32* high, opxy::Output output) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 held = output[i];
        output[i] = -multiply(low[i], held) - multiply(high[i], held);
    }
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.low.reset();
    voice.high.reset();
    voice.level = voice.snap = voice.depth = 0;
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
        voice.low.reset(efm::from_zero);
        voice.high.reset(efm::from_zero);
        voice.level = voice.snap = voice.depth = md::one;
        voice.fresh = 0;
    }
    i32 envelope = voice.depth;
    const efm::Ramp low_swing = b.low.deviation(b.deviation, envelope);
    envelope = voice.depth;
    const efm::Ramp high_swing = b.high.deviation(b.deviation, envelope);
    voice.depth = envelope;

    i32 low[opxy::block_frames], high[opxy::block_frames];
    efm::run_fed_modulator(voice.low, b.low.carrier, low_swing, b.low.modulator, low);
    efm::run_fed_carrier(voice.low, b.fb, low, opxy::block_frames);
    efm::run_fed_modulator(voice.high, b.high.carrier, high_swing, b.high.modulator, high);
    efm::run_fed_carrier(voice.high, b.fb, high, opxy::block_frames);
    run_held(voice, b.share, b.decay_step, b.snap_step, output);
    run_mix(low, high, output);
}
}
}
