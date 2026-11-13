#pragma once
// FM+'s screens in the stock look (F1, OP-XY-Dev media/stock-look-audit/mnmd):
// the design's operator diagram (docs/design/Main.dc.html, mnmView's FM+ branch)
// without letters or arrowheads. Modulators on the left as plain circles, a
// bundle of parallel lines as deep as each one's depth into the carrier, a plain
// 323232 disc, and the output wave leaving the carrier over a 323232 axis. Each part holds its encoder's grey (md_view.hpp,
// namespace stock): on STAT and DYN page 1 is all M1, so M1's four controls are
// four parts of M1. Nothing brightens with an amount.
//
// The bundles' lines and the operators are fixed parts (tools/art/stock.mjs):
// each line runs from its modulator's centre to the carrier's, its ends hidden
// under the operators drawn over it, so it never reshapes as an operator
// resizes. The paths draw what does reshape: the output wave, the feedback loop,
// DYN's two envelope sketches. The wave uses the real ratios (fmp.hpp), so it
// shows the sound's own steps; the header names each FRQ step.
//
// The diagram and its wave are centred as one group: the wave ends at x 432,
// as far from the right edge as the largest modulator is from the left (48 px).
#include "md_view.hpp"
#include "md_rig.hpp"
#include "fmp.hpp"
#include "live_view.hpp"

