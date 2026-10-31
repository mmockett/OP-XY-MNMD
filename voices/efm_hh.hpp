#pragma once
// EFM-HH: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_efm.hpp"

// An FM hi-hat in the manner of the Machinedrum's EFM-HH, written from scratch
// in fixed point and fitted to it (reference/fit_efm_hh.py). The controls:
//   PTCH  DEC   TREM  TFRQ
//   MOD   MFRQ  MDEC  FB
// One FM carrier (md_efm.hpp), 345 Hz to 1.4 kHz over PTCH, fed back on itself
// by FB, whose phase jumps to a random place every 32 samples, as the
// Machinedrum's does: so it never settles into a line at its pitch, and each
// jump is a click of random height. The clicks and the carrier's own level
// between them are one signal, measured: the gaps hold a sine at the pitch, its
// phase new after every click, and the clicks' heights correlate with the next
// one's by half the cosine of the pitch's turn over 32 samples (+0.43 at 690 Hz,
// -0.49 at 1.38 kHz). That leaves a notch at the pitch and repeats it up the band
// every 1378 Hz. A high-pass (a zero at DC, two poles at 0.2) passes each click
// falling to 0.39 and 0.09 of itself and leaves the sidebands louder than the
// carrier: past FB 40 it buzzes at the top of the band. DEC is EFM-BD's decay.
// TREM is a tremolo, TFRQ its rate (0.95 Hz to 59 Hz). A short hat (DEC under
// 64) chokes the open ones still ringing.
namespace md {
namespace efm_hh {
struct State {
    efm::Fed fm;
    i32 level;      // amplitude envelope, Q31
    i32 depth;      // MDEC's envelope, Q31
    i32 before;     // the high-pass's last input
    i32 y1, y2;     // and its last two outputs
    u32 tremolo;    // TREM's phase
    u32 count;      // samples since the last jump
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
// The carrier's phase jumps this often, in samples.
const u32 jump_period = 32;
// The high-pass: y[n] = x[n] - x[n-1] + 0.39 y[n-1] - 0.06 y[n-2].
const i32 pole1 = 0x31eb851f, pole2 = 0x07ae147b;
// A choked open hat dies 60 dB in 0.12 s.
const i32 choke_time = 778000;
// The carrier's level before the high-pass, 0.126 on the Machinedrum (its clicks
// 0.126 RMS at their peaks), 2 dB up as the TRX machines are.
const i32 carrier_level = 0x14380000;

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
    voice.level = voice.depth = voice.before = voice.y1 = voice.y2 = 0;
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
        voice.fm.reset(efm::from_zero);
        voice.open = parameters[DEC] >= 0x40000000;
        if (!voice.open) ++shared.closed_hats;
        voice.choke = shared.closed_hats;
        voice.level = voice.depth = md::one;
        voice.before = voice.y1 = voice.y2 = 0;
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
        if (voice.count == jump_period) {
            voice.count = 0;
            voice.fm.carrier += static_cast<u32>(md::noise(voice.seed));
        }
        ++voice.count;
        const i32 s = voice.fm.run(sweep.carrier.step(), static_cast<i32>(sweep.deviation.step()), sweep.modulator.step(), b.fb);
        const i32 tone = -multiply(s, carrier_level);
        const i32 y = tone - voice.before + multiply(voice.y1, pole1) - multiply(voice.y2, pole2);
        voice.before = tone;
        voice.y2 = voice.y1;
        voice.y1 = y;
        // TREM: the level dips to 1 - TREM at the tremolo's trough.
        const i32 swing = multiply(b.trem, 0x40000000 - multiply(md::sine(voice.tremolo + 0x40000000u), 0x40000000));
        voice.tremolo += static_cast<u32>(b.rate);
        const i32 envelope = multiply(voice.level, md::one - swing);
        output[i] = md::finish(multiply(y, envelope));
        voice.level = multiply(voice.level, fall);
    }
}
}
}
