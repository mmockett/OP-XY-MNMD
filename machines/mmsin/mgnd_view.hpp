#pragma once
// The Monomachine's GND machines' views, after the design's mnmView
// (docs/design/Main.dc.html), in the stock look (G1, OP-XY-Dev
// media/stock-look-audit/mnmd): the same drawings, thin, crossing the screen.
// GND-SIN: the textbook construction of a sine from a circle: the orb hollow in
// a neutral grey (no encoder), its radius, the tip's projection, and the sine
// CDCDCD at 1.2 px from the orb to the right edge, TUNE's (encoder 4) handle on
// the tip; the sine in two halves (a path spans at most 256 px). GND-NOIS: the noise trace, smoothed as RED
// reddens it, turning into TUNE's wave as RES pitches it, and with STON on a
// second trace for the stereo pair in encoder 3's grey (STON). The
// design's word for the noise's colour (white, pink, red) is not drawn: the
// SDK's text slots are not usable (docs/SCREEN.md) and the trace shows it.
#include "md_view.hpp"
#include "md_rig.hpp"
#include "live_view.hpp"

namespace md_mmsin {} namespace md = md_mmsin; namespace md_mmsin {
namespace mgnd_view {
using namespace md::view;

// Writes 32 points (stage Q10) into a path about (ox, oy) at scale 2, out and back.
template <class F>
inline void draw32(F& frame, u32 path, const i32* xs, const i32* ys, i32 ox, i32 oy, u32 key, u32 tone) {
    typename F::path_type& p = frame.paths[path];
    if (p.key != key) {
        Point points[32];
        for (u32 i = 0; i < 32; ++i) { points[i].x = (xs[i] - ox * unit) / 2; points[i].y = (ys[i] - oy * unit) / 2; }
        polyline(p, points, 32, key);
        md::live::unfold(p);
    }
    show_path(frame, path, ox, oy, tone, 2 * unit);
}

// 1 / sqrt((1 - f)^2 + f^2) at f = 0, 1/8, ..., 1 (Q30): the spread lost mixing two
// independent values f of the way from one to the other, given back.
static const i32 spread_of[9] = {0x40000000, 0x48686148, 0x50f44d89, 0x57cea99d, 0x5a82799a, 0x57cea99d, 0x50f44d89, 0x48686148, 0x40000000};

// The square root of GND-NOIS's level at RED 0, 16, ..., 127 against RED 0's, at
// middle C (Q28): the trace's height follows it, tallest near 64 where the sound is
// loudest, without running off the stage at Loud.
static const i32 nois_level_of[9] = {0x10000000, 0x10db675e, 0x119402d6, 0x12a371c7, 0x13d95a0b, 0x11b1bd2d, 0x0f7467bf, 0x0d1b38ba, 0x0b361fcd};
// sqrt((2 - k) / k), the spread the trace's smoothing (k = 1 - 0.92 RED) takes from
// the noise, given back (Q26, 1 .. 4.9) so RED smooths the trace without flattening it.
static const i32 nois_back_of[9] = {0x04000000, 0x047e70e1, 0x0510b205, 0x05bfe8c8, 0x069b86f2, 0x07c0b31c, 0x097002d2, 0x0c64e9da, 0x13988e14};

// The design's RND(i): noise that stays still between frames, a hash of the point.
inline i32 noise_at(u32 n) {
    n *= 2654435761u; n ^= n >> 15; n *= 2246822519u; n ^= n >> 13;
    return static_cast<i32>(n);
}
}

// GND-SIN: - - - - / - - - TUNE. The orb at (92, 110) on the stage (one row:
// the stage starts at y 30), radius 46; the sine from x 144 to the right edge,
// 2 + 3 x TUNE cycles, leaving the tip's height, turning with the orb when struck.
namespace gnd_sin {
enum Part : u32 { axis = 4, orb, radius, tip };
template <class F>
inline void view(F& frame, const i32* s, const view::Moment& m) {
    using namespace mgnd_view;
    const i32 cx = 92, cy = 110, r = 46 * unit, wx = 144 * unit;
    // Cycles over the span, Q16: 2 + 3 TUNE.
    const u32 cycles = 131072u + static_cast<u32>(multiply(md::positive(s[7]), 196608));
    // The design's 6 pi ph, 3 cycles over the hit, then one a second for as long as the note is held.
    const u32 turn = md::live::turning(md::live::of(m), 24214400u, md::rig::drum.length, md::live::one_hz);
    const u32 per_step = cycles * 1057u;                                // a 62nd of the cycles, as a phase (2^32 / 62 / 2^16)
    // The tip's angle, 35 degrees above the axis at rest, as a phase (clockwise on the stage).
    const u32 at = 0u - 417566040u - turn;
    i32 xs[32], ys[32];
    for (u32 side = 0; side < 2; ++side) {
        for (u32 i = 0; i < 32; ++i) {
            const u32 k = side * 31 + i;                                        // 0 .. 62 along the span
            xs[i] = wx + static_cast<i32>(k) * 5541;                            // 335.5 px / 62, Q10: to x 479.5
            ys[i] = cy * unit + multiply(md::sine(at - static_cast<u32>(k) * per_step), r);
        }
        draw32(frame, side ? view::wave : view::contour, xs, ys, side ? 396 : 228, cy,
               md::live::sealed(md::live::mix(md::live::mix(1u + side, s[7]), static_cast<i32>(turn))), stock::line4);
    }
    // The orb, its radius to the tip, and the tip's projection across to the sine's start.
    const i32 c = md::sine(at + 0x40000000u), n = md::sine(at);           // Q31
    const i32 tx = cx * unit + multiply(c, r), ty = cy * unit + multiply(n, r);
    stock::put(frame, axis, 240 * unit, cy * unit, stock::solid_a);
    stock::put(frame, orb, cx * unit, cy * unit, stock::hollow_n);
    stock::put(frame, radius, cx * unit, cy * unit, stock::solid_n, unit, ((static_cast<i32>(at) >> 16) * 3600) >> 16);
    const view::Point projection[2] = {{tx, ty}, {wx, ty}};
    stock::trace(frame, view::comb, projection, 2, (tx + wx) / 2 / unit, ty / unit,
                 md::live::sealed(md::live::mix(0x50524f4au, static_cast<i32>(at >> 8))), stock::line_a);
    stock::put(frame, tip, tx, ty, stock::solid4);
}
}

// GND-NOIS: ST RED STON - / - RES - TUNE. The noise across the stage, smoother
// as RED goes red and as tall as the sound is loud; ST lifts it with STON off;
// with STON on, two traces ST apart. TUNE packs the noise a third tighter or
// looser along the trace as it moves the sound up or down; RES turns the trace
// into a wave of 2 + 3 x TUNE cycles, as GND-SIN draws TUNE, as it narrows the
// noise to a pitch.
namespace gnd_nois {
enum Part : u32 { axis = 4 };
template <class F>
inline void view(F& frame, const i32* s, const view::Moment& m) {
    using namespace mgnd_view;
    // The noise moves for as long as the note sounds: held, then the hit's length after release.
    const i32 run = m.running(md::rig::drum.length);
    const i32 red = md::positive(s[1]), st = md::positive(s[0]), res = md::positive(s[5]);
    const bool stereo = s[2] >= 0x40000000;
    const i32 k = md::one - multiply(red, 0x75c28f5c);                      // the design's 1 - 0.92 red
    i32 tall = multiply(44 * unit, md::table9(nois_level_of, red)) * 8;
    const i32 back = md::table9(nois_back_of, red);
    // ST with STON off is level, up to +2.9 dB: 1.4 times as tall, as the sound's gain.
    if (!stereo) tall += multiply(tall, multiply(st, 0x33333333));
    const u32 shift = run > 0 ? static_cast<u32>(run) * 77u >> 10 : 0u;     // the design's 40 steps a hit (77 a second), as a table offset
    // The wave's share of the trace, none at RES 0; its cycles over the span, Q16,
    // as GND-SIN's; it drifts a cycle a second while the noise moves.
    const i32 tune = md::positive(s[7]);
    const i32 wave = res;
    const u32 cycles = 131072u + static_cast<u32>(multiply(tune, 196608));
    const u32 per_step = cycles * 1057u;                                     // a 62nd of the cycles, as a phase
    const u32 drift = run > 0 ? static_cast<u32>(run) * md::live::one_hz : 0u;
    // TUNE's packing: the noise table read 0.66 .. 1.34 of a step a point (Q16), 1 at 64.
    const u32 pack = static_cast<u32>(65536 + multiply(tune - 1082196480, 45000));
    for (u32 trace = 0; trace < (stereo ? 2u : 1u); ++trace) {
        // STON splits the trace in two, a little even at ST 0; ST spreads them.
        const i32 cy = (94 * unit) + (stereo ? (trace ? 1 : -1) * (4 * unit + multiply(st, 20 * unit)) : 0);
        const u32 seed = trace ? 0x29000u : 3u;
        // The smoothing runs in over the 16 points before the trace, so a red trace
        // starts already wandering rather than from the middle.
        i32 a = 0;
        for (u32 j = 0; j < 16; ++j) a += multiply(k, multiply(noise_at(seed + shift + j - 16u), 0x40000000) - a);
        i32 xs[64], ys[64];
        for (u32 i = 0; i < 63; ++i) {
            // The noise between two of the table's points, so TUNE packs it smoothly,
            // held to the points' own spread (two independent values mixed lose some).
            // At half scale from here on: the spread given back can pass full scale.
            const u32 at = i * pack, n = (at >> 16) + seed + shift;
            const i32 between = static_cast<i32>((at & 0xffffu) << 15);
            const i32 mixed = multiply(noise_at(n), md::one - between) + multiply(noise_at(n + 1), between);
            // a moves k of the way to the noise, in two halves: the noise less a can
            // pass 2^31, and must not wrap (g++ and the OP-XY wrap it where clang
            // does not; 4dde5da).
            const i32 half = multiply(k, (multiply(mixed, md::table9(spread_of, between)) >> 1) - (a >> 1));
            a += half;
            a += half;
            const i32 smooth = md::gain(multiply(a, back), 5);
            // The noise swells and shrinks the wave as a narrow band's does.
            const i32 sine = multiply(md::sine(i * per_step + drift + (trace ? 0xd0000000u : 0x30000000u)), 0x30000000 + multiply(smooth, 0x20000000));
            const i32 y = smooth + multiply(sine - smooth, wave);
            xs[i] = unit / 4 + static_cast<i32>(i) * 7919;   // 479.5 px / 62, Q10: x 0.25 to 479.7
            // Rounded off towards 70 px either side (a cubic, 105 px at full scale), so a
            // loud trace's peaks bend over rather than run flat along the stage's edge.
            const i32 off = md::clamp(multiply(y, tall) * 2, -105 * unit, 105 * unit) * 19972;   // 2^31 / (105 unit)
            ys[i] = md::clamp(cy - multiply(md::soft_clip(off), 105 * unit), 20 * unit, 168 * unit);   // never off the stage, 28 px under the header
        }
        for (u32 side = 0; side < 2; ++side) {
            i32 hx[32], hy[32];
            for (u32 i = 0; i < 32; ++i) { hx[i] = xs[side * 31 + i]; hy[i] = ys[side * 31 + i]; }
            const u32 path = trace ? (side ? view::extra : view::comb) : (side ? view::wave : view::contour);
            const u32 key = md::live::mix(md::live::mix(md::live::mix(md::live::mix(md::live::mix(trace * 4u + side * 2u, red), st), s[2]), res), s[7]);
            draw32(frame, path, hx, hy, side ? 359 : 123, 94, md::live::sealed(key + shift * 2u + drift), trace ? stock::line3 : stock::line4);
        }
    }
    stock::put(frame, axis, 240 * unit, 94 * unit, stock::solid_a);
}
}
}