namespace md_mmstat {} namespace md = md_mmstat; namespace md_mmstat {
namespace fm_panel {
using namespace md::view;

inline i32 get(const i32* s, int slot) { return md::positive(s[slot]); }
inline u32 v127(i32 slot) { return static_cast<u32>(md::positive(slot)) >> 24; }
const i32 half = 0x40000000;
// A control 0..127 between two stage pixel values, Q10.
inline i32 between(i32 slot, i32 at0, i32 at127) { return at0 * unit + static_cast<i32>(v127(slot)) * (at127 - at0) * unit / 127; }

// A bundle: 1 + round(4 depth) lines 3.5 px apart, from (x0, y0) to (x1, y1)
// (stage pixels), drawn by the fixed parts first .. first + 4 at `tenths`; the rest hidden.
inline u32 lines_of(i32 depth) { return 1 + ((static_cast<u32>(md::positive(depth)) + 0x10000000u) >> 29); }
template <class F>
inline void bundle(F& f, u32 first, i32 x0, i32 y0, i32 x1, i32 y1, i32 tenths, i32 depth, u32 tone) {
    const u32 n = lines_of(depth);
    const Turn t = rotated(tenths);
    for (u32 k = 0; k < 5; ++k) {
        if (k >= n) { hide(f, first + k); continue; }
        // Offset (k - (n - 1) / 2) 3.5 px along the normal (-sin, cos): 1.75 px a step of 2k - n + 1.
        const i32 off = (2 * static_cast<i32>(k) - static_cast<i32>(n) + 1) * 1792;
        const i32 mx = (x0 + x1) * unit / 2 - ((t.sin * off) >> 10), my = (y0 + y1) * unit / 2 + ((t.cos * off) >> 10);
        stock::put(f, first + k, mx, my, tone, unit, tenths);
    }
}
// An operator's circle, authored at radius `authored`, scaled to `radius` (both Q10).
template <class F>
inline void op(F& f, u32 node, i32 x, i32 y, i32 radius, i32 authored, u32 tone) {
    stock::put(f, node, x * unit, y * unit, tone, radius * unit / authored);
}
// A feedback loop: an arc `r` (Q10) about (x, y) from `from` tenths, clockwise, as
// far round as `amount` (0..300 degrees), on a path, with a handle at its end; none at 0.
template <class F>
inline void loop(F& f, u32 path, u32 handle, i32 x, i32 y, i32 r, i32 from, i32 amount, u32 line_tone, u32 handle_tone) {
    const i32 sweep = multiply(md::positive(amount), 3000);
    if (sweep < 60) return;                                  // under 6 degrees: none
    const u32 n = 2 + static_cast<u32>(sweep / 120);         // a point every 12 degrees, at most 27
    Point pts[32];
    for (u32 i = 0; i < n; ++i) {
        const Turn t = rotated(from + sweep * static_cast<i32>(i) / static_cast<i32>(n - 1));
        pts[i].x = x * unit + ((t.cos * (r >> 4)) >> 6);
        pts[i].y = y * unit + ((t.sin * (r >> 4)) >> 6);
    }
    const u32 key = md::live::sealed(md::live::mix(md::live::mix(0x4c4f4f50u + path, r), sweep));
    stock::trace(f, path, pts, n, x, y, key, line_tone);
    stock::put(f, handle, pts[n - 1].x, pts[n - 1].y, handle_tone);
}

// The output wave from x0 to x1 (stage px) about y cy, 40 px high, over `paths`
// paths of 32 points from `first_path`: 1.5 + 2 TUNE cycles, swung by `spin`
// (Q16 cycles). `wave(u, u16)` gives the value (Q31) for the carrier's phase u
// (Q32 cycles) and that phase in Q16 cycles, for the ratios.
template <class F, class W>
inline void output(F& frame, u32 first_path, u32 paths, i32 x0, i32 x1, i32 cy, i32 spin, i32 tune, u32 key, const W& wave) {
    const u32 steps = 31 * paths;
    const u32 step16 = (98304u + v127(tune) * 1032u) / steps;               // (cycles << 16) / steps
    const i32 dx = (x1 - x0) * unit / static_cast<i32>(steps);
    for (u32 side = 0; side < paths; ++side) {
        Point pts[32];
        for (u32 i = 0; i < 32; ++i) {
            const u32 k = side * 31 + i;
            const u32 u16 = k * step16 + static_cast<u32>(spin);              // Q16 cycles
            pts[i].x = x0 * unit + static_cast<i32>(k) * dx;
            pts[i].y = cy * unit - multiply(md::clamp(wave(u16 << 16, u16), -md::one, md::one), 40 * unit);
        }
        const i32 ox = (pts[0].x + pts[31].x) / 2 / unit;
        stock::trace(frame, first_path + side, pts, 32, ox, cy, key + side, stock::line4);
    }
}

// A modulator's sine with its feedback (Q28 cycles), solved twice as the sound's
// (fmp::feedback_swing) is, at its phase p.
inline i32 fed_back(u32 p, i32 fb) {
    if (!fb) return md::sine(p);
    const i32 first = md::sine(p + (static_cast<u32>(multiply(md::sine(p), fb)) << 4));
    return md::sine(p + (static_cast<u32>(multiply(first, fb)) << 4));
}

// One modulated sine of the wave: sin(2 pi u + I sin(2 pi r u)), I in Q28
// cycles (the radians of the design over 2 pi), r Q16.
inline i32 modulated(u32 u, u32 u16, i32 index, u32 ratio16, i32 sum_so_far = 0) {
    const i32 m = md::sine(u16 * ratio16);
    const u32 offset = static_cast<u32>(multiply(m, index)) << 4;
    return sum_so_far + md::sine(u + offset);
}

// Radians (Q31 fraction of `most`) to Q28 cycles: most / 2 pi.
inline i32 cycles(i32 fraction, i32 most_q28) { return multiply(md::clamp(fraction, 0, md::one), most_q28); }
const i32 radian = 42722830;   // 2^28 / 2 pi

// 2^27 / x for x (Q31) in 1/16 .. 1, without a divide: x is brought into
// 0.5 .. 1 by doublings and 1 / x read off a table there.
inline i32 inverse(i32 x) {
    static const i32 table[17] = {   // 2^29 / (0.5 + i / 32)
        1073741824, 1010580540, 954437177, 904203641, 858993459, 818089008, 780903145, 746950834,
        715827883, 687194767, 660764199, 636291451, 613566757, 592409282, 572662306, 554189329, 536870912,
    };
    u32 shift = 0;
    while (x < 0x40000000 && shift < 4) { x *= 2; ++shift; }
    const u32 position = static_cast<u32>(x - 0x40000000) >> 21;               // 0 .. 2^9: 16 segments of 2^5
    const u32 index = position >> 5;
    const i32 fraction = static_cast<i32>((position & 31u) << 26);
    const i32 y = index >= 16 ? table[16] : table[index] + multiply(table[index + 1] - table[index], fraction);
    return static_cast<i32>(shift >= 2 ? static_cast<u32>(y) << (shift - 2) : static_cast<u32>(y) >> (2 - shift));
}

// The animation: while a note is held the wave swings a cycle either way (spin,
// Q16 cycles, at the speed the design's hit turns it) and, as the design's
// live2, the depth eases off to 0.4 and back, a second each; both ease to rest
// after the release. swell is 1 at the note and at its breath's top, 0 at rest.
struct Live { i32 spin, ease, swell; };
inline Live moving(const Moment& m) {
    const md::live::Motion h = md::live::of(m);
    const i32 down = md::live::swell(h, md::live::one_hz);
    const Live now = {multiply(md::live::sway(h, md::live::half_hz), 65536), md::one - multiply(down, 0x4ccccccd), h.amount - down};
    return now;
}
inline u32 wave_key(const i32* s, const Live& l) {
    u32 key = 0x0f3a0001u;
    for (u32 i = 0; i < 8; ++i) key = md::live::mix(key, s[i]);
    return md::live::mix(md::live::mix(key, l.spin), l.ease ^ l.swell) & ~1u;   // key + side: two paths
}

// DYN's envelope sketches, x 104..148: 1FEN's pitch envelope falls 4 + 20 FEN
// px from y 26, 1VEN's level envelope rises 4 + 18 VEN px to y 52 (stage), each
// 1 - e^(-4 t) across.
template <class F>
inline void sketch(F& f, u32 path, i32 control, i32 y0, i32 dir, i32 most, u32 tone) {
    Point pts[21];
    const i32 depth = 4 * unit + multiply(md::positive(control), most * unit);
    for (u32 i = 0; i < 21; ++i) {
        const i32 rise = md::one - md::exp_neg(13107 * static_cast<i32>(i));   // 1 - e^(-4 i / 20)
        pts[i].x = 104 * unit + static_cast<i32>(i) * 2253;                     // 44 px / 20, Q10
        pts[i].y = y0 * unit + dir * multiply(rise, depth);
    }
    stock::trace(f, path, pts, 21, 126, y0 + dir * 12, md::live::sealed(md::live::mix(0x454e5600u + path, control)), tone);
}
}

// FM+STAT: 1FRQ 1FIN 1ENV 1FB / 2FRQ 2VOL TONE TUNE. M1 hollow, smaller as its
// ratio rises (encoder 1); 1FIN a dark pointer inside it, like a dial (2); 1ENV
// its bundle, mid (3); 1FB its loop, light, as far round as the feedback (4).
// Page 2: M2 hollow (2FRQ), its bundle dark (2VOL); TONE shows in the wave; TUNE
// the wave's cycles, CDCDCD.
namespace fmpstat {
enum Part : u32 { b1 = 4, b2 = b1 + 5, m1 = b2 + 5, pointer, m2, carrier, axis, loop_end };
template <class F>
inline void view(F& f, const i32* s, const view::Moment& m) {
    using namespace fm_panel;
    const i32 r1 = between(s[0], 20, 11), r2 = between(s[4], 16, 9);
    bundle(f, b1, 68, 58, 182, 98, 193, s[2], stock::solid3);
    bundle(f, b2, 68, 138, 182, 98, -193, s[5], stock::solid2);
    op(f, m1, 68, 58, r1, 15872, stock::hollow);   // authored 15.5 px
    stock::put(f, pointer, 68 * unit, 58 * unit, stock::solid2, unit, static_cast<i32>(v127(s[1])) * 2700 / 127 - 1350);
    op(f, m2, 68, 138, r2, 12800, stock::hollow);   // 12.5 px
    stock::put(f, carrier, 182 * unit, 98 * unit, stock::solid_a);
    stock::put(f, axis, 318 * unit, 98 * unit, stock::solid_a);
    loop(f, view::contour, loop_end, 68, 58, r1 + 8 * unit, -1500, s[3], stock::line4, stock::solid4);
    const Live l = moving(m);
    const i32 tone = half + opxy::q31::half(get(s, 6));                       // 0.5 + TONE
    const i32 i1 = cycles(multiply(multiply(get(s, 2), tone), l.ease), 3 * 2 * radian);   // 1env 3 tone live
    const i32 i2 = cycles(multiply(get(s, 5), tone), 5 * radian);                     // 2vol 2.5 tone
    // M1 as the sound has it: its listed ratio moved by 1FIN, with 1FB's feedback.
    const u32 r1q = static_cast<u32>(md::positive(fmp::ratio(s[0]) + fine(s[1]))) >> 12, r2q = static_cast<u32>(fmp::ratio(s[4])) >> 12;
    const i32 fb = fmp::feedback(s[3]);
    output(f, view::comb, 2, 204, 432, 98, l.spin, s[7], wave_key(s, l), [&](u32 u, u32 u16) {
        const u32 offset = (static_cast<u32>(multiply(fed_back(u16 * r1q, fb), i1)) + static_cast<u32>(multiply(md::sine(u16 * r2q), i2))) << 4;
        return md::sine(u + offset);
    });
}
}

// FM+PAR: 1FRQ 1ENV 2FRQ 2ENV / 3FRQ 3ENV TONE TUNE. Three modulator and carrier
// pairs: M1 hollow (1FRQ) with a dark bundle (1ENV), M2 mid (2FRQ) with a light
// bundle (2ENV), and page 2's M3 hollow (3FRQ) with a dark bundle (3ENV). The
// carriers stay one neutral grey; their leads join at the wave.
namespace fmppar {
enum Part : u32 { b1 = 4, b2 = b1 + 5, b3 = b2 + 5, m1 = b3 + 5, m2, m3, c1, c2, c3, lead1, lead2, lead3, axis };
template <class F>
inline void view(F& f, const i32* s, const view::Moment& m) {
    using namespace fm_panel;
    static const u32 tones[3] = {stock::solid2, stock::solid4, stock::solid2}, ops[3] = {stock::hollow, stock::solid3, stock::hollow};
    for (u32 k = 0; k < 3; ++k) {
        const i32 y = 40 + 56 * static_cast<i32>(k);
        bundle(f, b1 + 5 * k, 61, y, 159, y, 0, s[2 * k + 1], tones[k]);
        op(f, m1 + k, 61, y, between(s[2 * k], 13, 7), 10 * unit, ops[k]);
        stock::put(f, c1 + k, 159 * unit, y * unit, stock::solid_a);
        stock::put(f, lead1 + k, -9 * unit, 0, stock::solid_a);   // drawn in place, 9 px left
    }
    stock::put(f, axis, 322 * unit + unit / 2, 96 * unit, stock::solid_a);
    const Live l = moving(m);
    const i32 tone = half + opxy::q31::half(get(s, 6));
    i32 index[3], weight[3];
    u32 ratio[3];
    i32 total = 0;
    for (u32 k = 0; k < 3; ++k) {
        const i32 ev = get(s, 2 * k + 1);
        index[k] = cycles(multiply(multiply(ev, tone), l.ease), 4 * radian);   // kenv 2 tone live
        ratio[k] = static_cast<u32>(fmp::ratio(s[2 * k])) >> 12;
        weight[k] = multiply(ev, 0x20000000);                                   // quarters: the sum stays in range
        total += weight[k];
    }
    // s = sum(sin_k env_k) / max(0.3, sum env_k), the division read off a table.
    const i32 floor = 0x0999999a;                                                // 0.3, quartered
    const i32 norm = inverse(total > floor ? total : floor);
    output(f, view::comb, 2, 213, 432, 96, l.spin, s[7], wave_key(s, l), [&](u32 u, u32 u16) {
        i32 sum = 0;
        for (u32 k = 0; k < 3; ++k) sum += multiply(modulated(u, u16, index[k], ratio[k]), weight[k]);
        return md::gain(multiply(sum, norm), 4);
    });
}
}

// FM+DYN: 1FRQ 1FEN 1VOL 1VEN / 2FRQ 2ENV 2FB TUNE. M1 hollow (encoder 1), its
// pitch envelope's fall dark (1FEN, 2), its bundle mid (1VOL, 3), its level
// envelope's rise light (1VEN, 4). Page 2: M2 hollow (2FRQ), its bundle dark
// (2ENV), its loop mid (2FB); TUNE the wave's cycles.
namespace fmpdyn {
enum Part : u32 { b1 = 4, b2 = b1 + 5, m1 = b2 + 5, m2, carrier, axis, loop_end };
template <class F>
inline void view(F& f, const i32* s, const view::Moment& m) {
    using namespace fm_panel;
    const i32 r1 = between(s[0], 18, 10), r2 = between(s[4], 15, 8);
    bundle(f, b1, 66, 58, 192, 98, 176, s[2], stock::solid3);
    bundle(f, b2, 66, 138, 192, 98, -176, s[5], stock::solid2);
    op(f, m1, 66, 58, r1, 14 * unit, stock::hollow);
    op(f, m2, 66, 138, r2, 11776, stock::hollow);   // 11.5 px
    stock::put(f, carrier, 192 * unit, 98 * unit, stock::solid_a);
    stock::put(f, axis, 323 * unit, 98 * unit, stock::solid_a);
    sketch(f, view::wave, s[1], 26, 1, 20, stock::line2);
    sketch(f, view::comb, s[3], 52, -1, 18, stock::line4);
    loop(f, view::contour, loop_end, 66, 138, r2 + 8 * unit, 600, s[6], stock::line3, stock::solid3);
    const Live l = moving(m);
    // I1 = 1vol 3 (1 + 1ven (0.5 at rest, more with the note's swell)), I2 = 2env 2.5.
    const i32 lift = multiply(get(s, 3), half + opxy::q31::half(md::clamp(l.swell, 0, md::one)));
    const i32 i1 = cycles(get(s, 2), 3 * radian) + multiply(cycles(get(s, 2), 3 * radian), lift);
    const i32 i2 = cycles(get(s, 5), 5 * radian / 2);
    const u32 r1q = static_cast<u32>(fmp::dyn_ratio1(s[0])) >> 12, r2q = static_cast<u32>(fmp::dyn_ratio2(s[4])) >> 12;
    const i32 fb = fmp::feedback(s[6]);
    output(f, view::extra, 1, 214, 432, 98, l.spin, s[7], wave_key(s, l), [&](u32 u, u32 u16) {
        const u32 offset = (static_cast<u32>(multiply(md::sine(u16 * r1q), i1)) + static_cast<u32>(multiply(fed_back(u16 * r2q, fb), i2))) << 4;
        return md::sine(u + offset);
    });
}
}
}
