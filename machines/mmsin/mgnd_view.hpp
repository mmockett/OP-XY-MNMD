#pragma once
// The Monomachine's GND machines' views, after the design's mnmView
// (docs/design/Main.dc.html). GND-SIN: the turning orb and its projection are
// the rig's (md_rig.hpp); this draws the sine TUNE stretches, in two halves
// (a path spans at most 256 px). GND-NOIS: the noise trace, smoothed as RED
// reddens it, and with STON on a second trace for the stereo pair. The
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

// The design's RND(i), as a fixed table: noise that stays still between frames.
static const i32 scatter[64] = {
    1203450987, -804712311, 431876543, -1876543210, 987654321, -123456789, 1678901234, -1543210987,
    345678901, -987123456, 1456789012, -345612789, 765432109, -1987654321, 234567890, -654321098,
    1890123456, -1098765432, 567890123, -876543210, 1345678901, -234567891, 912345678, -1678901235,
    123456789, -1456789013, 1789012345, -567890124, 678901234, -1234567890, 1567890123, -789012345,
    -301234567, 1120987654, -1765432109, 210987654, 1987650123, -987650321, 456123789, -1324567890,
    1654321098, -210345678, 876012345, -1501234567, 32109876, 1432109876, -654987321, 1098765432,
    -1888777666, 777888999, -111222333, 1555666777, -999000111, 333444555, -1222333444, 888999000,
    -444555666, 1666777888, -1777888999, 222333444, 1111222333, -555666777, 999000222, -333444666,
};
}

// GND-SIN: - - - - / - - - TUNE. The wave from the orb to the right edge,
// 2 + 3 x TUNE cycles, turning with the orb when struck.
namespace gnd_sin {
template <class F>
inline void view(F& frame, const i32* s, const view::Moment& m) {
    using namespace mgnd_view;
    const i32 wx = 104 * unit, x1 = 472 * unit;
    // Cycles over the span, Q16: 2 + 3 TUNE; the orb's turn, 1.5 cycles over the hit.
    const u32 cycles = 131072u + static_cast<u32>(multiply(md::positive(s[7]), 196608));
    // The design's 6 pi ph, 3 cycles over the hit, then one a second for as long as the note is held.
    const u32 turn = md::live::turning(md::live::of(m), 24214400u, md::rig::drum.length, md::live::one_hz);
    const u32 per_step = cycles * 1057u;                                // a 62nd of the cycles, as a phase (2^32 / 62 / 2^16)
    i32 xs[32], ys[32];
    for (u32 side = 0; side < 2; ++side) {
        for (u32 i = 0; i < 32; ++i) {
            const u32 k = side * 31 + i;                                        // 0 .. 62 along the span
            xs[i] = wx + static_cast<i32>(k) * ((x1 - wx) / 62);
            const u32 u = static_cast<u32>(k) * per_step + turn;
            ys[i] = 80 * unit - multiply(md::sine(u), 40 * unit);
        }
        draw32(frame, side ? view::wave : view::contour, xs, ys, side ? 380 : 196, 80, md::live::sealed(md::live::mix(md::live::mix(1u + side, s[7]), static_cast<i32>(turn))), view::line);
    }
    // The orb's radius, its tip and the tip's projection turn with the wave's
    // start, about the orb's centre (the design's line#rad, its node and
    // dash#pj), in place of the rig's straight-line struck pose. At rest they
    // are where the art puts them.
    const i32 c = md::sine(turn + 0x40000000u), n = md::sine(turn);           // Q31
    const i32 cx = 52 * unit, cy = sy(80);
    const i32 tx = cx + multiply(c, 40 * unit), ty = cy - multiply(n, 40 * unit);
    const Point at[4] = {{cx + multiply(c, 20 * unit), cy - multiply(n, 20 * unit)}, {tx, ty},
                         {(tx + wx) / 2 - 3 * unit / 2, ty}, {(tx + wx) / 2 - 3 * unit / 2, ty}};
    for (u32 i = 0; i < 4; ++i) {
        opxy::draw::Node& node = frame.nodes[art_first + 2 + i];
        node.x = at[i].x; node.y = at[i].y; node.scale = unit;
        node.cos = unit; node.sin = 0;
    }
    // Clockwise is positive on the stage; the design's radius turns anticlockwise.
    opxy::draw::turn(frame, art_first + 2, opxy::draw::Turn{((c >> 20) + 1) >> 1, -(((n >> 20) + 1) >> 1)});
}
}

// GND-NOIS: ST RED STON - / - - - TUNE. The noise across the stage, smoother
// and taller as RED goes red; with STON on and ST up, two traces ST apart.
namespace gnd_nois {
template <class F>
inline void view(F& frame, const i32* s, const view::Moment& m) {
    using namespace mgnd_view;
    // The noise moves for as long as the note sounds: held, then the hit's length after release.
    const i32 run = m.running(md::rig::drum.length);
    const i32 red = md::positive(s[1]), st = md::positive(s[0]);
    const bool stereo = s[2] >= 0x40000000 && st > 0x028f5c29;
    const i32 k = md::one - multiply(red, 0x75c28f5c);                      // the design's 1 - 0.92 red
    const i32 tall = 44 * unit + multiply(red, 44 * unit);
    const u32 shift = run > 0 ? static_cast<u32>(run) * 77u >> 10 : 0u;     // the design's 40 steps a hit (77 a second), as a table offset
    for (u32 trace = 0; trace < (stereo ? 2u : 1u); ++trace) {
        const i32 cy = (80 * unit) + (stereo ? (trace ? 1 : -1) * multiply(st, 24 * unit) : 0);
        const u32 seed = trace ? 29u : 3u;
        i32 a = 0;
        i32 xs[64], ys[64];
        for (u32 i = 0; i < 63; ++i) {
            a += multiply(k, scatter[(i + seed + shift) & 63] - a);
            xs[i] = 12 * unit + static_cast<i32>(i) * (456 * unit / 62);
            ys[i] = cy - multiply(a, tall);
        }
        for (u32 side = 0; side < 2; ++side) {
            i32 hx[32], hy[32];
            for (u32 i = 0; i < 32; ++i) { hx[i] = xs[side * 31 + i]; hy[i] = ys[side * 31 + i]; }
            const u32 path = trace ? (side ? view::extra : view::comb) : (side ? view::wave : view::contour);
            draw32(frame, path, hx, hy, side ? 354 : 126, 80, md::live::sealed(md::live::mix(md::live::mix(md::live::mix(trace * 4u + side * 2u, red), st), s[2]) + shift * 2u), stereo && !trace ? view::mid : view::line);
        }
    }
}
}
}
