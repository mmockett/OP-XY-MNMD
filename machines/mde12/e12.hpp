#pragma once
// E12: the Machinedrum's sixteen sample machines, modules any engine can hold
// (docs/VOICES.md). They play the samples captured from the owner's own ROM by
// tools/capture_e12.py (private_e12.hpp, never committed); without the capture
// voices/silent/private_e12.hpp stands in and they are silent. Fitted to the
// Machinedrum's own (reference/fit_e12.py). Their page:
//   PTCH  DEC   HP    (own)
//   STRT  RTRG  RTIM  BEND
// As measured: PTCH plays the sample (v - 64) / 32 octaves from its own speed.
// DEC is an exponential fall, none at 127. HP is a two-pole high-pass from
// 30 Hz to 7.6 kHz, with HPQ its resonance. STRT starts the sample a step
// later each, a step of its own for each machine (from the capture). RTRG plays
// it again from STRT, RTRG - 1 times (without end at 126 and 127), every RTIM,
// in whole 32-sample blocks as the original. BEND starts the pitch up to an
// octave away and lets it drift back over tens of seconds. The machines' own
// controls: E12-BD's SNAP (in the HP slot) drives the sample up to 3 times
// harder into a clip, for SPLN's time; RING (SD), RATL (RS), BELL (RC), REAL
// (BR), SLEW (SH) and BC (BC) crossfade to a second sample; STOP (OH) cuts the
// sample after 5.8 ms a step.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "private_e12.hpp"

