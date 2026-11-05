#pragma once
// TRX-MA: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Maracas: a Machinedrum TRX-MA-style shaker, written from scratch in fixed
// point and fitted to the Machinedrum's own (reference/fit_ma.py). Its page:
//   ATT   SUS   REV   DAMP
//   RATL  RTYP  TONE  HARD
// As measured: a burst of grains of noise in a band from about 5 to 15 kHz,
// flickering 6 to 9 dB from millisecond to millisecond, gated off in 10 ms. Below
// the band it falls 24 to 30 dB an octave, with a slight knee at its lower edge.
// SUS is the burst's length, 10 to 190 ms; ATT a rise to it from 35 dB down, half a
// millisecond at 0 (so the first grain fades in) to 220 ms; REV a second shake, the
// return, 50 to 170 ms after the first; TONE moves the band up an octave; HARD closes
// it from above and sharpens its knee; RATL thickens the grains, leaves a faint
// rattle after the gate, 40 dB down at 127, and lets their full width through under
// the band, 6 dB under it at 127 and in proportion, rising over the first 50 ms: none
// at 0, where the Machinedrum has nothing under 2 kHz. Each grain starts ahead of the
// band's filters, so its edge is no click. DAMP darkens the band as the burst goes on.
// RTYP has no measurable effect on the Machinedrum; here it lengthens the grains,
// from sand to seeds. The Machinedrum's maracas peak 33 dB down; here they are lifted
// about 25 dB, to stand with its hats. Like the clap, they have no pitch: keys play them alike.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace trx_ma {
namespace {
using md::multiply;

enum { ATT, SUS, REV, DAMP, RATL, RTYP, TONE, HARD };

const i32 attack_rate[9] = {0x20000000, 8711894, 4355947, 2529260, 1781978, 1507828, 1306784, 1053128, 890989};
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
const i32 width_most = 0x0e000000;             // the grains' full width under the band, at RATL 127
// The band: two high-pass poles, a state-variable high-pass that peaks a little
// at the band's lower knee, as the Machinedrum's, and two low-pass poles. Both
// pairs at a position on this table, 3 .. 20 kHz over 2.75 octaves, and the knee
// at 0.7 of it:
const i32 band_poles[9] = {746927805, 899075334, 1068597906, 1251001836, 1438776724, 1621552923, 1787288339, 1924682131, 2026376759};
const i32 band_knee[9] = {321744866, 407370227, 515055050, 649721415, 816576347, 1020144662, 1262044541, 1536332324, 1803941934};
// TONE's steps: the position of all of them, which moves the band up an octave.
const i32 tone_position[9] = {622930149, 756416955, 845485857, 934434204, 1023551531, 1112446377, 1201449290, 1290521488, 1379484531};
// HARD's steps: the pairs, moved from where they are at 64, and the knee's damping.
// Low, the band opens from both sides; high, the knee sharpens.
const i32 hard_highpass[9] = {-1297231298, -647352839, -169998605, -103517970, 0, 0, 76609771, 76609771, 76609771};
const i32 hard_lowpass[9] = {1037269792, 244092888, -169998605, -103517970, 0, 0, 76609771, 76609771, 76609771};
const i32 knee_damping[9] = {1277752771, 1213328261, 1041529569, 848256041, 633507676, 536870912, 429496730, 386547057, 365072220};
// What the band loses at those steps, made up in the level.
const i32 tone_level[9] = {1591951620, 1666977987, 1502899238, 1610385550, 1848970001, 1913946814, 2004148347, 2147483647, 2098600950};
const i32 hard_level[9] = {249419190, 648529695, 1610385556, 1981207058, 2147483647, 2147483647, 2147483647, 2147483647, 2147483647};
const i32 damp_most = 0x1745d174;              // DAMP's reach, half an octave of the table
const i32 damp_step = 0x01db6db7;              // the darkening's pace, per block: all of it in 100 ms
// The burst's level: the Machinedrum's lifted 26 dB, its grains' peaks near
// the ceiling: they are spiky, and quieter would not stand with the kit.
const i32 burst_level = 1656822274;
const i32 after_gain = 32, ceiling = md::half_scale / 32;   // the output's gain, and where it stops at half scale

}
struct Block {
    i32 rate, grain_step, tail, position, position_low, position_high, knee_damp, level, damp, width, tail_block, tail_steps, release_steps;
    u32 length, again, chance;
    void plan(const i32* p) {
        rate = md::table9(attack_rate, p[ATT]);
        length = static_cast<u32>(md::table9(lengths, p[SUS]));
        again = static_cast<u32>(md::table9(returns, p[REV]));
        const u32 r = static_cast<u32>(md::positive(p[RATL]));
        chance = sparse + static_cast<u32>(multiply(static_cast<i32>(r >> 1), static_cast<i32>((dense - sparse) >> 1))) * 4;
        grain_step = md::decay(md::span(p[RTYP], grain_short, grain_long));
        tail = multiply(md::squared(p[RATL]), tail_most);
        position = md::table9(tone_position, p[TONE]);
        position_low = md::add(position, md::table9(hard_highpass, p[HARD]));
        position_high = md::add(position, md::table9(hard_lowpass, p[HARD]));
        knee_damp = md::table9(knee_damping, p[HARD]);
        level = multiply(multiply(md::table9(tone_level, p[TONE]), md::table9(hard_level, p[HARD])), burst_level);
        damp = multiply(md::positive(p[DAMP]), damp_most);
        width = multiply(md::positive(p[RATL]), width_most);
        const i32 tail_step = md::decay(tail_time);
        tail_block = md::per_block(tail_step);
        const i32 tail2 = multiply(tail_step, tail_step);
        tail_steps = multiply(tail2, tail2);
        const i32 release2 = md::decay(release_time), release = multiply(release2, release2);
        release_steps = multiply(release, release);
    }
};

