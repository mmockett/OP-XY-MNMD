#pragma once
// SWAVE-SAW, SWAVE-PULSE and SWAVE-ENS: the sound, modules any engine can hold
// (docs/VOICES.md). Written from scratch in fixed point and fitted to the
// Monomachine's own behaviour (reference/fit_swave.py). Each is played
// chromatically: the pitch is the played key, TUNE moves it +-2 semitones.
//
//   SWAVE-SAW    UNIL  UNIW  UNIX  -     /  SUBX  SUB1  SUB2  TUNE
//   SWAVE-PULSE  UNIL  UNIW  SUB1  SUB2  /  PW    PWAD  PWRS  TUNE
//   SWAVE-ENS    PCH2  PCH3  PCH4  WAVE  /  PW    CHRL  CHRW  TUNE
//
// SuperWave is a stack of saws (or pulses) that start in phase on each note:
// the main one, a pair detuned either side by UNIW at UNIL's level and, on
// SAW, an inner pair at half the detune at UNIX x UNIL. SUB1 and SUB2 are
// sines one and two octaves down, SUBX a saw an octave down. ENS plays the
// main oscillator and up to three more at PCH2..4's intervals, each with a
// chorus of two drifting copies.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

namespace md {
namespace swave {
using md::multiply;

// A saw from a phase: a rising ramp of +-2^24 (1/128 of full scale), so seven
// of them and the subs stay inside the filters' eighth of full scale.
inline i32 saw(u32 phase) { return static_cast<i32>(phase >> 7) - 0x01000000; }
// A pulse high for the first `width` of the cycle, zero mean, the saw's 2^25 peak to peak.
inline i32 pulse(u32 phase, u32 width) {
    return phase < width ? static_cast<i32>((0u - width) >> 7) : -static_cast<i32>(width >> 7);
}
// level (Q28, 0..8) times x.
inline i32 times(i32 x, i32 level_q28) { return multiply(x, level_q28) * 8; }

// TUNE: +-2 semitones about 64, continuous (the Monomachine's 127 is +1.97).
const i32 tune_centre = 1082196480;   // 64/127 in Q31
inline i32 tune(i32 slot) { return multiply(slot - tune_centre, 21675); }   // Q16 octaves
// UNIW: the outer pair's detune, 0 .. +-53.7 cents, linear (Q16 octaves at full control).
const i32 width_most = 2933;

// The base pitch of a block: the played key moved by TUNE.
inline u32 base(opxy::Pitch pitch, const i32* slots, int slot) { return md::transpose(pitch[0], tune(slots[slot])); }

// A one-pole high-pass at `ratio` x the fundamental, the coefficient 2 pi f / fs
// as a Q31: SuperWave's saws are thin at the fundamental (about 5 dB under a
// pure saw against their 2nd to 4th harmonics, at every key).
inline i32 tracking(u32 increment, i32 ratio_q28) {
    // 2 pi f / fs = pi * increment / 2^31, so in Q31 it is pi * increment.
    const u32 c = md::scale(increment, ratio_q28);   // increment x ratio
    const u32 limited = c > 0x28000000u ? 0x28000000u : c;
    return static_cast<i32>(limited) * 3 + multiply(static_cast<i32>(limited), 0x121fb544);   // x pi
}
}

namespace swave_saw {
struct State {
    u32 phase[5];      // main, outer pair, inner pair
    u32 sub[2];        // an octave and two octaves down
    i32 dent;          // the high-pass's low state
    md::Svf tone;      // the low-pass after it
    u32 fresh;
};
namespace {
enum { UNIL, UNIW, UNIX, SPARE, SUBX, SUB1, SUB2, TUNE };
// Levels as the Monomachine measures them, against the main saw (Q28): SUBX's
// octave-down saw reaches 2.65 x the main saw's fundamental, concave
// (1.37 x - 0.37 x^2); SUB1's sine 4.55 x and SUB2's 1.89 x, linear. SUBX runs
// through the high-pass with the saws, which takes its fundamental 4.7 dB
// further down than theirs, so its level here is that much higher.
const i32 subx_most = 0x4b333333;   // 4.70 x in Q28
const i32 sub1_most = 0x01547ae1;   // sines added after the high-pass: 1.33 x 2^24 peak
const i32 sub2_most = 0x007a3d71;   // 0.478 x 2^24
// The tone: a high-pass at 1.9 x the fundamental and a 2-pole low-pass near 3.4 kHz.
const i32 dent_ratio = 0x1e666666;  // 1.9 in Q28
const i32 low_pass = 485600;        // Q16 octaves above 20 Hz: 3.4 kHz
const i32 low_damping = 0x5a82799a; // Butterworth
const i32 out_level = 0x20800000;   // Q28: 2.03 x, then x 4: the engine's level (reference/fit_swave.py)
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32) {
    for (unsigned i = 0; i < 5; ++i) voice.phase[i] = 0;
    voice.sub[0] = voice.sub[1] = 0;
    voice.dent = 0;
    voice.tone.reset();
    trigger(voice, shared);
    voice.fresh = 0;
}

namespace {
// The block runs in passes over a buffer, each a short loop of its own with
// its state in locals, so each stays in registers: the saws, then the high-pass,
// the subs' sines, the low-pass and the output. A part whose level is 0 adds
// nothing (multiply(x, 0) is 0), so its pass is skipped and its phase moves on
// 64 steps at once.
// out = the main saw + the outer pair at `level`.
__attribute__((noinline)) void saws(i32* out, u32* phase, const u32* inc, i32 level) {
    u32 p0 = phase[0], p1 = phase[1], p2 = phase[2];
    const u32 i0 = inc[0], i1 = inc[1], i2 = inc[2];
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] = swave::saw(p0) + multiply(swave::saw(p1) + swave::saw(p2), level);
        p0 += i0; p1 += i1; p2 += i2;
    }
    phase[0] = p0; phase[1] = p1; phase[2] = p2;
}
// out += a pair of saws at `level`.
__attribute__((noinline)) void pair(i32* out, u32* phase, const u32* inc, i32 level) {
    u32 p0 = phase[0], p1 = phase[1];
    const u32 i0 = inc[0], i1 = inc[1];
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += multiply(swave::saw(p0) + swave::saw(p1), level);
        p0 += i0; p1 += i1;
    }
    phase[0] = p0; phase[1] = p1;
}
// out += SUBX's octave-down saw; the sub's phase moves on in the last pass.
__attribute__((noinline)) void sub_saw(i32* out, u32 phase, u32 inc, i32 level) {
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        out[i] += swave::times(swave::saw(phase), level);
        phase += inc;
    }
}
// The high-pass, then the subs' sines (SUB2's when Sub2).
template <bool Sub2>
__attribute__((noinline)) void dent_subs(i32* sum, State& voice, u32 main, i32 dent, i32 sub1, i32 sub2) {
    u32 s0 = voice.sub[0], s1 = voice.sub[1];
    const u32 half = main >> 1, quarter = main >> 2;
    i32 hp = voice.dent;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        i32 stack = sum[i];
        hp += multiply(stack - hp, dent);
        stack -= hp;
        stack += multiply(md::sine(s0), sub1);
        if (Sub2) stack += multiply(md::sine(s1), sub2);
        s0 += half;
        s1 += quarter;
        sum[i] = stack;
    }
    voice.sub[0] = s0; voice.sub[1] = s1;
    voice.dent = hp;
}
// The low-pass (md::Svf::run, on locals) and the output.
__attribute__((noinline)) void tone(opxy::Output output, const i32* sum, md::Svf& svf, i32 cut) {
    i32 low = svf.low, band = svf.band, high = svf.high;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        low += multiply(cut, band) * 2;
        high = sum[i] - low - multiply(low_damping, band) * 2;
        band += multiply(cut, high) * 2;
        // md::finish(md::gain(swave::times(low, out_level), 2)): the same clamp in one step.
        i32 v = static_cast<i32>(static_cast<u32>(multiply(low, out_level)) << 3);
        v = v > 0x10000000 ? 0x10000000 : v;
        v = v < -0x10000000 ? -0x10000000 : v;
        output[i] = static_cast<i32>(static_cast<u32>(v) << 2);
    }
    svf.low = low; svf.band = band; svf.high = high;
}
}

