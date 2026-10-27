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

namespace md_mmnois {} namespace md = md_mmnois; namespace md_mmnois {
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
const i32 stage_y = 30;
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

// Makes a path a polyline of n points (2..32) out and back, as the device's
// paths are always closed (md_screen's retrace): `ys` are path-local Q10.
template <class Path>
inline void polyline(Path& path, const Point* points, u32 n, u32 key) {
    if (n > 32) n = 32;
    for (u32 i = 0; i < 32; ++i) path.points[i] = points[i < n ? i : n - 1];
    for (u32 i = 0; i < 32; ++i) {
        path.points[63 - i].x = path.points[i].x;
        path.points[63 - i].y = path.points[i].y + 1;
    }
    // Distinct points: nudge any repeat along x.
    for (u32 i = 1; i < 64; ++i)
        if (path.points[i].x == path.points[i - 1].x && path.points[i].y == path.points[i - 1].y) path.points[i].x += 1;
    path.key = key ? key : 1;
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
        for (u32 i = 0; i < 64; ++i) { frame.paths[p].points[i].x = (static_cast<i32>(i) - 32) * unit; frame.paths[p].points[i].y = 0; }
        frame.paths[p].key = 0x7fffffffu;
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
