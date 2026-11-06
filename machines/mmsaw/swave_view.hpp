#pragma once
// SuperWave's views, after the design's mnmView (docs/design/Main.dc.html): the
// oscillator rows of SWAVE-SAW and SWAVE-PULSE, and SWAVE-ENS's chord keyboard
// and wave. ENS's black keys come from the rig (md_rig.hpp); these draw what
// changes shape, on the four paths. The rows go unlabelled (tools/art/rig.mjs
// leaves the design's labels out) and use the full width.
//
// A path spans at most 256 px (64 px of points at scale 2), so each full-width
// row is drawn in two halves, left and right, and each half-path carries
// several rows as one serpentine: a row left to right, the next back, the
// turns short uprights at the ends. A path has one shade, so a row's level
// shows in its height, as the design's does. The waves are drawn as the sound
// makes them: SUBX is a saw, as on the Monomachine (the design draws a square).
#include "md_view.hpp"
#include "md_rig.hpp"
#include "swave.hpp"   // ENS's intervals
#include "chord_view.hpp"
#include "live_view.hpp"

namespace md_mmsaw {} namespace md = md_mmsaw; namespace md_mmsaw {
namespace swave_view {
using namespace md::view;

const i32 half = 0x40000000;
inline i32 get(const i32* slots, int slot) { return md::positive(slots[slot]); }
inline u32 v127(i32 slot) { return static_cast<u32>(md::positive(slot)) >> 24; }

// The rows' period across the stage for TUNE 0..127, Q10 px: the design's
// 6 + 6 (tune - 0.5) cycles over 424 px. Its inverse is linear in TUNE.
static const i32 period_of[128] = {
    144725, 142482, 140306, 138196, 136149, 134161, 132231, 130355, 128532, 126759, 125035, 123356,
    121723, 120131, 118581, 117071, 115598, 114162, 112761, 111395, 110061, 108758, 107486, 106243,
    105029, 103842, 102682, 101548, 100438, 99352, 98289, 97249, 96231, 95234, 94257, 93300,
    92362, 91443, 90542, 89659, 88793, 87943, 87110, 86292, 85489, 84701, 83927, 83168,
    82422, 81689, 80970, 80263, 79568, 78885, 78213, 77553, 76904, 76266, 75638, 75021,
    74413, 73816, 73228, 72649, 72079, 71518, 70966, 70422, 69886, 69359, 68839, 68328,
    67823, 67326, 66837, 66354, 65879, 65410, 64947, 64492, 64042, 63599, 63162, 62731,
    62305, 61886, 61472, 61064, 60660, 60263, 59870, 59483, 59100, 58722, 58350, 57981,
    57618, 57259, 56904, 56554, 56208, 55867, 55529, 55196, 54866, 54540, 54219, 53901,
    53586, 53276, 52969, 52665, 52365, 52068, 51775, 51485, 51198, 50914, 50634, 50356,
    50082, 49811, 49542, 49276, 49014, 48754, 48496, 48242,
};
// Cycles per Q10 px, Q28: 2^28 / period.
inline u32 per_px_of(u32 tune) { return 1855u + ((tune * 3739u) >> 7); }

// Points on the stage (Q10) gathered for one path.
struct Pen {
    Point p[64];
    u32 n;
    void to(i32 x, i32 y) {
        if (n < 64) { p[n].x = x; p[n].y = y; ++n; }
    }
    // The points of another pen, last first: a row drawn back.
    void back(const Pen& row) {
        for (u32 i = row.n; i-- > 0;) to(row.p[i].x, row.p[i].y);
    }
};

// Writes a pen into a path about (ox, oy) on the stage at scale 2 and shows it.
template <class F>
inline void commit(F& frame, u32 path, const Pen& pen, i32 ox, i32 oy, u32 key, u32 tone, Paint paint = stroke) {
    typename F::path_type& out = frame.paths[path];
    if (out.key != key && pen.n) {
        Point points[64];
        for (u32 i = 0; i < pen.n; ++i) {
            points[i].x = (pen.p[i].x - ox * unit) / 2;
            points[i].y = (pen.p[i].y - oy * unit) / 2;
        }
        outline(out, points, pen.n, key);
        md::live::unfold(out);
    }
    place(frame, path, sx(ox), sy(oy), 2 * unit, tone, paint);
}

// A periodic wave between xs and xe (stage Q10): its centre and height (Q10),
// its phase at x 0 (Q28 cycles), cycles per Q10 px (Q28) and period (Q10 px).
enum Shape { saw_wave, pulse_wave, sine_wave };
struct Row {
    i32 yc, a;
    u32 phase, per_px;
    i32 period;
    Shape shape;
    u32 duty;     // pulse: the high part, Q28 of a cycle
    i32 wobble;   // pulse: PWAD's sway of the duty from cycle to cycle, Q28
};

const u32 cycle = 1u << 28;
inline u32 fraction(u32 u) { return u & (cycle - 1); }
// y for a wave value -1..1 (Q28 signed as i32 of +-2^28).
inline i32 at(const Row& r, i32 value_q28) { return r.yc - multiply(value_q28, r.a) * 8; }
inline i32 saw_value(u32 f) { return static_cast<i32>(f) * 2 - static_cast<i32>(cycle); }
// The duty of the cycle that starts at u: the design's sway, sin(0.7 x cycle).
inline u32 duty_at(const Row& r, u32 u) {
    if (!r.wobble) return r.duty;
    const i32 s = md::sine((u >> 28) * 478467000u + 239233500u);   // 0.7 rad a cycle, from the cycle's middle
    const i32 d = static_cast<i32>(r.duty) + multiply(s, r.wobble);
    return static_cast<u32>(md::clamp(d, static_cast<i32>(cycle / 25), static_cast<i32>(cycle - cycle / 25)));
}
// The distance to the next point where the wave turns, Q10 px, from phase f into a cycle.
inline i32 distance(const Row& r, u32 f) { return static_cast<i32>(((static_cast<u32>(cycle - f) >> 12) * static_cast<u32>(r.period >> 4)) >> 12); }

// Draws one row from xs to xe into a pen, left to right.
inline void row(Pen& pen, const Row& r, i32 xs, i32 xe) {
    u32 u = r.phase + static_cast<u32>(xs) * r.per_px;
    if (r.shape == sine_wave) {
        // Six points a cycle.
        const i32 step = r.period / 6;
        for (i32 x = xs;; x += step) {
            const i32 last = x >= xe;
            const i32 xx = last ? xe : x;
            const u32 uu = r.phase + static_cast<u32>(xx) * r.per_px;
            pen.to(xx, at(r, opxy::q31::half(opxy::q31::half(opxy::q31::half(md::sine(fraction(uu) << 4))))));
            if (last) return;
        }
    }
    if (r.shape == saw_wave) {
        pen.to(xs, at(r, saw_value(fraction(u))));
        i32 x = xs + distance(r, fraction(u));
        while (x < xe) {
            pen.to(x, at(r, static_cast<i32>(cycle)));
            pen.to(x, at(r, -static_cast<i32>(cycle)));
            x += r.period;
        }
        u = r.phase + static_cast<u32>(xe) * r.per_px;
        pen.to(xe, at(r, saw_value(fraction(u))));
        return;
    }
    // A pulse: high for the duty, then low; uprights at each edge.
    const i32 high = at(r, static_cast<i32>(cycle)), low = at(r, -static_cast<i32>(cycle));
    u32 f = fraction(u), d = duty_at(r, u - f);
    pen.to(xs, f < d ? high : low);
    i32 x = xs;
    while (true) {
        const bool up = f < d;
        // The next edge: the fall at the duty, or the rise at the cycle's end.
        const u32 to_edge = up ? d - f : cycle - f;
        const i32 step = static_cast<i32>(((to_edge >> 12) * static_cast<u32>(r.period >> 4)) >> 12);
        x += step;
        if (x >= xe) break;
        pen.to(x, up ? high : low);
        pen.to(x, up ? low : high);
        if (up) f = d;
        else { u += cycle - f; f = 0; d = duty_at(r, u); }
    }
    pen.to(xe, f < d ? high : low);
}
inline void row_back(Pen& pen, const Row& r, i32 xs, i32 xe) {
    Pen one;
    one.n = 0;
    row(one, r, xs, xe);
    pen.back(one);
}

// SAW's and PULSE's rows in the stock look (W1, OP-XY-Dev media/stock-look-audit/mnmd):
// each row crosses the whole screen, x 0 to 480; the main row CDCDCD at 1.2 px, the
// rest one neutral grey at 1 px; the code on handles at the rows' ends.
//
// A path spans at most 256 px, so a row is drawn in two halves, x 0..240 and
// 240..480, and a path's rows run as a serpentine whose turns are short uprights:
// at the screen's edges, and where the halves meet. Black masks (tools/art/stock.mjs)
// hide those, at the cost of a hairline gap at x 240. A path has one shade, so the
// main row has a pair of paths to itself and every other row shares the other
// pair: it takes all of a path's 64 points, so TUNE's cycles run from 2.5 to 5
// across the screen, not the design's 3.4 to 10.
namespace w1 {
enum Part : u32 { mask_mid = 4, mask_left, mask_right, left0, width0 = left0 + 7, reset = width0 + 4 };
const i32 x0 = unit / 4, xm = 240 * unit, x1 = 480 * unit - unit / 4;   // inside the screen by a quarter pixel
// Cycles per Q10 px, Q28, for TUNE 0..127: 2.5 to 5 cycles over 480 px; and the period, 2^28 / it, Q10 px.
inline u32 per_px_of(u32 tune) { return 1365u + ((tune * 1376u) >> 7); }
static const i32 period_of[128] = {
    196656, 195226, 193676, 192151, 190650, 189306, 187848, 186414, 185000, 183734, 182361, 181008,
    179676, 178481, 177185, 175908, 174649, 173520, 172295, 171087, 169896, 168827, 167667, 166523,
    165395, 164382, 163282, 162197, 161126, 160164, 159120, 158089, 157072, 156158, 155165, 154185,
    153217, 152347, 151402, 150468, 149546, 148718, 147817, 146927, 146048, 145257, 144398, 143548,
    142709, 141954, 141133, 140322, 139519, 138798, 138013, 137237, 136469, 135779, 135028, 134285,
    133550, 132889, 132169, 131457, 130753, 130119, 129429, 128746, 128070, 127462, 126800, 126144,
    125496, 124912, 124276, 123646, 123023, 122461, 121850, 121245, 120645, 120105, 119517, 118935,
    118358, 117838, 117272, 116711, 116156, 115655, 115110, 114569, 114034, 113551, 113025, 112504,
    111988, 111523, 111015, 110513, 110015, 109565, 109076, 108590, 108109, 107676, 107203, 106734,
    106269, 105850, 105393, 104940, 104490, 104085, 103643, 103205, 102770, 102378, 101950, 101526,
    101106, 100726, 100312, 99902, 99494, 99127, 98726, 98328,
};
// The design's row height: 0.42 of the row's pitch at full level, never under 0.06 of it.
inline i32 height(i32 row_height_q10, i32 level) {
    const i32 l = level < 0x07ae147b ? 0x07ae147b : level;
    return multiply(multiply(row_height_q10, 0x35c28f5c), l);
}
// The rows in `order` as serpentines on two half-paths, left (x 0..240, about x 120)
// and right (240..480, about 360), both about y 94. An odd number of rows draws its
// last one back over itself, so each path closes at the screen's edge.
template <class F>
inline void across(F& frame, const Row* rows, const u8* order, u32 count, u32 left, u32 right, u32 key, u32 tone) {
    for (u32 side = 0; side < 2; ++side) {
        Pen pen;
        pen.n = 0;
        for (u32 k = 0; k < count; ++k) {
            const Row& r = rows[order[k]];
            const bool outwards = (k % 2) == 0;   // left: from the edge to the middle; right: likewise
            if (side == 0) { if (outwards) row(pen, r, x0, xm); else row_back(pen, r, x0, xm); }
            else { if (outwards) row_back(pen, r, xm, x1); else row(pen, r, xm, x1); }
        }
        if (count % 2) {
            const Row& r = rows[order[count - 1]];
            if (side == 0) row_back(pen, r, x0, xm); else row(pen, r, xm, x1);
        }
        commit(frame, side ? right : left, pen, side ? 360 : 120, 94, key + side, tone);
    }
}
// The masks over the serpentines' turns, at the edges and down the middle.
template <class F>
inline void masks(F& frame) {
    stock::put(frame, mask_mid, 240 * unit, 94 * unit, stock::black);
    stock::put(frame, mask_left, 717, 94 * unit, stock::black);
    stock::put(frame, mask_right, 480 * unit - 717, 94 * unit, stock::black);
}
// UNIW's handle: where a row detuned by d (Q16, 0 .. 0.16) has drifted half a cycle
// from the main one, x = 240 / (cycles d) px; from the right edge, x 471, while that is past it.
inline i32 drift_x(u32 per_px, i32 d16) {
    const u32 denominator = per_px * static_cast<u32>(d16 > 0 ? d16 : 0);
    const u32 x = denominator ? (0xffffffffu / denominator) * 2u : 471u;
    return static_cast<i32>(x > 471u ? 471u : x) * unit;
}
}
inline u32 frame_key(const i32* s, i32 sliding, u32 salt) {
    u32 key = salt;
    for (u32 i = 0; i < 8; ++i) key = md::live::mix(key, s[i]);
    return md::live::mix(key, sliding) & ~0xfffu;   // the low bits tell the paths apart
}
// A note's scroll: the design's rows slide half a cycle either way, at 1/2 Hz
// (so as far as the design's in its first 0.5 s) until the release, Q28.
inline i32 sliding(const Moment& m) { return multiply(md::live::sway(md::live::of(m), md::live::half_hz), 1 << 27); }
inline u32 slide(i32 s, bool forwards) { return static_cast<u32>(forwards ? s : -s); }
// A detuned row's cycles per px and period, the cycles x (1 + d), d Q31 in -1..1.
inline void detune(Row& r, u32 per_px, i32 period, i32 d) {
    r.per_px = static_cast<u32>(static_cast<i32>(per_px) + multiply(static_cast<i32>(per_px), d));
    r.period = period - multiply(period, d) + multiply(multiply(period, d), d);
}
}

// SWAVE-SAW: UNIL UNIW UNIX - / SUBX SUB1 SUB2 TUNE. Eight rows: the outer and
// inner pairs about the main saw, then the subs. The code: UNIL's inner pair and
// SUBX's row hollow at their left ends (encoder 1), UNIX's outer pair mid
// (encoder 3), SUB1 dark and SUB2 mid; UNIW's dark handles at the unison rows'
// right ends, where each has drifted half a cycle from the main one, coming in
// from the edge as the width rises; TUNE (encoder 4) the cycles on every row, the
// main row CDCDCD.
namespace swave_saw {
template <class F>
inline void view(F& frame, const i32* s, const view::Moment& m) {
    using namespace swave_view;
    const i32 ph = sliding(m);
    const u32 tune = v127(s[7]);
    const u32 per_px = w1::per_px_of(tune);
    const i32 period = w1::period_of[tune];
    const i32 ul = get(s, 0), uw = get(s, 1), ux = multiply(get(s, 2), ul);
    const i32 rh = 17 * unit + 512;   // 17.5 px
    const i32 levels[8] = {ux, ul, md::one, ul, ux, get(s, 4), get(s, 5), get(s, 6)};
    const i32 spread[5] = {multiply(uw, 0x147ae148), multiply(uw, 0x0a3d70a4), 0, -multiply(uw, 0x0a3d70a4), -multiply(uw, 0x147ae148)};   // 0.16, 0.08
    Row rows[8];
    for (u32 i = 0; i < 8; ++i) {
        Row& r = rows[i];
        r.yc = 24 * unit + rh * static_cast<i32>(i) + rh / 2;
        r.a = w1::height(rh, levels[i]);
        r.phase = slide(ph, i % 2 != 0);
        r.shape = i == 6 || i == 7 ? sine_wave : saw_wave;
        r.duty = 0; r.wobble = 0;
        if (i < 5) detune(r, per_px, period, spread[i]);
        else { r.per_px = i == 7 ? per_px >> 2 : per_px >> 1; r.period = i == 7 ? period * 4 : period * 2; }
    }
    const u32 key = frame_key(s, ph, 0);
    static const u8 main_row[1] = {2}, others[7] = {0, 1, 3, 4, 5, 6, 7};
    w1::across(frame, rows, main_row, 1, view::contour, view::wave, key, stock::line4);
    w1::across(frame, rows, others, 7, view::comb, view::extra, key + 0x1000, stock::line_n);
    w1::masks(frame);
    // The left ends: rows 0, 1, 3, 4 (UNIX, UNIL, UNIL, UNIX), then SUBX, SUB1, SUB2.
    static const u8 left_rows[7] = {0, 1, 3, 4, 5, 6, 7}, left_code[7] = {2, 0, 0, 2, 0, 1, 2};
    for (u32 k = 0; k < 7; ++k) stock::put(frame, w1::left0 + k, 9 * unit, rows[left_rows[k]].yc, stock::coded(left_code[k]));
    // UNIW's handles on the unison rows: the outer pair drifts twice as fast as the inner.
    const i32 outer = multiply(uw, 10486), inner = multiply(uw, 5243);   // 0.16 and 0.08, Q16
    static const u8 width_rows[4] = {0, 1, 3, 4};
    for (u32 k = 0; k < 4; ++k)
        stock::put(frame, w1::width0 + k, w1::drift_x(per_px, k == 0 || k == 3 ? outer : inner), rows[width_rows[k]].yc, stock::solid2);
}
}

// SWAVE-PULSE: UNIL UNIW SUB1 SUB2 / PW PWAD PWRS TUNE. Five rows: the pair about
// the main pulse, then the subs; PW and PWAD shape every pulse, PWAD swaying each
// cycle's width. The code: UNIL's pair hollow at their left ends (encoder 1),
// UNIW's dark handles at their right ends, SUB1 mid and SUB2 light; PWRS on draws
// its reset bar, mid, at the rows' left; TUNE the cycles, the main row CDCDCD.
namespace swave_pulse {
template <class F>
inline void view(F& frame, const i32* s, const view::Moment& m) {
    using namespace swave_view;
    const i32 ph = sliding(m);
    const u32 tune = v127(s[7]);
    const u32 per_px = w1::per_px_of(tune);
    const i32 period = w1::period_of[tune];
    const i32 ul = get(s, 0), uw = get(s, 1);
    // The duty as the sound has it (voices/swave.hpp): 1% + 0.98 PW / 128.
    const u32 duty = 2684355u + v127(s[4]) * 2055209u;
    const i32 wobble = multiply(get(s, 5), 0x05999999);   // the design's 0.35 x PWAD, Q28
    const i32 rh = 27 * unit;
    const i32 levels[5] = {ul, md::one, ul, get(s, 2), get(s, 3)};
    const i32 spread[3] = {multiply(uw, 0x0a3d70a4), 0, -multiply(uw, 0x0a3d70a4)};
    Row rows[5];
    for (u32 i = 0; i < 5; ++i) {
        Row& r = rows[i];
        r.yc = 26 * unit + rh * static_cast<i32>(i) + rh / 2;
        r.a = w1::height(rh, levels[i]);
        r.phase = slide(ph, i % 2 != 0);
        r.shape = i < 3 ? pulse_wave : sine_wave;
        r.duty = duty; r.wobble = wobble;
        if (i < 3) detune(r, per_px, period, spread[i]);
        else { r.per_px = i == 4 ? per_px >> 2 : per_px >> 1; r.period = i == 4 ? period * 4 : period * 2; }
    }
    const u32 key = frame_key(s, ph, 0x2000);
    static const u8 main_row[1] = {1}, others[4] = {0, 2, 3, 4};
    w1::across(frame, rows, main_row, 1, view::contour, view::wave, key, stock::line4);
    w1::across(frame, rows, others, 4, view::comb, view::extra, key + 0x1000, stock::line_n);
    w1::masks(frame);
    static const u8 left_rows[4] = {0, 2, 3, 4}, left_code[4] = {0, 0, 2, 3};
    for (u32 k = 0; k < 4; ++k) stock::put(frame, w1::left0 + k, 9 * unit, rows[left_rows[k]].yc, stock::coded(left_code[k]));
    const i32 d16 = multiply(uw, 5243);   // 0.08, Q16
    for (u32 k = 0; k < 2; ++k) stock::put(frame, w1::width0 + k, w1::drift_x(per_px, d16), rows[k ? 2 : 0].yc, stock::solid2);
    // PWRS: every row restarts at the left, so its bar spans the rows.
    if (s[6] >= half) stock::put(frame, w1::reset, 2304, 93 * unit + 512, stock::solid3);
}
}

namespace swave_view {
// ENS's wave over x 258..472 at centre cy, `tall` Q10 px high: about 3 (1 + d)
// cycles at TUNE's middle, 1.5 to 4.5 across it as the rows' are, of the saw
// crossfaded by wv into the octave-down pulse of `duty` (Q24 of its two
// cycles); exact at every edge, straight between.
inline void ens_wave(Pen& pen, i32 cy, i32 tall, i32 d, u32 duty, i32 wv, u32 tune) {
    const u32 one = 1u << 24;                                   // a cycle, Q24
    const i32 xs = 258 * unit, px = 214 * unit;
    // The rows' cycles over 214 px, Q24, brought from TUNE's 3.04 to 3 at its middle
    // (0.9868, 1.0134 for the period).
    const i32 cycles = multiply(static_cast<i32>((static_cast<u32>(px) * per_px_of(tune)) >> 4), 0x7e50fe50);
    const u32 end = static_cast<u32>(cycles + multiply(cycles, d));
    const i32 base = multiply(period_of[tune], 0x40dbba2c) * 2;
    // Q10 px per Q24 cycle: the period / (1 + d), as (1 - d + d^2).
    const i32 per = base - multiply(base, d) + multiply(multiply(base, d), d);
    u32 edges[6];
    u32 e = 0;
    for (u32 c = 1; c * one < end && e < 6; ++c) edges[e++] = c * one;                  // the saw's resets (and the pulse's rise at 2)
    if (2 * duty < end) edges[e++] = 2 * duty;                                           // the pulse falls
    if (2 * one + 2 * duty < end && e < 6) edges[e++] = 2 * one + 2 * duty;
    for (u32 i = 1; i < e; ++i)
        for (u32 j = i; j > 0 && edges[j] < edges[j - 1]; --j) { const u32 t = edges[j]; edges[j] = edges[j - 1]; edges[j - 1] = t; }
    u32 from = 0;
    for (u32 i = 0; i <= e; ++i) {
        const u32 to = i < e ? edges[i] : end;
        for (u32 side = 0; side < 2; ++side) {
            const u32 u = side ? to - 1 : from + 1;                                      // just inside the stretch
            const i32 sawv = static_cast<i32>((u & (one - 1)) * 2) - static_cast<i32>(one);   // Q24, -1 .. 1
            const i32 pv = (u & (2 * one - 1)) < 2 * duty ? static_cast<i32>(one) : -static_cast<i32>(one);
            const i32 v = multiply(sawv, md::one - wv) + multiply(pv, wv);
            const i32 x = xs + static_cast<i32>((u >> 12) * static_cast<u32>(per)) / 4096;
            pen.to(md::clamp(x, xs, xs + px), cy - multiply(v, tall) * 128);
        }
        from = to;
    }
}
}

// SWAVE-ENS: PCH2 PCH3 PCH4 WAVE / PW CHRL CHRW TUNE. The chord on the keyboard
// (chord_view.hpp), and on the right the wave as WAVE and PW shape it, with the
// chorus's two copies. The design fills the chord's keys, but a mutable path
// is never filled on the device (the SDK's artwork gives it no fill).
namespace swave_ens {
template <class F>
inline void view(F& frame, const i32* s, const view::Moment& m) {
    using namespace swave_view;
    // The voices' semitones, rounded; the chord sits an octave up when a voice is below the main one.
    static const signed char semis[33] = {-12, -11, -10, -9, -8, -7, -7, -6, -5, -5, -4, -4, -3, -3, -2, -1,
                                          0, 1, 2, 3, 3, 4, 4, 5, 5, 6, 7, 7, 8, 9, 10, 11, 12};
    i32 lit[4];
    u32 count = 0;
    lit[count++] = 0;
    bool below = false;
    for (u32 v = 0; v < 3; ++v) {
        const unsigned step = md::swave_ens::step_of[v127(s[v])];
        if (step == md::swave_ens::off_step) continue;
        lit[count++] = semis[step];
        below = below || semis[step] < 0;
    }
    u32 shown = 0;
    for (u32 k = 0; k < count; ++k) {
        const i32 semi = lit[k] + (below ? 12 : 0);
        if (semi <= 23) lit[shown++] = semi;   // a voice an octave over the keyboard is left off it
    }
    const u32 keys_before = frame.paths[view::contour].key;
    chord_view::keyboard(frame, view::contour, lit, shown);
    if (frame.paths[view::contour].key != keys_before) md::live::unfold(frame.paths[view::contour]);
    // The wave: 3 cycles over x 258..472, the saw crossfading into the octave-down
    // pulse of duty PW / 128 (folded about 64), at the design's height of 34 px;
    // the chorus's copies above and below, a little wider and narrower. CHRL
    // fades the sound into the copies, so they part from the wave and brighten
    // with it, from nothing at 0; CHRW spreads them further.
    u32 pw = v127(s[4]);
    if (pw > 64) pw = 128 - pw;
    const u32 duty = (2684355u + pw * 2055209u) >> 4;   // of the octave-down cycle, Q24
    const i32 wv = get(s, 3), cl = get(s, 5), cw = get(s, 6);
    const i32 d = multiply(cw, 0x051eb852);             // the design's 0.04 cw
    // Each wave on its own path, drawn out and back over itself, so no stroke joins two of them.
    // While a note plays the wave breathes and the copies sway in to it and back
    // out, at the chorus's own drift of 1/2 Hz.
    const md::live::Motion held = md::live::of(m);
    const i32 tall = 34 * unit - multiply(md::live::swell(held, md::live::one_hz), 7 * unit);
    const i32 reach = multiply(cl, 8 * unit + multiply(cw, 14 * unit));
    const i32 apart = reach - multiply(reach, multiply(md::live::swell(held, md::live::half_hz), 0x60000000));
    const i32 ys[3] = {78 * unit, 78 * unit - apart, 78 * unit + apart};
    const i32 ds[3] = {0, d, -d};
    const u32 paths[3] = {view::comb, view::extra, view::wave};
    const bool chorus = apart >= unit;   // nearer than a pixel they only thicken the wave
    const u32 copies = cl < 0x20000000 ? view::faint : cl < 0x50000000 ? view::dim : view::dash;
    u32 key = md::live::mix(md::live::mix(0x51a7e002u, wv), s[4]);
    key = md::live::mix(md::live::mix(md::live::mix(key, s[7]), tall), cw);
    for (u32 k = 0; k < 3; ++k) {
        if (k && !chorus) break;
        Pen pen;
        pen.n = 0;
        ens_wave(pen, ys[k], tall, ds[k], duty, wv, v127(s[7]));
        const Pen out = pen;
        pen.back(out);
        commit(frame, paths[k], pen, 365, 78, md::live::sealed(md::live::mix(key, static_cast<i32>(k) + (k ? apart : 0))), k ? copies : view::line);
    }
}
}
}