namespace md_mde12 {} namespace md = md_mde12; namespace md_mde12 {
namespace e12 {
using md::multiply;

enum { PTCH, DEC, HP, OWN, STRT, RTRG, RTIM, BEND };
// What a machine's own control (slot 4) does.
enum Own { resonance, crossfade, rattle, stop, snap };
// The machines in id order (48..63), and their samples in private_e12.hpp's order:
// the sixteen, then the second samples of SD, RS, RC, BR, SH and BC.
enum { BD, SD, HT, LT, CP, RS, CB, CH, OH, RC, CC, BR, TA, TR, SH, BC };
struct Machine { unsigned char sample, second, own; };
const Machine machines[16] = {
    {0, 0, snap}, {1, 16, crossfade}, {2, 2, resonance}, {3, 3, resonance}, {4, 4, resonance},
    {5, 17, rattle}, {6, 6, resonance}, {7, 7, resonance}, {8, 8, stop}, {9, 18, crossfade},
    {10, 10, resonance}, {11, 19, crossfade}, {12, 12, resonance}, {13, 13, resonance},
    {14, 20, crossfade}, {15, 21, crossfade},
};

struct State {
    u32 index, fraction;   // where in the sample: whole frames, and Q27 of one
    i32 level;             // DEC's envelope, Q31
    i32 drive;             // SPLN's envelope of SNAP, Q31
    i32 bend;              // BEND's envelope, Q31: 1 at the hit, falling to 0
    u32 age;               // samples since the hit
    u32 retrigs;           // retrigs still to come
    u32 until;             // samples to the next
    md::Svf filter;
    u32 fresh;
};

namespace {
// The speed at which a key at middle C plays a sample as recorded: Q27 frames
// a sample from the key's phase increment (2^55 / middle C, Q28).
const i32 own_speed = 1414058298;
const i32 ptch_reach = 260096;                // 127 x 2048: (v - 64) / 32 octaves, Q16
const i32 ptch_middle = 131072;
const i32 bend_reach = 130048, bend_middle = 65536;   // 127 x 1024: (v - 64) / 64 octaves at the hit
const i32 bend_return = 1298000;              // and back: -60 dB in 2^19.8 samples, 21 s
// DEC: -60 dB after 2^(these) samples at DEC 0, 16, ..., 112, 127 (Q16): 15 ms .. 10 s; none above 126.5.
const i32 falls[9] = {614000, 614000, 734000, 810665, 897187, 995557, 1101005, 1228931, 1376256};
const i32 dec_open = 0x7f7f0000;             // 126.5
// HP: the cutoff in octaves above 20 Hz (Q16) at HP 0, 16, ..., 127: 45 Hz .. 6.3 kHz.
// HP 0 is in the capture already, so the filter runs only above it, or with HPQ.
const i32 highpass[9] = {76700, 169083, 278528, 354550, 417000, 473000, 516600, 535600, 535600};
// HPQ: half the damping (1 / 2Q) at HPQ 0, 16, ..., 127: Q 0.7, then 3.4 .. 6, and
// at HP 0 twice the Q (there the Machinedrum rings up to 34 dB; ours stops at 20,
// the filter's least stable damping).
const i32 dampings[9] = {1518500250, 319975063, 244813135, 223338299, 206158430, 195421011, 188978561, 182536110, 178241142};
const i32 damping_scale[9] = {0x40000000, 0x6ccccccd, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff};
// RTIM: the retrig period in samples at RTIM 0, 16, ..., 127, rounded to 32-sample blocks.
const i32 periods[9] = {32, 351, 670, 983, 1336, 1890, 2610, 3680, 5300};
// STOP (E12-OH): the sample stops after 5.8 ms a step, and 3 ms; never at 127.
const u32 stop_step = 256, stop_least = 132;
const i32 stop_fall = 393216;                 // then -60 dB in 64 samples
// RATL (E12-RS) crossfades late: the second sample's share at RATL 0, 16, ..., 127.
const i32 rattle_share[9] = {0, 0, 0, 2147484, 25769804, 77309411, 343597384, 1095216660, 0x7fffffff};
// SNAP (E12-BD): the sample driven 1 + 2 SNAP times into a clip at 0.36 (0.3 on the
// Machinedrum's scale, from the level of the clipped hits), for SPLN's time: the extra drive falls -60 dB after 2^(these)
// samples at SPLN 0, 16, ..., 127 (Q16), about 10 ms at 16, 260 ms at 64; held above 126.5.
const i32 snap_clip = 0x2e147ae1;
const i32 spln_falls[9] = {393216, 524288, 642000, 760000, 860000, 1030000, 1166541, 1245184, 1310720};

// A control on the original's 0..127 scale, rounded.
inline u32 value127(i32 parameter) { return (static_cast<u32>(multiply(md::positive(parameter), 127 << 16)) + 0x8000u) >> 16; }

// The sample at frame i, as Q31 halved: its 12 bits from bit 19.
inline i32 code(u32 i) {
    const u32 nibble = (static_cast<u32>(e12_data::low[i >> 1]) >> ((i & 1u) * 4)) & 15u;
    return (static_cast<i32>(e12_data::high[i]) * 16 + static_cast<i32>(nibble)) * 524288;
}
// Between frames i and i + 1, `fraction` (Q27) of the way.
inline i32 at(u32 i, u32 fraction) {
    const i32 a = code(i);
    return a + multiply(code(i + 1) - a, static_cast<i32>(fraction << 4));
}

struct Block {
    u32 increment;     // Q27 frames a sample, before BEND
    i32 bend;          // BEND's depth, Q16 octaves
    i32 decay_step, drive_step, frequency, damping;
    i32 share;         // the second sample's share
    i32 snap;          // SNAP's extra drive, Q31 of 2
    u32 begin, period, stop;
    bool decays, filtered, held;
    Block(const i32* p, opxy::Pitch pitch, unsigned machine) {
        const Machine& m = machines[machine];
        // (+ 1: the 0..127 scale lands a hair under each step, so PTCH 64 plays the sample exactly as recorded.)
        increment = md::transpose(md::scale(pitch[0], own_speed), multiply(md::positive(p[PTCH]), ptch_reach) + 1 - ptch_middle);
        bend = multiply(md::positive(p[BEND]), bend_reach) + 1 - bend_middle;
        decays = p[DEC] < dec_open;
        decay_step = md::decay(md::table9(falls, p[DEC]));
        // E12-BD's page has SNAP where the others have HP, and SPLN in its own slot.
        const bool snaps = m.own == e12::snap;
        filtered = !snaps && (p[HP] > 0x00800000 || (m.own == e12::resonance && p[OWN] > 0x00800000));
        frequency = md::cutoff(md::table9(highpass, p[HP]));
        damping = m.own == e12::resonance ? md::table9(dampings, p[OWN]) : dampings[0];
        damping = md::clamp(multiply(damping, md::table9(damping_scale, p[HP])), md::min_damping, md::one);
        share = m.own == e12::crossfade ? (p[OWN] >= 0x7f000000 ? md::one : multiply(md::positive(p[OWN]), 0x7f000000))
              : m.own == e12::rattle ? md::table9(rattle_share, p[OWN]) : 0;
        snap = snaps && p[OWN] > 0x00800000 ? md::positive(p[HP]) : 0;
        held = p[OWN] >= dec_open;
        drive_step = md::decay(md::table9(spln_falls, p[OWN]));
        begin = (value127(p[STRT]) * e12_data::strt[machine]) >> 8;
        period = (static_cast<u32>(md::table9(periods, p[RTIM])) + 16u) & ~31u;
        if (period < 32) period = 32;
        stop = m.own == e12::stop && p[OWN] < dec_open ? value127(p[OWN]) * stop_step + stop_least : 0;
    }
};
}

// Whether a sound is still to come however quiet it is now: a retrig.
inline bool waiting(const State& voice) { return voice.retrigs != 0; }
inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32) {
    voice.index = 0xffffffffu;   // nothing playing
    voice.fraction = 0;
    voice.level = voice.drive = voice.bend = 0;
    voice.age = voice.retrigs = voice.until = 0;
    voice.filter.reset();
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch, unsigned machine) {
    const Machine& m = machines[machine];
    const Block b(parameters, pitch, machine);
    const u32 first = e12_data::start[m.sample], frames = e12_data::start[m.sample + 1] - first;
    const u32 second = e12_data::start[m.second], second_frames = e12_data::start[m.second + 1] - second;
    const u32 length = b.share && second_frames > frames ? second_frames : frames;
    if (voice.fresh) {
        voice.fresh = 0;
        voice.index = b.begin;
        voice.fraction = 0;
        voice.level = voice.drive = voice.bend = md::one;
        voice.age = 0;
        const u32 r = value127(parameters[RTRG]);
        voice.retrigs = r >= 126 ? 0xffffffffu : r > 1 ? r - 1 : 0;
        voice.until = b.period;
        voice.filter.reset();
    }
    // BEND: the pitch starts `bend` away and returns, linear across the block.
    md::Glide glide;
    const i32 bend_next = multiply(voice.bend, md::per_block(md::decay(bend_return)));
    glide.plan(b.increment, multiply(voice.bend, b.bend), multiply(bend_next, b.bend));
    voice.bend = bend_next;
    const i32 level_a = e12_data::level[m.sample], level_b = e12_data::level[m.second];
    const i32 cut_step = md::decay(stop_fall);

    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        i32 x = 0;
        const u32 step = glide.step();
        if (voice.index < length) {
            x = voice.index < frames ? multiply(at(first + voice.index, voice.fraction), level_a) : 0;
            if (b.share) {
                const i32 s = voice.index < second_frames ? multiply(at(second + voice.index, voice.fraction), level_b) : 0;
                x += multiply(s - x, b.share);
            }
            const u32 next = voice.fraction + step;
            voice.index += next >> 27;
            voice.fraction = next & 0x07ffffffu;
        }
        if (b.snap) {
            // 1 + 2 SNAP times as hard, falling back over SPLN's time, into the clip.
            x = md::clamp(md::add(x, md::gain(multiply(x, multiply(voice.drive, b.snap)), 1)), -snap_clip, snap_clip);
            if (!b.held) voice.drive = multiply(voice.drive, b.drive_step);
        }
        if (b.filtered) {
            // A quarter in and four times out: the ringing states stay in range.
            voice.filter.run(multiply(x, 0x20000000), b.frequency, b.damping);
            x = md::gain(voice.filter.high, 2);
        }
        output[i] = md::finish(multiply(x, voice.level));
        if (b.decays) voice.level = multiply(voice.level, b.decay_step);
        if (b.stop && voice.age >= b.stop) voice.level = multiply(voice.level, cut_step);
        ++voice.age;
        if (voice.retrigs && --voice.until == 0) {
            voice.until = b.period;
            if (voice.retrigs != 0xffffffffu) --voice.retrigs;
            voice.index = b.begin;
            voice.fraction = 0;
        }
    }
}
}

// The sixteen machines.
namespace e12_bd {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::BD); }
}
namespace e12_sd {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::SD); }
}
namespace e12_ht {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::HT); }
}
namespace e12_lt {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::LT); }
}
namespace e12_cp {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::CP); }
}
namespace e12_rs {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::RS); }
}
namespace e12_cb {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::CB); }
}
namespace e12_ch {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::CH); }
}
namespace e12_oh {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::OH); }
}
namespace e12_rc {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::RC); }
}
namespace e12_cc {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::CC); }
}
namespace e12_br {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::BR); }
}
namespace e12_ta {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::TA); }
}
namespace e12_tr {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::TR); }
}
namespace e12_sh {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::SH); }
}
namespace e12_bc {
typedef e12::State State;
inline void reset(State& s, Common& c, u32 owner) { e12::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { e12::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p, opxy::Pitch pitch) { e12::render(s, c, o, p, pitch, e12::BC); }
}
}
