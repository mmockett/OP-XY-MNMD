#pragma once
// TRX-B2: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MD Kick 2: a Machinedrum TRX-B2-style kick, written from scratch in fixed
// point and fitted to the Machinedrum's own (reference/fit_b2.py). Its page:
//   PTCH  DEC   RAMP  HOLD
//   TICK  NOIS  DIRT  DIST
// As measured: one sine, its base 10 Hz at PTCH 0 to 106 Hz at 127, plus RAMP's
// extra Hz (up to 457, whatever PTCH is) falling with a fixed 38 ms time
// constant. The level swells about 4 dB over the first 20 ms, holds for HOLD's
// time (none to 9.5 s), then falls 60 dB in DEC's (55 ms to 20 s; none at 127).
// TICK opens it with a downward spike, twice the body's height from TICK 40 (1.7
// x here, under the ceiling), held 4 to 15 samples as TICK rises, the body
// starting downward with it, as the emulator's; NOIS adds a burst of noise above 1 kHz falling in 70 ms.
// DIST is a hard clip into a 30 Hz high-pass, harder to DIST 127 (the 3rd
// harmonic -31 dB at DIST 4, -12 at 16, -10 from 32); DIRT rounds the sine to
// coarser and coarser steps, down to three levels at 127 (the third harmonic
// gone, the fifth 14 dB down). As the Machinedrum's, both shape the sine before
// the envelope, measured with the harmonics steady from 30 to 400 ms: a fading
// kick keeps its shape, falls at DEC's rate whatever DIRT and DIST are, and is
// not gated to silence. TICK and NOIS are added after them. Lifted 7 dB over the
// Machinedrum's, to stand with TRX-BD. The key moves the pitch.
namespace md {
namespace trx_b2 {
struct State {
    u32 phase;
    i32 level;      // the body's envelope after the hold, Q31
    i32 swell;      // the opening swell, Q31 (rises to one)
    u32 hold;       // samples of hold still to run
    i32 ramp;       // RAMP's sweep, Q31
    i32 tick;       // TICK's spike, Q31
    u32 tick_hold;  // samples of the spike's hold still to run
    i32 burst;      // NOIS's envelope, Q31
    i32 hiss;       // the burst's high-pass state
    i32 droop;      // DIST's high-pass state
    u32 seed;
    u32 fresh;
};
namespace {
using md::multiply;

enum { PTCH, DEC, RAMP, HOLD, TICK, NOIS, DIRT, DIST };

const i32 base_octaves[9] = {-308637, -248952, -209947, -180202, -155902, -134917, -116383, -99641, -85066};
const i32 ramp_hz[9] = {0, 58894065, 119532380, 183146179, 236499687, 286775109, 356032067, 404152828, 468997862};
const i32 ramp_fall = 884664;                 // the sweep: a 38 ms time constant
const i32 decay_times[9] = {736891, 727880, 793416, 840763, 943675, 1049458, 1155408, 1294363, 1311602};
const i32 holds[9] = {0, 328, 1025, 2871, 7792, 20917, 56598, 151748, 389624};   // samples
const i32 swell_start = 0x4ccccccd;           // the opening, 4.4 dB under the settled level
const i32 swell_time = 737234;                // rising with an 8 ms time constant
const i32 tick_most = 0x6ccccccd;             // 1.7 x the body's height: twice would pass the ceiling
const i32 tick_time = 351113;                 // gone in 1 ms
const i32 tick_full = 676372803;              // TICK 40: the spike's full height
const i32 tick_rise = 1704565146;             // 127 / 40, a quarter of it
const i32 burst_most = 0x78000000;
const i32 burst_time = 759693;                // -60 dB in 70 ms
const i32 burst_highpass = 285167595;         // above 1 kHz
// DIST: the clip's threshold, at half (1.23 x the body's height), into a 30 Hz
// high-pass that droops each clipped half cycle: the Machinedrum's crest, 7 dB.
const i32 clip_threshold = 0x16800000;
const i32 clip_highpass = 9170000;
// DIRT: the rounding step at DIRT 0, 16, ..., 127, Q16 log2 of the step in Q31
// units: 2^19.5 up to the body's own height, 2^28.2: three levels, the third harmonic gone.
const i32 dirt_steps[9] = {1277952, 1323827, 1376256, 1507328, 1605632, 1677722, 1736704, 1802240, 1848115};
// The body peaks at -10.8 dB of full scale (the Machinedrum's -17.9, lifted 7 dB); half here.
const i32 body_level = 0x125a0000;

// DIRT's rounding to a step of 2^(log2 step), any fraction of an octave: scaled
// down by the fraction, rounded to a power of two, scaled back.
struct Crush {
    u32 mask, half;
    i32 down, up;
    void prepare(i32 log2_step) {
        const u32 l = static_cast<u32>(md::positive(log2_step));
        const u32 whole = l >> 16, index = (l >> 8) & 255u;
        mask = ~((1u << whole) - 1u);
        half = 1u << (whole - 1);
        down = exp2_table[256 - index];   // 2^(1 - f), Q30
        up = exp2_table[index];           // 2^f, Q30
    }
    i32 run(i32 x) const {
        const u32 u = static_cast<u32>(multiply(x, down)) + half;   // x 2^-f, plus half a step
        return multiply(static_cast<i32>(u & mask), up) * 2;
    }
};

struct Block {
    i32 base, ramp, decay_step, tick, burst, drive, dirt;
    u32 hold, tick_hold;
    Crush crush;
    explicit Block(const i32* p) {
        base = md::table9(base_octaves, p[PTCH]);
        ramp = md::table9(ramp_hz, p[RAMP]);
        decay_step = p[DEC] > 0x7e000000 ? md::one : md::decay(md::table9(decay_times, p[DEC]));   // none at 127
        hold = static_cast<u32>(md::table9(holds, p[HOLD]));
        const i32 t = md::positive(p[TICK]);
        tick = t < tick_full ? multiply(multiply(t, tick_rise) * 4, tick_most) : tick_most;
        tick_hold = 4 + static_cast<u32>(multiply(t, 12));   // 4 .. 15 samples
        burst = multiply(md::positive(p[NOIS]), burst_most);
        // DIST: a gain rising in a line from 1 to 4 over its first quarter, then on to 32. Q26.
        const i32 d = md::positive(p[DIST]), over = d - 0x20000000;
        drive = !d ? 0 : over < 0 ? 0x04000000 + (d >> 3) * 3 : 0x10000000 + over + (over >> 3) + (over >> 5);
        dirt = p[DIRT] > 0 ? 1 : 0;
        crush.prepare(md::table9(dirt_steps, p[DIRT]));
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase = 0;
    voice.level = voice.swell = voice.ramp = voice.tick = voice.burst = voice.hiss = voice.droop = 0;
    voice.hold = voice.tick_hold = 0;
    voice.seed = 0x1b873593u * owner;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.phase = 0;
        voice.level = voice.ramp = voice.tick = voice.burst = md::one;
        voice.swell = swell_start;
        voice.hold = b.hold;
        voice.tick_hold = b.tick_hold;
        voice.hiss = voice.droop = 0;
        voice.fresh = 0;
    }
    // The pitch: the base plus the sweep in Hz, linear between the block's ends.
    const u32 base = md::transpose(pitch[0], b.base);
    const i32 ramp_next = multiply(voice.ramp, md::per_block(md::decay(ramp_fall)));
    const u32 now = base + md::scale(pitch[0], multiply(voice.ramp, b.ramp));
    const u32 next = base + md::scale(pitch[0], multiply(ramp_next, b.ramp));
    u32 increment = now;
    const i32 slope = (static_cast<i32>(next) - static_cast<i32>(now)) / 64;
    voice.ramp = ramp_next;
    const i32 swell_step = md::decay(swell_time);
    const i32 tick_step = md::decay(tick_time);
    const i32 burst_step = md::decay(burst_time);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        // The sine at its full height: DIST and DIRT shape it there, before the
        // envelope, so the shape holds however far the hit has fallen.
        i32 x = -multiply(md::sine(voice.phase), body_level);
        voice.phase += increment;
        increment += static_cast<u32>(slope);
        if (b.drive) {
            const i32 hot = md::gain(multiply(x, b.drive), 5);
            const i32 clipped = md::clamp(hot, -clip_threshold, clip_threshold);
            voice.droop += multiply(clipped - voice.droop, clip_highpass);
            x = clipped - voice.droop;
        }
        if (b.dirt) x = b.crush.run(x);
        x = multiply(x, multiply(voice.level, voice.swell));
        // TICK: a downward spike.
        x -= multiply(voice.tick, multiply(b.tick, body_level)) * 2;
        // NOIS: a burst above 1 kHz.
        if (b.burst) {
            const i32 white = md::noise_eighth(voice.seed);
            md::smooth(voice.hiss, white, burst_highpass);
            x += multiply(multiply(white - voice.hiss, voice.burst), b.burst);
        }
        output[i] = md::finish(md::gain(x, 1));

        voice.swell += multiply(md::one - voice.swell, md::one - swell_step);
        if (voice.hold) --voice.hold;
        else voice.level = multiply(voice.level, b.decay_step);
        if (voice.tick_hold) --voice.tick_hold;
        else voice.tick = multiply(voice.tick, tick_step);
        voice.burst = multiply(voice.burst, burst_step);
    }
}
}
}
