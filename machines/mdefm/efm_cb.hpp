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
// level's decay; SNAP takes up to 78% of it away over the first 50 ms.
namespace md_mdefm {} namespace md = md_mdefm; namespace md_mdefm {
namespace efm_cb {
struct State {
    efm::Fed low, high;
    i32 level;      // amplitude envelope, Q31
    i32 snap;       // SNAP's fast envelope, Q31
    i32 depth;      // MDEC's envelope, Q31
    u32 fresh;      // a note has started and render has not yet run
};
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

struct Block {
    i32 base, decay_step, share, deviation, ratio, depth_step, fb;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        decay_step = md::decay(md::table9(dec_times, p[DEC]));
        share = md::table9(snap_shares, p[SNAP]);
        deviation = multiply(efm::sixteenths(p[MOD]), depth_scale);
        ratio = efm::ratio(p[MFRQ]);
        depth_step = md::decay(md::table9(efm::mdec_times, p[MDEC]));
        fb = multiply(efm::feedback(p[FB]), fb_scale);
    }
};
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32) {
    voice.low.reset();
    voice.high.reset();
    voice.level = voice.snap = voice.depth = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.low.reset();
        voice.high.reset();
        voice.level = voice.snap = voice.depth = md::one;
        voice.fresh = 0;
    }
    const i32 snap_step = md::decay(snap_time);
    const u32 base = md::transpose(pitch[0], b.base);
    i32 flat = 0, envelope = voice.depth;
    efm::Sweep low, high;
    low.plan(base, 0, flat, md::one, b.ratio, b.deviation, envelope, b.depth_step);
    envelope = voice.depth;
    high.plan(md::scale(base, fifth), 0, flat, md::one, b.ratio, b.deviation, envelope, b.depth_step);
    voice.depth = envelope;

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 l = voice.low.run(low.carrier.step(), static_cast<i32>(low.deviation.step()), low.modulator.step(), b.fb);
        const i32 h = voice.high.run(high.carrier.step(), static_cast<i32>(high.deviation.step()), high.modulator.step(), b.fb);
        // The level less SNAP's share of it, which falls away at the fast rate.
        const i32 held = multiply(voice.level - multiply(multiply(voice.level, b.share), md::one - voice.snap), tone_level);
        output[i] = md::finish(multiply(l, held) + multiply(h, held));
        voice.level = multiply(voice.level, b.decay_step);
        voice.snap = multiply(voice.snap, snap_step);
    }
}
}
}
