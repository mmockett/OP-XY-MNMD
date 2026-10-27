#pragma once
// DPRO-WAVE, DPRO-DDRW and DPRO-DENS: the sound, modules any engine can hold
// (docs/VOICES.md). One header for the three, as they share the DigiPRO's
// oscillator and its waves.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "private_dpro.hpp"

// The Monomachine's DigiPRO machines, written from scratch in fixed point and
// fitted to the original through the emulator (reference/README.md). Their
// waves are the original's own, captured from the owner's ROM by
// tools/capture_dpro.py into a private header: without it every DigiPRO
// engine builds and plays silence (docs/FAMILIES.md).
//
// The DigiPRO reads its tables raw, interpolated but never band-limited, so
// its high notes alias: these do the same. There are two kinds of table:
//   D01..D64  one cycle each (SINE, HSIN, TRI ...): DDRW and DENS play them
//   S01..S32  two cycles each, which WP moves through towards the next set:
//             DPRO-WAVE plays them
// D08 (RND) is noise that drifts from cycle to cycle, made here rather than stored.
namespace md_mmdens {} namespace md = md_mmdens; namespace md_mmdens {
namespace dpro {
using md::multiply;
using dpro_data::table_bits;
const u32 table_size = 1u << table_bits;
const u32 index_shift = 32 - table_bits;           // a phase's table index
const u32 rnd_wave = 7;                            // D08

// A table at a 32-bit phase, interpolated linearly: Q31, a full-scale int16 at
// half scale (the tables hold the emulator's level, tools/capture_dpro.py).
inline i32 read(const opxy::i16* table, u32 phase) {
    const u32 i = phase >> index_shift;
    const i32 fraction = static_cast<i32>((phase >> (index_shift - 15)) & 0x7fffu);
    const i32 a = table[i], b = table[(i + 1) & (table_size - 1)];
    return a * 32768 + (b - a) * fraction;
}

// A slot's 0..127 step, as the original's controls count (Q31 0..1 in).
inline u32 step(i32 slot) { return static_cast<u32>(md::positive(slot)) >> 24; }

// TUNE: a 32nd of a semitone a step from 64, so -2 at 0 and +1.97 at 127 as
// measured (Q16 octaves).
inline i32 tune(i32 slot) { return (static_cast<i32>(step(slot)) - 64) * 171; }

// RND: 512 random points a cycle, interpolated; every 8 cycles a fresh set, the
// cycles between crossfading: at note 24 a cycle is 0.94 like the last (the original 0.96).
// A voice holds 512 random levels; each set reads them from an offset 179 points
// on from the last's, so two sets in a row are unrelated, and an oscillator's
// `offset` keeps it apart from the others'.
inline u32 hash(u32 x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
struct Noise {
    signed char level[table_size];
    void fill(u32 seed) {
        for (u32 i = 0; i < table_size; ++i) level[i] = static_cast<signed char>(static_cast<int>(hash(i ^ seed) >> 24) - 128);
    }
};
const u32 set_apart = 179;
inline i32 rnd(const Noise& n, u32 phase, u32 cycle, u32 offset) {
    const u32 i = (phase >> index_shift) + (cycle >> 3) * set_apart + offset;
    const i32 along = static_cast<i32>((phase >> (index_shift - 15)) & 0x7fffu) << 16;   // Q31
    const i32 blend = static_cast<i32>((cycle & 7u) << 28);
    const u32 mask = table_size - 1;
    // 0.24 of full scale at most, as the original's peaks (rms 0.13).
    const i32 a0 = n.level[i & mask] * 4194304, a1 = n.level[(i + set_apart) & mask] * 4194304;
    const i32 b0 = n.level[(i + 1) & mask] * 4194304, b1 = n.level[(i + 1 + set_apart) & mask] * 4194304;
    const i32 a = a0 + multiply(a1 - a0, blend), b = b0 + multiply(b1 - b0, blend);
    return a + multiply(b - a, along);
}

// One D-wave at a phase: `cycle` counts the wave's cycles, for RND.
inline i32 d_wave(u32 wave, u32 phase, u32 cycle, const Noise& noise, u32 offset) {
    if (wave == rnd_wave && dpro_data::captured) return rnd(noise, phase, cycle, offset);
    return read(dpro_data::d_table[wave < dpro_data::d_waves ? wave : 0], phase);
}
// A D-wave control's wave: 64 waves, two steps each.
inline u32 d_index(i32 slot) { return step(slot) >> 1; }

// One D-wave oscillator: its phase and cycle count.
struct Osc {
    u32 phase, cycle;
    i32 run(u32 wave, u32 increment, const Noise& noise, u32 offset) {
        const i32 out = d_wave(wave, phase, cycle, noise, offset);
        const u32 next = phase + increment;
        cycle += next < phase;
        phase = next;
        return out;
    }
};
}

// DPRO-WAVE: an S-set, two cycles a table, at the played pitch.
//   WAVE  WP    WPM   WPRS
//   SYNC  SFRQ  -     TUNE
// WAVE picks S01..S32 (four steps each). WP moves the set towards the next: the
// original's shape changes in plateaus (0, 8..24, 32..48, 64..80, 96..120), so
// the set is captured at WP 0, 16, 48, 80 and 112 and crossfaded between them,
// and at 127 it is the next set, as measured (S32 reaches a wave of its own). WPM sweeps WP up and
// down the whole range as a triangle, 0.2 Hz at 16 to 1.3 Hz at 127; WPRS
// restarts the sweep at each note, otherwise it runs on. SYNC SFRQ restarts the
// wave at a fixed rate SFRQ sets, 17 to 150 Hz whatever the note; PRCH syncs to the previous track on the Monomachine, which the
// OP-XY has no equivalent of, so it plays unsynced. TUNE is +-2 semitones.
namespace dpro_wave {
struct State {
    u32 phase;      // the table's phase: two cycles a turn
    u32 sweep;      // WPM's triangle, a turn up and down
    u32 sync;       // SFRQ's restarting phase
    u32 seed;
    u32 fresh;      // a note has started and render has not yet run
};
namespace {
using md::multiply;
enum { WAVE, WP, WPM, WPRS, SYNC, SFRQ, SPARE, TUNE };

// Where each captured step sits on WP (Q8 of 0..127), and 2^30 / its span to the next.
const i32 step_at[6] = {0, 16 * 256, 48 * 256, 80 * 256, 112 * 256, 127 * 256};
const i32 step_reciprocal[5] = {262144, 131072, 131072, 131072, 279620};
// WPM's sweep rate as a phase increment a sample (2^32 a whole up and down) at WPM 0, 16, ..., 127:
// 1.33 Hz x (WPM / 127)^0.9 (reference/README.md).
const i32 sweep_rate[9] = {0, 20075, 37461, 53959, 69906, 85454, 100692, 115677, 129531};
// SFRQ: the restart rate as a phase increment at SFRQ 0, 16, ..., 127: 29 Hz x
// 2^((SFRQ - 32) / 39), whatever the note (51 Hz at 64 on notes 36 and 48 alike).
const i32 sync_rate[9] = {1600720, 2126265, 2824355, 3751640, 4983369, 6619496, 8792792, 11679620, 15241377};
}

inline void reset(State& s, Common&, u32 owner) {
    s.phase = 0;
    s.sync = 0;
    s.seed = dpro::hash(owner + 0x51ed270bu);
    s.fresh = 0;
    s.sweep = s.seed;          // free running: each voice starts somewhere
}
inline void trigger(State& s, Common&) { s.phase = 0; s.sync = 0; s.fresh = 1; }

inline void render(State& s, Common&, opxy::Output out, const i32* p, opxy::Pitch pitch) {
    const u32 set = dpro::step(p[WAVE]) >> 2;
    const u32 wpm = dpro::step(p[WPM]);
    // The note's cycle is pitch; the table holds two.
    const u32 cycle = md::transpose(pitch[0], dpro::tune(p[TUNE]));
    const u32 increment = cycle >> 1;
    if (s.fresh && dpro::step(p[WPRS]) >= 64) s.sweep = 0;
    s.fresh = 0;
    // WP, swept: a triangle folded into 0..127, Q8.
    const i32 rate = wpm ? md::table9(sweep_rate, p[WPM]) : 0;
    i32 position = static_cast<i32>(static_cast<u32>(md::positive(p[WP])) >> 16) * 127 >> 7;   // Q8 0..127
    if (wpm) {
        const i32 tri = static_cast<i32>(s.sweep >> 17);                                       // 0..32767: 0..254 Q7
        position += tri < 16384 ? tri * 2 : (32768 - tri) * 2;                                 // + 0..127 and back, Q8
        if (position > 127 * 256) position = 254 * 256 - position;
        s.sweep += static_cast<u32>(rate) * 64;
    }
    position = md::clamp(position, 0, 127 * 256);
    u32 j = 0;
    while (j < 4 && position >= step_at[j + 1]) ++j;
    const i32 fade = md::clamp((position - step_at[j]) * step_reciprocal[j], 0, 0x3fffffff) * 2;   // Q31
    const opxy::i16* a = dpro_data::s_table[set % dpro_data::s_waves][j];
    const u32 own = set % dpro_data::s_waves;
    const opxy::i16* b = j + 1 < dpro_data::s_steps ? dpro_data::s_table[own][j + 1]
                       : own + 1 < dpro_data::s_waves ? dpro_data::s_table[own + 1][0] : dpro_data::s_end;
    // SYNC: 0 off, 1 SFRQ, 2 PRCH (played unsynced).
    const u32 mode = dpro::step(p[SYNC]) * 3 >> 7;
    const u32 sync_increment = mode == 1 ? static_cast<u32>(md::table9(sync_rate, p[SFRQ])) : 0;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        const i32 x = dpro::read(a, s.phase), y = dpro::read(b, s.phase);
        out[i] = md::finish(x + multiply(y - x, fade));
        s.phase += increment;
        if (sync_increment) {
            const u32 next = s.sync + sync_increment;
            if (next < s.sync) s.phase = 0;
            s.sync = next;
        }
    }
}
}

// DPRO-DDRW: two D-waves, crossfaded.
//   WAV1  MIX   WAV2  TIME
//   BR1   WID   BR2   TUNE
// WAV1 and WAV2 pick D01..D64; MIX crossfades from WAV1 to WAV2. WID detunes
// the two apart in cents, WAV1 down and WAV2 up: +-27 cents at WID 64 and +-53
// at 127 when they are evenly mixed, less towards either end of MIX and none at
// its ends, as measured. BR1 and BR2 reduce each wave's bits, on the wave's own
// scale: 16 at 0, 7 at 80, 3 at 112, 1 at 127. TIME changed nothing measurable on a held note, so it
// does nothing here. TUNE is +-2 semitones.
namespace dpro_ddrw {
struct State {
    dpro::Osc one, two;
    dpro::Noise noise;
};
namespace {
using md::multiply;
enum { WAV1, MIX, WAV2, TIME, BR1, WID, BR2, TUNE };
const i32 spread_most = 2922;       // 53.5 cents, Q16 octaves
// A wave's bits kept: a mask on the wave scaled to its peak at 2^30, or 0 for all of them.
inline u32 keep(i32 slot) {
    const u32 bits = 16 - ((dpro::step(slot) * 15 + 64) >> 7);
    return bits >= 14 ? 0 : 0xffffffffu << (31 - bits);
}
// x, its bits cut on its wave's scale: up to its peak at 2^30, masked, and back.
inline i32 reduce(i32 x, u32 mask, u32 wave) {
    if (!mask) return x;
    const u32 w = wave < dpro_data::d_waves ? wave : 0;
    const i32 scaled = multiply(x, dpro_data::d_norm[w]) * 65536;
    return multiply(static_cast<i32>(static_cast<u32>(scaled) & mask), dpro_data::d_peak[w] * 65536);
}
}

inline void reset(State& s, Common&, u32 owner) {
    s.one.phase = s.one.cycle = s.two.phase = s.two.cycle = 0;
    s.noise.fill(dpro::hash(owner + 0x68e31da4u));
}
inline void trigger(State& s, Common&) { s.one.phase = s.two.phase = 0; }

inline void render(State& s, Common&, opxy::Output out, const i32* p, opxy::Pitch pitch) {
    const u32 one = dpro::d_index(p[WAV1]), two = dpro::d_index(p[WAV2]);
    const i32 mix = md::positive(p[MIX]);
    const u32 base = md::transpose(pitch[0], dpro::tune(p[TUNE]));
    // The detune: WID's, times how evenly the two are mixed (1 at the middle, 0 at the ends).
    const i32 off = mix > 0x40000000 ? mix - 0x40000000 : 0x40000000 - mix;   // 0 .. 2^30
    const i32 even = off >= 0x40000000 ? 0 : md::one - off * 2;
    const i32 spread = multiply(multiply(md::positive(p[WID]), even), spread_most);
    const u32 up = md::transpose(base, spread), down = md::transpose(base, -spread);
    const u32 keep1 = keep(p[BR1]), keep2 = keep(p[BR2]);
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        const i32 a = reduce(s.one.run(one, down, s.noise, 0), keep1, one);
        const i32 b = reduce(s.two.run(two, up, s.noise, 256), keep2, two);
        out[i] = md::finish(a + multiply(b - a, mix));
    }
}
}

