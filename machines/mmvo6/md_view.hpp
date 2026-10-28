#pragma once
// The screen of every machine, after the MD / MNM voice-screen design
// (docs/SCREEN.md): greyscale line art in the OP-XY's language. Under the
// stock header, a 480 x 160 stage from y 30: for Machinedrum machines an
// isometric instrument on the left (x 0..236) and the family's panel on the
// right (x 248..472); for Monomachine machines one full-width view. The
// design's info line under it is left out: the OP-XY names the engine.
//
// Each engine's artwork comes from tools/art/build.mjs: for every machine it holds,
// a run of art nodes (instrument and panel parts, each one shade), drawn
// relative to a pivot, parked off screen until placed. The machine's view
// (voices/<machine>_view.hpp) places them, shapes the four paths and writes
// the labels. Nothing here allocates, divides at run time or uses floats.
#include <opxy/draw.hpp>
#include "md_dsp.hpp"

namespace md_mmvo6 {} namespace md = md_mmvo6; namespace md_mmvo6 {
namespace view {
using opxy::i16;
using opxy::u8;
using namespace opxy::draw;

// Palette entries, the design's styles (tools/art/build.mjs writes the same order).
enum Shade : u32 {
    line, thin, mid, dim, faint, dash, label,      // strokes: #f7f5f5 (1.6, 1), #a8a9ad, #606166, #3a3b3f, #7a7b80, #8c8d92
    fill1, fill2, fill3, fill4, fill5,             // fills: #1c1d20, #3a3b40, #66676c, #a8a9ad, #f7f5f5
    outline0, outline1, outline2, outline3,        // fill + stroke pairs for the instruments' bodies
    shade_count
};

// The stage: design coordinates (0..480, 0..160) start at screen y 30.
const i32 stage_y = 46;   // under the two-page header
const i32 panel_x = 248, panel_w = 224;   // the Machinedrum family panel
inline i32 sx(i32 x) { return x * unit; }
inline i32 sy(i32 y) { return (stage_y + y) * unit; }

// The four mutable paths of 64 points, shared by all machines.
enum Paths : u32 { contour, wave, comb, extra, path_count };
// Common nodes after the paths: the hit's rings, two handles, the playhead, then
// a three-digit readout of seven segments each, for numbers that change (no
// text slots: the SDK's check of them fails, and the stroke font matches).
enum Common : u32 { ring_first = path_count, ring_count = 3, handle_a = ring_first + ring_count, handle_b, cursor,
                    digit_first, digit_segments = 7, digits = 3, art_first = digit_first + digit_segments * digits };

// The moment: how long since the hit, for the animations.
struct Moment {
    i32 age;              // Q10 seconds since the note started; large when idle
    u32 reduced_motion;
    // 0 .. 1 (Q10 unit) through an animation of `length` Q10 seconds, or -1 when
    // done; `speed` is 2^20 / length, a constant, so there is no runtime division.
    i32 phase(i32 length, i32 speed) const {
        if (reduced_motion || age >= length) return -1;
        return md::clamp((age * speed) >> 10, 0, unit - 1);
    }
};

// Hit bookkeeping, as md_screen's: a track change or the first frame is never a hit.
struct Hit {
    u32 serial, track, initialized;
    i32 age;
    bool update(const Input& input) {
        const bool baseline = !initialized || track != input.track;
        const bool fresh = !baseline && input.note_serial != serial;
        serial = input.note_serial;
        track = input.track;
        initialized = 1;
        if (baseline) age = 1 << 20;
        else if (fresh) age = 0;
        else age = md::clamp(age + md::clamp(input.elapsed, 0, 1024), 0, 1 << 20);
        return fresh;
    }
};

// One art part: its pivot on the stage and its shade (tools/art.py writes these).
struct Part { i16 x, y; u8 shade, paint; };

// A canvas over the engine's frame, for one machine's run of art parts.
template <class F>
struct Canvas {
    F& frame;
    const Part* parts;     // this machine's parts
    u32 first, count;      // their node numbers
    // A part at its pivot, moved by (dx, dy) stage pixels (Q10), scaled (Q10) and turned.
    void part(u32 i, i32 dx = 0, i32 dy = 0, i32 scale = unit, Turn turn = Turn{unit, 0}) {
        if (i >= count) return;
        const Part& p = parts[i];
        place(frame, first + i, sx(p.x) + dx, sy(p.y) + dy, scale, p.shade, static_cast<Paint>(p.paint));
        if (turn.cos != unit || turn.sin != 0) opxy::draw::turn(frame, first + i, turn);
    }
    void hide_part(u32 i) { if (i < count) hide(frame, first + i); }
    // Recolours a placed part.
    void tone(u32 i, u32 shade_index) {
        if (i < count) opxy::draw::shade(frame, first + i, shade_index, static_cast<Paint>(parts[i].paint));
    }
};

// Shows a mutable path at its origin on the stage, or hides it.
template <class F>
inline void show_path(F& frame, u32 path, i32 x, i32 y, u32 shade_index, i32 scale = unit) {
    place(frame, path, sx(x), sy(y), scale, shade_index, stroke);
}

// The device's SVG loader merges points that (nearly) repeat, leaving a path
// fewer points than the frame binds, and then it stops reshaping that path:
// its old shape stays on screen. Points closer than an eighth of a pixel on
// both axes count as one.
const i32 apart = unit / 8;
inline bool near(const Point& a, const Point& b) {
    const i32 dx = a.x - b.x, dy = a.y - b.y;
    return dx > -apart && dx < apart && dy > -apart && dy < apart;
}

// Moves q, reached from `from`, an eighth of a pixel to the left of its way in
// while it lands near one of the m points already placed.
inline void aside(Point& q, const Point& from, const Point* placed, u32 m) {
    const i32 side = apart;
    for (u32 tries = 0; tries < 4; ++tries) {
        bool seen = false;
        for (u32 j = 0; j < m && !seen; ++j) seen = near(placed[j], q);
        if (!seen) return;
        const i32 dx = q.x - from.x, dy = q.y - from.y;
        q.x += dy > 0 ? side : dy < 0 ? -side : 0;
        q.y += dx > 0 ? -side : dx < 0 ? side : 0;
    }
}

// Writes n path-local points (Q10) into a path as an outline the OP-XY draws
// without streaks: it draws them from an outline with no area, as a line
// traced out and back over itself, or one that turns straight back at a
// point. Repeated points go (near: within an eighth of a pixel); a point that
// revisits an earlier one moves an eighth of a pixel to the left of its way in
// (aside), so a retraced line becomes a thin ribbon and a spike a sliver; the
// longest edges are halved, each midpoint aside too, until there are 64
// points, each at least an eighth of a pixel from the others. An outline of
// one point collapses, which draws nothing.
template <class Path>
inline void outline(Path& path, const Point* points, u32 n, u32 key) {
    Point p[64];
    u32 m = 0;
    for (u32 i = 0; i < n && i < 64; ++i) {
        Point q = points[i];
        if (m && near(q, p[m - 1])) continue;
        if (m) aside(q, p[m - 1], p, m);
        p[m++] = q;
    }
    while (m > 1 && near(p[m - 1], p[0])) --m;
    if (!m) p[m++] = Point();
    // An edge halves while it spans two eighths of a pixel on an axis, so its
    // midpoint stays apart from both ends; edge m - 1 closes the outline.
    while (m < 64) {
        u32 at = 0;
        i32 longest = 0;
        for (u32 i = 0; i < m && m > 1; ++i) {
            const Point& next = p[i + 1 < m ? i + 1 : 0];
            const i32 dx = next.x - p[i].x, dy = next.y - p[i].y;
            const i32 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, span = ax > ay ? ax : ay;
            if (span > longest) { longest = span; at = i; }
        }
        if (longest < 2 * apart) break;
        const Point a = p[at], b = p[at + 1 < m ? at + 1 : 0];
        Point mid;
        mid.x = a.x + (b.x - a.x) / 2;
        mid.y = a.y + (b.y - a.y) / 2;
        aside(mid, a, p, m);
        for (u32 i = m; i > at + 1; --i) p[i] = p[i - 1];
        p[at + 1] = mid;
        ++m;
    }
    for (u32 i = 0; i < 64; ++i) path.points[i] = p[i < m ? i : m - 1];
    path.key = key ? key : 1;
}

// Makes a path a polyline of n points (up to 32), out and back as a thin
// ribbon (outline). `points` are path-local Q10.
template <class Path>
inline void polyline(Path& path, const Point* points, u32 n, u32 key) {
    if (n > 32) n = 32;
    Point both[64];
    for (u32 i = 0; i < n; ++i) both[i] = both[2 * n - 1 - i] = points[i];
    outline(path, both, 2 * n, key);
}

// The 0..127 readouts the Machinedrum and Monomachine show, for the header.
inline void value127(char* out, u32 capacity, i32 parameter) {
    const u32 v = static_cast<u32>(md::positive(parameter)) >> 24;
    char text[4];
    u32 n = 0;
    if (v >= 100) text[n++] = '1';
    if (v >= 10) text[n++] = static_cast<char>('0' + v / 10 % 10);
    text[n++] = static_cast<char>('0' + v % 10);
    text[n] = 0;
    copy(out, capacity, text);
}

// Joins up to three words with " / " into a text slot's 23 characters (ASCII:
// the device font's coverage beyond it is unknown).
inline void words(char* out, const char* a, const char* b = 0, const char* c = 0) {
    u32 n = 0;
    const char* parts[3] = {a, b, c};
    for (u32 k = 0; k < 3; ++k) {
        if (!parts[k] || !parts[k][0]) continue;
        if (n && n + 3 < text_capacity - 1) { out[n++] = ' '; out[n++] = '/'; out[n++] = ' '; }
        for (const char* s = parts[k]; *s && n < text_capacity - 1; ++s) out[n++] = *s;
    }
    out[n] = 0;
}

// Each frame starts here: the header shows 0..127 for the four positions, and
// the paths and common nodes are hidden until a view places them.
template <class F>
inline void begin(F& frame, const Input& input) {
    for (u32 i = 0; i < 4; ++i) value127(frame.header[i], header_capacity, input.parameters[i]);
    for (u32 n = 0; n < art_first; ++n) hide(frame, n);
    // Every path needs a shape, used or not.
    for (u32 p = 0; p < path_count; ++p) {
        if (frame.paths[p].key) continue;
        const Point ends[2] = {{-32 * unit, 0}, {31 * unit, 0}};
        polyline(frame.paths[p], ends, 2, 0x7fffffffu);
    }
}

// A number 0..999 in the readout, its top-left at (x, y) on the stage, in a shade.
// Segments a..g of each digit are the readout's nodes in that order.
template <class F>
inline void number(F& frame, i32 x, i32 y, u32 value, u32 tone = label) {
    static const u8 lit[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    // Segment centres in a 4 x 6 px digit, half pixels: a top, b upper right, c lower
    // right, d bottom, e lower left, f upper left, g middle (the artwork's segments).
    static const signed char at[7][2] = {{4, 0}, {8, 3}, {8, 9}, {4, 12}, {0, 9}, {0, 3}, {4, 6}};
    if (value > 999) value = 999;
    const u32 digits_[3] = {value / 100, value / 10 % 10, value % 10};
    const u32 first = value >= 100 ? 0 : value >= 10 ? 1 : 2;
    for (u32 d = 0; d < digits; ++d) {
        for (u32 seg = 0; seg < digit_segments; ++seg) {
            const u32 node = digit_first + d * digit_segments + seg;
            if (d < first || !(lit[digits_[d]] & (1u << seg))) { hide(frame, node); continue; }
            place(frame, node, sx(x + static_cast<i32>(d - first) * 6) + at[seg][0] * (unit / 2), sy(y) + at[seg][1] * (unit / 2), unit, tone, stroke);
        }
    }
}

// The art nodes shown in one shade, in another (stroked).
template <class F>
inline void restyle(F& frame, u32 from, u32 to) {
    for (u32 n = art_first; n < F::node_count; ++n)
        if (frame.nodes[n].shade == from && frame.nodes[n].x < hidden_x * unit) shade(frame, n, to, stroke);
}

// A cheap geometry key from quantized parameters, never zero.
inline u32 key_of(i32 a, i32 b, i32 c, i32 d) {
    return 1u + (static_cast<u32>(a) >> 25) + ((static_cast<u32>(b) >> 25) << 7)
         + ((static_cast<u32>(c) >> 25) << 14) + ((static_cast<u32>(d) >> 25) << 21);
}
}
}