inline void render(State& voice, Common&, opxy::Output output, const i32* p, opxy::Pitch pitch) {
    if (voice.fresh) {
        // Every oscillator starts in phase, as the Monomachine's do.
        for (unsigned i = 0; i < 5; ++i) voice.phase[i] = 0;
        voice.sub[0] = voice.sub[1] = 0;
        voice.fresh = 0;
    }
    const u32 main = swave::base(pitch, p, TUNE);
    const i32 outer = multiply(md::positive(p[UNIW]), swave::width_most);
    const u32 inc[5] = {main, md::transpose(main, outer), md::transpose(main, -outer),
                        md::transpose(main, outer / 2), md::transpose(main, -(outer / 2))};
    const i32 unil = md::positive(p[UNIL]);
    const i32 unix = multiply(md::positive(p[UNIX]), unil);
    const i32 x = md::positive(p[SUBX]);
    const i32 subx = multiply(multiply(x, 0x57ae147b) - multiply(multiply(x, x), 0x178d4fdf), subx_most) * 2;  // 1.37 x - 0.37 x^2, halved
    const i32 sub1 = multiply(md::positive(p[SUB1]), sub1_most);
    const i32 sub2 = multiply(md::positive(p[SUB2]), sub2_most);
    const i32 dent = swave::tracking(main, dent_ratio);
    const i32 cut = md::cutoff(low_pass);
    i32 sum[opxy::block_frames];
    saws(sum, voice.phase, inc, unil);
    if (unix) pair(sum, voice.phase + 3, inc + 3, unix);
    else { voice.phase[3] += inc[3] << 6; voice.phase[4] += inc[4] << 6; }   // 64 steps (block_frames)
    if (subx) sub_saw(sum, voice.sub[0], main >> 1, subx);
    if (sub2) dent_subs<true>(sum, voice, main, dent, sub1, sub2);
    else dent_subs<false>(sum, voice, main, dent, sub1, sub2);
    tone(output, sum, voice.tone, cut);
}
}

