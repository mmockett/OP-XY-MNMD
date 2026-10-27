#pragma once
// The chord keyboard of SWAVE-ENS and DPRO-DENS, after the design's keyboard():
// two octaves of white keys over x 14..234, y 22..114 (the black keys are the
// rig's), the played note a handle under its key and each other voice a tick
// hanging from the keyboard's bottom edge. One path draws the outline and the
// ticks, as the runs between ticks lie along that edge.
#include "md_view.hpp"

namespace md_mmpulse {} namespace md = md_mmpulse; namespace md_mmpulse {
namespace chord_view {
using namespace md::view;

// `keys`: the voices' keys, 0..23 from the lower C, the played note first; up to 4.
template <class F>
inline void keyboard(F& frame, u32 path, const i32* keys, u32 n) {
    const i32 x0 = 14 * unit, top = 22 * unit, bottom = 114 * unit, ww = 16091;   // 220 / 14 px
    static const signed char white_of[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};   // semitone -> white key, or black
    if (n > 4) n = 4;
    i32 xs[4];
    u32 key = 0x51a7e001u;
    for (u32 k = 0; k < n; ++k) {
        const i32 semi = md::clamp(keys[k], 0, 23), r = semi % 12, octave = semi / 12;
        xs[k] = white_of[r] >= 0 ? x0 + (white_of[r] + octave * 7) * ww + ww / 2 : x0 + (white_of[r - 1] + 1 + octave * 7) * ww;
        key = key * 31u + static_cast<u32>(semi + 1);
    }
    Point p[64];
    u32 m = 0;
    const Point corners[4] = {{x0, bottom}, {x0, top}, {x0 + 14 * ww, top}, {x0 + 14 * ww, bottom}};
    for (u32 i = 0; i < 4; ++i) p[m++] = corners[i];
    for (i32 i = 13; i >= 1; --i) {
        const i32 x = x0 + i * ww;
        p[m].x = x; p[m++].y = bottom;
        p[m].x = x; p[m++].y = top;
        p[m].x = x; p[m++].y = bottom;
    }
    for (u32 k = 1; k < n; ++k) {
        p[m].x = xs[k]; p[m++].y = bottom;
        p[m].x = xs[k]; p[m++].y = bottom + 8 * unit;
        p[m].x = xs[k] + 1; p[m++].y = bottom;
    }
    // The rest along the bottom edge towards the first corner, two pixels apart.
    for (u32 i = m; i < 64; ++i) { p[i].x = x0 + static_cast<i32>(63 - i) * 2 * unit + unit; p[i].y = bottom; }
    typename F::path_type& out = frame.paths[path];
    if (out.key != key) {
        for (u32 i = 0; i < 64; ++i) { p[i].x = (p[i].x - 124 * unit) / 2; p[i].y = (p[i].y - 68 * unit) / 2; }
        outline(out, p, 64, key);
    }
    show_path(frame, path, 124, 68, thin, 2 * unit);
    if (n) place(frame, handle_a, xs[0], sy(0) + bottom + 8 * unit, unit, fill5, fill_and_stroke);
}
}
}