struct State {
    i32 rise;        // ATT's envelope, Q31 (rises to one)
    i32 gate;        // the burst's gate, Q31
    u32 left;        // samples of the burst still to run
    u32 again;       // samples until REV's second shake (0: none)
    i32 grain;       // the current grains' level, Q31
    i32 tail;        // RATL's rattle after the gate, Q31
    i32 dark;        // DAMP's darkening so far, Q31 of the most
    i32 low1, low2;  // the band's high-pass poles
    md::Svf knee;    // its lower knee
    i32 top1, top2;  // its low-pass poles
    u32 seed;
    u32 fresh;
    i32 seen_dark, highpass, lowpass, knee_at, width;   // the band, at seen_dark
    u32 stale;        // the block's figures want working out, whatever the controls read
    i32 seen[8];      // the controls they were worked out from
    Block block;
};

// Whether a sound is still to come however quiet it is now: the burst, or REV's second shake.
inline bool waiting(const State& voice) { return voice.left != 0 || voice.again != 0; }
inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void clear(State& voice) {
    voice.low1 = voice.low2 = voice.top1 = voice.top2 = 0;
    voice.knee.reset();
}
// The block's stages, a loop each: out of line, so each has a hardware loop and all the
// registers to itself, and on locals, as the buffers might otherwise be the voice.
// A grain starts now and then; each rings briefly.
__attribute__((noinline)) inline void run_grains(State& voice, u32 chance, i32 step, i32* x) {
    u32 seed = voice.seed;
    i32 grain = voice.grain;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 white = md::noise_eighth(seed);
        if ((seed << 16) < chance) grain = md::one;
        x[i] = multiply(white, grain);
        grain = multiply(grain, step);
    }
    voice.seed = seed;
    voice.grain = grain;
}
// The grains through the band, their full width added under the high-passes...
__attribute__((noinline)) inline void run_band(State& voice, i32 highpass, i32 knee, i32 damping, i32 width, i32* x) {
    i32 low1 = voice.low1, low2 = voice.low2;
    md::Svf svf = voice.knee;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 grain = x[i];
        i32 band = grain - md::smooth(low1, grain, highpass);
        band -= md::smooth(low2, band, highpass);
        svf.step(band, knee, damping);
        x[i] = svf.high + multiply(grain, width);
    }
    voice.low1 = low1;
    voice.low2 = low2;
    voice.knee = svf;
}
// ...on to the low-pass, in a line where the envelope is one.
__attribute__((noinline)) inline void run_top(State& voice, i32 lowpass, i32* x) {
    i32 top1 = voice.top1, top2 = voice.top2;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        top1 += multiply(x[i] - top1, lowpass);
        top2 += multiply(top1 - top2, lowpass);
        x[i] = top2;
    }
    voice.top1 = top1;
    voice.top2 = top2;
}
__attribute__((noinline)) inline void run_line(i32 envelope, i32 slope, opxy::Output x) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        x[i] = md::clamp(multiply(x[i], envelope), -ceiling, ceiling) * after_gain;
        envelope += slope;
    }
}
// Rising or gating: the envelope moves every fourth sample, by its steps to the fourth power.
__attribute__((noinline)) inline void run_moving(State& voice, const Block& b, opxy::Output x) {
    const i32 rate = md::gain(b.rate, 2), level = b.level;
    i32 rise = voice.rise, gate = voice.gate, tail = voice.tail;
    u32 left = voice.left, again = voice.again;
    i32 envelope = multiply(multiply(rise, gate), level) + multiply(tail, level);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        x[i] = md::clamp(multiply(x[i], envelope), -ceiling, ceiling) * after_gain;
        if (rise == md::one && left) --left;
        if (again && !--again) {
            // REV: the return shake.
            gate = md::one;
            left = b.length;
        }
        if ((i & 3) == 3) {
            if (rise < md::one) {
                const i32 add = multiply(rise, rate);
                rise = rise > md::one - add ? md::one : rise + add;
            } else if (!left) {
                if (gate == md::one) tail = b.tail;
                gate = multiply(gate, b.release_steps);
            }
            tail = multiply(tail, b.tail_steps);
            envelope = multiply(multiply(rise, gate), level) + multiply(tail, level);
        }
    }
    voice.rise = rise; voice.gate = gate; voice.tail = tail;
    voice.left = left; voice.again = again;
}
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.rise = voice.gate = voice.grain = voice.tail = voice.dark = 0;
    voice.left = voice.again = 0;
    clear(voice);
    voice.seed = 0x68e31da4u * owner;
    voice.stale = 1;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch) {
    // Only when a control has moved.
    u32 moved = voice.stale;
    for (unsigned c = 0; c < 8; ++c) moved |= static_cast<u32>(parameters[c] ^ voice.seen[c]);
    if (moved) {
        voice.stale = 0;
        for (unsigned c = 0; c < 8; ++c) voice.seen[c] = parameters[c];
        voice.block.plan(parameters);
    }
    const Block& b = voice.block;
    if (voice.fresh) {
        voice.rise = attack_floor;
        voice.gate = md::one;
        voice.left = b.length;
        voice.again = b.again;
        voice.grain = voice.tail = voice.dark = 0;
        clear(voice);
        voice.fresh = 0;
    }
    // The band for this block: TONE, HARD and DAMP's darkening so far; the same until one moves.
    if (moved || voice.dark != voice.seen_dark) {
        voice.seen_dark = voice.dark;
        const i32 darkened = multiply(voice.dark, b.damp);
        voice.highpass = md::table9(band_poles, md::clamp(b.position_low - darkened, 0, md::one));
        voice.lowpass = md::table9(band_poles, md::clamp(b.position_high - darkened, 0, md::one));
        voice.knee_at = md::table9(band_knee, md::clamp(b.position - darkened, 0, md::one));
        // The grains' width under the band, all of it by half of the darkening's 100 ms.
        voice.width = multiply(b.width, voice.dark < 0x40000000 ? voice.dark * 2 : md::one);
    }
    const i32 highpass = voice.highpass, lowpass = voice.lowpass, knee = voice.knee_at, width = voice.width;

    run_grains(voice, b.chance, b.grain_step, output);
    run_band(voice, highpass, knee, b.knee_damp, width, output);
    run_top(voice, lowpass, output);
    if (voice.rise == md::one && voice.left >= opxy::block_frames &&
        (!voice.again || voice.again > opxy::block_frames)) {
        // Shaking: the attack is over and the burst runs through the block, so
        // the gate holds and only the rattle's tail falls, in a straight line.
        const i32 tail_next = multiply(voice.tail, b.tail_block);
        const i32 envelope = multiply(voice.gate, b.level) + multiply(voice.tail, b.level);
        run_line(envelope, (multiply(voice.gate, b.level) + multiply(tail_next, b.level) - envelope) / 64, output);
        voice.tail = tail_next;
        voice.left -= opxy::block_frames;
        if (voice.again) voice.again -= opxy::block_frames;
    } else {
        run_moving(voice, b, output);
    }
    voice.dark = voice.dark < md::one - damp_step ? voice.dark + damp_step : md::one;
}
}
}
