#pragma once
// Two-operator FM as the Machinedrum's EFM machines make it, measured through
// the emulator (reference/fit_efm_bd.py and the other EFM fitters). Written
// from scratch: a sine carrier, frequency-modulated by a sine modulator at a
// ratio of the carrier's pitch.
//
// - MFRQ sets the ratio, MFRQ / 16 (0 plays as 1: 1/16 .. 7.94), and the
//   modulator follows the carrier's pitch sweeps.
// - MOD sets the deviation, MOD / 16 times the carrier's frequency, so the
//   index is MOD / (16 x ratio): about 1 at MOD 64 and MFRQ 64.
// - The modulation is true FM: the modulator moves the carrier's phase
//   increment. With feedback (MFB, FB) the modulator's own output is fed back
//   into its phase one sample late, about MFB / 20 radians at full output,
//   which both roughens it and, because the late feedback leaves the
//   modulator with a mean, pulls the carrier's pitch down: the Machinedrum's
//   drift of the carrier with MFB, and its chaos near the top.
// - Both oscillators start 0.11 cycle in, the carrier heading down, in EFM-BD
//   and EFM-RS, whose first sample is about half their peak. The other machines
//   start at a zero crossing, one sample in, and rise over the first quarter
//   cycle (first sample / peak 0.06 on EFM-CB, 0.09 on EFM-CP, 0.07 on EFM-SD,
//   0.05 on EFM-HH): they pass from_zero or fed_start to reset().
#include "md_dsp.hpp"

namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace efm {
const u32 start_phase = 0x1c28f5c3u;   // 0.11 cycle
const u32 from_zero = 0;
const u32 fed_start = 0x02d0e560u;     // 0.011 cycle: EFM-SD's pair, and EFM-CP's modulator

// The swept machines' times (EFM-BD and EFM-XT measure the same), -60 dB as
// Q16 log2 samples at the controls 0, 16, ..., 112, 127 (md::table9).
// RDEC: the sweep's time constant 1.8 ms .. 2.4 s.
const i32 rdec_times[9] = {596307, 692903, 786813, 871323, 957382, 1039287, 1119398, 1200248, 1276625};
// DEC: 35 ms .. 12.6 s.
const i32 dec_times[9] = {694157, 736891, 793416, 854102, 934567, 1011122, 1087794, 1170568, 1250679};
// MDEC: the modulation's time constant 2 ms .. 4.8 s.
const i32 mdec_times[9] = {606269, 710141, 808609, 896349, 986860, 1078748, 1172753, 1264000, 1342161};
// HPF, where a machine has one: one pole from off through 410 Hz (16) to
// 3.3 kHz (127), as 1 - e^(-2 pi f / fs) (measured on EFM-SD).
const i32 hpf_coefficients[9] = {0, 120985838, 238694234, 350077827, 453755706, 551453243, 639008794, 720745177, 802659084};

// The feedback amount for a control (Q31 0..1): MFB / 20 radians at 127,
// 1.01 cycles, as the shift that run() applies (Q31 quarter cycles).
inline i32 feedback(i32 control) { return multiply(positive(control), 0x205bc01a); }

// A carrier and its modulator.
struct Pair {
    u32 carrier, modulator;
    i32 last;   // the modulator's previous output, for feedback
    void reset(u32 phase = start_phase) { carrier = modulator = phase; last = 0; }
    // One sample: the carrier's sine (Q31). `increment` is the carrier's,
    // `deviation` its swing at full modulator output (both phase increments),
    // `modulation` the modulator's increment and `fb` from feedback().
    i32 run(u32 increment, i32 deviation, u32 modulation, i32 fb) {
        const i32 y = sine(modulator + (static_cast<u32>(multiply(fb, last)) << 3));
        last = y;
        modulator += modulation;
        const i32 out = sine(carrier);
        carrier += increment + static_cast<u32>(multiply(deviation, y));
        return out;
    }
};

// A carrier fed back on itself, frequency-modulated by a plain modulator: the
// pair the other way round (EFM-CB, EFM-HH). The same arguments as Pair::run.
struct Fed {
    u32 carrier, modulator;
    i32 last;   // the carrier's previous output, for feedback
    void reset(u32 phase = start_phase) { carrier = modulator = phase; last = 0; }
    i32 run(u32 increment, i32 deviation, u32 modulation, i32 fb) {
        const i32 y = sine(carrier + (static_cast<u32>(multiply(fb, last)) << 3));
        last = y;
        carrier += increment + static_cast<u32>(multiply(deviation, sine(modulator)));
        modulator += modulation;
        return y;
    }
};

// A phase increment that moves in a straight line across one block.
struct Ramp {
    u32 value;
    i32 slope;
    void plan(u32 now, u32 next) {
        value = now;
        slope = (static_cast<i32>(next >> 1) - static_cast<i32>(now >> 1)) / 32;
    }
    u32 step() { const u32 v = value; value += static_cast<u32>(slope); return v; }
};

// A control (Q31 0..1) as a multiple, control * 127 / 16, Q28 (0 .. 7.94):
// RAMP, MOD and MFRQ read this way.
inline i32 sixteenths(i32 control) { return multiply(positive(control), 0x7f000000); }

// MFRQ as the modulator's ratio to the carrier, where 0 plays as 1 (a
// sixteenth of the carrier), as the Machinedrum's does.
inline i32 ratio(i32 control) {
    const i32 r = sixteenths(control);
    return r < 0x01000000 ? 0x01000000 : r;
}

// The swept machines' block: the carrier's increment `base` multiplied by
// 1 + ramp_from x ramp (RAMP's sweep), the modulator at `ratio` to it, and the
// deviation (`depth`, Q28 multiples of the carrier) under MDEC's envelope.
// Each runs in a straight line across the block; the envelopes step on.
struct Sweep {
    Ramp carrier, modulator, deviation;
    void plan(u32 base, i32 ramp_from, i32& ramp, i32 ramp_step, i32 ratio, i32 depth, i32& envelope, i32 depth_step) {
        const i32 ramp_next = multiply(ramp, per_block(ramp_step));
        const i32 envelope_next = multiply(envelope, per_block(depth_step));
        const u32 now = scale(base, 0x10000000 + multiply(ramp_from, ramp));
        const u32 next = scale(base, 0x10000000 + multiply(ramp_from, ramp_next));
        carrier.plan(now, next);
        modulator.plan(scale(now, ratio), scale(next, ratio));
        deviation.plan(scale(now, multiply(depth, envelope)), scale(next, multiply(depth, envelope_next)));
        ramp = ramp_next;
        envelope = envelope_next;
    }
};
}
}
