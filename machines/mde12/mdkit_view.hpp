#pragma once
// The drum kits' per-pad screens, the whole screen: MD KIT's machines draw the
// sounding part of their instrument in section (namespace fig), MD E12's a
// strip of tape on a splicing block (namespace fig_e12), centred under the header.
#include "trx_view.hpp"

namespace md_mde12 {} namespace md = md_mde12; namespace md_mde12 {
// MD KIT's sections: the sounding part of each instrument drawn large in
// section, as on an engraved patent plate, the whole screen. Each control moves one physical part, the same way across the
// families: pitch the head's waves (a fork's tines, a cymbal's or bell's size),
// decay how far they carry or how many rings leave an edge, a ramp or bump the
// dent the striker presses in, tone and filters the snare wires, a cymbal's
// thickness and the teeth of its rim, metal and harmonics hammer marks and the
// overtone riding the head, a room the echo chamber, distortion the clamps
// that close on the head. How far a control moves its part follows how much it
// changes the sound (travel, measured by tools/art/travel.py). The sections are
// drawn on the four paths; the fixed parts (tools/art/fig.mjs) sit in the
// common nodes' places, which md_view.hpp's begin hides every frame.
namespace fig {
using namespace md::view;
using opxy::u16;

// travel:begin (tools/art/travel.py writes this table: edit the tool, then run it)
// How far each control moves its part, Q8 (256 = all the way), in proportion to
// the change it makes to the sound (full at 24 dB of band energy, at least 0.2).
static const opxy::u16 travel[33][8] = {
    {69, 141, 154, 76, 51, 124, 51, 207},   // TRX-BD: PTCH 6.5, DEC 13.2, RAMP 14.5, RDEC 7.1, STRT 4.3, NOIS 11.6, HARM 1.7, CLIP 19.4
    {51, 224, 51, 51, 78, 63, 51, 160},   // TRX-SD: PTCH 0.7, DEC 21.0, BUMP 0.8, BENV 0.5, SNAP 7.4, TONE 5.9, TUNE 4.8, CLIP 15.0
    {169, 256, 61, 73, 51, 99, 51, 0},   // TRX-XT: PTCH 15.8, DEC 27.9, RAMP 5.7, RDEC 6.8, DAMP 3.8, DIST 9.3, DTYP 1.5
    {51, 51, 51, 222, 219, 51, 51, 51},   // TRX-CP: CLPY 2.3, TONE 3.4, HARD 3.5, RICH 20.8, RATE 20.6, ROOM 3.2, RSIZ 4.7, RTUN 4.2
    {150, 256, 189, 0, 0, 0, 0, 0},   // TRX-RS: PTCH 14.1, DEC 33.5, DIST 17.7
    {163, 256, 51, 51, 113, 51, 0, 0},   // TRX-CB: PTCH 15.3, DEC 62.2, ENH 4.2, DAMP 3.5, TONE 10.6, BUMP 1.9
    {51, 256, 145, 89, 51, 0, 0, 0},   // TRX-CH: GAP 4.6, DEC 28.8, HPF 13.6, LPF 8.4, MTAL 1.7
    {57, 256, 146, 90, 51, 0, 0, 0},   // TRX-OH: GAP 5.3, DEC 27.1, HPF 13.7, LPF 8.4, MTAL 2.0
    {57, 100, 51, 70, 61, 51, 0, 0},   // TRX-CY: RICH 5.3, DEC 9.4, TOP 3.8, TTUN 6.6, SIZE 5.7, PEAK 4.7
    {256, 256, 256, 51, 51, 51, 84, 56},   // TRX-MA: ATT 69.8, SUS 49.7, REV 52.2, DAMP 2.6, RATL 4.2, RTYP 2.8, TONE 7.9, HARD 5.3
    {135, 86, 89, 51, 113, 51, 0, 0},   // TRX-CL: PTCH 12.7, DEC 8.1, DUAL 8.4, ENH 1.3, TUNE 10.6, CLIC 0.0
    {200, 243, 68, 93, 51, 117, 51, 0},   // TRX-XC: PTCH 18.8, DEC 22.8, RAMP 6.3, RDEC 8.8, DAMP 3.9, DIST 11.0, DTYP 1.2
    {185, 256, 91, 209, 51, 78, 256, 166},   // TRX-B2: PTCH 17.4, DEC 25.7, RAMP 8.5, HOLD 19.6, TICK 0.0, NOIS 7.4, DIRT 41.7, DIST 15.6
    {256, 256, 218, 175, 232, 95, 234, 243},   // EFM-BD: PTCH 26.9, DEC 36.7, RAMP 20.4, RDEC 16.4, MOD 21.8, MFRQ 8.9, MDEC 22.0, MFB 22.7
    {199, 256, 126, 256, 185, 80, 171, 126},   // EFM-SD: PTCH 18.6, DEC 39.1, NOIS 11.8, NDEC 24.0, MOD 17.4, MFRQ 7.5, MDEC 16.0, HPF 11.8
    {215, 256, 243, 223, 110, 171, 101, 51},   // EFM-XT: PTCH 20.2, DEC 27.5, RAMP 22.8, RDEC 20.9, MOD 10.3, MFRQ 16.0, MDEC 9.5, CLIC 0.1
    {256, 256, 233, 256, 199, 79, 217, 118},   // EFM-CP: PTCH 43.2, DEC 31.6, CLPS 21.9, CDEC 38.4, MOD 18.7, MFRQ 7.4, MDEC 20.3, HPF 11.1
    {256, 256, 256, 127, 125, 82, 255, 51},   // EFM-RS: PTCH 48.1, DEC 29.1, MOD 26.4, HPF 11.9, SNAR 11.7, SPTC 7.7, SDEC 23.9, SMOD 4.7
    {256, 256, 85, 256, 135, 169, 51, 0},   // EFM-CB: PTCH 24.4, DEC 54.1, SNAP 7.9, FB 30.9, MOD 12.6, MFRQ 15.9, MDEC 3.0
    {51, 256, 74, 91, 51, 51, 51, 61},   // EFM-HH: PTCH 2.5, DEC 72.2, TREM 7.0, TFRQ 8.5, MOD 1.7, MFRQ 1.8, MDEC 1.9, FB 5.7
    {256, 256, 123, 137, 227, 84, 51, 0},   // EFM-CY: PTCH 25.6, DEC 70.8, FB 11.5, HPF 12.8, MOD 21.2, MFRQ 7.9, MDEC 3.1
    {96, 78, 51, 51, 51, 51, 0, 0},   // PI-BD: PTCH 9.0, DEC 7.3, HARD 0.0, HAMR 1.1, TENS 3.6, DAMP 0.0
    {176, 148, 51, 51, 51, 83, 156, 0},   // PI-SD: PTCH 16.5, DEC 13.9, HARD 0.3, RING 2.2, TENS 0.7, RVOL 7.8, RDEC 14.6
    {97, 256, 51, 51, 51, 102, 203, 51},   // PI-MT: PTCH 9.1, DEC 30.0, HARD 0.0, HAMR 1.5, TUNE 1.3, DAMP 9.6, SIZE 19.1, POS 0.3
    {205, 213, 51, 73, 0, 0, 0, 0},   // PI-ML: PTCH 19.2, DEC 20.0, HARD 4.8, TENS 6.9
    {161, 256, 107, 0, 69, 82, 0, 0},   // PI-MA: GRNS 15.1, DEC 32.2, GLEN 10.0, SIZE 6.5, HARD 7.7
    {121, 120, 51, 51, 60, 156, 0, 0},   // PI-RS: PTCH 11.3, DEC 11.2, HARD 1.3, RING 0.8, RVOL 5.6, RDEC 14.7
    {145, 122, 136, 51, 57, 61, 148, 75},   // PI-RC: PTCH 13.6, DEC 11.4, HARD 12.7, RING 0.0, AG 5.3, AU 5.7, BR 13.9, GRAB 7.0
    {51, 218, 256, 51, 51, 51, 94, 133},   // PI-CC: PTCH 1.4, DEC 20.4, HARD 27.9, RING 2.1, AG 0.0, AU 0.0, BR 8.8, GRAB 12.5
    {97, 256, 51, 51, 51, 89, 113, 256},   // PI-HH: PTCH 9.1, DEC 28.0, CLSN 3.7, RING 2.2, AG 3.3, AU 8.3, BR 10.6, CLOS 27.5
    {256, 198, 256, 256, 0, 0, 0, 0},   // GND-SN: PTCH 55.9, DEC 18.6, RAMP 54.4, RDEC 41.1
    {256, 0, 0, 0, 0, 0, 0, 0},   // GND-NS: DEC 123.6
    {256, 256, 198, 251, 0, 0, 0, 0},   // GND-IM: UP 58.0, UVAL 63.1, DOWN 18.6, DVAL 23.5
};
// travel:end

// tools/art/fig.mjs's parts, in its order, in the common nodes' places.
enum Part : u32 { ring_r0 = 4, ring_l0, ring_r1, ring_l1, ring_r2, ring_l2, rod, clutch, hand, clamp_t, clamp_b, felt,
                  speaker, mic, stand, front0, front1, front2, front3, bead0, part_end = bead0 + 8 };
static const u8 part_shade[part_end - ring_r0] = {mid, mid, mid, mid, dim, dim, dim, outline2, outline2, outline2, outline2,
    outline2, outline2, outline2, mid, thin, mid, mid, mid, fill4, fill4, fill4, fill4, fill4, fill4, fill4, fill4};

// e^(-x / reach) per pixel, Q16, as DEC (or a reach) sets the reach from 12 to 202 px: by its value in 64ths.
static const u16 per_reach[65] = {5461, 4378, 3654, 3135, 2745, 2441, 2198, 1999, 1833, 1693, 1572, 1468, 1376, 1295, 1224,
    1159, 1101, 1049, 1002, 958, 918, 882, 848, 816, 787, 760, 735, 711, 689, 668, 648, 630, 612, 596, 580, 565, 551, 538,
    525, 513, 501, 490, 479, 469, 459, 450, 441, 432, 424, 416, 408, 401, 394, 387, 380, 374, 368, 362, 356, 350, 345, 339,
    334, 329, 324};
// 1 / width of a dent, 4 .. 38 px, Q16 per pixel, by its control in 64ths.
static const u16 per_width[65] = {16384, 14463, 12945, 11716, 10700, 9846, 9118, 8490, 7944, 7463, 7037, 6658, 6317, 6009,
    5730, 5476, 5243, 5029, 4832, 4650, 4481, 4324, 4178, 4041, 3913, 3792, 3679, 3573, 3472, 3377, 3287, 3202, 3121, 3044,
    2970, 2901, 2834, 2770, 2709, 2651, 2595, 2542, 2491, 2441, 2394, 2348, 2305, 2262, 2222, 2182, 2144, 2108, 2072, 2038,
    2005, 1973, 1942, 1912, 1883, 1854, 1827, 1800, 1774, 1749, 1725};

const i32 px = unit;   // a pixel, Q10
// The drawings are laid out about x 362, where they sat beside the instrument;
// the whole screen is theirs now, so they draw centred, x 240.
const i32 shift = -122 * unit;

// A machine's controls, each moved about its middle by its travel.
struct Ctl {
    const i32* s;
    const u16* k;
    u32 raw(int slot, u32 otherwise = 64) const { return trx_panel::raw(s, slot, otherwise); }
    // 0 .. 65536 (Q16): the control, its travel about the middle; the middle when absent.
    i32 v(int slot) const {
        if (slot < 0) return 32768;
        const i32 r = static_cast<i32>(raw(slot)) * 516;
        return 32768 + (((r - 32768) * static_cast<i32>(k[slot])) >> 8);
    }
    bool has(int slot) const { return slot >= 0; }
};
// From a to b (Q10 pixels, or any value within 2^21) by t (Q16).
inline i32 lerp(i32 a, i32 b, i32 t) { return a + (((b - a) >> 4) * (t >> 4) >> 8); }
inline i32 scaleq(i32 a, i32 t) { return ((a >> 4) * (t >> 4)) >> 8; }   // a t, t Q16

// A run of stage points (Q10) for one path.
struct Pts {
    Point p[64];
    u32 n;
    Pts() : n(0) {}
    void add(i32 x, i32 y) { if (n < 64) { p[n].x = x; p[n].y = y; ++n; } }
};
// A rotation (tenths of a degree) of a vector, Q10.
inline void spin(i32& x, i32& y, Turn t) {
    const i32 rx = (x >> 5) * t.cos - (y >> 5) * t.sin, ry = (x >> 5) * t.sin + (y >> 5) * t.cos;
    x = rx >> 5; y = ry >> 5;
}

// Draws points as a closed outline (or, `ribbon`, a line out and back) on a
// path about (ox, oy) on the stage, at scale 2, turned by `tenths` about it.
// The key is the points' own hash, so an unchanged shape costs nothing.
template <class F>
inline void draw(F& f, u32 path, const Pts& q, i32 ox, i32 oy, u32 tone, bool ribbon = false, i32 tenths = 0) {
    Point local[64];
    u32 key = ribbon ? 2166136261u : 84696351u;
    const u32 n = ribbon && q.n > 32 ? 32 : q.n;
    for (u32 i = 0; i < n; ++i) {
        local[i].x = (q.p[i].x - ox) / 2;
        local[i].y = (q.p[i].y - oy) / 2;
        key = (key ^ static_cast<u32>(local[i].x)) * 16777619u;
        key = (key ^ static_cast<u32>(local[i].y)) * 16777619u;
    }
    key |= 1;
    if (f.paths[path].key != key) {
        if (ribbon) polyline(f.paths[path], local, n, key);
        else outline(f.paths[path], local, n, key);
        f.paths[path].key = key;
    }
    place(f, path, ox + shift, stage_y * unit + oy, 2 * unit, tone, stroke);
    if (tenths) opxy::draw::turn(f, path, rotation(tenths));
}
// A fixed part at (x, y) on the stage (Q10), scaled and turned.
template <class F>
inline void put(F& f, u32 part, i32 x, i32 y, i32 scale = unit, i32 tenths = 0) {
    place(f, part, x + shift, stage_y * unit + y, scale, part_shade[part - ring_r0],
          part_shade[part - ring_r0] >= fill1 ? fill_and_stroke : stroke);
    if (tenths) opxy::draw::turn(f, part, rotation(tenths));
}
// Rings of sound at an edge: 1 to 3 as DEC (or a ring count) rises, pushed out
// `out` by a hit. side: -1 left, 1 right, 0 both, at xl and xr.
template <class F>
inline void rings(F& f, i32 xl, i32 xr, i32 y, i32 count16, i32 out, int side = 0, i32 scale = unit) {
    const u32 n = 1 + static_cast<u32>((count16 * 2 + 32768) >> 16);
    for (u32 k = 0; k < n && k < 3; ++k) {
        if (side >= 0) put(f, ring_r0 + 2 * k, xr + out, y, scale);
        if (side <= 0) put(f, ring_l0 + 2 * k, xl - out, y, scale);
    }
}

// The hit, in sixteen steps so the screen redraws a few times a hit, not every
// frame: `left` Q16 falls from 1 as it lands to 0, `step` 0..15, -1 at rest.
struct Hit {
    i32 left, step;
    explicit Hit(const view::Moment& m, rig::Timing t = rig::drum) {
        const i32 ph = m.phase(t.length, t.speed);
        step = ph < 0 ? -1 : ph >> 6;
        left = ph < 0 ? 0 : (15 - step) << 12;   // Q16, 15/16 .. 0
    }
    bool on() const { return step >= 0; }
};

// ---- Membranes: a drum's head across its shell, in section ----
enum Striker { beater, stick, mallet, rimstick };
struct Mem {
    int ptch, dec, dent, dentw, lift, ball, hold, clip, clipk, felt, harm, hfrq, hreach, kink, wires, wtune, wlift, wkink, fan, pos, size, hpf;
    Striker striker;
    int depth;        // 0 a kick's, 1 a tom's, 2 a snare's
    bool spring;      // an FM drum's exciter under the head
};
inline Mem mem(Striker s, int depth) {
    Mem c;
    c.ptch = c.dec = c.dent = c.dentw = c.lift = c.ball = c.hold = c.clip = c.clipk = c.felt = c.harm = c.hfrq = c.hreach = c.kink = -1;
    c.wires = c.wtune = c.wlift = c.wkink = c.fan = c.pos = c.size = c.hpf = -1;
    c.striker = s; c.depth = depth; c.spring = false;
    return c;
}

// A striker: a beater or mallet swinging from a pivot above, a stick from a
// fulcrum at its butt, or a stick lying across the rim. Its tip lands at (tx, ty).
template <class F>
inline void striker(F& f, u32 path, Striker kind, i32 tx, i32 ty, i32 raise16, i32 ball, bool hit, i32 x0) {
    Pts q;
    if (kind == beater || kind == mallet) {
        // Hung from (px, 4): the ball's centre is the tip less its radius.
        const i32 len = ty - ball - 4 * px;
        const i32 tenths = hit ? 0 : lerp(50, 400, raise16) * (kind == beater ? 1 : -1);
        const Turn t = rotation(tenths);
        const i32 pivx = tx, pivy = 4 * px;
        const i32 w = (kind == beater ? 11 : 10) * px / 10;
        // Down the shaft's left edge, around the ball, up its right edge, turned about the pivot.
        i32 xs[24], ys[24];
        u32 n = 0;
        xs[n] = -w; ys[n++] = 0;
        xs[n] = -w; ys[n++] = len - ball + px;
        for (u32 k = 0; k <= 16; ++k) {
            const Turn a = rotation(static_cast<i32>(2700 - 160 - k * 3280 / 16));   // round the ball from the shaft's left
            xs[n] = (a.cos * ball) >> 10; ys[n++] = len + ((-a.sin * ball) >> 10);
        }
        xs[n] = w; ys[n++] = len - ball + px;
        xs[n] = w; ys[n++] = 0;
        for (u32 i = 0; i < n; ++i) { i32 x = xs[i], y = ys[i]; spin(x, y, t); q.add(pivx + x, pivy + y); }
        draw(f, path, q, pivx, pivy + len / 2, thin);
        return;
    }
    // A stick, 84 px from butt to tip, its tip at (tx, ty): from the upper left at
    // 33 degrees, or lying nearly flat across the rim. `raise` lifts the tip about the butt.
    const i32 len = 84 * px;
    const i32 slope = kind == stick ? 330 : 60;
    const Turn t0 = rotation(slope);
    const i32 bx = tx - ((t0.cos * (len >> 10))), by = ty - ((t0.sin * (len >> 10)));
    const i32 tenths = kind == stick ? (hit ? 0 : -lerp(30, 260, raise16)) : (hit ? -40 : 0);
    const Turn t = rotation(slope + tenths);
    // Along +x from the butt: 3.2 px wide there, 1.4 at the tip, and a round tip `ball` across.
    i32 xs[16], ys[16];
    u32 n = 0;
    xs[n] = 0; ys[n++] = -32 * px / 10;
    xs[n] = len - ball; ys[n++] = -14 * px / 10;
    for (u32 k = 1; k < 8; ++k) {
        const Turn a = rotation(-900 + static_cast<i32>(k) * 1800 / 8);
        xs[n] = len - ball + ((a.cos * ball) >> 10); ys[n++] = (a.sin * ball) >> 10;
    }
    xs[n] = len - ball; ys[n++] = 14 * px / 10;
    xs[n] = 0; ys[n++] = 32 * px / 10;
    for (u32 i = 0; i < n; ++i) { i32 x = xs[i], y = ys[i]; spin(x, y, t); q.add(bx + x, by + y); }
    i32 mx = len / 2, my = 0;
    spin(mx, my, t);
    draw(f, path, q, bx + mx, by + my, thin);
    (void)x0;
}

template <class F>
inline void membrane(F& f, const Ctl& c, const Mem& m, const view::Moment& moment) {
    const Hit h(moment);
    const i32 w = m.size >= 0 ? lerp(88 * px, 116 * px, c.v(m.size)) : 116 * px;   // a bigger drum is lower
    const i32 cx = 362 * px, x0 = cx - w, x1 = cx + w, L = x1 - x0, yh = 64 * px;
    i32 yb = (m.depth == 0 ? 148 : m.depth == 1 ? 132 : 116) * px;
    if (m.hpf >= 0) yb -= scaleq(18 * px, c.v(m.hpf));   // a high-pass thins the shell
    // Where it is struck, and how deep the dent (RAMP, BUMP, TENS) and how wide.
    const i32 xs = m.pos >= 0 ? x0 + scaleq(L, lerp(4000, 32000, c.v(m.pos))) : x0 + L / 5;
    i32 depth = m.dent >= 0 ? lerp(2 * px, 24 * px, c.v(m.dent)) : 3 * px;
    if (!h.on()) depth = depth * 7 / 8;
    const u32 wj = m.dentw >= 0 ? static_cast<u32>(c.v(m.dentw)) >> 10 : 20;
    // The clamps: closer as distortion rises, shown only when it is up.
    const bool clamped = m.clip >= 0 && c.raw(m.clip, 0) > 2;
    const i32 gap = clamped ? lerp(30 * px, 8 * px, c.v(m.clip)) : 64 * px;
    // The head's waves: PTCH half-waves, carried DEC's reach from the strike, an
    // overtone riding them, a hit's swell falling back in sixteen steps.
    const i32 amp = h.on() ? 8 * px + scaleq(8 * px, h.left) : 8 * px;
    const i32 half = lerp(2 * 256, 8 * 256, c.v(m.ptch));                       // Q8 half-waves across
    const i32 inv = per_reach[static_cast<u32>(c.v(m.dec)) >> 10];
    i32 hamp = 0, hmul = 3 * 256 + 180;
    if (m.harm >= 0) hamp = scaleq(scaleq(amp, 36000), c.v(m.harm));
    else if (m.hfrq >= 0) hamp = scaleq(amp, 12000);
    if (m.hfrq >= 0) hmul = lerp(384, 6 * 256, c.v(m.hfrq));
    if ((half * hmul >> 8) > 24 * 256) hmul = (24 * 256 << 8) / (half >> 4) >> 4;
    const i32 hinv = m.hreach >= 0 ? per_reach[static_cast<u32>(c.v(m.hreach)) >> 10] : inv;
    const i32 jag = m.kink >= 0 ? scaleq(5 * px, c.v(m.kink)) : 0;
    Pts head;
    for (u32 i = 0; i < 32; ++i) {
        const i32 x = x0 + L * static_cast<i32>(i) / 31;
        const i32 d = x > xs ? x - xs : xs - x;
        i32 taper = static_cast<i32>(i < 16 ? i : 31 - i) * 14 * 65536 / 31;
        if (taper > 65536) taper = 65536;
        const u32 ph = static_cast<u32>(half) * i * 270600u;                      // pi half t, t = i / 31
        i32 y = multiply(multiply(amp, md::sine(ph)), md::exp_neg((d >> 4) * inv >> 6));
        if (hamp) {
            const u32 hp = static_cast<u32>((half * hmul) >> 8) * i * 270600u + 0x20000000u;
            y += multiply(multiply(hamp, md::sine(hp)), md::exp_neg((d >> 4) * hinv >> 6));
        }
        if (jag) y += scaleq(jag, static_cast<i32>(trx_panel::rnd(i * 7 + 3))) - jag / 2;
        y = scaleq(y, taper);
        const i32 lim = gap - 3 * px / 2;
        y = md::clamp(y, -lim, lim);
        const i32 g = (d >> 4) * static_cast<i32>(per_width[wj]) >> 12;             // (d / width), Q12 ... squared below
        const i32 gq = (g * g) >> 8;                                                 // Q16
        y += multiply(depth, md::exp_neg(gq));
        head.add(x, yh + md::clamp(y, -lim, lim + depth));
    }
    draw(f, md::view::contour, head, cx, yh, thin, true);
    // The shell: hoops capping its walls, the bottom head and its hoop as a band.
    Pts shell;
    const i32 t4 = 9 * px / 2;
    const i32 sp[16][2] = {{x0 - 9 * px, yh - t4}, {x0 - px, yh - t4}, {x0 - px, yh + t4}, {x0 - 3 * px, yh + t4}, {x0 - 3 * px, yb - px},
                           {x1 + 3 * px, yb - px}, {x1 + 3 * px, yh + t4}, {x1 + px, yh + t4}, {x1 + px, yh - t4}, {x1 + 9 * px, yh - t4},
                           {x1 + 9 * px, yh + t4}, {x1 + 7 * px, yh + t4}, {x1 + 7 * px, yb + 4 * px}, {x0 - 7 * px, yb + 4 * px},
                           {x0 - 7 * px, yh + t4}, {x0 - 9 * px, yh + t4}};
    for (u32 i = 0; i < 16; ++i) shell.add(sp[i][0], sp[i][1]);
    draw(f, md::view::wave, shell, cx, (yh + yb) / 2, mid);
    // The striker, lifted by its control (STRT, SNAP, HAMR); HOLD rests it in the dent.
    i32 raise = c.v(m.lift);
    if (m.hold >= 0) raise = lerp(raise, 0, c.v(m.hold));
    const i32 ball = m.ball >= 0 ? lerp(m.striker == stick || m.striker == rimstick ? 4 * px : 10 * px,
                                        m.striker == stick || m.striker == rimstick ? 2 * px : 5 * px, c.v(m.ball))
                                 : (m.striker == stick || m.striker == rimstick ? 5 * px / 2 : 8 * px);
    const i32 contact = yh + (h.on() || (m.hold >= 0 && c.raw(m.hold, 0) > 100) ? depth : 0);
    if (m.striker == rimstick) striker(f, md::view::comb, rimstick, x0 + 80 * px, yh - 2 * px, 0, ball, h.on(), x0);
    else striker(f, md::view::comb, m.striker, xs, contact - 2 * px, raise, ball, h.on(), x0);
    // The clamps, their tilt the distortion's character (DTYP).
    if (clamped) {
        const i32 tilt = m.clipk >= 0 ? lerp(-40, 40, c.v(m.clipk)) : 0;
        const i32 sc = (w * 1024) / (116 * px);   // their length follows the drum's
        put(f, clamp_t, cx, yh - gap - 3 * px / 2, sc < unit ? sc : unit, tilt);
        put(f, clamp_b, cx, yh + gap + 3 * px / 2, sc < unit ? sc : unit, -tilt);
    }
    // A felt damper inside the shell, wider as DAMP rises.
    if (m.felt >= 0) put(f, felt, x1 - 30 * px, yh + 8 * px, lerp(560, 1700, c.v(m.felt)));
    // Under the head: snare wires on their strainer, or an FM drum's exciter, or NOIS's spray.
    Pts x;
    if (m.wires >= 0) {
        const i32 on = c.v(m.wires);
        const i32 drop = lerp(16 * px, 2 * px, on) + (h.on() ? scaleq(4 * px, h.left) : 0);
        const i32 a = lerp(3 * px / 2, 9 * px / 2, on);
        const i32 per = m.wtune >= 0 ? lerp(7 * px, 21 * px / 5, c.v(m.wtune)) : 5 * px;   // TUNE winds them tighter
        const i32 reach = m.wlift >= 0 ? lerp(L / 3, L - 40 * px, c.v(m.wlift)) : L - 48 * px;
        const i32 jig = m.wkink >= 0 ? scaleq(3 * px, c.v(m.wkink)) : 0;
        const i32 wx0 = cx - reach / 2, wx1 = cx + reach / 2, y = yb + 4 * px + drop + a;
        x.add(wx0 - 7 * px, y - 3 * px); x.add(wx0, y - 3 * px);
        u32 k = 0;
        for (i32 xx = wx0; xx <= wx1 && x.n < 60; xx += per / 2, ++k)
            x.add(xx, y + (k & 1 ? a : -a) + (jig ? scaleq(jig, static_cast<i32>(trx_panel::rnd(k + 5))) - jig / 2 : 0));
        x.add(wx1, y - 3 * px); x.add(wx1 + 7 * px, y - 3 * px);
        x.add(wx1 + 7 * px, y + a + 3 * px); x.add(wx0 - 7 * px, y + a + 3 * px);
        draw(f, md::view::extra, x, cx, y, c.raw(m.wires, 64) > 8 ? thin : dim);
    } else if (m.spring) {
        // The exciter: a magnet on the shell's floor, its spring up to the head;
        // the modulation (MOD) stretches it, its ratio (MFRQ) winds more coils.
        const i32 my = yb - 6 * px, top = yh + 10 * px;
        const i32 coils = m.hfrq >= 0 ? lerp(3 * 256, 9 * 256, c.v(m.hfrq)) >> 8 : 5;
        const i32 a = m.harm >= 0 ? lerp(2 * px, 8 * px, c.v(m.harm)) : 4 * px;
        x.add(cx - 10 * px, my + 4 * px); x.add(cx - 10 * px, my - 4 * px); x.add(cx, my - 4 * px);
        const i32 n = coils * 2;
        for (i32 k = 1; k < n && x.n < 30; ++k) x.add(cx + (k & 1 ? a : -a), my - 4 * px - (my - 4 * px - top) * k / n);
        x.add(cx, top);
        draw(f, md::view::extra, x, cx, (my + top) / 2, mid, true);
    } else if (m.fan >= 0) {
        const u32 n = static_cast<u32>((c.v(m.fan) * 8 + 32768) >> 16);
        for (u32 k = 0; k < n && x.n < 30; ++k) {
            const Turn a = rotation(-150 - static_cast<i32>(k) * 1150 / 7);   // up and to the right of the strike
            const i32 r0 = (h.on() ? 18 : 12) * px, r1 = r0 + (6 + static_cast<i32>(trx_panel::rnd(k + 4) >> 13)) * px;
            x.add(xs + (a.cos * (r0 >> 10)), yh - 2 * px + (a.sin * (r0 >> 10)) * 4 / 5);
            x.add(xs + (a.cos * (r1 >> 10)), yh - 2 * px + (a.sin * (r1 >> 10)) * 4 / 5);
        }
        if (n) draw(f, md::view::extra, x, xs, yh - 20 * px, h.on() ? thin : mid, true);
    }
}

// ---- Cymbals: a cymbal's section, bell and bow, on its rod ----
struct Cym {
    int size, dec, thick, teeth, period, reach, marks, ping, bell, gap, tilt, grab, ring, lift, spring, coils, rag;
    bool hats, inverse;   // inverse: the control is a pitch, so a bigger value draws a smaller cymbal
};
inline Cym cym(bool hats, bool inverse) {
    Cym c;
    c.size = c.dec = c.thick = c.teeth = c.period = c.reach = c.marks = c.ping = c.bell = c.gap = c.tilt = c.grab = c.ring = c.lift = c.spring = c.coils = c.rag = -1;
    c.hats = hats; c.inverse = inverse;
    return c;
}
// One half (side -1 or 1) of a cymbal's section, or the whole (side 0), about
// (cx, cy): bell, bow out to the rim `bow` below (or above, flipped), the
// thickness, the teeth of its rim, hammer marks dented into the bow.
struct Profile { i32 r, bellh, bow, thick, teeth, from, rag; u32 period, marks; };
inline void section(Pts& q, i32 cx, i32 cy, const Profile& p, int side, bool flip) {
    const i32 rb = 16 * px, dir = flip ? -1 : 1;
    i32 top[32], bot[32], xs[32];
    for (u32 i = 0; i < 32; ++i) {
        i32 r, sgn;
        if (side) { r = 3 * px + (p.r - 3 * px) * static_cast<i32>(i) / 31; sgn = side; }
        else { const i32 x = -p.r + 2 * p.r * static_cast<i32>(i) / 31; r = x < 0 ? -x : x; sgn = x < 0 ? -1 : 1; if (r < 3 * px) r = 3 * px; }
        xs[i] = cx + sgn * r;
        i32 hgt;
        if (r < rb) { const i32 u = (r << 6) / 16 >> 10; hgt = -p.bellh + p.bellh * u * u / 4096 * 9 / 10; }   // a dome: (r / rb)^2
        else hgt = -p.bellh / 10 + p.bow * ((r - rb) >> 6) / ((p.r - rb) >> 6 ? (p.r - rb) >> 6 : 1);
        // The rim's teeth, from `from` (a fraction of the radius, Q16) out.
        const i32 at = scaleq(p.r, p.from);
        i32 tooth = 0;
        if (side && r > at && p.teeth) {
            const i32 ramp = ((r - at) >> 6) * 1024 / ((p.r - at) >> 6 ? (p.r - at) >> 6 : 1);   // Q10
            tooth = (i % p.period == 0 ? -p.teeth : p.teeth * 3 / 10) * ramp >> 10;
            if (p.rag) tooth += scaleq(p.rag, static_cast<i32>(trx_panel::rnd(i * 5 + 1))) - p.rag / 2;
        }
        // Hammer marks: dents three samples apart from the bell out.
        if (side && p.marks && i >= 7 && i < 7 + 3 * p.marks && (i - 7) % 3 == 1) tooth += 5 * px / 2;
        top[i] = cy + dir * (hgt + tooth);
        bot[i] = cy + dir * (hgt + p.thick - p.thick * 6 * static_cast<i32>(i) / 310);
    }
    for (u32 i = 0; i < 32; ++i) q.add(xs[i], top[i]);
    for (u32 i = 32; i-- > 0;) q.add(xs[i], bot[i]);
}

template <class F>
inline void cymbal(F& f, const Ctl& c, const Cym& m, const view::Moment& moment) {
    const Hit h(moment);
    const i32 cx = 362 * px;
    i32 R = m.size >= 0 ? lerp(72 * px, 122 * px, m.inverse ? 65536 - c.v(m.size) : c.v(m.size)) : 104 * px;
    if (m.hats && R > 108 * px) R = 108 * px;
    Profile p;
    p.r = R;
    p.bellh = m.bell >= 0 ? lerp(7 * px, 19 * px, c.v(m.bell)) : 10 * px;
    p.bow = m.hats ? 12 * px : 14 * px;
    if (h.on() && !m.hats) p.bow += scaleq(8 * px, h.left);   // the bow flexes as it is struck
    p.thick = m.thick >= 0 ? lerp(26 * px / 5, 8 * px / 5, c.v(m.thick)) : 3 * px;
    p.teeth = m.teeth >= 0 ? lerp(0, 34 * px / 10, c.v(m.teeth)) : 0;
    p.period = m.period >= 0 ? 2 + static_cast<u32>((c.v(m.period) * 2 + 32768) >> 16) : 2;
    p.period = 6 - p.period;   // a higher tune: finer teeth
    p.from = m.reach >= 0 ? lerp(50000, 20000, c.v(m.reach)) : 36000;
    p.marks = m.marks >= 0 ? static_cast<u32>((c.v(m.marks) * 4 + 32768) >> 16) : 0;
    p.rag = m.rag >= 0 ? scaleq(5 * px, c.v(m.rag)) : 0;
    const i32 out = h.on() ? scaleq(5 * px, 65536 - h.left) : 0;
    const i32 rscale = m.ring >= 0 ? lerp(820, 1240, c.v(m.ring)) : unit;
    const i32 dec = c.v(m.dec);
    if (m.hats) {
        // The pair: the bottom hat's rim at y 112, the top's GAP (or CLOS) above it,
        // lifted on a hit; CLSN tips the top hat down onto the bottom one.
        const i32 rim = 112 * px;
        i32 gap = m.gap >= 0 ? lerp(2 * px, 32 * px, c.v(m.gap)) : 6 * px;
        if (h.on()) gap += scaleq(6 * px, h.left);
        const i32 ct = rim - gap - p.bow, cb = rim + p.bow;
        const i32 tilt = m.tilt >= 0 ? lerp(0, (gap >> 10) * 6, c.v(m.tilt)) : 0;   // tenths: the top rim down onto the bottom one
        Pts a, b, z;
        section(a, cx, ct, p, -1, false);
        section(b, cx, ct, p, 1, false);
        draw(f, md::view::contour, a, cx, ct, thin, false, tilt);
        draw(f, md::view::wave, b, cx, ct, thin, false, tilt);
        Profile pb = p;
        pb.teeth = 0; pb.marks = 0; pb.rag = 0;
        section(z, cx, cb, pb, 0, true);
        draw(f, md::view::comb, z, cx, cb, thin);
        put(f, rod, cx, 84 * px);
        put(f, clutch, cx, ct - p.bellh - 4 * px);
        rings(f, cx - R - 3 * px, cx + R + 3 * px, rim - gap / 2, dec, out, 0, rscale);
        if (m.spring >= 0) {
            // EFM-HH's tremolo: a spring on the rod above the clutch, stretched by
            // TREM, wound by TFRQ.
            Pts s;
            const i32 top = 8 * px, bottom = ct - p.bellh - 8 * px;
            const i32 coils = m.coils >= 0 ? lerp(3 * 256, 10 * 256, c.v(m.coils)) >> 8 : 5;
            const i32 a = lerp(2 * px, 7 * px, c.v(m.spring));
            s.add(cx, bottom);
            for (i32 k = 1; k < 2 * coils && s.n < 30; ++k) s.add(cx + (k & 1 ? a : -a), bottom - (bottom - top) * k / (2 * coils));
            s.add(cx, top);
            draw(f, md::view::extra, s, cx, (top + bottom) / 2, mid, true);
        }
    } else {
        // A ride or crash on its tilter, tilting as it is struck.
        const i32 cy = 92 * px;
        const i32 tenths = h.on() ? -scaleq(40, h.left) : 0;
        Pts a, b;
        section(a, cx, cy, p, -1, false);
        section(b, cx, cy, p, 1, false);
        draw(f, md::view::contour, a, cx, cy, thin, false, tenths);
        draw(f, md::view::wave, b, cx, cy, thin, false, tenths);
        put(f, rod, cx, cy + p.bellh + 84 * px + 2 * px - p.bellh);
        put(f, clutch, cx, cy - p.bellh - 4 * px);
        rings(f, cx - R - 3 * px, cx + R + 3 * px, cy + p.bow - 6 * px, dec, out, 0, rscale);
        if (m.ping >= 0) {
            // TOP: the bell's ping, as rings above it.
            const u32 n = static_cast<u32>((c.v(m.ping) * 4 + 32768) >> 16);
            Pts s;
            for (u32 k = 0; k < n; ++k) {
                const i32 r = (8 + 5 * static_cast<i32>(k)) * px + out / 2;
                for (u32 j = 0; j <= 6; ++j) {
                    const u32 jj = k & 1 ? 6 - j : j;
                    const Turn t = rotation(-1440 + static_cast<i32>(jj) * 1080 / 6);
                    s.add(cx + (t.cos * (r >> 10)), cy - p.bellh - 2 * px + (t.sin * (r >> 10)));
                }
            }
            if (n) draw(f, md::view::comb, s, cx, cy - p.bellh - 14 * px, mid, true);
        }
        if (m.lift >= 0) {
            // HARD: the stick over the bow, higher for a harder stroke.
            const i32 tx = cx + R * 5 / 8, ty = cy + p.bow * 5 / 8 - 3 * px;
            striker(f, md::view::comb, stick, tx, ty, c.v(m.lift), 5 * px / 2, h.on(), 0);
        }
        if (m.grab >= 0) {
            // GRAB: a hand closing on the rim the sooner it grabs; from 80 it never does.
            if (c.raw(m.grab) < 80) put(f, hand, cx + R + lerp(2 * px, 30 * px, c.v(m.grab)), cy + p.bow);
        }
    }
}

// ---- The echo chamber: a clap's wavefronts from a loudspeaker to a microphone ----
struct Room { int count, spacing, jitter, scale, aim, size, ceiling, rays, cone, ripple, rfrq, rreach; bool pitched; };
template <class F>
inline void chamber(F& f, const Ctl& c, const Room& r, const view::Moment& moment) {
    const Hit h(moment);
    const i32 vx = 362 * px, vy = 76 * px, fx0 = 212 * px, fx1 = 512 * px, fy0 = 4 * px, fy1 = 158 * px;
    // The room: deeper as it grows (RSIZ, or DEC), its ceiling lower for a higher tuning (RTUN).
    const i32 w = lerp(170 * px, 60 * px, c.v(r.size));
    const i32 hh = scaleq(w, lerp(62000, 30000, c.v(r.ceiling)));
    const i32 bx0 = vx - w / 2, bx1 = vx + w / 2, by1 = vy + scaleq(w, 23600), by0 = by1 - hh;
    Pts lw, rw, back;
    lw.add(fx0, fy0); lw.add(bx0, by0); lw.add(bx0, by1); lw.add(fx0, fy1);
    rw.add(fx1, fy0); rw.add(bx1, by0); rw.add(bx1, by1); rw.add(fx1, fy1);
    back.add(bx0, by0); back.add(bx1, by0); back.add(bx1, by1); back.add(bx0, by1);
    draw(f, md::view::contour, lw, (fx0 + bx0) / 2, 84 * px, dim);
    draw(f, md::view::wave, rw, (fx1 + bx1) / 2, 84 * px, dim);
    draw(f, md::view::comb, back, vx, (by0 + by1) / 2, mid);
    const i32 sx = 240 * px, sy = 104 * px, mx = 484 * px, my = 94 * px;
    put(f, speaker, sx, sy, r.cone >= 0 ? lerp(r.pitched ? 1230 : 820, r.pitched ? 820 : 1230, c.v(r.cone)) : unit,
        r.aim >= 0 ? lerp(-80, 80, c.v(r.aim)) : 0);
    put(f, mic, mx, my);
    put(f, stand, mx, my);
    // The hands: wavefronts RATE (CDEC) apart, uneven with CLPY, wider with RICH;
    // a hit sends them across the room.
    const u32 n = r.count >= 0 ? 1 + static_cast<u32>((c.v(r.count) * 3 + 32768) >> 16) : 4;
    const i32 spacing = lerp(5 * px, 18 * px, c.v(r.spacing));
    const i32 travel_out = h.on() ? scaleq(60 * px, 65536 - h.left) : 0;
    const i32 sc = r.scale >= 0 ? lerp(770, 1380, c.v(r.scale)) : unit;
    const i32 jit = r.jitter >= 0 ? c.v(r.jitter) : 0;
    for (u32 k = 0; k < n && k < 4; ++k) {
        const i32 j = scaleq(scaleq(spacing * 6 / 5, jit), static_cast<i32>(trx_panel::rnd(k + 2))) - scaleq(spacing * 3 / 5, jit);
        put(f, front0 + k, sx + 2 * px + travel_out + static_cast<i32>(k) * spacing + j, sy, sc);
    }
    // ROOM: reflections off the walls into the microphone.
    if (r.rays >= 0) {
        const u32 rays = static_cast<u32>((c.v(r.rays) * 5 + 32768) >> 16);
        const i32 marks[5][2] = {{bx0 + w * 3 / 10, by0}, {bx1, by0 + hh * 2 / 5}, {bx0 + w * 7 / 10, by1}, {bx0, by0 + hh * 3 / 5}, {bx0 + w * 11 / 20, by0}};
        Pts q;
        for (u32 k = 0; k < rays; ++k) {
            q.add(k & 1 ? mx - 7 * px : sx + 6 * px, k & 1 ? my - 3 * px : sy);
            q.add(marks[k][0], marks[k][1]);
        }
        if (rays) { q.add(rays & 1 ? mx - 7 * px : sx + 6 * px, rays & 1 ? my - 3 * px : sy); draw(f, md::view::extra, q, vx, 84 * px, h.on() ? mid : dim, true); }
    }
    // FM: the leading front rippled (MOD deep, MFRQ often, MDEC along it).
    if (r.ripple >= 0) {
        const i32 a = lerp(0, 4 * px, c.v(r.ripple));
        const i32 waves = lerp(2 * 256, 8 * 256, c.v(r.rfrq));
        const i32 along = lerp(30 * 64, 64 * 64, c.v(r.rreach)) >> 6;   // of 64
        i32 rr = 14 * px + travel_out + static_cast<i32>(n) * spacing;
        if (rr > 120 * px) rr = 120 * px;
        Pts q;
        for (u32 i = 0; i < 24; ++i) {
            const Turn t = rotation(-260 + static_cast<i32>(i) * 520 / 23);
            const i32 e = static_cast<i32>(i) * 64 / 23 < along ? a : 0;
            const i32 rad = rr + multiply(e, md::sine(static_cast<u32>(waves) * i * 11673000u));
            q.add(sx + 2 * px + (t.cos * (rad >> 10)), sy + (t.sin * (rad >> 10)));
        }
        draw(f, md::view::extra, q, sx + rr, sy, thin, true);
    }
}

// ---- A cowbell in section, on its mount ----
struct Bell { int size, dec, thick, dent, second, damp, lift, rag, ripple, rfrq, rreach; };
template <class F>
inline void cowbell(F& f, const Ctl& c, const Bell& b, const view::Moment& moment) {
    const Hit h(moment);
    const i32 k = lerp(81920, 49152, c.v(b.size));   // Q16 1.25 .. 0.75: a bigger bell is lower
    const i32 cx = (b.second >= 0 && c.raw(b.second, 0) > 2 ? 376 : 362) * px, yt = 30 * px;
    const i32 H = scaleq(76 * px, k), wt = scaleq(20 * px, k), wm = scaleq(38 * px, k), yb = yt + H;
    const i32 t = b.thick >= 0 ? lerp(5 * px / 2, 6 * px, c.v(b.thick)) : 7 * px / 2;
    const i32 dent = b.dent >= 0 ? lerp(0, 6 * px, c.v(b.dent)) : 0;
    const i32 rag = b.rag >= 0 ? scaleq(4 * px, c.v(b.rag)) : 0;
    const i32 ra = b.ripple >= 0 ? scaleq(3 * px, c.v(b.ripple)) : 0;
    const i32 rf = b.rfrq >= 0 ? lerp(2 * 256, 7 * 256, c.v(b.rfrq)) : 3 * 256;
    const i32 rr = b.rreach >= 0 ? lerp(4, 12, c.v(b.rreach)) : 12;
    Pts q;
    // Outer: up the left wall (struck low on it, from the left), across the top, down the right wall.
    for (u32 i = 0; i <= 12; ++i) {
        const i32 y = yb - H * static_cast<i32>(i) / 12, x = cx - wm + (wm - wt) * static_cast<i32>(i) / 12;
        i32 bump = (i == 3 ? dent : i == 2 || i == 4 ? dent / 2 : 0);
        if (ra && (static_cast<i32>(i) > 3 ? static_cast<i32>(i) - 3 : 3 - static_cast<i32>(i)) < rr) bump += multiply(ra, md::sine(static_cast<u32>(rf) * i * 21845333u));
        if (rag) bump += scaleq(rag, static_cast<i32>(trx_panel::rnd(i + 9)));
        q.add(x + bump, y);
    }
    for (u32 i = 0; i <= 12; ++i) {
        const i32 y = yt + H * static_cast<i32>(i) / 12, x = cx + wt + (wm - wt) * static_cast<i32>(i) / 12;
        q.add(x + (rag ? scaleq(rag, static_cast<i32>(trx_panel::rnd(i + 21))) : 0), y);
    }
    // The mouth's front lip, a shallow curve back to the start.
    for (u32 i = 1; i < 8; ++i) {
        const Turn a = rotation(static_cast<i32>(i) * 1800 / 8);
        q.add(cx + ((a.cos * (wm >> 10))), yb + ((a.sin * 5) ));
    }
    const i32 swing = h.on() ? scaleq(40, h.left) : 0;
    draw(f, md::view::contour, q, cx, yt, thin, false, swing);
    // The mouth's back lip, seen through it: TONE's thicker wall stands it further in.
    Pts lip;
    for (u32 i = 0; i <= 8; ++i) {
        const Turn a = rotation(1800 + static_cast<i32>(i) * 1800 / 8);
        lip.add(cx + ((a.cos * ((wm - t) >> 10))), yb + ((a.sin * 5)) - t / 2);
    }
    draw(f, md::view::wave, lip, cx, yt, mid, true, swing);
    put(f, rod, cx, yt - 84 * px - 4 * px);
    put(f, clutch, cx, yt - 3 * px);
    if (b.second >= 0 && c.raw(b.second, 0) > 2) {
        // ENH: a second, smaller bell behind.
        const i32 k2 = lerp(26000, 52000, c.v(b.second)), c2 = 288 * px;
        const i32 H2 = scaleq(76 * px, k2), wt2 = scaleq(20 * px, k2), wm2 = scaleq(38 * px, k2), yt2 = yt + 14 * px;
        Pts s;
        s.add(c2 - wm2, yt2 + H2); s.add(c2 - wt2, yt2); s.add(c2 + wt2, yt2); s.add(c2 + wm2, yt2 + H2);
        draw(f, md::view::extra, s, c2, yt2, mid);
    }
    // The stick, low on the left wall; SNAP lifts it.
    striker(f, md::view::comb, stick, cx - wm + (wm - wt) * 3 / 12 - 2 * px + dent, yb - H * 3 / 12, b.lift >= 0 ? c.v(b.lift) : 16384, 5 * px / 2, h.on(), 0);
    if (b.damp >= 0) put(f, hand, cx + wm + lerp(26 * px, 0, c.v(b.damp)), yb - 4 * px);
    rings(f, cx - wm - 3 * px, cx + wm + 3 * px, yb + 2 * px, c.v(b.dec), h.on() ? scaleq(5 * px, 65536 - h.left) : 0);
}

// ---- Claves: two hardwood sticks, one struck on the other ----
template <class F>
inline void claves(F& f, const Ctl& c, const view::Moment& moment) {
    const Hit h(moment);
    // PTCH: the struck clave shorter; DUAL: the second longer, struck with it;
    // TUNE: the second's angle; ENH: its grain; CLIC: the spark where they meet.
    const i32 L1 = lerp(200 * px, 110 * px, c.v(0)), cx = 352 * px, cy = 108 * px;
    const i32 L2 = lerp(50 * px, 150 * px, c.v(2)), ang2 = lerp(-200, -520, c.v(4));
    const i32 lift = h.on() ? 0 : 10 * px;
    struct Rod { static void make(Pts& q, i32 x, i32 y, i32 len, i32 tenths) {
        const Turn t = rotation(tenths);
        const i32 r = 4 * px;
        for (u32 k = 0; k <= 6; ++k) {   // the right end's round, then the left's
            const Turn a = rotation(-900 + static_cast<i32>(k) * 300);
            i32 dx = len / 2 + ((a.cos * r) >> 10), dy = (a.sin * r) >> 10;
            spin(dx, dy, t); q.add(x + dx, y + dy);
        }
        for (u32 k = 0; k <= 6; ++k) {
            const Turn a = rotation(900 + static_cast<i32>(k) * 300);
            i32 dx = -len / 2 + ((a.cos * r) >> 10), dy = (a.sin * r) >> 10;
            spin(dx, dy, t); q.add(x + dx, y + dy);
        }
    } };
    Pts a, b;
    Rod::make(a, cx, cy, L1, -120);
    draw(f, md::view::contour, a, cx, cy, thin);
    const i32 hx = cx + L1 / 8, hy = cy - 10 * px;   // where the second strikes
    i32 dx = 0, dy = -(L2 / 2 + lift);
    spin(dx, dy, rotation(ang2 + 900));
    Rod::make(b, hx + dx, hy + dy, L2, ang2);
    draw(f, md::view::wave, b, hx + dx, hy + dy, thin);
    // ENH: the struck clave's grain, a line along it.
    Pts g;
    const i32 gl = lerp(10 * px, L1 - 20 * px, c.v(3));
    for (u32 k = 0; k < 2; ++k) {
        i32 ex = (k ? gl / 2 : -gl / 2), ey = px;
        spin(ex, ey, rotation(-120)); g.add(cx + ex, cy + ey);
    }
    draw(f, md::view::comb, g, cx, cy, dim, true);
    // CLIC: a spark at the contact.
    const u32 n = 1 + static_cast<u32>((c.v(5) * 5 + 32768) >> 16);
    Pts s;
    for (u32 k = 0; k < n; ++k) {
        const Turn t = rotation(-1500 - static_cast<i32>(k) * 1300 / 6);
        const i32 r0 = 6 * px, r1 = (h.on() ? 16 : 11) * px;
        s.add(hx + t.cos * (r0 >> 10), hy - 6 * px + t.sin * (r0 >> 10));
        s.add(hx + t.cos * (r1 >> 10), hy - 6 * px + t.sin * (r1 >> 10));
    }
    draw(f, md::view::extra, s, hx, hy - 12 * px, h.on() ? thin : mid, true);
    rings(f, cx - L1 / 2 - 4 * px, cx + L1 / 2 + 4 * px, cy + 8 * px, c.v(1), h.on() ? scaleq(5 * px, 65536 - h.left) : 0);
}

// ---- A gourd shaker in section, its beads inside ----
struct Shake { int count, size, bead, type, damp, wall, att, sus, rev, dec, spread; };
template <class F>
inline void shaker(F& f, const Ctl& c, const Shake& s, const view::Moment& moment) {
    const Hit h(moment, rig::shaker);
    const i32 k = s.size >= 0 ? lerp(48000, 78000, c.v(s.size)) : 65536;
    const i32 cx = 380 * px, cy = 76 * px, rx = scaleq(58 * px, k), ry = scaleq(44 * px, k);
    // A hit swings it and throws its beads, every other step (one way, then the
    // other), so a frame moves its beads at most every other step.
    const i32 left2 = h.on() ? (15 - (h.step & ~1)) << 12 : 0;
    const i32 swing = h.on() && h.step >= 2 ? ((h.step & 2) ? 1 : -1) * scaleq(60, left2) : 0;
    Pts g;
    // The egg from its handle's right side round to its left, then the handle down and back.
    for (u32 i = 0; i <= 40; ++i) {
        const Turn t = rotation(1350 - 120 - static_cast<i32>(i) * 3360 / 40);
        g.add(cx + ((t.cos * (rx >> 4)) >> 6), cy - ((t.sin * (ry >> 4)) >> 6));
    }
    const Turn hl = rotation(1350);
    const i32 hx = cx + ((hl.cos * (rx >> 4)) >> 6) * 9 / 10, hy = cy + ((hl.sin * (ry >> 4)) >> 6) * 9 / 10;
    g.add(hx - 3 * px, hy + 2 * px); g.add(hx - 42 * px, hy + 40 * px); g.add(hx - 36 * px, hy + 46 * px); g.add(hx + 3 * px, hy + 8 * px);
    draw(f, md::view::contour, g, cx, cy, thin, false, swing);
    if (s.wall >= 0) {
        // TONE: the shell's wall, thicker.
        const i32 t = lerp(2 * px, 8 * px, c.v(s.wall));
        Pts w;
        for (u32 i = 0; i < 32; ++i) {
            const Turn a = rotation(static_cast<i32>(i) * 3600 / 32);
            w.add(cx + ((a.cos * ((rx - t) >> 4)) >> 6), cy - ((a.sin * ((ry - t) >> 4)) >> 6));
        }
        draw(f, md::view::wave, w, cx, cy, mid, false, swing);
    }
    // The beads: RATL (GRNS) of them, HARD's size, in a row or scattered (RTYP),
    // settled to the bottom by DAMP, spread by GLEN; a hit throws them about.
    const u32 n = s.count >= 0 ? 2 + static_cast<u32>((c.v(s.count) * 6 + 32768) >> 16) : 6;
    const i32 bs = s.bead >= 0 ? lerp(700, 1450, c.v(s.bead)) : unit;
    const i32 sp = s.spread >= 0 ? lerp(18000, 60000, c.v(s.spread)) : 46000;
    const i32 ty = s.type >= 0 ? c.v(s.type) : 0, dp = s.damp >= 0 ? c.v(s.damp) : 0;
    const u32 jolt = h.on() ? static_cast<u32>(h.step) / 2 : 0;   // a new throw every other step
    for (u32 b = 0; b < n && b < 8; ++b) {
        const i32 rowx = cx - scaleq(rx, sp) + scaleq(2 * scaleq(rx, sp), static_cast<i32>(b) * 65536 / 7);
        const i32 rowy = cy + ry / 3;
        const i32 scx = cx + scaleq(scaleq(rx * 3 / 2, sp), static_cast<i32>(trx_panel::rnd(b * 3 + 1))) - scaleq(rx * 3 / 4, sp);
        const i32 scy = cy + scaleq(ry, static_cast<i32>(trx_panel::rnd(b * 3 + 2))) - ry / 2;
        i32 x = lerp(rowx, scx, ty), y = lerp(rowy, scy, ty);
        y = lerp(y, cy + ry * 2 / 3 - static_cast<i32>(b & 1) * 6 * px, dp);
        if (h.on() && h.step >= 2) {   // after the instrument's own first move, staggered
            x += scaleq(scaleq(16 * px, left2), static_cast<i32>(trx_panel::rnd(b + jolt * 11))) - scaleq(8 * px, left2);
            y -= scaleq(scaleq(18 * px, left2), static_cast<i32>(trx_panel::rnd(b * 5 + jolt * 7)));
        }
        put(f, bead0 + b, x, y, bs);
    }
    if (s.att >= 0) {
        // The shake's stroke over the gourd: out (ATT), held (SUS), back (REV).
        const i32 r = rx + 16 * px;
        const i32 a1 = lerp(100, 600, c.v(s.att)), hold = lerp(0, 300, c.v(s.sus)), a3 = lerp(100, 600, c.v(s.rev));
        Pts q;
        for (u32 i = 0; i <= 8; ++i) {
            const Turn t = rotation(-1350 + static_cast<i32>(i) * a1 / 8);
            q.add(cx + t.cos * (r >> 10), cy + t.sin * (r >> 10));
        }
        for (u32 i = 1; i <= 4; ++i) {
            const Turn t = rotation(-1350 + a1 + static_cast<i32>(i) * hold / 4);
            q.add(cx + t.cos * (r >> 10), cy + t.sin * (r >> 10));
        }
        for (u32 i = 0; i <= 8; ++i) {
            const Turn t = rotation(-1350 + a1 + hold - static_cast<i32>(i) * a3 / 8);
            const i32 r2 = r - 6 * px;
            q.add(cx + t.cos * (r2 >> 10), cy + t.sin * (r2 >> 10));
        }
        draw(f, md::view::comb, q, cx, cy - r * 3 / 4, mid, true);
    }
    if (s.dec >= 0) rings(f, cx - rx - 4 * px, cx + rx + 4 * px, cy, c.v(s.dec), 0);
}

// ---- The bench: a tuning fork, a radio, a cam, a bar on its cords ----
template <class F>
inline void fork(F& f, const Ctl& c, const view::Moment& moment) {
    const Hit h(moment);
    // PTCH: shorter tines; RAMP: their tips splayed (the pitch's drop); RDEC: how
    // far down the bend reaches; DEC: the rings at the tips. A hit swings them apart.
    const i32 cx = 362 * px, yy = 104 * px;
    const i32 Lt = lerp(102 * px, 44 * px, c.v(0));
    i32 splay = lerp(0, 13 * px, c.v(2));
    if (h.on()) splay += scaleq(splay * 2 / 5 + 3 * px, h.left);
    const i32 from = 65536 - lerp(10000, 62000, c.v(3));   // where the bend starts, Q16 along the tine
    for (int side = -1; side <= 1; side += 2) {
        Pts q, back;
        for (u32 i = 0; i < 26; ++i) {
            const i32 t = static_cast<i32>(i) * 65536 / 25, y = yy - scaleq(Lt, t);
            i32 b = 0;
            if (t > from) { const i32 u = ((t - from) >> 4) * 4096 / ((65536 - from) >> 8 ? (65536 - from) >> 4 : 1); b = (u * u) >> 12; }   // Q12
            const i32 x = cx + side * (7 * px + (splay * b >> 12));
            q.add(x - 8 * px / 5, y); back.add(x + 8 * px / 5, y);
        }
        // The yoke's half below, and for the left the stem down to the box.
        Pts all;
        if (side < 0) { all.add(cx - 2 * px, 116 * px); all.add(cx - 2 * px, yy + 9 * px); }
        for (u32 k = 0; k < 4; ++k) {
            const Turn a = rotation(900 + side * static_cast<i32>(k) * 300);
            all.add(cx + ((a.cos * 86) >> 10) * 1 * 0 + side * (8 * px - ((900 - static_cast<i32>(k) * 300) * 0)) * 0 + ((-side * a.sin + 0) * 0) + (side * ((1024 - a.sin) * 7 >> 10) * px / 1) * 0 + cx * 0 + side * static_cast<i32>(k) * 2 * px - side * 0, yy + 8 * px - static_cast<i32>(k) * 2 * px);
        }
        for (u32 i = 0; i < 26; ++i) all.add(q.p[i].x, q.p[i].y);
        for (u32 i = 26; i-- > 0;) all.add(back.p[i].x, back.p[i].y);
        all.add(cx + side * 9 * px, yy + 4 * px);
        if (side < 0) { all.add(cx + 2 * px, yy + 9 * px); all.add(cx + 2 * px, 116 * px); }
        draw(f, side < 0 ? md::view::contour : md::view::wave, all, cx + side * 8 * px, yy - Lt / 2, thin);
    }
    // The resonator box, open at the right, and a mallet resting beside it.
    Pts box;
    const i32 bp[9][2] = {{298, 116}, {356, 116}, {356, 120}, {434, 120}, {434, 124}, {440, 124}, {440, 146}, {434, 146}, {434, 150}};
    for (u32 i = 0; i < 9; ++i) box.add(bp[i][0] * px, bp[i][1] * px);
    box.add(298 * px, 150 * px);
    draw(f, md::view::comb, box, 368 * px, 133 * px, mid);
    Pts mallet;
    const i32 mp[6][2] = {{262, 158}, {292, 146}, {296, 140}, {302, 144}, {297, 150}, {263, 160}};
    for (u32 i = 0; i < 6; ++i) mallet.add(mp[i][0] * px, mp[i][1] * px);
    draw(f, md::view::extra, mallet, 282 * px, 150 * px, mid);
    const i32 tip = yy - Lt + 4 * px;
    rings(f, cx - 12 * px - splay, cx + 12 * px + splay, tip, c.v(1), h.on() ? scaleq(5 * px, 65536 - h.left) : 0);
}

template <class F>
inline void radio(F& f, const Ctl& c, const view::Moment& moment) {
    const Hit h(moment);
    // A valve radio's loudspeaker in section: the cone shakes with the noise while
    // it lasts; DEC: the rings in front of the grille.
    Pts box, cone, grille, mag;
    const i32 bp[4][2] = {{300, 30}, {424, 30}, {424, 142}, {300, 142}};
    for (u32 i = 0; i < 4; ++i) box.add(bp[i][0] * px, bp[i][1] * px);
    draw(f, md::view::contour, box, 362 * px, 86 * px, mid);
    const i32 jit = h.on() ? scaleq(4 * px, static_cast<i32>(trx_panel::rnd(static_cast<u32>(h.step) * 3 + 1))) : 0;
    const i32 cp[6][2] = {{334, 78}, {380, 50}, {384, 52}, {384, 120}, {380, 122}, {334, 94}};
    for (u32 i = 0; i < 6; ++i) cone.add(cp[i][0] * px + (i == 0 || i == 5 ? jit : jit / 3), cp[i][1] * px);
    draw(f, md::view::wave, cone, 360 * px, 86 * px, thin);
    for (u32 k = 0; k < 5; ++k) {
        const i32 y = (54 + 15 * static_cast<i32>(k)) * px;
        grille.add((k & 1 ? 414 : 396) * px, y); grille.add((k & 1 ? 396 : 414) * px, y);
    }
    draw(f, md::view::comb, grille, 405 * px, 84 * px, dim, true);
    const i32 mp[4][2] = {{316, 76}, {334, 76}, {334, 96}, {316, 96}};
    for (u32 i = 0; i < 4; ++i) mag.add(mp[i][0] * px, mp[i][1] * px);
    draw(f, md::view::extra, mag, 325 * px, 86 * px, mid);
    rings(f, 0, 428 * px, 86 * px, c.v(0), h.on() ? scaleq(6 * px, 65536 - h.left) : 0, 1);
}

template <class F>
inline void cam(F& f, const Ctl& c, const view::Moment& moment) {
    const Hit h(moment);
    // An impulse as a cam's profile: a lobe UP long and UVAL high, then a dip
    // DOWN long and DVAL deep; a hit turns the cam under its follower.
    const i32 cx = 362 * px, cy = 104 * px, r0 = 32 * px;
    const i32 up = lerp(200, 1300, c.v(0)), uh = lerp(0, 22 * px, c.v(1)), dn = lerp(200, 1300, c.v(2)), dd = lerp(0, 16 * px, c.v(3));
    Pts q;
    for (u32 i = 0; i < 64; ++i) {
        const i32 a = static_cast<i32>(i) * 3600 / 64;   // tenths, clockwise from the top
        i32 r = r0;
        if (a < up) r += scaleq(uh, 32768 - (md::sine(static_cast<u32>(a) * 1193046u * 2u + 0x40000000u) >> 16));
        else if (a >= up + 150 && a < up + 150 + dn) r -= scaleq(dd, 32768 - (md::sine(static_cast<u32>(a - up - 150) * 1193046u * 2u + 0x40000000u) >> 16) * 0 - 0);
        const Turn t = rotation(a - 900);
        q.add(cx + t.cos * (r >> 10), cy + t.sin * (r >> 10));
    }
    const i32 tenths = h.on() ? -h.step * 225 : 150;
    draw(f, md::view::contour, q, cx, cy, thin, false, tenths);
    // The follower: a lever from its pivot at the left, its roller on the cam's top.
    i32 top = r0;
    {
        // The profile under the roller: the cam turned by `tenths`, so its angle -tenths.
        const i32 a = ((-tenths) % 3600 + 3600) % 3600;
        if (a < up) top += scaleq(uh, 32768 - (md::sine(static_cast<u32>(a) * 1193046u * 2u + 0x40000000u) >> 16));
        else if (a >= up + 150 && a < up + 150 + dn) top -= dd;
    }
    const i32 ry = cy - top - 5 * px;
    Pts lever;
    lever.add(290 * px, 30 * px); lever.add(cx - 4 * px, ry - 2 * px); lever.add(cx + 4 * px, ry - 6 * px);
    for (u32 k = 0; k <= 8; ++k) { const Turn t = rotation(static_cast<i32>(k) * 3600 / 8); lever.add(cx + t.cos * 5, ry + t.sin * 5); }
    lever.add(cx - 4 * px, ry + 1 * px); lever.add(290 * px, 33 * px);
    draw(f, md::view::wave, lever, 326 * px, (30 * px + ry) / 2, mid);
    Pts axle;
    for (u32 k = 0; k < 8; ++k) { const Turn t = rotation(static_cast<i32>(k) * 450); axle.add(cx + t.cos * 4, cy + t.sin * 4); }
    draw(f, md::view::comb, axle, cx, cy, mid);
}

template <class F>
inline void bar(F& f, const Ctl& c, const view::Moment& moment) {
    const Hit h(moment);
    // A metal bar hung on cords: PTCH shortens it, TENS bows it as it is struck,
    // HARD a smaller, harder mallet, DEC the rings at its ends.
    const i32 cx = 362 * px, by = 100 * px;
    const i32 half = lerp(118 * px, 70 * px, c.v(0));
    const i32 bow = scaleq(lerp(px, 12 * px, c.v(3)), h.on() ? 26000 + scaleq(39536, h.left) : 26000);
    Pts q, back;
    for (u32 i = 0; i < 32; ++i) {
        const i32 t = static_cast<i32>(i) * 65536 / 31, x = cx - half + scaleq(2 * half, t);
        const i32 u = (t - 32768) >> 5;   // -1024 .. 1024
        const i32 sag = bow - (bow >> 10) * ((u * u) >> 10);
        q.add(x, by - 4 * px + sag); back.add(x, by + 4 * px + sag);
    }
    Pts all;
    for (u32 i = 0; i < 32; ++i) all.add(q.p[i].x, q.p[i].y);
    for (u32 i = 32; i-- > 0;) all.add(back.p[i].x, back.p[i].y);
    draw(f, md::view::contour, all, cx, by, thin);
    // The frame and cords, from its nodes (0.224 of its length from each end).
    const i32 nl = cx - half + scaleq(2 * half, 14680), nr = cx + half - scaleq(2 * half, 14680);
    Pts cord;
    const i32 cb = by - 4 * px + bow * 3 / 5, w6 = 3 * px / 5;
    const i32 cp[12][2] = {{238 * px, 18 * px}, {486 * px, 18 * px}, {486 * px, 22 * px}, {nr + w6, 22 * px}, {nr + w6, cb}, {nr - w6, cb},
                           {nr - w6, 22 * px}, {nl + w6, 22 * px}, {nl + w6, cb}, {nl - w6, cb}, {nl - w6, 22 * px}, {238 * px, 22 * px}};
    for (u32 i = 0; i < 12; ++i) cord.add(cp[i][0], cp[i][1]);
    draw(f, md::view::wave, cord, cx, 60 * px, mid);
    striker(f, md::view::comb, mallet, cx + 10 * px, by - 4 * px + bow, 20000, lerp(10 * px, 5 * px, c.v(2)), h.on(), 0);
    rings(f, cx - half - 3 * px, cx + half + 3 * px, by, c.v(1), h.on() ? scaleq(5 * px, 65536 - h.left) : 0);
}
}

// The machines' views, by their slots (kit.h), each with its row of travel.
#define MD_FIG_VIEW(ns, row, ...) namespace ns { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { \
    const fig::Ctl c = {s, fig::travel[row]}; __VA_ARGS__ } }
