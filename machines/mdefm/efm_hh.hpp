#pragma once
// EFM-HH: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM hi-hat in the manner of the Machinedrum's EFM-HH, written from scratch
// in fixed point and fitted to it (reference/fit_efm_hh.py). The controls:
//   PTCH  DEC   TREM  TFRQ
//   MOD   MFRQ  MDEC  FB
// On the Machinedrum the hat's noise is a click of random height every 32
// samples, each falling to 0.4 and 0.1 of itself over the next two; here too,
// over a 300 Hz high-pass. DEC is EFM-BD's decay.
// Over it an FM carrier (md_efm.hpp), 345 Hz to 1.4 kHz over PTCH, fed back on
// itself by FB, through a 5 kHz one-pole high-pass that leaves its sidebands
// louder than it: past FB 40 it buzzes at the top of the band, as the
// Machinedrum's does. TREM is a tremolo, TFRQ its rate (0.95 Hz to 59 Hz). A
// short hat (DEC under 64) chokes the open ones still ringing.
namespace md {
namespace efm_hh {
struct State {
    efm::Fed fm;
    i32 level;      // amplitude envelope, Q31
    i32 depth;      // MDEC's envelope, Q31
    i32 click;      // the noise's one-pole state
    i32 rumble;     // the clicks' high-pass state
    i32 low;        // the high-pass's state
    u32 tremolo;    // TREM's phase
    u32 count;      // samples since the last click
    u32 seed;
    u32 choke;      // an open hat: the closed hats started when it was struck
    u32 open;       // this hit is an open hat, which a closed one chokes
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PTCH, DEC, TREM, TFRQ, MOD, MFRQ, MDEC, FB };

// Pitch: 345 Hz * 2^(2 PTCH), in Q16 octaves from middle C.
const i32 base_low = 26155, base_span = 131072;
// TFRQ: the tremolo's rate as a phase increment, 0.95 Hz .. 59 Hz.
const i32 tremolo_rates[9] = {92522, 155826, 262957, 438262, 749915, 1246612, 2093918, 3535313, 5746101};
// FB: 0.75 of MFB's feedback on EFM-BD.
const i32 fb_scale = 0x60000000;
// The high-pass: one pole at 5 kHz.
const i32 highpass = 1094194833;
// The clicks: every 32 samples, then 0.4 of the last one each sample, through
// a one-pole high-pass at 300 Hz.
const u32 click_period = 32;
const i32 click_fall = 0x33333333;
const i32 click_highpass = 89855360;
// A choked open hat dies 60 dB in 0.12 s.
const i32 choke_time = 778000;
// The levels, 2 dB up on the Machinedrum's as the TRX machines are: the clicks
// (0.085 RMS at their peaks) and the carrier (0.1 before the high-pass).
const i32 click_level = 0x28500000;
const i32 carrier_level = 0x16a00000;

struct Block {
    i32 base, decay_step, trem, rate, deviation, ratio, depth_step, fb;
    explicit Block(const i32* p) {
        base = md::span(p[PTCH], base_low, base_low + base_span);
        decay_step = md::decay(md::table9(efm::dec_times, p[DEC]));
        trem = md::positive(p[TREM]);
        rate = md::table9(tremolo_rates, p[TFRQ]);
        deviation = efm::sixteenths(p[MOD]);
        ratio = efm::ratio(p[MFRQ]);
        depth_step = md::decay(md::table9(efm::mdec_times, p[MDEC]));
        fb = multiply(efm::feedback(p[FB]), fb_scale);
    }
};
}

inline void trigger(State& voice, Common&) {
    voice.fresh = 1;
}
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.fm.reset();
    voice.level = voice.depth = voice.click = voice.rumble = voice.low = 0;
    voice.tremolo = voice.count = 0;
    voice.seed = 0x6c078965u * owner;
    voice.choke = shared.closed_hats;
    voice.open = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common& shared, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.fm.reset();
        voice.open = parameters[DEC] >= 0x40000000;
        if (!voice.open) ++shared.closed_hats;
        voice.choke = shared.closed_hats;
        voice.level = voice.depth = md::one;
        voice.click = voice.rumble = voice.low = 0;
        voice.tremolo = 0;
        voice.count = 0;
        voice.fresh = 0;
    }
    const bool choked = voice.open && voice.choke != shared.closed_hats;
    const i32 fall = choked ? md::decay(choke_time) : b.decay_step;
    i32 flat = 0;
    efm::Sweep sweep;
    sweep.plan(md::transpose(pitch[0], b.base), 0, flat, md::one, b.ratio, b.deviation, voice.depth, b.depth_step);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 s = voice.fm.run(sweep.carrier.step(), static_cast<i32>(sweep.deviation.step()), sweep.modulator.step(), b.fb);
        const i32 tone = multiply(s, carrier_level);
        voice.low += multiply(tone - voice.low, highpass);
        voice.click = multiply(voice.click, click_fall);
        if (++voice.count >= click_period) {
            voice.count = 0;
            voice.click = multiply(md::noise(voice.seed), click_level);
        }
        // TREM: the level dips to 1 - TREM at the tremolo's trough.
        const i32 swing = multiply(b.trem, 0x40000000 - multiply(md::sine(voice.tremolo + 0x40000000u), 0x40000000));
        voice.tremolo += static_cast<u32>(b.rate);
        const i32 envelope = multiply(voice.level, md::one - swing);
        voice.rumble += multiply(voice.click - voice.rumble, click_highpass);
        output[i] = md::finish(multiply(tone - voice.low + voice.click - voice.rumble, envelope));
        voice.level = multiply(voice.level, fall);
    }
}
}
}
