#pragma once
// PI-MA: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

// A Machinedrum PI-MA-style maraca: seeds rattling in a shell, written from
// scratch in fixed point and fitted to the Machinedrum's own
// (reference/fit_pi_ma.py). Its page:
//   GRNS  DEC   GLEN  -
//   SIZE  HARD
// The model is a shaker's: the shake's energy falls away over DEC's time
// (-60 dB in 0.2 to 4.6 s); while it lasts, seeds hit the shell at random,
// GRNS of them a second (200 to 16,000), each a click of noise ringing GLEN's
// time (4 to 120 ms); the shell colours them, a band near 3.4 kHz that SIZE
// raises and HARD lowers. As measured: more and longer grains are smoother
// and louder, SIZE moves the brightness from 3.2 to 6 kHz and HARD from 5.2
// down to 2.3 kHz.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace pi_ma {
struct State {
    i32 shake;      // the shake's energy, Q31
    i32 sound;      // the seeds' ringing, Q31
    u32 seed;
    md::Svf shell;
    u32 fresh;
};
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
const i32 shell_damping = 0x20000000;
// Each grain adds the shake's energy times this to the ringing, a random share of it.
const i32 grain_level = 0x14000000;
const i32 out_level = 0x60000000;   // 8 dB over the emulator's main mix (with render's gain), to sit in a kit
// HARD: a harder shake, 5 dB louder at 127 than at 0.
const i32 hard_soft = 0x47cf267e;

struct Block {
    u32 chance;
    i32 shake_step, grain_step, tune, amount;
    explicit Block(const i32* p) {
        chance = md::transpose(grain_rate, multiply(md::positive(p[GRNS]), grain_span));
        shake_step = md::decay(md::table9(decay_times, p[DEC]));
        grain_step = md::decay(md::table9(grain_times, p[GLEN]));
        tune = md::cutoff(shell_centre + md::table9(size_offsets, p[SIZE]) + md::table9(hard_offsets, p[HARD]));
        amount = multiply(multiply(grain_level, md::table9(many_soft, p[GRNS])), md::table9(long_soft, p[GLEN]));
        amount = multiply(amount, md::span(p[HARD], hard_soft, md::one));
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.shake = voice.sound = 0;
    voice.seed = 0x2f6b3a91u * owner;
    voice.shell.reset();
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.fresh = 0;
        voice.shake = md::one;
        voice.sound = 0;
        voice.shell.reset();
    }
    if (voice.shake < 0x00010000 && voice.sound < 0x00010000) {
        for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = 0;
        return;
    }
    const i32 shake_next = multiply(voice.shake, md::per_block(b.shake_step));
    const i32 shake_slope = (shake_next - voice.shake) / 64;
    i32 shake = voice.shake;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        md::noise(voice.seed);
        // A seed hits: the ringing takes a random share of the shake's energy.
        if (voice.seed < b.chance) {
            md::noise(voice.seed);
            const i32 share = static_cast<i32>(voice.seed >> 1);
            voice.sound = md::add(voice.sound, multiply(multiply(shake, b.amount), share) * 8);
        }
        voice.sound = multiply(voice.sound, b.grain_step);
        md::noise(voice.seed);
        const i32 n = static_cast<i32>(voice.seed >> 3) - 0x10000000;   // an eighth of full scale
        voice.shell.run(multiply(n, voice.sound), b.tune, shell_damping);
        output[i] = md::finish(md::gain(multiply(voice.shell.band, out_level), 3));
        shake += shake_slope;
    }
    voice.shake = shake_next;
}
}
}
