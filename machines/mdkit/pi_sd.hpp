#pragma once
// PI-SD: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_modal.hpp"

// A Machinedrum PI-SD-style snare: a struck head over snare wires, written
// from scratch in fixed point and fitted to the Machinedrum's own
// (reference/fit_pi_sd.py). Its page:
//   PTCH  DEC   HARD  RING
//   TENS  RVOL  RDEC
// As measured: the head rings in five modes, at 1, 1.21 and 1.77 times a
// fundamental from 86 Hz (PTCH 0) to 340 Hz (127), with quieter ones at 0.59
// and 3 times; TENS tightens it by up to 9 %. DEC sets their ring, -60 dB in
// 20 to 280 ms. RING brings up a sixth mode at 2.75 times, which rings on. The
// strike adds a 5 ms burst of noise, falling 6 dB an octave from about 1.5 kHz
// (its spectrum falls from 1 to 13 kHz by 12 dB, and it carries the 300 to
// 1000 Hz under the head); the wires (RVOL, as its square) rattle for RDEC's
// time, from a few ms to 0.7 s, in equal power per octave from 500 Hz to 8 kHz,
// less above. HARD only tunes the head up 3 %.
namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace pi_sd {
const unsigned mode_count = 6;
struct State {
    modal::Mode modes[mode_count];
    i32 burst;       // the strike's noise envelope, Q31
    i32 wires;       // the wires' envelope, Q31
    u32 seed;
    i32 low, high;   // the noise: its 200 Hz high-pass and 1.5 kHz low-pass
    i32 top;         // the 5 kHz low-pass over the wires' upper part
    u32 hold;        // blocks of the strike's burst still held
    u32 age;         // blocks since the strike, through the head's swell
    i32 wires_out;   // the wires' gain the last block ended on, which the next starts from
    u32 carrying;    // it is set: a note's first block starts from the envelope
    u32 fresh;
};
namespace {
using md::multiply;

enum { PTCH, DEC, HARD, RING, TENS, RVOL, RDEC };

// The fundamental from 86 Hz (PTCH 0) to 340 Hz (127), Q16 octaves from middle C
// at PTCH 0, 16, ..., 127 (TENS 40, HARD 64), and TENS's -3 .. +6 % around it.
const i32 base[9] = {-102187, -86150, -69702, -53343, -37101, -20532, -4022, 12526, 24261};
const i32 tens_low = -2865, tens_span = 8947;
// The modes: Q16 octaves above the fundamental, and their levels (Q31).
const i32 ratios[mode_count] = {0, 17866, 54199, -49887, 103872, 95680};
const i32 levels[mode_count] = {md::one, 0x5ffc8890, 0x30000000, 0x08138562, 0x040c3714, 0};
// DEC: -60 dB in 20 .. 276 ms, Q16 log2 samples at DEC 0, 16, ..., 127; the third
// mode shortens to 0.72 of it at the top.
const i32 decay_times[9] = {641246, 688593, 761034, 791506, 823868, 847934, 861746, 878908, 889404};
const i32 third_shorter = -31000;
// RING: the sixth mode's level, Q31 at RING 0, 16, ..., 127 (-36 .. -21 dB under
// the fundamental), ringing as long as the head at RING 0 and 2.5 octaves longer
// at 127.
const i32 ring_levels[9] = {34035322, 30334013, 42847932, 57138636, 101608459, 107629139, 127917471, 152030200,
                            191394682};
const i32 ring_longer = 163840;
// The strike: noise under 1.5 kHz falling 60 dB in 25 ms after a 4.4 ms hold.
const i32 burst_time = 662344;
const u32 burst_hold = 3;   // blocks
const i32 burst_level = 0x20000000;
// HARD: a harder strike tunes the head up 3 % (Q16 octaves).
const i32 hard_low = -1121, hard_span = 3040;
// The wires: RDEC's fall, -60 dB in 20 ms .. 0.68 s at RDEC 0, 16, ..., 127.
const i32 wire_times[9] = {641246, 662344, 694157, 727880, 793416, 831752, 880050, 924488, 974658};
const i32 wire_level = 0x3c000000;
// One-poles: the noise above 200 Hz; under 1.5 kHz; and under 5 kHz, the wires' top.
const i32 noise_low = 0x03a5e354, noise_high = 0x18a13c35, wire_top = 0x41380000;
// The head swells in over its first 4 blocks (6 ms), as the Machinedrum's.
const u32 swell_blocks = 4;
const i32 swell_step = md::one / 4;
// The fundamental peaks 2 dB over the Machinedrum's (reference/pi_measure.py, LEVEL_DB).
const i32 body_level = 0x1b000000;

struct Block {
    i32 base, step, third_step, ring_step, ring_level, wire_amount, wire_step;
    explicit Block(const i32* p) {
        base = md::table9(pi_sd::base, p[PTCH]) + md::span(p[TENS], tens_low, tens_low + tens_span)
               + md::span(p[HARD], hard_low, hard_low + hard_span) - 352;   // 352: TENS 40 and HARD 64
        const i32 time = md::table9(decay_times, p[DEC]);
        step = md::decay(time);
        third_step = md::decay(time + multiply(md::positive(p[DEC]), third_shorter));
        ring_step = md::decay(time + multiply(md::positive(p[RING]), ring_longer));
        ring_level = md::table9(ring_levels, p[RING]);
        wire_amount = multiply(md::squared(p[RVOL]), wire_level);
        wire_step = md::decay(md::table9(wire_times, p[RDEC]));
    }
};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common& shared, u32 owner) {
    for (unsigned k = 0; k < mode_count; ++k) {
        voice.modes[k].phase = voice.modes[k].increment = 0;
        voice.modes[k].level = 0;
    }
    voice.burst = voice.wires = 0;
    voice.low = voice.high = voice.top = 0;
    voice.hold = 0;
    voice.age = swell_blocks;
    voice.seed = 0x9e3779b9u * owner;
    voice.carrying = 0;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(State& voice, Common&, opxy::Output output, const i32* parameters, opxy::Pitch pitch) {
    const Block b(parameters);
    if (voice.fresh) {
        voice.fresh = 0;
        for (unsigned k = 0; k < mode_count; ++k) {
            voice.modes[k].phase = 0;
            voice.modes[k].level = md::one;
        }
        voice.burst = md::one;
        voice.wires = md::one;
        voice.hold = burst_hold;
        voice.age = 0;
        voice.carrying = 0;
    }
    i32 mixed[opxy::block_frames];
    for (unsigned i = 0; i != opxy::block_frames; ++i) mixed[i] = 0;
    const i32 swell = voice.age < swell_blocks ? swell_step * static_cast<i32>(voice.age) : md::one;
    const i32 swell_next = voice.age + 1 < swell_blocks ? swell + swell_step : md::one;
    if (voice.age < swell_blocks) ++voice.age;
    for (unsigned k = 0; k < mode_count; ++k) {
        modal::Mode& mode = voice.modes[k];
        if (mode.level < 0x00010000) continue;
        mode.increment = md::transpose(pitch[0], b.base + ratios[k]);
        const i32 level = k == mode_count - 1 ? b.ring_level : levels[k];
        const i32 step = k == mode_count - 1 ? b.ring_step : k == 2 ? b.third_step : b.step;
        const i32 amount = multiply(level, body_level);
        // The head's fundamental and its neighbour swell in; the upper modes sound at once.
        if (k < 2) modal::ring(mode, step, multiply(amount, swell), multiply(amount, swell_next), mixed);
        else modal::ring(mode, step, amount, mixed);
    }
    if (voice.burst > 0x00100000 || voice.wires > 0x00100000) {
        const i32 burst_step = voice.hold ? md::one : md::decay(burst_time);
        const bool first = voice.hold == burst_hold;
        if (voice.hold) --voice.hold;
        // The two gains in straight lines across the block.
        const i32 burst_next = multiply(voice.burst, md::per_block(burst_step));
        const i32 wires_next = multiply(voice.wires, md::per_block(b.wire_step));
        // The wires' from where the last block's ended, so RVOL moved glides.
        i32 burst = multiply(voice.burst, burst_level), wires = voice.carrying ? voice.wires_out : multiply(voice.wires, b.wire_amount);
        voice.wires_out = multiply(wires_next, b.wire_amount);
        voice.carrying = 1;
        if (first) burst += (burst >> 1) + (burst >> 2);   // the stick's first 1.5 ms is 5 dB over the rest
        const i32 burst_slope = (multiply(burst_next, burst_level) - burst) / 64;
        const i32 wires_slope = (voice.wires_out - wires) / 64;
        voice.burst = burst_next;
        voice.wires = wires_next;
        for (unsigned i = 0; i != opxy::block_frames; ++i) {
            const i32 n = opxy::q31::half(md::noise(voice.seed));
            const i32 high = n - md::smooth(voice.low, n, noise_low);
            const i32 body = md::smooth(voice.high, high, noise_high);
            const i32 top = md::smooth(voice.top, high, wire_top);
            mixed[i] += multiply(body, burst) + multiply(body + (top >> 3) + (top >> 4), wires);
            burst += burst_slope;
            wires += wires_slope;
        }
    }
    for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = md::finish(md::gain(mixed[i], 1));
}
}
}
