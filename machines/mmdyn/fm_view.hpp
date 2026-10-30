#pragma once
// FM+'s screens, after the design's operator diagrams (docs/design/Main.dc.html,
// mnmView's FM+ branch): modulators on the left, the carrier, arrows as thick as
// each modulator's depth, and the output wave on the right. The operators,
// arrowheads, feedback loops and labels come from the rig (md_rig.hpp), the
// loops brightened by FB; these
// draw what changes shape on the four paths: the arrows, DYN's two envelope
// curves and the output wave. The wave uses the real ratios (fmp.hpp), so it
// shows the sound's own steps; the design's ratio labels are left to the
// header, which shows each FRQ step by name.
#include "md_view.hpp"
#include "md_rig.hpp"
#include "fmp.hpp"
#include "live_view.hpp"

namespace md_mmdyn {} namespace md = md_mmdyn; namespace md_mmdyn {
namespace fm_panel {
using namespace md::view;

inline i32 get(const i32* s, int slot) { return md::positive(s[slot]); }
inline i32 q(i32 fraction_q31, i32 pixels) { return multiply(fraction_q31, pixels * unit); }   // Q10 pixels
const i32 half = 0x40000000;

// Writes a polyline of up to 32 points (stage Q10) into a path, about its
// origin (ox, oy), at scale 2, and shows it.
template <class F>
inline void draw(F& frame, u32 path, const Point* stage, u32 n, i32 ox, i32 oy, u32 key, u32 tone) {
    typename F::path_type& p = frame.paths[path];
    if (p.key != key) {
        Point points[32];
        for (u32 i = 0; i < n && i < 32; ++i) {
            points[i].x = (stage[i].x - ox * unit) / 2;
            points[i].y = (stage[i].y - oy * unit) / 2;
        }
        polyline(p, points, n, key);
        md::live::unfold(p);
    }
    show_path(frame, path, ox, oy, tone, 2 * unit);
}

// The design's arrow: 1 + round(4 depth) parallel lines 3.5 px apart, from the
// modulator to 8 px short of the carrier, drawn as one line that runs out
// along each and back along the next, and ends at the tip.
struct Arrow { i32 x1, y1, x2, y2, cos, sin; };   // stage pixels; direction Q31
inline u32 lines_of(i32 depth) { return 1 + ((static_cast<u32>(md::positive(depth)) + 0x10000000u) >> 29); }

inline u32 bundle(Point* out, u32 n, const Arrow& a, i32 depth) {
    const u32 lines = lines_of(depth);
    const i32 tip_x = a.x2 * unit - multiply(a.cos, 8 * unit), tip_y = a.y2 * unit - multiply(a.sin, 8 * unit);
    for (u32 i = 0; i < lines; ++i) {
        // Offset (i - (lines - 1) / 2) 3.5 px along the normal (-sin, cos).
        const i32 o = (2 * static_cast<i32>(i) - static_cast<i32>(lines) + 1) * (unit * 7 / 4);
        const i32 nx = -multiply(a.sin, o), ny = multiply(a.cos, o);
        const Point base = {a.x1 * unit + nx, a.y1 * unit + ny}, tip = {tip_x + nx, tip_y + ny};
        // Odd lines run base to tip, even ones back: the bundle is one stroke.
        const bool out_bound = (i & 1u) == ((lines - 1) & 1u);
        out[n++] = out_bound ? base : tip;
        out[n++] = out_bound ? tip : base;
    }
    return n;
}

// The output wave from x 282 to 472, y = 80 - 46 s: three cycles over two
// paths of 32 points, as the design draws it, or two over one path, which
// keeps a single path's wave legible, half as many to half as many again across
// TUNE. `wave(u)` gives s (Q31) for the carrier's phase u (Q32 cycles) and that
// phase in Q16 cycles, for the ratios.
template <class F, class W>
inline void output(F& frame, u32 first_path, u32 paths, i32 spin, i32 tune, u32 key, const W& wave) {
    // 3 cycles over 62 steps or 2 over 31, and 190 px, as constants: no runtime division.
    const u32 step16 = ((paths > 1 ? 3171u : 4228u) * md::live::stretch(tune)) >> 7;   // (cycles << 16) / steps
    const i32 dx = paths > 1 ? 3138 : 6276;                // 190 px / steps, Q10
    for (u32 side = 0; side < paths; ++side) {
        Point pts[32];
        for (u32 i = 0; i < 32; ++i) {
            const u32 k = side * 31 + i;
            const u32 u16 = k * step16 + static_cast<u32>(spin);   // Q16 cycles
            pts[i].x = 282 * unit + static_cast<i32>(k) * dx;
            pts[i].y = 80 * unit - multiply(md::clamp(wave(u16 << 16, u16), -md::one, md::one), 46 * unit);
        }
        draw(frame, first_path + side, pts, 32, paths < 2 ? 377 : side ? 424 : 330, 80, key + side, line);
    }
}

// A modulator's sine with its feedback (Q28 cycles), solved twice as the sound's
// (fmp::run) is, at its phase p.
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

// The feedback loop (the rig's one dashed part) brightens with its amount, up
// the greys from dim when off.
template <class F>
inline void feedback(F& frame, i32 amount) {
    static const u32 tones[5] = {dim, dash, label, mid, line};
    const u32 v = static_cast<u32>(md::positive(amount)) >> 24;
    restyle(frame, dash, tones[v == 0 ? 0 : 1 + (v >> 5)]);
}

// DYN's two envelope sketches, x 96..150: 1fen falls from the top in
// 4 + 30 FEN px, 1ven rises in 4 + 30 VEN px (the design's curves).
static const opxy::u16 step_over_width[128] = {
    29491, 27847, 26376, 25053, 23856, 22768, 21775, 20866, 20029, 19256, 18541, 17878, 17260, 16683, 16144, 15638,
    15163, 14717, 14295, 13898, 13521, 13165, 12827, 12505, 12200, 11909, 11632, 11367, 11114, 10872, 10640, 10418,
    10205, 10001, 9805, 9616, 9434, 9259, 9091, 8928, 8771, 8620, 8474, 8332, 8196, 8063, 7935, 7811,
    7691, 7574, 7461, 7351, 7244, 7141, 7040, 6942, 6847, 6755, 6664, 6577, 6491, 6408, 6327, 6248,
    6170, 6095, 6022, 5950, 5880, 5811, 5744, 5679, 5615, 5553, 5492, 5432, 5374, 5316, 5260, 5206,
    5152, 5099, 5048, 4997, 4948, 4899, 4852, 4805, 4759, 4714, 4670, 4627, 4584, 4543, 4502, 4461,
    4422, 4383, 4345, 4308, 4271, 4234, 4199, 4164, 4129, 4096, 4062, 4029, 3997, 3965, 3934, 3903,
    3873, 3843, 3814, 3785, 3757, 3729, 3701, 3674, 3647, 3620, 3594, 3569, 3543, 3518, 3494, 3470,
};

template <class F>
inline void curve(F& frame, u32 path, i32 control, i32 top, i32 depth_px, bool scaled, u32 key, u32 tone) {
    const u32 r = static_cast<u32>(md::positive(control)) >> 24;
    Point pts[31];
    for (u32 i = 0; i < 31; ++i) {
        const i32 rise = md::one - md::exp_neg(static_cast<i32>(step_over_width[r]) * static_cast<i32>(i));
        // 1fen: 6 + 20 (1 - e) F + 20 (1 - F); 1ven: 30 + 20 (1 - e).
        const i32 y = scaled ? (6 * unit + multiply(multiply(rise, control), depth_px * unit) + q(md::one - control, depth_px))
                             : top * unit + multiply(rise, depth_px * unit);
        pts[i].x = 96 * unit + static_cast<i32>(i) * 1843;   // 54 px / 30, Q10
        pts[i].y = y;
    }
    draw(frame, path, pts, 31, 123, top + 10, key, tone);
}
}

// FM+STAT: M1 and M2 into the carrier, M1 with its feedback loop.
namespace fmpstat {
template <class F>
inline void view(F& f, const i32* s, const view::Moment& m) {
    using namespace fm_panel;
    using view::Point;
    static const Arrow a1 = {72, 46, 176, 74, 2073644052, 558288783}, a2 = {72, 114, 176, 86, 2073644052, -558288783};
    Point pts[32];
    draw(f, view::contour, pts, bundle(pts, 0, a1, s[2]), 124, 60, lines_of(s[2]) + 16, lines_of(s[2]) > 1 ? view::line : view::dim);
    draw(f, view::wave, pts, bundle(pts, 0, a2, s[5]), 124, 100, lines_of(s[5]) + 32, lines_of(s[5]) > 1 ? view::line : view::dim);
    feedback(f, s[3]);
    const Live l = moving(m);
    const i32 tone = half + opxy::q31::half(get(s, 6));                       // 0.5 + TONE
    const i32 i1 = cycles(multiply(multiply(get(s, 2), tone), l.ease), 3 * 2 * radian);   // 1env 3 tone live
    const i32 i2 = cycles(multiply(get(s, 5), tone), 5 * radian);                     // 2vol 2.5 tone
    // M1 as the sound has it: its listed ratio moved by 1FIN, with 1FB's feedback.
    const u32 r1 = static_cast<u32>(md::positive(fmp::ratio(s[0]) + fine(s[1]))) >> 12, r2 = static_cast<u32>(fmp::ratio(s[4])) >> 12;
    const i32 fb = fmp::feedback(s[3]);
    output(f, view::comb, 2, l.spin, s[7], wave_key(s, l), [&](u32 u, u32 u16) {
        const u32 offset = (static_cast<u32>(multiply(fed_back(u16 * r1, fb), i1)) + static_cast<u32>(multiply(md::sine(u16 * r2), i2))) << 4;
        return md::sine(u + offset);
    });
}
}

// FM+PAR: three modulator and carrier pairs, each carrier as bright as its envelope.
namespace fmppar {
template <class F>
inline void view(F& f, const i32* s, const view::Moment& m) {
    using namespace fm_panel;
    using view::Point;
    Point pts[32];
    for (u32 k = 0; k < 3; ++k) {
        const i32 ev = get(s, 2 * k + 1), y = 26 + 52 * static_cast<i32>(k);
        const i32 tip = 170 - static_cast<i32>(static_cast<u32>(multiply(ev, 6 * unit) + unit / 2) >> 10);
        const Arrow a = {68, y, tip, y, md::one, 0};
        draw(f, k, pts, bundle(pts, 0, a, ev), 115, y, lines_of(ev) + 16 * (k + 1) + (static_cast<u32>(tip) << 8),
             lines_of(ev) > 1 ? view::line : view::dim);
    }
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
    output(f, view::extra, 1, l.spin, s[7], wave_key(s, l), [&](u32 u, u32 u16) {
        i32 sum = 0;
        for (u32 k = 0; k < 3; ++k) sum += multiply(modulated(u, u16, index[k], ratio[k]), weight[k]);
        return md::gain(multiply(sum, norm), 4);
    });
}
}

// FM+DYN: M1 with its pitch and level envelopes, M2 with its feedback loop.
namespace fmpdyn {
template <class F>
inline void view(F& f, const i32* s, const view::Moment& m) {
    using namespace fm_panel;
    using view::Point;
    static const Arrow a1 = {72, 46, 176, 74, 2073644052, 558288783}, a2 = {72, 114, 176, 86, 2073644052, -558288783};
    Point pts[32];
    u32 n = bundle(pts, 0, a1, s[2]);
    if (!(lines_of(s[2]) & 1u)) { pts[n] = pts[n - 2]; pts[n + 1] = pts[n - 1]; n += 2; }   // end at the tip
    // The hop to M2's tip runs under the arrowheads and through the carrier,
    // whose filled node is drawn over the paths.
    const Point hop[3] = {{a1.x2 * unit, a1.y2 * unit}, {190 * unit, 80 * unit}, {a2.x2 * unit, a2.y2 * unit}};
    for (u32 i = 0; i < 3; ++i) pts[n++] = hop[i];
    // M2's bundle from its tip: reversed.
    Point second[10];
    const u32 m2 = bundle(second, 0, a2, s[5]);
    for (u32 i = 0; i < m2; ++i) pts[n + i] = second[(lines_of(s[5]) & 1u) ? m2 - 1 - i : i];
    n += m2;
    const bool either = lines_of(s[2]) > 1 || lines_of(s[5]) > 1;
    draw(f, view::contour, pts, n, 124, 80, lines_of(s[2]) * 8 + lines_of(s[5]) + 64, either ? view::line : view::dim);
    curve(f, view::wave, get(s, 1), 6, 20, true, md::live::sealed(md::live::mix(1u, s[1])), view::mid);
    curve(f, view::comb, get(s, 3), 30, 20, false, md::live::sealed(md::live::mix(2u, s[3])), view::dim);
    feedback(f, s[6]);
    const Live l = moving(m);
    // I1 = 1vol 3 (1 + 1ven (0.5 at rest, more with the note's swell)), I2 = 2env 2.5.
    const i32 lift = multiply(get(s, 3), half + opxy::q31::half(md::clamp(l.swell, 0, md::one)));
    const i32 i1 = cycles(get(s, 2), 3 * radian) + multiply(cycles(get(s, 2), 3 * radian), lift);
    const i32 i2 = cycles(get(s, 5), 5 * radian / 2);
    const u32 r1 = static_cast<u32>(fmp::dyn_ratio1(s[0])) >> 12, r2 = static_cast<u32>(fmp::dyn_ratio2(s[4])) >> 12;
    const i32 fb = fmp::feedback(s[6]);
    output(f, view::extra, 1, l.spin, s[7], wave_key(s, l), [&](u32 u, u32 u16) {
        const u32 offset = (static_cast<u32>(multiply(md::sine(u16 * r1), i1)) + static_cast<u32>(multiply(fed_back(u16 * r2, fb), i2))) << 4;
        return md::sine(u + offset);
    });
}
}
}
