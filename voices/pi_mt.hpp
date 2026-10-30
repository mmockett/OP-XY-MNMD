#pragma once
// PI-MT: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_modal.hpp"

// A Machinedrum PI-MT-style tom: a struck head over a resonant shell, written
// from scratch in fixed point and fitted to the Machinedrum's own
// (reference/fit_pi_mt.py). Its page:
//   PTCH  DEC   HARD  HAMR
//   TUNE  DAMP  SIZE  POS
// As measured: the head rings at a fundamental from 69 Hz (PTCH 0) to 276 Hz
// (127), two octaves, with modes at 1.1 and 1.99 times it, and one more that
// TUNE places from 1.01 to 2.09 times and POS (where it is struck) brings up
// by 10 dB. Under it the shell rings as a tube, harmonics of a pitch that SIZE
// sets and PTCH does not move: 941 Hz at SIZE 0 down to 190 Hz at 127, its
// period growing in step with SIZE. DEC sets the ring, -60 dB in 29 ms per
// step for the head and 1.43 times that for the shell; DAMP shortens the
// shell, its upper harmonics most. Above the tube, six faint modes from 3.1 to
// 7 kHz (which SIZE moves by a quarter of the shell's move, and PTCH not at
// all) ring -60 dB in 90 to 190 ms whatever DEC is, as the Machinedrum's do, and
// DAMP cuts them to 25 ms. The beater adds a short burst near 1.8 kHz;
// HAMR (a heavier beater) lowers and softens it and the shell's upper
// harmonics. HARD barely moves the Machinedrum's, so it does nothing here.
namespace md {
namespace pi_mt {
const unsigned head_count = 4, tube_count = 9, upper_count = 6, mode_count = head_count + tube_count + upper_count;
struct State {
    modal::Mode modes[mode_count];
    i32 strike;      // the beater's burst envelope, Q31
    u32 seed;
    md::Svf band;    // the burst's band
    u32 opened;      // the first block, with the upper modes swelling in, is over
    u32 fresh;
};
namespace {
using md::multiply;

enum { PTCH, DEC, HARD, HAMR, TUNE, DAMP, SIZE, POS };

// The head's fundamental, 69 Hz * 4^PTCH, Q16 octaves from middle C, and its modes:
// 1.1 and 1.99 times, and TUNE's at 1.01 .. 2.09 times (at TUNE 0, 16, ..., 127).
const i32 head_low = -126017, head_span = 131072;
const i32 head_ratios[3] = {0, 9011, 65062};
const i32 tune_ratios[9] = {941, 12890, 23497, 33033, 41695, 49630, 56950, 63744, 69698};
const i32 head_levels[3] = {1936109276, 1114114600, 884972684};   // -0.9, -5.7, -7.7 dB under the shell
const i32 pos_low = 423574557, pos_high = 1370660400;           // TUNE's mode: -14 .. -4 dB with POS
// The shell: its pitch at SIZE 0, 16, ..., 127 (Q16 octaves from middle C), its
// harmonics and their levels, and how much shorter each rings than the first.
const i32 tube[9] = {120996, 82773, 55630, 34566, 17350, 2791, -9822, -20949, -30310};
const i32 harmonics[tube_count] = {0, 65536, 103872, 131072, 152170, 169408, 183983, 196608, 207744};
const i32 tube_levels[tube_count] = {2147483647, 854928639, 1293986858, 797865829, 641106036,
                                     558380014, 816450512, 428479319, 453868315};
const i32 tube_longer = 33800, harmonic_shorter = 22938;   // +0.52 octave; -0.35 per harmonic
// Above the tube: 3143, 3452, 3782, 4395, 5338 and 7000 Hz at SIZE 64 (Q16 octaves
// from middle C), their levels at HAMR 0 (Q31; HAMR 64 is 2 dB under it, 127 is 12 dB), and
// their ring, -60 dB in 190, 175, 160, 145, 120 and 90 ms (Q16 log2 samples); DAMP
// 127 shortens them 2.2 octaves.
const i32 upper[upper_count] = {235048, 243914, 252546, 266749, 285127, 310756};
const i32 upper_levels[upper_count] = {992509047, 2147483647, 884574619, 1401956292, 702642596, 497432864};
const i32 upper_times[upper_count] = {854102, 846327, 837854, 828546, 810654, 783454};
const i32 upper_soft = 0x60000000, upper_damp = 144179;
// DEC: the head falls 60 dB in 40 ms at DEC 0, then 29.3 ms per step (Q16 log2 samples).
const i32 decay_times[9] = {706782, 939494, 1005030, 1043366, 1070566, 1091664, 1108902, 1123476, 1135360};
// DAMP: how much of its shortening the shell takes, at DAMP 0, 16, ..., 127; then
// -2.2 octaves for the first harmonic and 0.73 more for each one after.
const i32 damp_share[9] = {0, 21474836, 150323855, 365072220, 794568950, 1395864371, 2147483647, 2147483647,
                           2147483647};
const i32 damp_first = 144179, damp_each = 47841;
// HAMR: each harmonic after the first 1.5 dB softer at HAMR 127 (Q31 per harmonic).
const i32 hamr_soft = 0x6bb2d604;
// The beater's burst: noise in a band near 1.8 kHz, falling 60 dB in 40 ms; HAMR
// lowers it 0.6 octaves and softens it 6 dB (Q16 octaves above 20 Hz).
const i32 strike_centre = 425445, strike_hamr = -39322;
const i32 strike_time = 681000;
const i32 strike_level = 0x0c000000, strike_soft = 0x40000000;
// The shell peaks 2 dB over the Machinedrum's (reference/pi_measure.py, LEVEL_DB).
const i32 body_level = 0x07300000;

struct Block {
    i32 offsets[mode_count], steps[mode_count], levels[mode_count], strike_tune, strike_amount;
    Block(const i32* p) {
        const i32 head = md::span(p[PTCH], head_low, head_low + head_span);
        strike_tune = md::cutoff(strike_centre + multiply(p[HAMR], strike_hamr));
        strike_amount = multiply(strike_level, md::span(p[HAMR], md::one, strike_soft));
        const i32 time = md::table9(decay_times, p[DEC]);
        for (unsigned k = 0; k < 3; ++k) {
            offsets[k] = head + head_ratios[k];
            steps[k] = md::decay(time);
            levels[k] = head_levels[k];
        }
        offsets[3] = head + md::table9(tune_ratios, p[TUNE]);
        steps[3] = md::decay(time + tube_longer);
        levels[3] = md::span(p[POS], pos_low, pos_high);
        const i32 shell = md::table9(tube, p[SIZE]);
        const i32 damp = md::table9(damp_share, p[DAMP]);
        const i32 soften = md::span(p[HAMR], md::one, hamr_soft);
        i32 tilt = md::one;
        for (unsigned k = 0; k < tube_count; ++k) {
            offsets[head_count + k] = shell + harmonics[k];
            const i32 shorter = static_cast<i32>(k) * harmonic_shorter
                                + multiply(damp, damp_first + static_cast<i32>(k) * damp_each);
            steps[head_count + k] = md::decay(time + tube_longer - shorter);
            levels[head_count + k] = multiply(tube_levels[k], tilt);
            tilt = multiply(tilt, soften);
        }
        const i32 shell_move = (shell - tube[4]) / 4;
        const i32 upper_soften = md::one - multiply(md::squared(p[HAMR]), upper_soft);
        const i32 upper_short = multiply(md::positive(p[DAMP]), upper_damp);
        for (unsigned k = 0; k < upper_count; ++k) {
            offsets[head_count + tube_count + k] = upper[k] + shell_move;
            steps[head_count + tube_count + k] = md::decay(upper_times[k] - upper_short);
            levels[head_count + tube_count + k] = multiply(upper_levels[k], upper_soften);
        }
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    for (unsigned k = 0; k < mode_count; ++k) {
        voice.modes[k].phase = voice.modes[k].increment = 0;
        voice.modes[k].level = 0;
    }
    voice.strike = 0;
    voice.opened = 1;
    voice.seed = 0x5bd1e995u * owner;
    voice.band.reset();
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.fresh = 0;
        for (unsigned k = 0; k < mode_count; ++k) {
            // The upper modes start out of step, so they don't add up to a click.
            voice.modes[k].phase = k < head_count + tube_count ? 0 : k * 0x9e3779b9u;
            voice.modes[k].level = md::one;
        }
        voice.strike = md::one;
        voice.band.reset();
        voice.opened = 0;
    }
    i32 mixed[opxy::block_frames];
    for (unsigned i = 0; i != opxy::block_frames; ++i) mixed[i] = 0;
    for (unsigned k = 0; k < mode_count; ++k) {
        modal::Mode& mode = voice.modes[k];
        if (mode.level < 0x00010000) continue;
        mode.increment = md::transpose(pitch[0], b.offsets[k]);
        const i32 amount = multiply(b.levels[k], body_level);
        modal::ring(mode, b.steps[k], k < head_count + tube_count || voice.opened ? amount : 0, amount, mixed);
    }
    voice.opened = 1;
    if (voice.strike > 0x00100000) {
        const i32 step = md::decay(strike_time);
        for (unsigned i = 0; i != opxy::block_frames; ++i) {
            voice.band.run(md::noise_eighth(voice.seed), b.strike_tune, 0x20000000);
            mixed[i] += md::gain(multiply(voice.band.band, multiply(voice.strike, b.strike_amount)), 2);
            voice.strike = multiply(voice.strike, step);
        }
    }
    modal::finish_doubled(mixed, output);
}
}
}
