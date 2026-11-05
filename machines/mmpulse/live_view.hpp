#pragma once
// What the Monomachine views share for a note's motion and for outlines the
// device draws whole.
//
// Motion: a view that moves while a note plays takes it from here. It runs for as
// long as a note is held (docs/SCREEN.md), eases back to the resting drawing in
// 0.2 s from the release, and needs no state: Moment already knows the times.
// With the track's amp envelope known it follows that instead: it swells over
// the attack, holds at the sustain level while notes are held and fades over the
// release, still moving, so it lasts as long as the sound.
// A hold's cost is the few paths it reshapes, a node each.
#include "md_view.hpp"

namespace md_mmpulse {} namespace md = md_mmpulse; namespace md_mmpulse {
namespace live {
using namespace md::view;

const i32 tail = 205;   // Q10 seconds: 0.2 s after the last release
// Phases per Q10 second, for a rate in cycles a second.
const u32 half_hz = 2097152u, one_hz = 4194304u;

struct Motion {
    i32 time;     // Q10 seconds since the note, stopped at the release (running on with the envelope)
    i32 amount;   // Q31: all of the motion while held, easing to none (the envelope's level)
    const Moment* envelope;   // the moment, when its envelope is known: turning() follows it
};
inline Motion of(const Moment& m) {
    Motion out = {0, 0, 0};
    if (m.amp.known) {
        out.amount = m.level();
        if (!out.amount) return out;
        out.time = m.age;
        out.envelope = &m;
        return out;
    }
    if (m.reduced_motion || m.quiet >= tail) return out;
    const i32 left = (tail - m.quiet) * 10475529;   // Q31 of the tail
    out.amount = multiply(left, left);              // a curve that eases out
    // When the SDK can't say which notes are held, quiet counts from the note.
    const i32 since = m.age - m.quiet;
    out.time = since > 0 ? since : m.age;
    return out;
}
// A swing either way at `rate`, Q31: 0 at the note and at rest.
inline i32 sway(const Motion& m, u32 rate) { return multiply(md::sine(static_cast<u32>(m.time) * rate), m.amount); }
// A swell from 0 up to 1 and back at `rate`, Q31: 0 at the note and at rest.
inline i32 swell(const Motion& m, u32 rate) {
    const i32 c = md::sine(static_cast<u32>(m.time) * rate + 0x40000000u);
    return multiply(opxy::q31::half(md::one) - opxy::q31::half(c), m.amount);
}
// A phase that keeps turning, at `fast` for the first `until` (Q10 s), then at
// `slow`; on release it turns back the short way to 0. The two's complement
// keeps it continuous as it wraps.
// With the envelope known it turns as far as the level has carried the note:
// it gathers speed over the attack and slows with the decay. A note that dies
// away while held (no sustain) brings what is left of its turn home as its
// level falls; from the release it turns back to 0 as the release falls.
inline u32 travel_turn(const Amp& a, i32 t, u32 fast, i32 until, u32 slow) {
    const u32 x = static_cast<u32>(held_travel(a, t < until ? t : until)), y = static_cast<u32>(held_travel(a, t));
    return x * fast + (y - x) * slow;
}
inline u32 turning(const Motion& m, u32 fast, i32 until, u32 slow) {
    if (m.envelope) {
        const Moment& e = *m.envelope;
        const i32 h = e.held();
        u32 p = travel_turn(e.amp, h, fast, until, slow);
        if (!e.amp.sustain && h >= e.amp.attack) {
            const i32 end = static_cast<i32>(travel_turn(e.amp, e.amp.attack + e.amp.decay, fast, until, slow));
            p -= static_cast<u32>(multiply(static_cast<i32>(end), md::one - held_level(e.amp, h)));
        }
        return e.quiet > 0 ? static_cast<u32>(multiply(static_cast<i32>(p), fall(e.amp, e.quiet))) : p;
    }
    const u32 t = static_cast<u32>(m.time);
    const u32 p = m.time < until ? t * fast : static_cast<u32>(until) * fast + (t - static_cast<u32>(until)) * slow;
    return static_cast<u32>(multiply(static_cast<i32>(p), m.amount));
}

// TUNE on a wave, as SuperWave's rows show it: 0.5 .. 1.5 times the cycles, as
// (64 + v) / 128 so 64 draws exactly what it always did.
inline u32 stretch(i32 tune) { return 64u + (static_cast<u32>(md::positive(tune)) >> 24); }

// A path's key from everything its shape depends on. key_of keeps six bits of
// each value and xors some together, so a turn of a control or a slow motion
// could leave the key, and so the drawing, as it was.
inline u32 mix(u32 key, i32 value) { return (key ^ static_cast<u32>(value)) * 16777619u; }
inline u32 sealed(u32 key) { return key | 1u; }

// Takes the straight-back turns out of an outline already written by
// md::view::outline, which only moves a point that lands on an earlier one: a
// turn back along a line without returning to a point (a crosshair's arm, a
// glyph's stem) stays, and the device draws it as streaks. Where the way out
// lies within 1/16 px of the way in, the far end of the shorter edge moves an
// eighth of a pixel off the longer one's line, on the side it already leans to.
// Vectors are in eighths of Q10 so the products fit 32 bits.
template <class Path>
inline void unfold(Path& path) {
    Point* p = path.points;
    u32 m = 64;
    while (m > 1 && p[m - 1].x == p[m - 2].x && p[m - 1].y == p[m - 2].y) --m;
    if (m < 3) return;
    // Again while a move made a turn next door; it settles in a pass or two.
    bool moved = true;
    for (u32 pass = 0; pass < 8 && moved; ++pass) {
        moved = false;
        for (u32 i = 0; i < m; ++i) {
            Point& a = p[i ? i - 1 : m - 1];
            const Point& b = p[i];
            Point& c = p[i + 1 < m ? i + 1 : 0];
            const i32 ux = (b.x - a.x) >> 3, uy = (b.y - a.y) >> 3, vx = (c.x - b.x) >> 3, vy = (c.y - b.y) >> 3;
            if (ux * vx + uy * vy >= 0) continue;
            const i32 cross = ux * vy - uy * vx;
            const i32 su = span(a, b) >> 3, sv = span(b, c) >> 3, most = su > sv ? su : sv;
            // cross 8^2 / (edge 8) is the Q10 distance off the way in: 1/16 px is 64,
            // and an edge's span can be 0.71 of its length.
            if ((cross < 0 ? -cross : cross) > 12 * most) continue;
            const bool out = su >= sv;                       // the longer edge is the way in: c moves
            const i32 lx = out ? ux : vx, ly = out ? uy : vy;
            // Square to the long edge, the way that makes the cross bigger.
            const i32 side = cross >= 0 ? 1 : -1;
            const i32 mx = -side * (ly > 0 ? apart : ly < 0 ? -apart : 0), my = side * (lx > 0 ? apart : lx < 0 ? -apart : 0);
            Point& far = out ? c : a;
            const Point& next = p[out ? (i + 2 < m ? i + 2 : i + 2 - m) : (i >= 2 ? i - 2 : i + m - 2)];
            far.x += mx;
            far.y += my;
            moved = true;
            if (near(far, next)) { far.x += mx; far.y += my; }   // past it, rather than onto it
        }
    }
    md::view::blunt(p, m);   // a move here can sharpen a turn outline() already squared
    for (u32 i = m; i < 64; ++i) p[i] = p[m - 1];
}
}
}