// DPRO-DENS: an ensemble of up to four D-waves in a chord, each with a chorus.
//   PCH2  PCH3  PCH4  WAVE
//   -     CHRL  CHRW  TUNE
// PCH2..4 add voices at an interval from the note, in steps of four read off the
// original's sound (its LCD rounds them two steps apart): 0..61 go -12..-1
// semitones with the just 2/3, 3/4, 4/5 and 5/6 among them in pitch order, 62
// and 63 are OFF, 64..127 go +1..+12 with 6/5, 5/4, 4/3 and 3/2. Each voice is a
// quarter of DDRW's level. WAVE picks D01..D64. The chorus is a copy of each
// voice detuned slowly up and down (0.6 Hz, about 15 cents): CHRL deepens it,
// CHRW spreads it in stereo on the original, so in the OP-XY's mono voice it
// cancels more as it widens, as the original does summed to mono. TUNE is +-2 semitones.
namespace dpro_dens {
struct State {
    dpro::Osc voice[4], chorus[4];
    u32 lfo;
    dpro::Noise noise;
};
namespace {
using md::multiply;
enum { PCH2, PCH3, PCH4, WAVE, SPARE, CHRL, CHRW, TUNE };
const i32 off = 0x7fffffff;
// The intervals, Q16 octaves: below, -12, -11, -10, -9, -8, 2/3, -7, -6, -5, 3/4, -4, 4/5, 5/6, -3, -2, -1; above, +1, +2, +3, 6/5, 5/4, +4, 4/3, +5, +6, +7, 3/2, +8, +9, +10, +11, +12.
const i32 below[16] = {-65536, -60075, -54613, -49152, -43691, -38336, -38229, -32768, -27307, -27200, -21845, -21098, -17238, -16384, -10923, -5461};
const i32 above[16] = {5461, 10923, 16384, 17238, 21098, 21845, 27200, 27307, 32768, 38229, 38336, 43691, 49152, 54613, 60075, 65536};
inline i32 interval(i32 slot) {
    const u32 v = dpro::step(slot);
    return v < 62 ? below[(v + 2) >> 2] : v < 64 ? off : above[(v - 64) >> 2];
}
inline i32 quarter(i32 x) { return opxy::q31::half(opxy::q31::half(x)); }
const u32 lfo_rate = 58438 * 64;     // 0.6 Hz, a block
const i32 detune_most = 819;         // 15 cents, Q16 octaves
}

inline void reset(State& s, Common&, u32 owner) {
    for (unsigned k = 0; k < 4; ++k) {
        s.voice[k].phase = s.voice[k].cycle = 0;
        s.chorus[k].phase = dpro::hash(owner + k);   // the copies start apart
        s.chorus[k].cycle = 0;
    }
    s.lfo = dpro::hash(owner + 0x2545f491u);
    s.noise.fill(s.lfo);
}
inline void trigger(State& s, Common&) {
    for (unsigned k = 0; k < 4; ++k) s.voice[k].phase = 0;
}

inline void render(State& s, Common&, opxy::Output out, const i32* p, opxy::Pitch pitch) {
    const u32 wave = dpro::d_index(p[WAVE]);
    const u32 base = md::transpose(pitch[0], dpro::tune(p[TUNE]));
    // The chorus: its LFO a triangle, its share of the mix CHRL x (1 - 0.7 CHRW).
    s.lfo += lfo_rate;
    const i32 tri = static_cast<i32>(s.lfo >> 16) < 32768 ? static_cast<i32>(s.lfo >> 15) - 32768
                                                          : 98303 - static_cast<i32>(s.lfo >> 15);   // -32768..32767
    const i32 detune = multiply(tri * 65536, detune_most);
    const i32 depth = multiply(md::positive(p[CHRL]), md::one - multiply(md::positive(p[CHRW]), 0x5999999a));
    const i32 shift[4] = {0, interval(p[PCH2]), interval(p[PCH3]), interval(p[PCH4])};
    u32 increment[4], copy[4];
    bool on[4];
    for (unsigned k = 0; k < 4; ++k) {
        on[k] = shift[k] != off;
        increment[k] = on[k] ? md::transpose(base, shift[k]) : 0;
        copy[k] = on[k] ? md::transpose(increment[k], k & 1 ? detune : -detune) : 0;
    }
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 dry = 0, wet = 0;
        for (unsigned k = 0; k < 4; ++k) {
            if (!on[k]) continue;
            dry += quarter(s.voice[k].run(wave, increment[k], s.noise, 0));
            if (depth) wet += quarter(s.chorus[k].run(wave, copy[k], s.noise, 64 * (k + 1)));
        }
        out[i] = md::finish(dry + multiply(wet, depth));
    }
}
}
}
