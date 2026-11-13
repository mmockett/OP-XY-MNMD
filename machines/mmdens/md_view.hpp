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
// the labels. Nothing here allocates or uses floats, and nothing divides at
// run time but through() (a division a call).
#include <opxy/draw.hpp>
#include "md_dsp.hpp"

namespace md_mmdens {} namespace md = md_mmdens; namespace md_mmdens {
namespace view {
// Stock OS 1.2.1's amp envelope as times and shapes (lib/amp_curve.h, copied
// byte-identical beside every engine): only 32-bit multiplies, no divides.
namespace stock_amp {
#include "amp_curve.h"
}
using opxy::i16;
using opxy::u8;
using namespace opxy::draw;

// opxy::draw's sine and rotation, the same math kept in each file that draws.
// The SDK's are plain inline: a copy GCC keeps out of line becomes a global
// symbol, and another machine's copy then clashes with it in one firmware
// (open-xy docs/toolchain.md, "One definition for shared code").
// sin of a tenth-degree angle, Q10, interpolated between whole degrees.
static inline i32 sine_tenths(i32 tenths) {
    tenths %= 3600;
    if (tenths < 0) tenths += 3600;
    const i32 sign = tenths >= 1800 ? -1 : 1;
    tenths %= 1800;
    if (tenths > 900) tenths = 1800 - tenths;
    const i32 degree = tenths / 10, fraction = tenths % 10;
    const i32 low = sine_q10[degree], high = sine_q10[degree < 90 ? degree + 1 : 90];
    return sign * (low + (high - low) * fraction / 10);
}
// A clockwise rotation by tenths of a degree.
static inline Turn rotated(i32 tenths) {
    const Turn turn = {sine_tenths(tenths + 900), sine_tenths(tenths)};
    return turn;
}

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

// The track's amp envelope (SDK 1.10's input.amp), as stock OS 1.2.1 plays it
// (amp_curve.h): a note's motion swells over the attack, falls over the decay to
// the sustain level while notes are held, and fades over the release after the
// last one ends, so the screen moves for as long as the sound does
// (docs/SCREEN.md). A larger attack or decay word is a longer stage, but a
// larger release word is a SHORTER release. The attack rises concavely,
// L(u) = 2 (1 - 0.505^u); the decay falls exponentially and stops at the
// sustain plus 1%; the sustain is a linear level; the release falls
// exponentially from wherever it starts. Zero, as a Moment built without one
// has it, is unknown: the screen moves as it always did.
struct Amp {
    u32 known;
    i32 attack, decay, release;   // Q10 seconds: note-on to peak, peak to sustain, full to -66 dB
    i32 sustain;                  // Q31, linear
    i32 attack_word, decay_word, release_word;   // the stages' words, the decay and release held to `least`
};
// A fall faster than this would jump: the decay and release words are held to
// stock's fourth-fastest time constant, 23 ms, so motion eases out over at
// least about 0.1 s (a decay to nothing) or 0.18 s (a release to -66 dB).
const i32 least = 4 << 24;
// Microseconds as Q10 seconds, and back (up to 2^20 Q10 s), in 32 bits.
inline i32 q10_of(u32 us) { return static_cast<i32>(((us >> 10) * 1074u + (((us & 1023u) * 1074u) >> 10)) >> 10); }
inline u32 us_of(i32 t) {
    const u32 q = static_cast<u32>(md::clamp(t, 0, 1 << 20));
    return (q >> 4) * 15625u + (((q & 15u) * 15625u) >> 4);
}
// A Q15 level (0 .. 32768) as Q31.
inline i32 q31_of(u32 q15) { return q15 >= 32768u ? md::one : static_cast<i32>(q15 << 16); }
// A template, so an engine that never reads the envelope builds on an SDK before 1.10.
template <class In>
inline Amp amp_of(const In& input) {
    Amp a = {0, 0, 0, 0, 0, 0, 0, 0};
    if (!envelope_known(input.amp)) return a;   // unqualified: found when used, so older SDKs never need it
    a.known = 1;
    // A sustain under 1/64 is none: such a note dies away while held, and its motion comes home.
    a.sustain = input.amp.sustain < (1 << 25) ? 0 : input.amp.sustain;
    a.attack_word = input.amp.attack;
    a.decay_word = input.amp.decay < least ? least : input.amp.decay;
    a.release_word = input.amp.release > md::one - least ? md::one - least : input.amp.release;
    a.attack = q10_of(stock_amp::attack_us(a.attack_word));
    a.decay = q10_of(stock_amp::decay_us(a.decay_word, a.sustain));
    a.release = q10_of(stock_amp::release_us(a.release_word));
    return a;
}
// t / length, Q31, 0 .. 1: a division a call.
inline i32 through(i32 t, i32 length) {
    if (t <= 0) return 0;
    if (t >= length) return md::one;
    return static_cast<i32>(((static_cast<u32>(t) << 16) / static_cast<u32>(length)) << 15);
}
// 2 / -ln(0.505) - 2, Q31: the attack's integral.
const i32 attack_area = 1991652530;
// The level t (Q10 s) into a note still held, Q31: up the attack's concave
// rise, then down the decay's exponential to the sustain level.
inline i32 held_level(const Amp& a, i32 t) {
    if (t < a.attack) return q31_of(65536u - 2u * stock_amp::attack_frame_q15(a.attack_word, us_of(t)));
    if (t >= a.attack + a.decay) return a.sustain;
    const i32 l = q31_of(stock_amp::decay_frame_q15(a.decay_word, us_of(t - a.attack)));
    return l > a.sustain ? l : a.sustain;
}
// How far the level has carried a held note by t (Q10 s): its integral, Q10 s,
// so t itself for a level that is full from the start.
inline i32 held_travel(const Amp& a, i32 t) {
    if (t <= 0) return 0;
    // The attack: 2 t - 2 T (1 - 0.505^(t/T)) / -ln 0.505, T the time to 0.99.
    const i32 rise = t < a.attack ? t : a.attack;
    const i32 lost = multiply(q10_of(stock_amp::attack_us(a.attack_word)),
                              q31_of(32768u - stock_amp::attack_frame_q15(a.attack_word, us_of(rise))));
    i32 d = 2 * rise - 2 * lost - multiply(lost, attack_area);
    if (d < 0) d = 0;
    if (t <= a.attack) return d;
    // The decay: tau (1 - e^(-x/tau)) from full, then the sustain level.
    const i32 x = t - a.attack < a.decay ? t - a.attack : a.decay;
    d += multiply(q10_of(stock_amp::decay_tau_us(a.decay_word)), q31_of(32768u - stock_amp::decay_frame_q15(a.decay_word, us_of(x))));
    if (t > a.attack + a.decay) d += multiply(a.sustain, t - a.attack - a.decay);
    return d;
}
// What the release leaves, `quiet` Q10 s into it, Q31: e^(-quiet/tau).
inline i32 fall(const Amp& a, i32 quiet) { return q31_of(stock_amp::release_frame_q15(a.release_word, us_of(quiet))); }

// The moment: how long since the hit, for the animations.
struct Moment {
    i32 age;              // Q10 seconds since the note started; large when idle
    u32 reduced_motion;
    i32 quiet;            // Q10 seconds since a note was last held (Hit::quiet)
    Amp amp;              // the track's amp envelope; zero, unknown, unless the engine reads it
    // 0 .. 1 (Q10 unit) through an animation of `length` Q10 seconds, or -1 when
    // done; `speed` is 2^20 / length, a constant, so there is no runtime division.
    i32 phase(i32 length, i32 speed) const {
        if (reduced_motion || age >= length) return -1;
        return md::clamp((age * speed) >> 10, 0, unit - 1);
    }
    // Q10 seconds since the note started, while a note is held and for `tail`
    // after the last release, for motion that lasts as long as the note does;
    // -1 when still. With the envelope known it lasts as long as the sound,
    // whatever the tail, and runs at the envelope's level: the time the level
    // has carried the note (travelled).
    i32 running(i32 tail) const {
        if (amp.known) return level() ? travelled() : -1;
        if (reduced_motion || quiet >= tail) return -1;
        return age;
    }
    // With the envelope known: Q10 seconds the note was held, until now while it
    // is, else until its release.
    i32 held() const { const i32 h = age - quiet; return h > 0 ? h : 0; }
    // The envelope's level now, Q31, in steps of 1/2048 so that it reaches 0 where
    // stock's release ends, at -66 dB: 0 at rest, once the sound has died away,
    // and under reduced motion.
    i32 level() const {
        if (reduced_motion || !amp.known || age >= (1 << 20)) return 0;
        i32 at = held_level(amp, held());
        if (quiet > 0) at = multiply(at, fall(amp, quiet));
        return at & ~0xfffff;
    }
    // The level's integral over the note, release included, Q10 s.
    i32 travelled() const {
        const i32 h = held();
        i32 d = held_travel(amp, h);
        if (quiet > 0)   // tau (1 - e^(-quiet/tau)) from the level it fell from
            d += multiply(multiply(held_level(amp, h), q10_of(stock_amp::release_tau_us(amp.release_word))),
                          md::one - fall(amp, quiet));
        return d;
    }
};

// Hit bookkeeping, as md_screen's: a track change or the first frame is never a hit.
struct Hit {
    u32 serial, track, initialized;
    i32 age;
    // Q10 seconds since a note was last held, or since the note started when the
    // SDK can't say which notes are held (held_notes beyond 128).
    i32 quiet;
    bool update(const Input& input) {
        const bool baseline = !initialized || track != input.track;
        const bool fresh = !baseline && input.note_serial != serial;
        const bool held = input.held_notes && input.held_notes <= 128;
        const i32 step = md::clamp(input.elapsed, 0, 1024);
        serial = input.note_serial;
        track = input.track;
        initialized = 1;
        if (baseline) age = quiet = 1 << 20;
        else {
            age = fresh ? 0 : md::clamp(age + step, 0, 1 << 20);
            quiet = fresh || held ? 0 : md::clamp(quiet + step, 0, 1 << 20);
        }
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
// An edge's length on its longer axis.
inline i32 span(const Point& a, const Point& b) {
    const i32 dx = b.x - a.x, dy = b.y - a.y, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    return ax > ay ? ax : ay;
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

// Squares off every turn within about 3.6 degrees of straight back, an eighth of a
// pixel across: a stroke's mitre at such a point would run far out. An outline with no
// room left gives up the point that bends it least for it.
inline void blunt(Point* p, u32& m) {
    for (u32 i = 0; i < m && m > 3; ++i) {
        const Point& a = p[i ? i - 1 : m - 1];
        const Point b = p[i];
        const Point& c = p[i + 1 < m ? i + 1 : 0];
        const i32 ux = (b.x - a.x) >> 4, uy = (b.y - a.y) >> 4, vx = (c.x - b.x) >> 4, vy = (c.y - b.y) >> 4;
        const i32 dot = ux * vx + uy * vy, cross = ux * vy - uy * vx;
        if (dot >= 0 || (cross < 0 ? -cross : cross) > (-dot >> 4)) continue;
        const i32 side = cross < 0 ? -1 : 1;
        Point q = b;
        q.x += side * (uy > 0 ? -apart : uy < 0 ? apart : 0);
        q.y += side * (ux > 0 ? apart : ux < 0 ? -apart : 0);
        if (near(q, c)) continue;
        if (m == 64) {
            u32 straightest = 64;
            i32 least = 0x7fffffff;
            for (u32 j = 0; j < m; ++j) {
                if (j == i || j == (i ? i - 1 : m - 1) || j == (i + 1 < m ? i + 1 : 0)) continue;
                const Point& f = p[j ? j - 1 : m - 1];
                const Point& g = p[j];
                const Point& h = p[j + 1 < m ? j + 1 : 0];
                const i32 sx = (g.x - f.x) >> 4, sy = (g.y - f.y) >> 4, tx = (h.x - g.x) >> 4, ty = (h.y - g.y) >> 4;
                if (span(f, g) < 4 * apart || span(g, h) < 4 * apart) continue;   // an end already squared
                const i32 bend = sx * ty - sy * tx, size = bend < 0 ? -bend : bend;
                if (size < least) { least = size; straightest = j; }
            }
            if (straightest == 64) return;
            for (u32 j = straightest; j + 1 < m; ++j) p[j] = p[j + 1];
            --m;
            if (straightest < i) --i;
        }
        for (u32 j = m; j > i + 1; --j) p[j] = p[j - 1];
        p[i + 1] = q;
        ++m;
        ++i;   // past the new point
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
    blunt(p, m);
    // An edge halves while it spans four eighths of a pixel on an axis, so both
    // halves keep the quarter pixel a closing edge needs; edge m - 1 closes the
    // outline.
    while (m < 64) {
        u32 at = 0;
        i32 longest = 0;
        for (u32 i = 0; i < m && m > 1; ++i) {
            const Point& next = p[i + 1 < m ? i + 1 : 0];
            const i32 dx = next.x - p[i].x, dy = next.y - p[i].y;
            const i32 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, span = ax > ay ? ax : ay;
            if (span > longest) { longest = span; at = i; }
        }
        if (longest < 4 * apart) break;
        const Point a = p[at], b = p[at + 1 < m ? at + 1 : 0];
        Point mid;
        mid.x = a.x + (b.x - a.x) / 2;
        mid.y = a.y + (b.y - a.y) / 2;
        aside(mid, a, p, m);
        for (u32 i = m; i > at + 1; --i) p[i] = p[i - 1];
        p[at + 1] = mid;
        ++m;
    }
    blunt(p, m);   // again: a midpoint moved aside can sharpen a turn past the limit
    // The outline is a loop, so it may start anywhere: start it where both its
    // edges are longest. The device draws a closing edge only an eighth of a
    // pixel long (a ribbon's end, on a level or upright line) as a long sliver
    // along the next edge.
    u32 start = 0;
    i32 best = -1;
    for (u32 i = 0; i < m && m > 2; ++i) {
        const Point& a = p[i ? i - 1 : m - 1];
        const Point& b = p[i];
        const Point& c = p[i + 1 < m ? i + 1 : 0];
        const i32 in = span(a, b), out = span(b, c), shorter = in < out ? in : out;
        if (shorter > best) { best = shorter; start = i; }
    }
    for (u32 i = 0; i < 64; ++i) path.points[i] = p[i < m ? (start + i) % m : (start + m - 1) % m];
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

// The stock look (OP-XY-Dev media/stock-look-2026-11-06): the palette of the
// Monomachine engines drawn in it (tools/art/stock.mjs, STOCK_PALETTE), whose
// views draw the whole screen. The grey is the knob: a part's grey says which
// encoder moves it, hollow (black with a CDCDCD edge) for encoder 1, then
// 4B4B4B, 969696 and CDCDCD; 7D7D7D, 646464 and 323232 belong to no encoder. The
// line entries are for the paths, which draw at scale 2: 1.2 px for line4, 1 px
// for the rest. Grey never stands for an amount.
namespace stock {
enum Tone : u32 { line4, line3, line2, line_n, line_d, line_a, edge4, edge_n,
                  solid2, solid3, solid4, solid_n, solid_a, hollow, black, hollow_n };
// An encoder's (0..3) handle or solid part.
inline u32 coded(u32 encoder) { return encoder == 0 ? hollow : encoder == 1 ? solid2 : encoder == 2 ? solid3 : solid4; }
// A fixed part (tools/art/stock.mjs) at (x, y) on the stage, Q10, scaled and turned
// (tenths of a degree, clockwise); a filled tone fills.
template <class F>
inline void put(F& f, u32 node, i32 x, i32 y, u32 tone, i32 scale = unit, i32 tenths = 0) {
    place(f, node, x, stage_y * unit + y, scale, tone, tone >= solid2 ? fill_and_stroke : stroke);
    if (tenths) opxy::draw::turn(f, node, rotated(tenths));
}
}
}
}