MD_FIG_VIEW(trx_bd, 0, fig::Mem k = fig::mem(fig::beater, 0); k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.lift = 4; k.fan = 5; k.harm = 6; k.clip = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_sd, 1, fig::Mem k = fig::mem(fig::stick, 2); k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.lift = 4; k.wires = 5; k.wtune = 6; k.clip = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_xt, 2, fig::Mem k = fig::mem(fig::stick, 1); k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.felt = 4; k.clip = 5; k.clipk = 6; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_cp, 3, fig::Room r = {-1, 4, 0, 3, 1, 6, 7, 5, 2, -1, -1, -1, false}; fig::chamber(f, c, r, m);)
MD_FIG_VIEW(trx_rs, 4, fig::Mem k = fig::mem(fig::rimstick, 2); k.ptch = 0; k.dec = 1; k.clip = 2; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_cb, 5, fig::Bell b = {0, 1, 4, 5, 2, 3, -1, -1, -1, -1, -1}; fig::cowbell(f, c, b, m);)
MD_FIG_VIEW(trx_ch, 6, fig::Cym k = fig::cym(true, false); k.gap = 0; k.dec = 1; k.thick = 2; k.teeth = 3; k.marks = 4; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(trx_oh, 7, fig::Cym k = fig::cym(true, false); k.gap = 0; k.dec = 1; k.thick = 2; k.teeth = 3; k.marks = 4; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(trx_cy, 8, fig::Cym k = fig::cym(false, false); k.marks = 0; k.dec = 1; k.ping = 2; k.bell = 3; k.size = 4; k.teeth = 5; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(trx_ma, 9, fig::Shake k = {4, -1, 7, 5, 3, 6, 0, 1, 2, -1, -1}; fig::shaker(f, c, k, m);)
MD_FIG_VIEW(trx_cl, 10, fig::claves(f, c, m);)
MD_FIG_VIEW(trx_xc, 11, fig::Mem k = fig::mem(fig::stick, 1); k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.felt = 4; k.clip = 5; k.clipk = 6; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_b2, 12, fig::Mem k = fig::mem(fig::beater, 0); k.ptch = 0; k.dec = 1; k.dent = 2; k.hold = 3; k.ball = 4; k.fan = 5; k.kink = 6; k.clip = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_bd, 13, fig::Mem k = fig::mem(fig::beater, 0); k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.harm = 4; k.hfrq = 5; k.hreach = 6; k.kink = 7; k.spring = true; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_sd, 14, fig::Mem k = fig::mem(fig::stick, 2); k.ptch = 0; k.dec = 1; k.wires = 2; k.wlift = 3; k.harm = 4; k.hfrq = 5; k.hreach = 6; k.hpf = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_xt, 15, fig::Mem k = fig::mem(fig::stick, 1); k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.harm = 4; k.hfrq = 5; k.hreach = 6; k.ball = 7; k.spring = true; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_cp, 16, fig::Room r = {2, 3, -1, -1, 7, 1, -1, -1, 0, 4, 5, 6, true}; fig::chamber(f, c, r, m);)
MD_FIG_VIEW(efm_rs, 17, fig::Mem k = fig::mem(fig::rimstick, 2); k.ptch = 0; k.dec = 1; k.harm = 2; k.hpf = 3; k.wires = 4; k.wtune = 5; k.wlift = 6; k.wkink = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_cb, 18, fig::Bell b = {0, 1, -1, -1, -1, -1, 2, 3, 4, 5, 6}; fig::cowbell(f, c, b, m);)
MD_FIG_VIEW(efm_hh, 19, fig::Cym k = fig::cym(true, true); k.size = 0; k.dec = 1; k.spring = 2; k.coils = 3; k.teeth = 4; k.period = 5; k.reach = 6; k.rag = 7; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(efm_cy, 20, fig::Cym k = fig::cym(false, true); k.size = 0; k.dec = 1; k.rag = 2; k.thick = 3; k.teeth = 4; k.period = 5; k.reach = 6; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(pi_bd, 21, fig::Mem k = fig::mem(fig::mallet, 0); k.ptch = 0; k.dec = 1; k.ball = 2; k.lift = 3; k.dent = 4; k.felt = 5; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(pi_sd, 22, fig::Mem k = fig::mem(fig::stick, 2); k.ptch = 0; k.dec = 1; k.ball = 2; k.harm = 3; k.dent = 4; k.wires = 5; k.wlift = 6; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(pi_mt, 23, fig::Mem k = fig::mem(fig::mallet, 1); k.ptch = 0; k.dec = 1; k.ball = 2; k.lift = 3; k.hfrq = 4; k.felt = 5; k.size = 6; k.pos = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(pi_ml, 24, fig::bar(f, c, m);)
MD_FIG_VIEW(pi_ma, 25, fig::Shake k = {0, 4, 5, -1, -1, -1, -1, -1, -1, 1, 2}; fig::shaker(f, c, k, m);)
MD_FIG_VIEW(pi_rs, 26, fig::Mem k = fig::mem(fig::rimstick, 2); k.ptch = 0; k.dec = 1; k.ball = 2; k.harm = 3; k.wires = 4; k.wlift = 5; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(pi_rc, 27, fig::Cym k = fig::cym(false, true); k.size = 0; k.dec = 1; k.lift = 2; k.ring = 3; k.teeth = 4; k.marks = 5; k.thick = 6; k.grab = 7; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(pi_cc, 28, fig::Cym k = fig::cym(false, true); k.size = 0; k.dec = 1; k.lift = 2; k.ring = 3; k.teeth = 4; k.marks = 5; k.thick = 6; k.grab = 7; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(pi_hh, 29, fig::Cym k = fig::cym(true, true); k.size = 0; k.dec = 1; k.tilt = 2; k.ring = 3; k.teeth = 4; k.marks = 5; k.thick = 6; k.gap = 7; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(gnd_sn, 30, fig::fork(f, c, m);)
MD_FIG_VIEW(gnd_ns, 31, fig::radio(f, c, m);)
MD_FIG_VIEW(gnd_im, 32, fig::cam(f, c, m);)
#undef MD_FIG_VIEW

// ---- MD E12: a strip of tape on a splicing block ----
// The sample as tape seen from above, its recording developed into stripes
// across it, as a magnetic viewer shows them: PTCH packs them closer (the tape
// played faster), DEC fades them along it, HP thins their lower halves, BEND
// crowds or spreads the first of them (the pitch starting away and drifting
// back). STRT cuts the start away: the scrap lies above the block, and the
// stripes begin part way into the sound. RTRG splices the start in again,
// patch after patch, RTIM apart. A machine's own control: a second sample's
// tape spliced in on the diagonal (RING, RATL, BELL, REAL, SLEW, BC), the end
// cut shorter the sooner it stops (STOP), the start saturated, stripes clipped flat (SNAP, for
// SPLN's length), or the tape's edge creased (HPQ). A hit runs the tape left
// through the block in sixteen steps.
namespace fig_e12 {
using namespace md::view;
using fig::Ctl;
using fig::Pts;
using fig::Hit;
using fig::lerp;
using fig::scaleq;
using fig::px;
using opxy::u16;

// travel-e12:begin (tools/art/travel.py writes this table: edit the tool, then run it)
// How far each control moves its part, Q8 (256 = all the way), in proportion to
// the change it makes to the sound (full at 24 dB of band energy, at least 0.2).
static const opxy::u16 travel_e12[16][8] = {
    {256, 256, 51, 59, 249, 256, 256, 168},   // E12-BD: PTCH 26.5, DEC 71.3, SNAP 2.8, SPLN 5.5, STRT 23.3, RTRG 31.6, RTIM 31.4, BEND 15.8
    {256, 256, 219, 93, 256, 256, 146, 256},   // E12-SD: PTCH 53.0, DEC 73.3, HP 20.5, RING 8.7, STRT 33.3, RTRG 34.1, RTIM 13.7, BEND 28.2
    {256, 256, 256, 51, 216, 256, 256, 232},   // E12-HT: PTCH 35.8, DEC 77.4, HP 33.3, HPQ 3.3, STRT 20.2, RTRG 33.2, RTIM 30.8, BEND 21.7
    {256, 256, 256, 51, 256, 256, 254, 197},   // E12-LT: PTCH 32.6, DEC 85.3, HP 33.1, HPQ 3.2, STRT 34.3, RTRG 32.1, RTIM 23.8, BEND 18.5
    {256, 256, 227, 51, 120, 256, 118, 256},   // E12-CP: PTCH 64.1, DEC 56.5, HP 21.3, HPQ 3.3, STRT 11.2, RTRG 35.5, RTIM 11.1, BEND 34.6
    {256, 256, 248, 117, 256, 256, 256, 256},   // E12-RS: PTCH 49.2, DEC 72.7, HP 23.2, RATL 11.0, STRT 92.1, RTRG 34.8, RTIM 33.9, BEND 28.9
    {256, 256, 235, 51, 114, 256, 185, 256},   // E12-CB: PTCH 47.9, DEC 58.5, HP 22.1, HPQ 2.6, STRT 10.7, RTRG 31.9, RTIM 17.3, BEND 27.4
    {256, 256, 256, 51, 195, 256, 170, 256},   // E12-CH: PTCH 49.0, DEC 71.9, HP 25.7, HPQ 3.1, STRT 18.3, RTRG 40.3, RTIM 16.0, BEND 25.0
    {256, 256, 179, 256, 256, 128, 61, 256},   // E12-OH: PTCH 58.2, DEC 64.4, HP 16.8, STOP 58.3, STRT 82.0, RTRG 12.0, RTIM 5.7, BEND 28.7
    {256, 256, 161, 92, 142, 203, 57, 232},   // E12-RC: PTCH 50.0, DEC 63.2, HP 15.1, BELL 8.6, STRT 13.3, RTRG 19.1, RTIM 5.4, BEND 21.8
    {256, 256, 196, 51, 132, 119, 51, 256},   // E12-CC: PTCH 58.0, DEC 69.1, HP 18.4, HPQ 2.7, STRT 12.4, RTRG 11.1, RTIM 4.7, BEND 25.6
    {256, 256, 228, 121, 171, 256, 155, 256},   // E12-BR: PTCH 53.9, DEC 70.7, HP 21.4, REAL 11.3, STRT 16.0, RTRG 37.4, RTIM 14.5, BEND 27.6
    {256, 256, 148, 51, 105, 256, 98, 256},   // E12-TA: PTCH 60.7, DEC 52.3, HP 13.9, HPQ 2.6, STRT 9.9, RTRG 26.4, RTIM 9.2, BEND 33.6
    {256, 256, 179, 51, 116, 192, 51, 256},   // E12-TR: PTCH 45.3, DEC 56.3, HP 16.8, HPQ 2.6, STRT 10.8, RTRG 18.0, RTIM 4.0, BEND 25.1
    {256, 256, 148, 131, 70, 256, 124, 256},   // E12-SH: PTCH 56.0, DEC 54.7, HP 13.9, SLEW 12.3, STRT 6.6, RTRG 34.9, RTIM 11.7, BEND 31.1
    {256, 256, 228, 123, 158, 256, 256, 256},   // E12-BC: PTCH 43.8, DEC 71.8, HP 21.4, BC 11.6, STRT 14.8, RTRG 40.9, RTIM 34.4, BEND 25.2
};
// travel-e12:end

// tools/art/fig.mjs's E12 parts, in its order, in the common nodes' places.
enum Part : u32 { block = 4, ch_top, ch_bot, slot45, slot90, blade, patch0, diag = patch0 + 4, part_end };
static const u8 part_shade[part_end - block] = {mid, dim, dim, dim, dim, outline2, outline2, outline2, outline2, outline2, thin};
template <class F>
inline void put(F& f, u32 part, i32 x, i32 y) {
    const u8 s = part_shade[part - block];
    place(f, part, x + fig::shift, stage_y * unit + y, unit, s, s >= fill1 ? fill_and_stroke : stroke);
}

enum Own { crease, second, stop, snap };
template <class F>
inline void tape(F& f, const Ctl& c, i32 decay16, Own own, u32 take, const view::Moment& moment) {
    const Hit h(moment);
    const i32 slide = h.on() ? -h.step * 3 * px / 2 : 0;   // the tape runs left through the block
    const i32 cx = 362 * px, yc = 92 * px, half = 20 * px;
    const i32 lead = 242 * px + slide, cut = 306 * px + slide;
    i32 end = 490 * px;
    if (own == stop) end = c.raw(3) >= 127 ? 490 * px : lerp(318 * px, 480 * px, c.v(3));   // STOP: the sooner, the shorter; never at 127
    end += slide;
    // The block, its channel and slots, and the blade resting by the cut.
    put(f, block, cx, yc);
    put(f, ch_top, cx, yc - half - px);
    put(f, ch_bot, cx, yc + half + px);
    put(f, slot45, 276 * px, yc);
    put(f, slot90, 306 * px, yc);
    put(f, blade, 306 * px, yc - 42 * px);
    // The tape: from the leader on the left, its top edge creased by HPQ, to its
    // end cut on the diagonal.
    const i32 crease_a = own == crease ? lerp(0, 3 * px, c.v(3)) : 0;
    Pts t;
    for (u32 i = 0; i < 28; ++i) {
        const i32 x = lead + (end - lead) * static_cast<i32>(i) / 27;
        const i32 w = crease_a ? multiply(crease_a, md::sine(i * 0x26000000u)) : 0;
        t.add(x, yc - half + w);
    }
    t.add(end - 8 * px, yc + half);
    t.add(lead, yc + half);
    fig::draw(f, md::view::contour, t, 366 * px + slide, yc, thin);   // within 128 px of every point
    // The stripes: one a step through the sound, from STRT's step, again from it at each splice.
    const i32 spacing0 = lerp(10 * px, 16 * px / 5, c.v(0));
    const i32 bend = c.v(7) - 32768;                                                       // -32768 .. 32768
    const i32 kd = scaleq(decay16 >> 4, 65536 - c.v(1));                                    // the fall a step, Q12
    const i32 s0 = lerp(0, 14 * 256, c.v(4)) >> 8;                                          // STRT: steps cut away
    const i32 low = own == snap ? 65536 : 65536 - scaleq(50000, c.v(2));                    // HP thins the lower halves
    const i32 drive = own == snap ? lerp(65536, 3 * 65536, c.v(2)) : 65536;                 // SNAP: up to 3 times, clipped
    const i32 driven = own == snap ? lerp(2, 12, c.v(3)) : 0;                              // for SPLN's steps
    const i32 joins = c.raw(5, 0) > 1 ? 1 + ((c.v(5) * 3 + 32768) >> 16) : 1;               // RTRG: the splices
    const i32 every = lerp(24 * px, 76 * px, c.v(6));
    const i32 second_at = own == second ? lerp(end - 4 * px, cut + 14 * px, c.v(3)) : end + 64 * px;
    i32 xs[32], up[32], dn[32];
    u32 n = 0;
    i32 x = cut + 3 * px, step = 0, join = 1;
    while (x < end - 10 * px && n < 32) {
        if (join < joins && x >= cut + join * every) { step = 0; ++join; }
        const i32 s = s0 + step;
        // BEND: the first steps crowded (or spread), drifting back.
        const i32 crowd = scaleq(bend, md::exp_neg(s * 65536 / 6) >> 15) / 2;
        const i32 sp = scaleq(spacing0, 65536 - crowd);
        const u32 seed = (x >= second_at ? static_cast<u32>(s) + 40u : static_cast<u32>(s)) + take * 7u;   // each machine its own recording
        i32 a = scaleq(18 * px, md::exp_neg(s * kd >> 2) >> 15);
        a = scaleq(a, 36000 + scaleq(29536, static_cast<i32>(trx_panel::rnd(seed * 3 + 1))));
        if (step < driven) a = scaleq(a, drive);
        if (a > 18 * px) a = 18 * px;
        if (a < px) a = px;
        xs[n] = x; up[n] = a; dn[n] = scaleq(a, low) < px ? px : scaleq(a, low);
        ++n; ++step;
        x += sp < 3 * px ? 3 * px : sp;
    }
    for (u32 p = 0; p < 2; ++p) {
        Pts q;
        for (u32 k = p * 16; k < n && k < p * 16 + 16; ++k) {
            const bool down = k & 1;
            q.add(xs[k], yc + (down ? dn[k] : -up[k]));
            q.add(xs[k], yc + (down ? -up[k] : dn[k]));
        }
        if (q.n >= 2) fig::draw(f, p ? md::view::wave : md::view::comb, q, (q.p[0].x + q.p[q.n - 1].x) / 2, yc, p ? thin : thin, true);
    }
    // The splicing-tape patches: the leader's join, then RTRG's.
    for (i32 k = 0; k < joins && k < 4; ++k) put(f, patch0 + static_cast<u32>(k), cut + k * every, yc);
    if (own == second && second_at < end) put(f, diag, second_at, yc);
    // STRT: the cut-off start, lying above the block.
    if (c.raw(4, 0) > 2) {
        const i32 len = lerp(8 * px, 70 * px, c.v(4));
        Pts sc;
        sc.add(242 * px, 26 * px); sc.add(242 * px + len, 22 * px); sc.add(242 * px + len, 38 * px); sc.add(242 * px, 42 * px);
        fig::draw(f, md::view::extra, sc, 242 * px + len / 2, 32 * px, mid);
    }
}
}

// E12's machines: each class's fall along the strip (the design's kmap, Q16) and its own control.
#define MD_E12_VIEW(ns, row, decay, own) namespace ns { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { \
    const fig::Ctl c = {s, fig_e12::travel_e12[row]}; fig_e12::tape(f, c, decay, fig_e12::own, row, m); } }
MD_E12_VIEW(e12_bd, 0, 196608, snap)
MD_E12_VIEW(e12_sd, 1, 262144, second)
MD_E12_VIEW(e12_ht, 2, 196608, crease)
MD_E12_VIEW(e12_lt, 3, 196608, crease)
MD_E12_VIEW(e12_rs, 5, 589824, second)
MD_E12_VIEW(e12_cb, 6, 262144, crease)
MD_E12_VIEW(e12_ch, 7, 524288, crease)
MD_E12_VIEW(e12_oh, 8, 163840, stop)
MD_E12_VIEW(e12_rc, 9, 91750, second)
MD_E12_VIEW(e12_cc, 10, 91750, crease)
MD_E12_VIEW(e12_br, 11, 262144, second)
MD_E12_VIEW(e12_ta, 12, 327680, crease)
MD_E12_VIEW(e12_tr, 13, 262144, crease)
MD_E12_VIEW(e12_sh, 14, 327680, second)
MD_E12_VIEW(e12_bc, 15, 196608, second)
#undef MD_E12_VIEW
namespace e12_cp { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) {
    const fig::Ctl c = {s, fig_e12::travel_e12[4]}; fig_e12::tape(f, c, 327680, fig_e12::crease, 4, m); } }
}
