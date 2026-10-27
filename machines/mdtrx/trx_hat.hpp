#pragma once
// TRX-CH and TRX-OH: the sound, modules any engine can hold (docs/VOICES.md).
// Both play the same hat; TRX-OH holds and falls longer. Fitted to the
// Machinedrum's own (reference/fit_hh.py). Their page:
//   GAP   DEC   HPF   LPF
//   MTAL
// As measured: eight metallic partials between 3.8 and 9.3 kHz in a band from
// HPF (2.5 .. 8 kHz) to LPF (2.2 .. 12 kHz). The level holds for DEC's time,
// then falls at GAP's rate. MTAL weights the lower partials. TRX-OH has its
// own, longer holds and a slower fall. A closed hat chokes the open ones, on
// any voice of the engine. The Machinedrum's hats have no pitch; here the key
// transposes them.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

namespace md_mdtrx {} namespace md = md_mdtrx; namespace md_mdtrx {
namespace trx_hat {
struct State {
    u32 phase[8];    // the eight metallic squares
    i32 level;       // the envelope after the hold, Q31
    u32 hold;        // samples of hold still to run
    u32 seed;
    i32 low1, low2;  // the high-pass's two poles
    i32 top1, top2;  // the low-pass's two poles
    u32 choke;       // the closed-hat count this open hat started under
    u32 open;        // this note is an open hat
    u32 fresh;
};
namespace {
using md::multiply;

enum { GAP, DEC, HPF, LPF, MTAL };

// The partials at middle C, Q16 octaves from it: 3804, 4241, 5325, 5936, 6845, 7632, 8364, 9330 Hz.
const unsigned partial_count = 8, lower_partials = 2;
const i32 partials[partial_count] = {253096, 263378, 284898, 295169, 308640, 318930, 327589, 337923};
const i32 square = 0x01800000;                       // each at 3/256: eight stay below 1/8
const i32 highpass_low = 643515395, highpass_high = 1460544865;   // 2.5 .. 8 kHz, two poles: higher, the squares' aliases take over
const i32 lowpass_low = 577838028, lowpass_high = 1758964773;    // 2.2 .. 12 kHz, two poles
// The hold at DEC 0, 16, ..., 127, in samples: closed 5 .. 200 ms, open 30 .. 520 ms.
const i32 holds[9] = {220, 1102, 2205, 3528, 4851, 5512, 6174, 7497, 8820};
const i32 open_holds[9] = {1323, 5292, 8820, 12348, 15876, 17640, 18963, 20727, 22932};
// GAP: the fall after the hold, -60 dB from 85 ms (closed) or 100 .. 600 ms (open,
// longer with DEC) at GAP 0, to 400 ms closed or 2.3 s open at GAP 127.
const i32 fall_short = 778050, open_fall_short = 793416, open_fall_shortest_long = 962824;
const i32 fall_long_closed = 924488, fall_long_open = 1089872;
const i32 choke_time = 600000;                       // a choked open hat: -60 dB in about 6 ms

struct Block {
    i32 weight_low, weight_high, highpass, lowpass, fall_step, open;
    u32 hold;
    Block(const i32* p, bool opened) {
        // MTAL: the two lower partials from a quarter to full; the upper ones from full to 0.6.
        weight_low = md::span(p[MTAL], 0x20000000, md::one);
        weight_high = md::span(p[MTAL], md::one, 0x4ccccccd);
        highpass = md::span(p[HPF], highpass_low, highpass_high);
        lowpass = md::span(p[LPF], lowpass_low, lowpass_high);
        open = opened ? md::one : 0;   // TRX-OH, or TRX-CH
        const i32 shortest = md::span(open, fall_short, md::span(p[DEC], open_fall_short, open_fall_shortest_long));
        const i32 longest = md::span(open, fall_long_closed, fall_long_open);
        fall_step = md::decay(md::span(p[GAP], shortest, longest));
        hold = static_cast<u32>(md::span(open, md::table9(holds, p[DEC]), md::table9(open_holds, p[DEC])));
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    for (unsigned k = 0; k < partial_count; ++k) voice.phase[k] = 0x20000000u * k;
    voice.level = 0;
    voice.hold = 0;
    voice.seed = 0x6c078965u * owner;
    voice.low1 = voice.low2 = voice.top1 = voice.top2 = 0;
    voice.choke = shared.closed_hats;
    voice.open = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common& shared, opxy::Output output, const i32* parameters, opxy::Pitch pitch, bool opened) {
    const Block b(parameters, opened);
    if (voice.fresh) {
        voice.open = opened;
        if (!voice.open) ++shared.closed_hats;
        voice.choke = shared.closed_hats;
        voice.level = md::one;
        voice.hold = b.hold;
        voice.fresh = 0;
    }
    const bool choked = voice.open && voice.choke != shared.closed_hats;
    const i32 fall = choked ? md::decay(choke_time) : b.fall_step;
    if (choked) voice.hold = 0;

    u32 increment[partial_count];
    for (unsigned k = 0; k < partial_count; ++k) increment[k] = md::transpose(pitch[0], partials[k]);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        i32 low = 0, high = 0;
        for (unsigned k = 0; k < lower_partials; ++k) {
            low += voice.phase[k] & 0x80000000u ? -square : square;
            voice.phase[k] += increment[k];
        }
        for (unsigned k = lower_partials; k < partial_count; ++k) {
            high += voice.phase[k] & 0x80000000u ? -square : square;
            voice.phase[k] += increment[k];
        }
        const i32 metal = multiply(low, b.weight_low) + multiply(high, b.weight_high);
        // Two high-pass poles, then two low-pass poles.
        md::smooth(voice.low1, metal, b.highpass);
        md::smooth(voice.low2, metal - voice.low1, b.highpass);
        const i32 band = metal - voice.low1 - voice.low2;
        md::smooth(voice.top1, band, b.lowpass);
        md::smooth(voice.top2, voice.top1, b.lowpass);
        output[i] = md::finish(multiply(md::gain(voice.top2, 3), voice.level));
        if (voice.hold) --voice.hold;
        else voice.level = multiply(voice.level, fall);
    }
}
}

namespace trx_ch {
typedef trx_hat::State State;
inline void reset(State& s, Common& c, u32 owner) { trx_hat::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { trx_hat::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { trx_hat::render(s, c, o, p, pitch, false); }
}

namespace trx_oh {
typedef trx_hat::State State;
inline void reset(State& s, Common& c, u32 owner) { trx_hat::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { trx_hat::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { trx_hat::render(s, c, o, p, pitch, true); }
}
}
