#pragma once
// TRX-MA: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Maracas: a Machinedrum TRX-MA-style shaker, written from scratch in fixed
// point and fitted to the Machinedrum's own (reference/fit_ma.py). Its page:
//   ATT   SUS   REV   DAMP
//   RATL  RTYP  TONE  HARD
// As measured: a burst of grains of noise in a band from about 5 to 15 kHz,
// flickering 6 to 9 dB from millisecond to millisecond, gated off in 10 ms.
// SUS is the burst's length, 10 to 190 ms; ATT a rise to it from 35 dB down,
// none to 220 ms; REV a second shake, the return, 50 to 170 ms after the
// first; TONE moves the band up (its centre 8.2 to 13.6 kHz), HARD down (13.8
// to 9.7 kHz); RATL thickens the grains and leaves a faint rattle after the
// gate, 40 dB down at 127; DAMP darkens the band as the burst goes on. RTYP has
// no measurable effect on the Machinedrum; here it lengthens the grains, from
// sand to seeds. The Machinedrum's maracas peak 33 dB down; here they are
// lifted 26 dB, to stand with its hats. Like the clap, they have no pitch:
// keys play them alike.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace trx_ma {
struct State {
    i32 rise;        // ATT's envelope, Q31 (rises to one)
    i32 gate;        // the burst's gate, Q31
    u32 left;        // samples of the burst still to run
    u32 again;       // samples until REV's second shake (0: none)
    i32 grain;       // the current grains' level, Q31
    i32 tail;        // RATL's rattle after the gate, Q31
    i32 dark;        // DAMP's darkening so far, Q31 of the most
    i32 low1, low2, top1, top2;   // the band's poles
    u32 seed;
    u32 fresh;
};
namespace {
using md::multiply;

enum { ATT, SUS, REV, DAMP, RATL, RTYP, TONE, HARD };

const i32 attack_rate[9] = {0, 8711894, 4355947, 2529260, 1781978, 1507828, 1306784, 1053128, 890989};
const i32 attack_floor = 0x0248b0a3;           // 35 dB down
const i32 lengths[9] = {441, 1654, 2866, 3859, 4851, 5733, 6615, 7525, 8379};   // samples
const i32 returns[9] = {0, 1102, 2205, 3308, 4410, 5292, 6174, 6857, 7497};     // samples
const i32 release_time = 575710;               // the gate: -60 dB in 10 ms
// Grains: how often one starts (of 2^32, per sample) from RATL 0 to 127, and how
// long each rings, 1.5 to 4 ms to -60 dB, from RTYP.
const u32 sparse = 0x05000000u, dense = 0x10000000u;
const i32 grain_short = 396340, grain_long = 489076;
const i32 tail_time = 897288;                  // the rattle after the gate: -60 dB in 300 ms
const i32 tail_most = 0x0147ae14;              // 40 dB down at RATL 127
// The band: two high-pass poles at 4.5 kHz and two low-pass at 9 kHz, times
// 2^(offset), at offsets -2 .. +0.75 octaves in nine steps.
const i32 band_highpass[9] = {318040882, 395258494, 488576948, 599881862, 730446716, 880402674, 1048078071, 1229308635, 1416942834};
const i32 band_lowpass[9] = {588980124, 717767061, 865997085, 1032191669, 1212438732, 1399867154, 1584642285, 1754910063, 1898964931};
// Positions on those tables (Q31 of the 2.75 octaves): no offset, TONE's reach
// (-0.74 .. +0.58 octave), HARD's (+0.6 .. -0.3), DAMP's (down to an octave):
// wider than the measured centres move, as the low-pass's top is soft.
const i32 centre = 0x5d1745d1;
const i32 tone_low = -0x22719227, tone_span = 0x3d70a3d7;
const i32 hard_low = 0x1bed61bf, hard_span = -0x29e4129e;
const i32 damp_most = 0x1745d174;
const i32 damp_step = 0x01db6db7;              // the darkening's pace, per block: all of it in 100 ms
// The burst's level: the Machinedrum's lifted 26 dB, its grains' peaks near
// the ceiling: they are spiky, and quieter would not stand with the kit.
const i32 level = 0x65800000;   // at twice

struct Block {
    i32 rate, grain_step, tail, position, damp;
    u32 length, again, chance;
    explicit Block(const i32* p) {
        rate = md::table9(attack_rate, p[ATT]);
        length = static_cast<u32>(md::table9(lengths, p[SUS]));
        again = static_cast<u32>(md::table9(returns, p[REV]));
        const u32 r = static_cast<u32>(md::positive(p[RATL]));
        chance = sparse + static_cast<u32>(multiply(static_cast<i32>(r >> 1), static_cast<i32>((dense - sparse) >> 1))) * 4;
        grain_step = md::decay(md::span(p[RTYP], grain_short, grain_long));
        tail = multiply(md::squared(p[RATL]), tail_most);
        position = centre + tone_low + multiply(md::positive(p[TONE]), tone_span)
                 + hard_low + multiply(md::positive(p[HARD]), hard_span);
        damp = multiply(md::positive(p[DAMP]), damp_most);
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.rise = voice.gate = voice.grain = voice.tail = voice.dark = 0;
    voice.left = voice.again = 0;
    voice.low1 = voice.low2 = voice.top1 = voice.top2 = 0;
    voice.seed = 0x68e31da4u * owner;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.rise = b.rate ? attack_floor : md::one;
        voice.gate = md::one;
        voice.left = b.length;
        voice.again = b.again;
        voice.grain = voice.tail = voice.dark = 0;
        voice.low1 = voice.low2 = voice.top1 = voice.top2 = 0;
        voice.fresh = 0;
    }
    // The band for this block: TONE, HARD and DAMP's darkening so far.
    const i32 where = md::clamp(b.position - multiply(voice.dark, b.damp), 0, md::one);
    const i32 highpass = md::table9(band_highpass, where);
    const i32 lowpass = md::table9(band_lowpass, where);
    const i32 release = md::decay(release_time);
    const i32 tail_step = md::decay(tail_time);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 white = md::noise_eighth(voice.seed);
        // A grain starts now and then; each rings briefly.
        if (static_cast<u32>(md::noise(voice.seed)) < b.chance) voice.grain = md::one;
        // The band.
        md::smooth(voice.low1, white, highpass);
        md::smooth(voice.low2, white - voice.low1, highpass);
        md::smooth(voice.top1, white - voice.low1 - voice.low2, lowpass);
        md::smooth(voice.top2, voice.top1, lowpass);
        const i32 envelope = multiply(multiply(voice.rise, voice.gate), level) + multiply(voice.tail, level);
        output[i] = md::finish(md::gain(multiply(multiply(voice.top2, voice.grain), envelope), 4));

        voice.grain = multiply(voice.grain, b.grain_step);
        if (voice.rise < md::one) {
            const i32 add = multiply(voice.rise, b.rate);
            voice.rise = voice.rise > md::one - add ? md::one : voice.rise + add;
        } else if (voice.left) {
            --voice.left;
        } else {
            if (voice.gate == md::one) voice.tail = b.tail;
            voice.gate = multiply(voice.gate, release);
        }
        voice.tail = multiply(voice.tail, tail_step);
        if (voice.again && !--voice.again) {
            // REV: the return shake.
            voice.gate = md::one;
            voice.left = b.length;
        }
    }
    voice.dark = voice.dark < md::one - damp_step ? voice.dark + damp_step : md::one;
}
}
}
