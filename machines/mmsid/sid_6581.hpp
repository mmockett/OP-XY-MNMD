#pragma once
// SID-6581: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"


// MNM SID: a Monomachine SID-6581-style oscillator, written from scratch in
// fixed point and fitted to the Monomachine's own behaviour
// (reference/fit_sid_6581.py). The eight controls follow the SID-6581 page,
// top row on the encoders and the bottom row as their alternates:
//   PW    PWAD  PWRS  WAVE
//   MOD   MSRC  MFRQ  TUNE
// One oscillator at the played pitch, and a second, the modulator, at MFRQ's
// frequency, the same on every key (or at the previous note's pitch, MSRC =
// PRCH). WAVE picks triangle, saw, pulse, the mixed wave or noise; MOD rings
// the triangles of the two, or syncs the modulator's wave to the oscillator,
// or both. Pulse
// width is symmetric about PW 64, which is a silent full-width pulse; PWAD
// sweeps it at a steady rate, from PW at each note when PWRS is on. The
// edges are band-limited (polyBLEP); the Monomachine's gentle top-end roll-off
// follows, two one-pole low-passes at 6 kHz.
namespace md_mmsid {} namespace md = md_mmsid; namespace md_mmsid {
namespace sid_6581 {
struct State {
    u32 phase;      // the oscillator, at the played pitch
    u32 mod_phase;  // the modulator
    u32 noise_phase;// noise's clock, 16 x the pitch
    i32 noise;      // the noise sample held, Q29
    u32 seed;
    u32 sweep;      // PWAD's offset to PW, 2^32 = 128 steps of PW
    u32 source;     // MSRC = PRCH: the previous note's pitch
    i32 held;       // the output one sample behind, with its edge corrections
    i32 low1, low2; // the roll-off
    i32 ring_high;  // the ring product's high-pass
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;

enum { PW, PWAD, PWRS, WAVE, MOD, MSRC, MFRQ, TUNE };
enum { tri, saw, pulse, mix, noise_wave };
enum { off, ring, sync, ring_sync };

// Waves are Q29 inside: +-1 is +-2^29, a quarter of full scale at the output.
const i32 unit = 0x20000000;
// PWAD: the sweep's speed at PWAD 0, 16, ..., 127, per sample (2^32 = 128 PW steps):
// 40 steps a second at 16, 185 at 64, 332 at 127.
const i32 sweep_rate[9] = {0, 30436, 69243, 105006, 140768, 167400, 194793, 224468, 252622};
// TUNE: +-2 semitones, 1/32 of a semitone a step; Q16 octaves.
const i32 tune_span = 21674, tune_low = 10923;   // 127 steps of 1/384 octave
// MFRQ: the modulator at 2^((MFRQ - 32) / 32) x 261.6 Hz, 131 Hz .. 2 kHz, whatever
// the key or TUNE; Q16 octaves from middle C.
const u32 middle_c = 25479004u;
const i32 ratio_span = 260096, ratio_low = 65536;   // 127 steps of 1/32 octave
// The ring: (a + b + 1.1 HP(a b)) at 0.25, its product through a 200 Hz high-pass.
const i32 ring_level = 530428000, ring_product = 0x46666666, ring_highpass = 61190000;
// MIX: 0.438 (w - 0.292) of a raised-cosine hump centred at 0.8 of the cycle,
// 0.3 either side, rippled ten times a cycle; nothing in the first half.
const u32 mix_centre = 3435973837u, mix_width = 1288490189u;
const i32 mix_scale = 940823645, mix_mean = 313468000;   // 0.438 Q31; 0.292 Q30
// The roll-off: 1 - e^(-2 pi 6000 / 44100), Q31.
const i32 roll_off = 1233733000;

inline u32 step127(i32 slot) { return static_cast<u32>(md::positive(slot)) >> 24; }
inline u32 choice(i32 slot, u32 count) { return step127(slot) * count >> 7; }

// num / den in Q31 for num < den <= 2^31, by shifts and subtractions (no divide).
inline i32 fraction(u32 num, u32 den) {
    u32 r = 0;
    for (u32 i = 0; i < 31; ++i) {
        num <<= 1;
        r <<= 1;
        if (num >= den) { num -= den; r |= 1; }
    }
    return static_cast<i32>(r);
}

inline i32 triangle(u32 p) { return static_cast<i32>(((p & 0x80000000u) ? ~p : p) >> 1) - unit; }
inline i32 sawtooth(u32 p) { return unit - static_cast<i32>(p >> 2); }   // falling, as the Monomachine's
// Zero-mean pulse, high for `duty` of the cycle (2^32 = all).
inline i32 pulse_high(u32 duty) { return static_cast<i32>((0u - duty) >> 2); }
inline i32 pulse_low(u32 duty) { return -static_cast<i32>(duty >> 2); }

inline i32 mixed(u32 p) {
    if (p < 0x80000000u) return opxy::q31::half(multiply(-mix_mean, mix_scale));   // the mean, taken off (Q29)
    const u32 away = p > mix_centre ? p - mix_centre : mix_centre - p;
    i32 w = 0;
    if (away < mix_width) {
        // hump = 1 - away / 0.3, Q31; raised = (1 - cos(pi hump)) / 2.
        const i32 hump = md::one - md::gain(multiply(static_cast<i32>(away >> 1), 1789569707), 2);
        const i32 c = md::sine(static_cast<u32>(hump) + 0x40000000u);
        const i32 raised = opxy::q31::half(md::one) - opxy::q31::half(c);
        // ripple 1 - cos(2 pi 10 (u - 0.8)) / 2, Q30.
        const i32 r = md::sine((p - mix_centre) * 10u + 0x40000000u);
        const i32 ripple = 0x40000000 - opxy::q31::half(opxy::q31::half(r));
        w = multiply(raised, ripple);   // Q30
    }
    return opxy::q31::half(multiply(w - mix_mean, mix_scale));   // Q29
}

inline i32 wave_at(u32 wave, u32 p, u32 duty) {
    switch (wave) {
    case tri: return triangle(p);
    case saw: return sawtooth(p);
    case pulse: return p < duty ? pulse_high(duty) : pulse_low(duty);
    default: return mixed(p);
    }
}

// A band-limited step of height h, x (Q31) of a sample before this one: the
// corrections to this sample and the one before it (polyBLEP).
inline void blep(i32 h, i32 x, i32& now, i32& before) {
    const i32 rest = md::one - x;
    now -= multiply(opxy::q31::half(h), multiply(rest, rest));
    before += multiply(opxy::q31::half(h), multiply(x, x));
}
}

inline void reset(State& s, Common&, u32 owner) {
    s.phase = s.mod_phase = s.noise_phase = 0;
    s.noise = 0;
    s.seed = 0x9e3779b9u ^ (owner * 0x85ebca6bu);
    s.sweep = 0;
    s.source = 0;
    s.held = s.low1 = s.low2 = s.ring_high = 0;
    s.fresh = 0;
}

inline void trigger(State& s, Common&) { s.fresh = 1; }

inline void render(State& s, Common& common, opxy::Output out, const i32* slots, opxy::Pitch pitch) {
    if (s.fresh) {
        s.fresh = 0;
        // PWRS: the sweep starts from PW at each note.
        if (step127(slots[PWRS]) >= 64) s.sweep = 0;
        // PRCH: the note started before this one is the modulator.
        s.source = common.previous_pitch ? common.previous_pitch : pitch[0];
        common.previous_pitch = pitch[0];
    }
    const u32 wave = choice(slots[WAVE], 5), mod = choice(slots[MOD], 4);
    const i32 tune = multiply(md::positive(slots[TUNE]), tune_span) - tune_low;
    const u32 inc = md::transpose(pitch[0], tune);
    const u32 mod_inc = choice(slots[MSRC], 2) ? s.source
        : md::transpose(middle_c, multiply(md::positive(slots[MFRQ]), ratio_span) - ratio_low);
    // PW: symmetric about 64, duty 1 - |PW + sweep - 64| / 128: 0.5 at 0 and 127, full at 64.
    const i32 rate = md::table9(sweep_rate, slots[PWAD]);
    if (rate) s.sweep += static_cast<u32>(rate) * opxy::block_frames;
    else s.sweep = 0;
    const u32 width = (static_cast<u32>(multiply(md::positive(slots[PW]), 0x7f000000)) << 1) + s.sweep;   // 2^32 = 128
    const u32 apart = width >= 0x80000000u ? width - 0x80000000u : 0x80000000u - width;
    const bool silent = apart < 0x100u;   // a full-width pulse: nothing to hear
    const u32 duty = silent ? 0xffffff00u : 0u - apart;

    const bool synced = mod == sync || mod == ring_sync, ringing = mod == ring || mod == ring_sync;
    // The wave heard: the oscillator's, or the modulator's when it is synced.
    const u32 heard_inc = synced ? mod_inc : inc;
    const u32 noise_inc = heard_inc < 0x10000000u ? heard_inc << 4 : 0;
    const i32 level = ringing ? ring_level : md::one;

    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 now = 0, before = 0;
        const u32 p = s.phase + inc;
        const bool wrapped = p < s.phase;
        s.phase = p;
        i32 x = 0;   // how far past the oscillator's wrap this sample is, Q31
        if (wrapped) x = fraction(p, inc);
        u32 q;       // the heard wave's phase
        if (synced) {
            const u32 free = s.mod_phase + mod_inc;
            q = free;
            if (wrapped) {
                // The sync restarts the heard wave half way through its cycle; rung, at its start.
                q = (ringing ? 0u : 0x80000000u) + (static_cast<u32>(multiply(static_cast<i32>(mod_inc >> 1), x)) << 1);
                if (!ringing && wave != noise_wave) blep(wave_at(wave, q, duty) - wave_at(wave, free, duty), x, now, before);
            }
            s.mod_phase = q;
        } else {
            q = p;
            s.mod_phase += mod_inc;
        }
        const u32 last = q - heard_inc;   // the heard wave's phase a sample ago, unless reset
        i32 v;
        if (ringing) {
            // The triangles of the heard wave and the other oscillator, rung.
            const i32 a = triangle(q), b = triangle(synced ? p : s.mod_phase);
            const i32 product = md::gain(multiply(a, b), 2);
            s.ring_high += multiply(product - s.ring_high, ring_highpass);
            v = a + b + multiply(product - s.ring_high, ring_product) * 2;
        } else if (wave == noise_wave) {
            const u32 n = s.noise_phase + noise_inc;
            if (!noise_inc || n < s.noise_phase) {
                md::noise(s.seed);
                s.noise = static_cast<i32>(s.seed >> 2) - unit;
            }
            s.noise_phase = n;
            v = s.noise;
        } else {
            v = wave == pulse && silent ? 0 : wave_at(wave, q, duty);
            // The heard wave's own edges, when the sync did not just reset it.
            const bool own = !(synced && wrapped) && q < last;   // it wrapped by itself
            if (wave == saw && own) blep(2 * unit, fraction(q, heard_inc), now, before);
            if (wave == pulse && !silent) {
                const i32 rise = pulse_high(duty) - pulse_low(duty);
                if (own) blep(rise, fraction(q, heard_inc), now, before);
                if (!(synced && wrapped) && q - duty < heard_inc) blep(-rise, fraction(q - duty, heard_inc), now, before);
            }
        }
        v = multiply(v, level) + now;
        const i32 sample = s.held + before;
        s.held = v;
        s.low1 += multiply(sample - s.low1, roll_off);
        s.low2 += multiply(s.low1 - s.low2, roll_off);
        out[i] = md::finish(s.low2);
    }
}
}
}