namespace swave_pulse {
struct State {
    u32 phase[3];      // main and the detuned pair
    u32 sub[2];
    u32 offset;        // PWAD's sweep of the width, free-running unless PWRS
    i32 dent;
    md::Svf tone;
    u32 fresh;
};
namespace {
enum { UNIL, UNIW, SUB1, SUB2, PW, PWAD, PWRS, TUNE };
// PW: the duty runs 1% .. 99% across the control, 0.98 / 128 a step, so 64 is
// square and 0 and 127 sound alike, nearly silent. PWAD sweeps it on,
// wrapping, at 0.0422 of a cycle per second per step (5.4 a second at 127);
// PWRS restarts the sweep at each note. The pair is detuned half as far as SAW's.
const u32 duty_low = 42949673u;            // 1% of 2^32
const u32 duty_step = 32883343u;            // 0.98 / 128 of 2^32
const u32 sweep_per_block = 263040u;        // 2^32 x 0.0422 / 44100 x 64, per step of PWAD
const i32 sub1_most = 0x0657ae14;           // 6.34 x 2^24 peak: 6.05 x the square's fundamental after the dent
const i32 sub2_most = 0x027d70a4;           // 2.49 x 2^24: 2.51 x
// The tone, as SAW's but milder: a high-pass at 0.75 x the fundamental, a low-pass near 3.4 kHz.
const i32 dent_ratio = 0x0c000000;          // 0.75 in Q28
const i32 low_pass = 485600;
const i32 low_damping = 0x5a82799a;
const i32 out_level = 0x13ae147b;           // Q28: 1.23 x, then x 4 (reference/fit_swave.py)
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    voice.phase[0] = voice.phase[1] = voice.phase[2] = 0;
    voice.sub[0] = voice.sub[1] = 0;
    voice.offset = owner * 0x9e3779b9u;   // free-running sweeps start apart
    voice.dent = 0;
    voice.tone.reset();
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* p, opxy::Pitch pitch) {
    if (voice.fresh) {
        voice.phase[0] = voice.phase[1] = voice.phase[2] = 0;
        voice.sub[0] = voice.sub[1] = 0;
        if (p[PWRS] >= 0x40000000) voice.offset = 0;
        voice.fresh = 0;
    }
    const u32 main = swave::base(pitch, p, TUNE);
    const i32 pair = multiply(md::positive(p[UNIW]), swave::width_most) / 2;
    const u32 inc[3] = {main, md::transpose(main, pair), md::transpose(main, -pair)};
    const i32 unil = md::positive(p[UNIL]);
    const u32 width = duty_low + (static_cast<u32>(md::positive(p[PW])) >> 24) * duty_step + voice.offset;
    voice.offset += (static_cast<u32>(md::positive(p[PWAD])) >> 24) * sweep_per_block;
    const i32 sub1 = multiply(md::positive(p[SUB1]), sub1_most);
    const i32 sub2 = multiply(md::positive(p[SUB2]), sub2_most);
    const i32 dent = swave::tracking(main, dent_ratio);
    const i32 cut = md::cutoff(low_pass);
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        i32 stack = swave::pulse(voice.phase[0], width)
                  + multiply(swave::pulse(voice.phase[1], width) + swave::pulse(voice.phase[2], width), unil);
        for (unsigned k = 0; k < 3; ++k) voice.phase[k] += inc[k];
        voice.dent += multiply(stack - voice.dent, dent);
        stack -= voice.dent;
        stack += multiply(md::sine(voice.sub[0]), sub1) + multiply(md::sine(voice.sub[1]), sub2);
        voice.sub[0] += main >> 1;
        voice.sub[1] += main >> 2;
        voice.tone.run(stack, cut, low_damping);
        output[i] = md::finish(md::gain(swave::times(voice.tone.low, out_level), 2));
    }
}
}

