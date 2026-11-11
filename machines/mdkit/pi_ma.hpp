#pragma once
// PI-MA: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_modal.hpp"

// A Machinedrum PI-MA-style maraca: seeds rattling in a shell, written from
// scratch in fixed point and fitted to the Machinedrum's own
// (reference/fit_pi_ma.py). Its page:
//   GRNS  DEC   GLEN  -
//   SIZE  HARD
// The model is a shaker's: the shake's energy falls away over DEC's time
// (-60 dB in 0.2 to 4.6 s); while it lasts, seeds hit the shell at random,
// GRNS of them a second (200 to 16,000), each a click of noise ringing GLEN's
// time (4 to 120 ms); the shell colours them, a broad band near 3.4 kHz that
// SIZE raises and HARD lowers, with the body of the gourd under it, from 300 Hz
// up. The shake opens on its first seed. As measured: more and longer grains are smoother
// and louder, SIZE moves the brightness from 3.2 to 6 kHz and HARD from 5.2
// down to 2.3 kHz.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace pi_ma {
namespace {
using md::multiply;

enum { GRNS, DEC, GLEN, NONE, SIZE, HARD };

// GRNS: 200 grains a second at GRNS 0, 6.32 octaves more at 127 (a chance per sample,
// Q32), each softer the more there are (Q31 at GRNS 0, 16, ..., 127), so the whole
// grows only 10 dB.
const u32 grain_rate = 19478310;
const i32 grain_span = 414187;
const i32 many_soft[9] = {2147483647, 1573728702, 1016084592, 727662285, 486328662, 360520418,
                          276649485, 207457847, 145187713};
// DEC: the shake falls 60 dB in 0.2 .. 4.6 s; GLEN: each grain in 4 .. 120 ms (Q16 log2
// samples at 0, 16, ..., 127).
const i32 decay_times[9] = {858952, 890765, 929101, 962824, 993504, 1020133, 1076658, 1121096, 1155408};
const i32 grain_times[9] = {489076, 527412, 575710, 620148, 658484, 696820, 738595, 776931, 810654};
// Longer grains are softer, 18 dB at the top, so the whole grows only 4 dB.
const i32 long_soft[9] = {2147483647, 1657409659, 1279174713, 987256190, 761955951, 588071139, 453868315, 350291715,
                          270352174};
// The shell: a band near 1.8 kHz, heard near 3.4 kHz (Q16 octaves above 20 Hz), moved by SIZE and HARD (Q16
// octaves at 0, 16, ..., 127).
const i32 shell_centre = 425450;
const i32 size_offsets[9] = {-3691, -12081, -6134, -2755, 0, 10306, 23295, 35901, 55819};
const i32 hard_offsets[9] = {37966, 24525, 15463, 5491, 0, -14377, -19223, -33126, -39397};
// How wide the band is, half the damping at SIZE 0, 16, ..., 127: the Machinedrum's
// narrows as it rises; and the body under it (Q31), most at the smallest SIZE.
const i32 shell_damping[9] = {0x38000000, 0x38000000, 0x38000000, 0x38000000, 0x38000000, 0x33000000, 0x29000000,
                              0x1f000000, 0x18000000};
const i32 body_levels[9] = {0x4ccccccd, 0x46666666, 0x40000000, 0x39999999, 0x30000000, 0x26666666, 0x1999999a,
                            0x0ccccccd, 0x06666666};
const i32 base_corner = 0x0559d4f5;   // a one-pole at 300 Hz
// Each grain adds the shake's energy times this to the ringing, a random share of it.
const i32 grain_level = 0x14000000;
const i32 out_level = 0x60000000;   // 8 dB over the emulator's main mix (with render's gain), to sit in a kit
// HARD: a harder shake, 5 dB louder at 127 than at 0.
const i32 hard_soft = 0x47cf267e;

}
struct Block {
    u32 chance;
    i32 shake_fall, grain_step, tune, amount, damping, body;
    void plan(const i32* p) {
        chance = md::transpose(grain_rate, multiply(md::positive(p[GRNS]), grain_span));
        shake_fall = md::per_block(md::decay(md::table9(decay_times, p[DEC])));
        grain_step = md::decay(md::table9(grain_times, p[GLEN]));
        damping = md::table9(shell_damping, p[SIZE]);
        body = md::table9(body_levels, p[SIZE]);
        tune = md::cutoff(shell_centre + md::table9(size_offsets, p[SIZE]) + md::table9(hard_offsets, p[HARD]));
        amount = multiply(multiply(grain_level, md::table9(many_soft, p[GRNS])), md::table9(long_soft, p[GLEN]));
        amount = multiply(amount, md::span(p[HARD], hard_soft, md::one));
    }
};
struct State {
    i32 shake;      // the shake's energy, Q31
    i32 sound;      // the seeds' ringing, Q31
    u32 seed;
    md::Svf shell;
    i32 base;       // the shell's low output under 300 Hz, taken off it
    u32 fresh;
    u32 stale;      // the block's figures want working out, whatever the controls read
    i32 seen[8];    // the controls they were worked out from
    Block block;
};

// The seeds in the shell: out of line and on locals, as `output` might otherwise be the voice
// and every field would be read and written back each sample.
__attribute__((noinline)) inline void shake_block(State& voice, const Block& b, i32 shake_slope, opxy::Output output) {
    u32 seed = modal::live_seed(voice.seed);
    i32 shake = voice.shake, sound = voice.sound, base = voice.base;
    md::Svf shell = voice.shell;
    const u32 chance = b.chance;
    const i32 amount = b.amount, grain_step = b.grain_step, tune = b.tune, damping = b.damping, body_level = b.body;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        modal::noise_on(seed);
        // A seed hits: the ringing takes a random share of the shake's energy.
        if (seed < chance) {
            modal::noise_on(seed);
            const i32 share = static_cast<i32>(seed >> 1);
            sound = md::add(sound, multiply(multiply(shake, amount), share) * 8);
        }
        sound = multiply(sound, grain_step);
        modal::noise_on(seed);
        const i32 n = static_cast<i32>(seed >> 3) - 0x10000000;   // an eighth of full scale
        shell.step(multiply(n, sound), tune, damping);
        const i32 under = shell.low - md::smooth(base, shell.low, base_corner);
        // md::finish(md::gain(x, 3)), with no branches in the loop.
        output[i] = md::clamp(multiply(shell.band + multiply(under, body_level) * 2, out_level), -half_scale / 8, half_scale / 8) * 8;
        shake += shake_slope;
    }
    voice.seed = seed;
    voice.sound = sound;
    voice.base = base;
    voice.shell = shell;
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.shake = voice.sound = 0;
    voice.seed = 0x2f6b3a91u * owner;
    voice.shell.reset();
    voice.base = 0;
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
        voice.fresh = 0;
        voice.shake = md::one;
        voice.sound = md::gain(b.amount, 3);   // the first seed, at the shake's full energy
        voice.shell.reset();
        voice.base = 0;
    }
    if (voice.shake < 0x00010000 && voice.sound < 0x00010000) {
        for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = 0;
        return;
    }
    const i32 shake_next = multiply(voice.shake, b.shake_fall);
    shake_block(voice, b, (shake_next - voice.shake) / 64, output);
    voice.shake = shake_next;
}
}
}
