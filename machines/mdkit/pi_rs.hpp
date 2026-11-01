#pragma once
// PI-RS: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_modal.hpp"

// A Machinedrum PI-RS-style rim shot: a stick across the rim and head of a
// snare, written from scratch in fixed point and fitted to the Machinedrum's
// own (reference/fit_pi_rs.py). Its page:
//   PTCH  DEC   HARD  RING
//   RVOL  RDEC
// As measured: the rim rings in nine modes at fixed frequencies, 144 to 668 Hz,
// that PTCH does not move; PTCH tunes the head under them (PI-SD's, 86 to
// 340 Hz) and with it which rim modes sound, as measured at each step. DEC sets the ring, -60 dB in 0.2 to 1.7 s. The stick's crack is
// a 5 ms burst of noise, about 1.7 kHz at PTCH 64 and moving with it, that
// HARD makes louder. RING brings up the head's ringing mode (2.75 times), and
// RVOL and RDEC are the snare wires: RVOL 127 adds 16 dB over RVOL 0, in equal
// power per octave from 500 Hz to 8 kHz and less above, falling 60 dB in 100 ms
// (RDEC 0) to 650 ms (127), 190 ms at the design's 40.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace pi_rs {
const unsigned rim_count = 9, mode_count = rim_count + 3;   // the rim, two head modes, the ring
namespace {
using md::multiply;

enum { PTCH, DEC, HARD, RING, RVOL, RDEC };

// The rim's modes, Q16 octaves from middle C: 144, 176, 230, 299, 341, 384, 505, 538, 668 Hz;
// and their levels (Q31) at PTCH 0, 16, ..., 127: the head under them sets which ring.
const i32 rim[rim_count] = {-56457, -37699, -12265, 12655, 24912, 36181, 62178, 68215, 88555};
const i32 rim_levels[rim_count][9] = {
    {845832629, 1007422162, 1987572851, 2147483647, 494141766, 361985823, 359882141, 471987111, 292238252},
    {77704515, 287514164, 208998918, 515843532, 375892301, 150588163, 48829995, 186062659, 61933934},
    {67116644, 253788186, 717074090, 264521237, 588496236, 552871044, 1028750284, 115044042, 69509842},
    {120998362, 114018038, 106258335, 376024241, 1006414341, 385527113, 1402648783, 1215293396, 261150218},
    {42137956, 22363785, 13974356, 29837903, 88582575, 74812595, 70881580, 323871796, 270296125},
    {247388728, 144997592, 169160179, 60163131, 349484761, 973005614, 238800770, 814274251, 376324840},
    {19374719, 86440029, 86541463, 75636681, 50902038, 57131915, 171902798, 977268097, 177090694},
    {66213226, 75252469, 164596695, 107756593, 135270351, 70876448, 180026898, 787715101, 101280410},
    {127941750, 70798399, 23324284, 124390668, 79285846, 97543893, 42149596, 121881060, 351928600},
};
// The head: PI-SD's fundamental (Q16 octaves from middle C at PTCH 0, 16, ..., 127)
// and its second mode, 12 dB under the rim, ringing half as long.
const i32 head[9] = {-102187, -86150, -69702, -53343, -37101, -20532, -4022, 12526, 24261};
const i32 head_second = 17866, head_level = 0x20000000, head_shorter = -65536;
// RING: the head's ringing mode at 2.75 times, Q31 at RING 0, 16, ..., 127, ringing
// as long as the rim.
const i32 ring_ratio = 95680;
const i32 ring_levels[9] = {0, 10737418, 42949673, 107374182, 214748365, 429496730, 858993459, 1288490189,
                            1717986918};
// DEC: -60 dB in 0.2 .. 1.66 s, Q16 log2 samples at DEC 0, 16, ..., 127.
const i32 decay_times[9] = {858952, 858952, 929101, 965924, 995756, 1014830, 1032973, 1046902, 1059040};
// The crack: noise in a band at 1.7 kHz (PTCH 64), +-0.95 octaves with PTCH (Q16
// octaves above 20 Hz), held 4.4 ms then falling 60 dB in 8 ms; HARD sets its
// level, -9 dB at 0 to +1.5 dB at 127 around HARD 64.
const i32 crack_centre = 341403, crack_ptch = 124518;   // the band's centre sits under the heard pitch
// The crack's level by PTCH, an eighth of it at 0, 16, ..., 127 (Q31): +14 dB at
// PTCH 0 to -13 dB at 127 around PTCH 64.
const i32 crack_ptch_level[9] = {1392643250, 920110271, 607910827, 401642701, 268435456, 181484642, 122698677,
                                 82004914, 58055033};
const u32 crack_hold = 3;   // blocks
// Every mode here is under 1 kHz, so past the strike's first blocks they run at a quarter
// of the rate (modal::Low).
const u32 low_after = 8;   // blocks: 12 ms
const i32 crack_time = 554612;
const i32 crack_quiet = 0x16b54338, crack_loud = 0x4c106ba6;   // HARD 0 and 127, at half scale (HARD 64: 0x40000000)
const i32 crack_level = 0x67000000;
// The wires: RDEC's fall, -60 dB in 100, 122, 160, 200, 240, 305, 375, 500 and
// 650 ms at RDEC 0, 16, ..., 127 (far longer than PI-SD's at low RDEC).
const i32 wire_times[9] = {793416, 812217, 837854, 858952, 876190, 898851, 918386, 945586, 970392};
const i32 wire_level = 0x24000000;
const i32 noise_low = 0x03a5e354, wire_body = 0x18a13c35, wire_top = 0x41380000;   // the one-poles
// The rim peaks 2 dB over the Machinedrum's (reference/pi_measure.py, LEVEL_DB).
const i32 body_level = 0x03600000;

}
struct Block {
    i32 amounts[mode_count];
    u32 increments[mode_count];
    i32 fall, head_fall, crack_tune, crack_amount, crack_step, wire_amount, wire_step;
    void plan(const i32* p, u32 middle) {
        for (unsigned k = 0; k < rim_count; ++k) amounts[k] = multiply(md::table9(rim_levels[k], p[PTCH]), body_level);
        amounts[rim_count] = amounts[rim_count + 1] = multiply(head_level, body_level);
        amounts[rim_count + 2] = multiply(md::table9(ring_levels, p[RING]), body_level);
        const i32 tuned = md::table9(pi_rs::head, p[PTCH]);
        for (unsigned k = 0; k < rim_count; ++k) increments[k] = md::transpose(middle, rim[k]);
        increments[rim_count] = md::transpose(middle, tuned);
        increments[rim_count + 1] = md::transpose(middle, tuned + head_second);
        increments[rim_count + 2] = md::transpose(middle, tuned + ring_ratio);
        const i32 time = md::table9(decay_times, p[DEC]);
        fall = md::per_block(md::decay(time));
        head_fall = md::per_block(md::decay(time + head_shorter));
        crack_tune = md::cutoff(crack_centre + multiply(p[PTCH] - 0x40000000, crack_ptch * 2));
        const i32 hard = md::positive(p[HARD]);
        crack_amount = hard < 0x40000000 ? md::span(hard * 2, crack_quiet, 0x40000000)
                                         : md::span((hard - 0x40000000) * 2, 0x40000000, crack_loud);
        crack_amount = md::gain(multiply(multiply(crack_amount, crack_level), md::table9(crack_ptch_level, p[PTCH])), 3);
        crack_step = md::decay(crack_time);
        wire_amount = multiply(md::squared(p[RVOL]), wire_level);
        wire_step = md::decay(md::table9(wire_times, p[RDEC]));
    }
};
struct State {
    modal::Mode modes[mode_count];
    i32 crack;       // the stick's burst envelope, Q31
    i32 wires;       // the wires' envelope, Q31
    u32 hold;        // blocks of the crack still held
    u32 seed;
    i32 low, high, top;   // the wires: 200 Hz high-pass, 1.5 kHz low-pass, 5 kHz over their upper part
    md::Svf band;    // the crack's band
    u32 fresh;
    u32 stale;       // the block's figures want working out, whatever the controls read
    u32 seen_pitch;
    i32 seen[8];     // the controls they were worked out from
    Block block;
    u32 age;         // blocks since the strike, up to low_after
    i32 low_end;     // the low modes' value where the last block ended
};