namespace swave_ens {
// Each of the four voices: the dry oscillator and the chorus's two copies.
struct State {
    u32 phase[4][3];
    u32 half;          // bit 3v+k: which half of the octave-down cycle each oscillator is in
    u32 drift;         // the chorus's slow sweep
    i32 dent;          // the high-pass's low state
    u32 fresh;
};
namespace {
enum { PCH2, PCH3, PCH4, WAVE, PW, CHRL, CHRW, TUNE };
// PCH2..4: 33 steps of an interval from the main oscillator, sorted, about 4
// values each: -12 .. +12 semitones with the just 6:5, 5:4, 4:3 and 3:2 either
// way beside the tempered ones, and off at 61..63 (reference/fit_swave.py).
const unsigned char step_of[128] = {
    0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8,
    8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16,
    17, 17, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 23, 23, 23, 23, 24, 24, 24, 25,
    25, 25, 25, 26, 26, 26, 26, 27, 27, 27, 27, 28, 28, 28, 28, 29, 29, 29, 29, 30, 30, 30, 30, 31, 31, 31, 31, 32, 32, 32, 32, 32,
};
const unsigned off_step = 16;
const i32 interval[33] = {   // Q16 octaves
    -65536, -60075, -54613, -49152, -43691, -38336, -38229, -32768, -27307, -27200, -21845, -21098, -17238, -16384, -10923, -5461,
    0, 5461, 10923, 16384, 17238, 21098, 21845, 27200, 27307, 32768, 38229, 38336, 43691, 49152, 54613, 60075, 65536,
};
// WAVE crossfades the saw into a pulse an octave down, as loud at PW 64 as the
// saw: its duty is PW / 128 of that longer cycle, folded about 64, so 64 is a
// square an octave down, 32 a quarter-duty pulse whose strongest line is the
// played note's, and 0 nearly silent (1%, as PULSE's).
const i32 pulse_level = 0x55a1cac1;   // 0.67
const u32 duty_low = 42949673u, duty_step = 32883343u;   // 1%, and 0.98 / 128 a step
// A high-pass at half the main oscillator's pitch: the original's thin fundamentals.
const i32 dent_ratio = 0x08000000;    // 0.5 in Q28
// CHRL fades the dry voices into two copies each at 0.61, detuned 6 .. 16
// cents either way by CHRW, drifting +-3 cents over two seconds.
const i32 wet_level = 0x4e147ae1;     // 0.61
const i32 chorus_low = 328, chorus_span = 546, drift_depth = 164;   // Q16 octaves
const u32 drift_per_block = 3115964u;  // 2^32 x 0.5 Hz x 64 / 44100
const i32 out_level = 0x3628f5c3;      // Q28: 3.385 x, then x 4 (reference/fit_swave.py)
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    for (unsigned v = 0; v < 4; ++v)
        for (unsigned k = 0; k < 3; ++k) voice.phase[v][k] = 0;
    voice.half = 0;
    voice.drift = owner * 0x9e3779b9u;
    voice.dent = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* p, opxy::Pitch pitch) {
    if (voice.fresh) {
        for (unsigned v = 0; v < 4; ++v)
            for (unsigned k = 0; k < 3; ++k) voice.phase[v][k] = 0;
        voice.half = 0;
        voice.fresh = 0;
    }
    const u32 main = swave::base(pitch, p, TUNE);
    // The chorus's detune now: a triangle of the drift.
    const u32 tri = voice.drift < 0x80000000u ? voice.drift : 0u - voice.drift;   // 0 .. 2^31
    voice.drift += drift_per_block;
    const i32 spread = chorus_low + multiply(md::positive(p[CHRW]), chorus_span)
                     + multiply(static_cast<i32>(tri) - 0x40000000, drift_depth * 2);
    u32 inc[4][3];
    unsigned voices = 0;
    for (unsigned v = 0; v < 4; ++v) {
        u32 at = main;
        if (v) {
            const unsigned s = step_of[static_cast<u32>(md::positive(p[PCH2 + v - 1])) >> 24];
            if (s == off_step) continue;
            at = md::transpose(main, interval[s]);
        }
        inc[voices][0] = at;
        inc[voices][1] = md::transpose(at, spread);
        inc[voices][2] = md::transpose(at, -spread);
        ++voices;
    }
    // The pulse's duty, of the octave-down cycle.
    u32 pw = static_cast<u32>(md::positive(p[PW])) >> 24;
    if (pw > 64) pw = 128 - pw;
    const u32 width = duty_low + pw * duty_step;
    const i32 dent = swave::tracking(main, dent_ratio);
    const i32 w = md::positive(p[WAVE]);
    const i32 saw_part = md::one - w, pulse_part = multiply(w, pulse_level);
    const i32 chorus = md::positive(p[CHRL]);
    const i32 dry = md::one - chorus, wet = multiply(chorus, wet_level);
    // Each copy's saw and pulse weights; a silent copy is skipped.
    const i32 saw_of[3] = {multiply(saw_part, dry), multiply(saw_part, wet), multiply(saw_part, wet)};
    const i32 pulse_of[3] = {multiply(pulse_part, dry), multiply(pulse_part, wet), multiply(pulse_part, wet)};
    const unsigned first = dry ? 0 : 1, last = wet ? 3 : 1;
    i32 sum[opxy::block_frames];
    for (unsigned i = 0; i != opxy::block_frames; ++i) sum[i] = 0;
    for (unsigned v = 0; v < voices; ++v) {
        for (unsigned k = first; k < last; ++k) {
            const u32 bit = 1u << (v * 3 + k);
            u32 ph = voice.phase[v][k], slow_half = (voice.half & bit) ? 0x80000000u : 0u;
            const u32 step = inc[v][k];
            // The pulse is one of two levels the whole block: weigh them once.
            const i32 a = saw_of[k];
            const i32 high = multiply(static_cast<i32>((0u - width) >> 7), pulse_of[k]), low = multiply(-static_cast<i32>(width >> 7), pulse_of[k]);
            for (unsigned i = 0; i != opxy::block_frames; ++i) {
                const u32 slow = (ph >> 1) + slow_half;   // the octave-down phase
                sum[i] += multiply(swave::saw(ph), a) + (slow < width ? high : low);
                const u32 next = ph + step;
                if (next < ph) slow_half ^= 0x80000000u;
                ph = next;
            }
            voice.phase[v][k] = ph;
            voice.half = slow_half ? voice.half | bit : voice.half & ~bit;
        }
    }
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        voice.dent += multiply(sum[i] - voice.dent, dent);
        output[i] = md::finish(md::gain(swave::times(sum[i] - voice.dent, out_level), 2));
    }
}
}
}
