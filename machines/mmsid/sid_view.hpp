#pragma once
// SID-6581's view, after the design's 8-bit stepped wave (docs/design/Main.dc.html,
// the SID-6581 branch of the Monomachine views): the wave the controls make,
// quantized to 13 levels and drawn as steps across the stage, three cycles in
// 47 steps on three of the mutable paths, which share their end corners. The
// axis is art (tools/art/build.mjs). The design's words
// ("saw · ring", "mod from previous track") are left out: they change with the
// steps, and the device takes only fixed text or one node a stroke; the header
// names the steps. The wave is the sound's own (voices/sid_6581.hpp): its pulse width
// with PWAD's sweep moving since the note, its mixed wave, its sync at MFRQ's
// ratio to middle C; the ring is drawn as the design draws it, the wave times
// the modulator's sine.
#include "md_view.hpp"
#include "md_rig.hpp"
#include "sid_6581.hpp"

namespace md {
namespace sid_6581 {
namespace draw {
const i32 left = 12, cy = 80;
const i32 step_px = 9935;          // (468 - 12) / 47 steps, Q10 (9.7 px)
const i32 level_px = 9557;         // 56 px / 6, Q10
const u32 steps = 47;
const u32 step_phase = 274145072u; // three cycles over 47 steps: 3 x 2^32 / 47

// The heard wave's value at oscillator phase p, Q29 (as the sound's).
inline i32 value(u32 wave, u32 p, u32 step, u32 duty, u32 noise_key) {
    if (wave == noise_wave) {
        u32 x = (step + 1) * 2654435761u ^ noise_key;   // a new value each step, moving with time
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return static_cast<i32>(x >> 2) - md::sid_6581::unit;
    }
    return wave_at(wave, p, duty);
}
}

template <class F>
inline void view(F& frame, const i32* slots, const view::Moment& moment) {
    const u32 wave = choice(slots[WAVE], 5), mod = choice(slots[MOD], 4);
    const bool synced = mod == sync || mod == ring_sync, ringing = mod == ring || mod == ring_sync;
    // While a note plays (its first two seconds) the one wave moves: PWAD's sweep
    // since the note and a phase scroll. Idle, it is the controls' wave, unswept.
    const i32 ph = moment.phase(2048, 512);   // Q10 over 2 s
    const u32 rate = static_cast<u32>(md::table9(sweep_rate, slots[PWAD]));
    const u32 age = ph >= 0 ? static_cast<u32>(moment.age) : 0u;
    const u32 spin = ph >= 0 ? static_cast<u32>(ph) << 23 : 0u;   // two cycles over the two seconds
    const u32 width = (static_cast<u32>(multiply(md::positive(slots[PW]), 0x7f000000)) << 1) + rate * 43u * age;   // 44100 / 1024
    const u32 apart = width >= 0x80000000u ? width - 0x80000000u : 0x80000000u - width;
    const u32 duty = apart < 0x100u ? 0xffffff00u : 0u - apart;
    // The modulator's ratio to the oscillator at middle C, Q28: 2^((MFRQ - 32) / 32).
    const i32 ratio = static_cast<i32>(md::transpose(1u << 26, multiply(md::positive(slots[MFRQ]), ratio_span) - ratio_low) << 2);
    const u32 noise_key = wave == noise_wave && ph >= 0 ? (age >> 5) * 0x9e3779b9u : 0x2545f491u;

    i32 ys[draw::steps];
    for (u32 k = 0; k < draw::steps; ++k) {
        const u32 p = k * draw::step_phase + spin;
        u32 q = p;
        if (synced) q = (ringing ? 0u : 0x80000000u) + (static_cast<u32>(multiply(static_cast<i32>(p >> 1), ratio)) << 4);
        i32 s = draw::value(ringing ? tri : wave, q, k, duty, noise_key);
        if (ringing) {   // the design's ring: the wave times the modulator's sine
            const u32 m = static_cast<u32>(multiply(static_cast<i32>(p >> 1), ratio)) << 4;
            s = multiply(s, md::sine(m + 0x0c3c9eecu)) * 2;     // + 0.3 rad
        }
        s = md::clamp(s, -md::sid_6581::unit, md::sid_6581::unit);
        // 13 levels: round(6 s).
        const u32 t = static_cast<u32>(s + md::sid_6581::unit);                // 0 .. 2^30
        const i32 n = static_cast<i32>(((t >> 8) * 6u + (1u << 20)) >> 21) - 6;
        ys[k] = draw::cy * opxy::draw::unit - n * draw::level_px;
    }
    const u32 key = md::view::key_of(slots[WAVE], slots[MOD], slots[MFRQ], static_cast<i32>(duty >> 1)) ^ noise_key ^ spin;
    // Each step's two corners, left to right; path j draws corners 31 j .. 31 j + 31,
    // so neighbouring paths share a corner and the risers between them are drawn.
    opxy::draw::Point corners[2 * draw::steps];
    for (u32 k = 0; k < draw::steps; ++k) {
        const i32 x0 = draw::left * opxy::draw::unit + static_cast<i32>(k) * draw::step_px;
        corners[2 * k].x = x0; corners[2 * k].y = ys[k];
        corners[2 * k + 1].x = x0 + draw::step_px; corners[2 * k + 1].y = ys[k];
    }
    static const u32 paths[3] = {md::view::contour, md::view::wave, md::view::comb};
    for (u32 j = 0; j < 3; ++j) {
        typename F::path_type& path = frame.paths[paths[j]];
        const opxy::draw::Point* mine = corners + 31 * j;
        const i32 ox = (mine[0].x + mine[31].x) >> 11;          // the middle, whole pixels
        const u32 path_key = (key * 3u + j) | 1u;
        if (path.key != path_key) {
            opxy::draw::Point points[32];
            for (u32 i = 0; i < 32; ++i) {
                points[i].x = opxy::q31::half(mine[i].x - ox * opxy::draw::unit);
                points[i].y = opxy::q31::half(mine[i].y - draw::cy * opxy::draw::unit);
            }
            md::view::polyline(path, points, 32, path_key);
        }
        md::view::show_path(frame, paths[j], ox, draw::cy, md::view::line, 2 * opxy::draw::unit);
    }
}
}
}