// The stick's burst and the wires, one sample of noise between them: out of line, so the
// loop has a hardware loop and all the registers to itself, and on locals, as `mixed` might
// otherwise be the voice and every field would be read and written back each sample.
__attribute__((noinline)) inline void crack_and_wires(State& voice, const Block& b, i32 crack_step, i32* mixed) {
    u32 seed = voice.seed;
    i32 low = voice.low, high_pole = voice.high, top_pole = voice.top, crack = voice.crack, wires = voice.wires;
    md::Svf band = voice.band;
    const i32 tune = b.crack_tune, amount = b.crack_amount, wire_amount = b.wire_amount, wire_step = b.wire_step;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 n = md::noise_eighth(seed);
        band.run(n, tune, 0x18000000);
        const i32 high = n - md::smooth(low, n, noise_low);
        const i32 body = md::smooth(high_pole, high, wire_body);
        const i32 top = md::smooth(top_pole, high, wire_top);
        mixed[i] += md::gain(multiply(band.band, multiply(crack, amount)), 3)
                    + multiply(body + (top >> 3) + (top >> 4), multiply(wires, wire_amount)) * 4;
        crack = multiply(crack, crack_step);
        wires = multiply(wires, wire_step);
    }
    voice.seed = seed;
    voice.low = low; voice.high = high_pole; voice.top = top_pole;
    voice.crack = crack; voice.wires = wires;
    voice.band = band;
}
// Once the crack has reached zero its band adds nothing, and nothing reads it before the next strike.
__attribute__((noinline)) inline void wires_alone(State& voice, const Block& b, i32* mixed) {
    u32 seed = voice.seed;
    i32 low = voice.low, high_pole = voice.high, top_pole = voice.top, wires = voice.wires;
    const i32 wire_amount = b.wire_amount, wire_step = b.wire_step;
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        const i32 n = md::noise_eighth(seed);
        const i32 high = n - md::smooth(low, n, noise_low);
        const i32 body = md::smooth(high_pole, high, wire_body);
        const i32 top = md::smooth(top_pole, high, wire_top);
        mixed[i] += multiply(body + (top >> 3) + (top >> 4), multiply(wires, wire_amount)) * 4;
        wires = multiply(wires, wire_step);
    }
    voice.seed = seed;
    voice.low = low; voice.high = high_pole; voice.top = top_pole;
    voice.wires = wires;
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    for (unsigned k = 0; k < mode_count; ++k) {
        voice.modes[k].phase = voice.modes[k].increment = 0;
        voice.modes[k].level = 0;
    }
    voice.crack = voice.wires = 0;
    voice.hold = 0;
    voice.low = voice.high = voice.top = 0;
    voice.band.reset();
    voice.seed = 0x7f4a7c15u * owner;
    voice.stale = 1;
    voice.age = 0;
    voice.low_end = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    // Only when a control or the key has moved.
    u32 moved = voice.stale | (pitch[0] ^ voice.seen_pitch);
    for (unsigned c = 0; c < 8; ++c) moved |= static_cast<u32>(parameters[c] ^ voice.seen[c]);
    if (moved) {
        voice.stale = 0;
        voice.seen_pitch = pitch[0];
        for (unsigned c = 0; c < 8; ++c) voice.seen[c] = parameters[c];
        voice.block.plan(parameters, pitch[0]);
    }
    const Block& b = voice.block;
    if (voice.fresh) {
        voice.fresh = 0;
        for (unsigned k = 0; k < mode_count; ++k) {
            voice.modes[k].phase = 0;
            voice.modes[k].level = md::one;
        }
        voice.crack = voice.wires = md::one;
        voice.hold = crack_hold;
        voice.band.reset();
        voice.age = 0;
    }
    i32 mixed[opxy::block_frames];
    for (unsigned i = 0; i != opxy::block_frames; ++i) mixed[i] = 0;
    if (voice.age < low_after) {
        ++voice.age;
        for (unsigned k = 0; k < mode_count; ++k) {
            modal::Mode& mode = voice.modes[k];
            if (mode.level < 0x00010000 || !b.amounts[k]) continue;
            mode.increment = b.increments[k];
            modal::ring_fall(mode, k == rim_count || k == rim_count + 1 ? b.head_fall : b.fall, b.amounts[k], b.amounts[k], mixed);
        }
    } else {
        const bool opening = voice.age == low_after;
        voice.age = low_after + 1;
        modal::Low low;
        for (unsigned i = 0; i <= modal::low_frames; ++i) low.points[i] = 0;
        if (!opening) low.points[0] = voice.low_end;
        for (unsigned k = 0; k < mode_count; ++k) {
            modal::Mode& mode = voice.modes[k];
            if (mode.level < 0x00010000 || !b.amounts[k]) continue;
            mode.increment = b.increments[k];
            modal::ring_fall_low(mode, k == rim_count || k == rim_count + 1 ? b.head_fall : b.fall, b.amounts[k], low, opening);
        }
        voice.low_end = low.points[modal::low_frames];
        modal::spread_low(low.points, mixed);
    }
    if (voice.crack > 0x00100000 || voice.wires > 0x00100000) {
        if (voice.crack) {
            const i32 crack_step = voice.hold ? md::one : b.crack_step;
            if (voice.hold) --voice.hold;
            crack_and_wires(voice, b, crack_step, mixed);
        } else {
            wires_alone(voice, b, mixed);
        }
    }
    modal::finish_doubled(mixed, output);
}
}
}
