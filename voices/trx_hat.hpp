#pragma once
// TRX-CH and TRX-OH: the sound, modules any engine can hold (docs/VOICES.md).
// Both play the same hat; TRX-OH holds and falls longer. Fitted to the
// Machinedrum's own (reference/fit_hh.py). Their page:
//   GAP   DEC   HPF   LPF
//   MTAL
// As measured: the lines of the cymbal's oscillators (trx_cy.hpp) sounded raw,
// the odd harmonics of five squares at 432, 513, 551, 760 and 848 Hz and every
// harmonic of a pulse at 387 Hz of duty 3/8, with white noise about as loud as all
// of them, through one chain: three high-pass poles (HPF: 4.9 .. 11.8 kHz, its level
// made up by 26 dB) into a four-pole low-pass that peaks at its edge (LPF: 4.9 .. 9.2 kHz). MTAL trades them: the lines from an eighth to full, the noise
// from full to 0.22. The level holds for DEC's time, then falls at GAP's rate.
// TRX-OH has its own, longer holds and a slower fall. A closed hat chokes the open
// ones, on any voice of the engine. The oscillators run free, so no two hits are
// alike. The Machinedrum's hats have no pitch; here the key transposes the lines.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

namespace md {
namespace trx_hat {
struct State {
    u32 phase[6];    // the five squares, then the pulse
    i32 level;       // the envelope after the hold, Q31
    u32 hold;        // samples of hold still to run
    u32 seed;
    i32 low1, low2, low3;   // the high-pass's three poles
    md::Svf top1, top2;     // the low-pass's two stages
    u32 choke;       // the closed-hat count this open hat started under
    u32 open;        // this note is an open hat
    u32 fresh;
};
namespace {
using md::multiply;

enum { GAP, DEC, HPF, LPF, MTAL };

// The oscillators at middle C, Q16 octaves from it: the squares, then the pulse.
const unsigned partial_count = 6, squares = 5;
const i32 partials[partial_count] = {47494, 63627, 70465, 100869, 111195, 36978};
const u32 pulse_duty = 0x60000000u;                  // 3/8
const i32 square = 67314531;                        // at most six of them, about 0.19
const i32 after_gain = 1024, ceiling = md::half_scale / 1024;   // the output's gain, and where it stops at half scale
const i32 noise_level = 982000220;                  // the noise at MTAL 0, of its full eighth
// HPF 0, 16, ..., 127: the high-pass poles at 4.9 .. 11.8 kHz, and the level that makes up for them.
const i32 highpass_at[9] = {1075385698, 1142654238, 1218866578, 1300536154, 1382654297, 1467867314, 1556523066, 1657974566, 1748232258};
const i32 highpass_level[9] = {69381118, 86930938, 114799593, 160511876, 227305163, 340287538, 553272353, 998465481, 2147483647};
// The low-pass: a broad stage fixed at 3.3 kHz, then a resonant one that moves.
// LPF 0, 16, ..., 127: the broad stage's damping, the resonant stage's frequency (4.9 .. 9.2 kHz).
const i32 broad_at = 504658657, resonant_damping = 177167400;
const i32 broad_damping[9] = {430662480, 401860530, 429782531, 484981242, 539021639, 602079057, 688708895, 793467358, 779217775};
const i32 lowpass_level[9] = {1537906262, 1537906262, 1573728701, 1686280684, 1765752645, 1827805018, 1892038047, 2050830961, 2147483647};
const i32 resonant_at[9] = {740572770, 809404789, 866917096, 926829205, 994561238, 1068373561, 1143581935, 1221186426, 1305416187};
// The hold at DEC 0, 16, ..., 127, in samples: closed 5 .. 200 ms, open 30 .. 520 ms.
const i32 holds[9] = {220, 1102, 2205, 3528, 4851, 5512, 6174, 7497, 8820};
const i32 open_holds[9] = {1323, 5292, 8820, 12348, 15876, 17640, 18963, 20727, 22932};
// GAP: the fall after the hold, -60 dB from 85 ms (closed) or 100 .. 600 ms (open,
// longer with DEC) at GAP 0, to 400 ms closed or 2.3 s open at GAP 127.
const i32 fall_short = 778050, open_fall_short = 793416, open_fall_shortest_long = 962824;
const i32 fall_long_closed = 924488, fall_long_open = 1089872;
const i32 choke_time = 600000;                       // a choked open hat: -60 dB in about 6 ms

struct Block {
    i32 lines, noise, highpass, broad, resonant, fall_step, open;
    u32 hold;
    Block(const i32* p, bool opened) {
        // MTAL: the lines from an eighth to full, the noise from full to 0.22.
        const i32 made_up = multiply(md::table9(highpass_level, p[HPF]), md::table9(lowpass_level, p[LPF]));
        lines = multiply(multiply(md::span(p[MTAL], 0x10000000, md::one), made_up), square);
        noise = multiply(multiply(md::span(p[MTAL], md::one, 0x1c28f5c3), made_up), noise_level);
        highpass = md::table9(highpass_at, p[HPF]);
        broad = md::table9(broad_damping, p[LPF]);
        resonant = md::table9(resonant_at, p[LPF]);
        open = opened ? md::one : 0;   // TRX-OH, or TRX-CH
        const i32 shortest = md::span(open, fall_short, md::span(p[DEC], open_fall_short, open_fall_shortest_long));
        const i32 longest = md::span(open, fall_long_closed, fall_long_open);
        fall_step = md::decay(md::span(p[GAP], shortest, longest));
        hold = static_cast<u32>(md::span(open, md::table9(holds, p[DEC]), md::table9(open_holds, p[DEC])));
    }
};
}

// The block's stages, a loop each: out of line, so each has a hardware loop and all the
// registers to itself.
__attribute__((noinline)) inline void make_pulse(u32 phase, u32 increment, i32 lines, i32* band) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        band[i] = phase < pulse_duty ? lines : -lines;
        phase += increment;
    }
}
__attribute__((noinline)) inline void add_square(u32 phase, u32 increment, i32 lines, i32* band) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        band[i] += phase & 0x80000000u ? -lines : lines;
        phase += increment;
    }
}
// One chain for both: three high-pass poles...
__attribute__((noinline)) inline void add_noise_through_highpass(State& voice, i32 noise, i32 highpass, i32* band) {
    u32 seed = voice.seed;
    i32 low1 = voice.low1, low2 = voice.low2, low3 = voice.low3;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        i32 x = band[i] + multiply(md::noise_eighth(seed), noise);
        x -= md::smooth(low1, x, highpass);
        x -= md::smooth(low2, x, highpass);
        x -= md::smooth(low3, x, highpass);
        band[i] = x;
    }
    voice.seed = seed;
    voice.low1 = low1;
    voice.low2 = low2;
    voice.low3 = low3;
}
// ...then the low-pass, its broad stage first so the resonant one has headroom.
__attribute__((noinline)) inline void finish_block(State& voice, i32 broad, i32 resonant, i32 level, i32 slope, const i32* band, opxy::Output output) {
    md::Svf top1 = voice.top1, top2 = voice.top2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        top1.run(band[i], broad_at, broad);
        top2.run(top1.low, resonant, resonant_damping);
        output[i] = md::clamp(multiply(top2.low, level), -ceiling, ceiling) * after_gain;
        level += slope;
    }
    voice.top1 = top1;
    voice.top2 = top2;
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    for (unsigned k = 0; k < partial_count; ++k) voice.phase[k] = 0;
    voice.level = 0;
    voice.hold = 0;
    voice.seed = 0x6c078965u * owner;
    voice.low1 = voice.low2 = voice.low3 = 0;
    voice.top1.reset();
    voice.top2.reset();
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
        for (unsigned k = 0; k < partial_count; ++k) voice.phase[k] = static_cast<u32>(md::noise(voice.seed));
        voice.fresh = 0;
    }
    const bool choked = voice.open && voice.choke != shared.closed_hats;
    const i32 fall = choked ? md::decay(choke_time) : b.fall_step;
    if (choked) voice.hold = 0;

    u32 increment[partial_count];
    for (unsigned k = 0; k < partial_count; ++k) increment[k] = md::transpose(pitch[0], partials[k]);
    // The level after the hold, in a straight line across the block.
    i32 next = voice.level;
    if (voice.hold >= opxy::block_frames) {
        voice.hold -= opxy::block_frames;
    } else {
        if (voice.hold) {
            for (unsigned i = voice.hold; i < opxy::block_frames; ++i) next = multiply(next, fall);
        } else {
            next = multiply(next, md::per_block(fall));
        }
        voice.hold = 0;
    }
    i32 level = voice.level;
    const i32 level_slope = (next - level) / 64;
    voice.level = next;
    i32 band[opxy::block_frames];
    make_pulse(voice.phase[squares], increment[squares], b.lines, band);
    voice.phase[squares] += increment[squares] << 6;
    for (unsigned k = 0; k < squares; ++k) {
        add_square(voice.phase[k], increment[k], b.lines, band);
        voice.phase[k] += increment[k] << 6;
    }
    add_noise_through_highpass(voice, b.noise, b.highpass, band);
    finish_block(voice, b.broad, b.resonant, level, level_slope, band, output);
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
