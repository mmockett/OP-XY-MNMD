#pragma once
// The drum kits' per-pad screens, the whole screen: MD KIT's machines draw the
// sounding part of their instrument in section (namespace fig), MD E12's a
// strip of tape on a splicing block (namespace fig_e12), centred under the header.
#include "trx_view.hpp"

namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
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
// the change it makes to the sound (the root of its share of 24 dB of band energy, at least 0.5).
static const opxy::u16 travel[33][8] = {
    {128, 184, 183, 137, 132, 180, 128, 170},   // TRX-BD: PTCH 5.6, DEC 12.4, RAMP 12.2, RDEC 6.9, STRT 6.4, NOIS 11.8, HARM 1.1, CLIP 10.6
    {128, 239, 128, 128, 142, 128, 128, 203},   // TRX-SD: PTCH 0.7, DEC 21.0, BUMP 0.8, BENV 0.5, SNAP 7.4, TONE 5.9, TUNE 4.8, CLIP 15.0
    {208, 256, 128, 137, 128, 160, 128, 0},   // TRX-XT: PTCH 15.8, DEC 27.9, RAMP 5.7, RDEC 6.8, DAMP 3.8, DIST 9.3, DTYP 1.5
    {128, 128, 128, 227, 256, 128, 163, 128},   // TRX-CP: CLPY 3.1, TONE 2.6, HARD 4.0, RICH 18.9, RATE 24.9, ROOM 3.6, RSIZ 9.7, RTUN 5.8
    {196, 256, 220, 0, 0, 0, 0, 0},   // TRX-RS: PTCH 14.1, DEC 33.5, DIST 17.7
    {163, 256, 128, 128, 148, 128, 0, 0},   // TRX-CB: PTCH 9.8, DEC 44.5, ENH 4.2, DAMP 2.9, TONE 8.0, BUMP 0.4
    {128, 256, 128, 134, 128, 0, 0, 0},   // TRX-CH: GAP 5.1, DEC 31.1, HPF 4.6, LPF 6.5, MTAL 5.2
    {128, 256, 128, 135, 128, 0, 0, 0},   // TRX-OH: GAP 5.8, DEC 29.3, HPF 4.3, LPF 6.7, MTAL 5.0
    {128, 175, 128, 128, 146, 128, 0, 0},   // TRX-CY: RICH 5.6, DEC 11.2, TOP 2.3, TTUN 2.5, SIZE 7.8, PEAK 2.8
    {256, 256, 256, 128, 256, 128, 128, 191},   // TRX-MA: ATT 67.2, SUS 49.2, REV 48.3, DAMP 1.4, RATL 25.0, RTYP 2.8, TONE 5.9, HARD 13.4
    {186, 149, 151, 128, 170, 128, 0, 0},   // TRX-CL: PTCH 12.7, DEC 8.1, DUAL 8.4, ENH 1.3, TUNE 10.6, CLIC 0.0
    {226, 250, 132, 155, 128, 173, 128, 0},   // TRX-XC: PTCH 18.8, DEC 22.8, RAMP 6.3, RDEC 8.8, DAMP 3.9, DIST 11.0, DTYP 1.2
    {194, 247, 153, 256, 128, 142, 231, 225},   // TRX-B2: PTCH 13.8, DEC 22.3, RAMP 8.5, HOLD 29.1, TICK 0.0, NOIS 7.4, DIRT 19.5, DIST 18.5
    {256, 256, 236, 212, 244, 156, 245, 249},   // EFM-BD: PTCH 26.9, DEC 36.7, RAMP 20.4, RDEC 16.4, MOD 21.8, MFRQ 8.9, MDEC 22.0, MFB 22.7
    {226, 256, 180, 256, 217, 147, 210, 180},   // EFM-SD: PTCH 18.6, DEC 40.0, NOIS 11.8, NDEC 24.0, MOD 17.3, MFRQ 7.9, MDEC 16.1, HPF 11.9
    {198, 256, 227, 215, 241, 142, 237, 128},   // EFM-XT: PTCH 14.4, DEC 38.6, RAMP 18.9, RDEC 16.9, MOD 21.3, MFRQ 7.4, MDEC 20.5, CLIC 0.7
    {256, 256, 252, 256, 223, 143, 236, 175},   // EFM-CP: PTCH 43.0, DEC 33.0, CLPS 23.2, CDEC 38.7, MOD 18.2, MFRQ 7.5, MDEC 20.5, HPF 11.2
    {256, 256, 256, 180, 179, 145, 255, 128},   // EFM-RS: PTCH 48.1, DEC 29.1, MOD 26.4, HPF 11.9, SNAR 11.7, SPTC 7.7, SDEC 23.9, SMOD 4.7
    {256, 256, 156, 256, 186, 226, 136, 0},   // EFM-CB: PTCH 29.2, DEC 58.2, SNAP 8.9, FB 31.0, MOD 12.6, MFRQ 18.8, MDEC 6.8
    {150, 256, 137, 144, 128, 128, 128, 179},   // EFM-HH: PTCH 8.2, DEC 70.6, TREM 6.9, TFRQ 7.6, MOD 4.6, MFRQ 3.7, MDEC 3.3, FB 11.8
    {256, 256, 205, 193, 247, 249, 128, 0},   // EFM-CY: PTCH 24.3, DEC 62.9, FB 15.5, HPF 13.6, MOD 22.3, MFRQ 22.7, MDEC 2.7
    {144, 141, 128, 128, 128, 128, 0, 0},   // PI-BD: PTCH 7.6, DEC 7.3, HARD 0.0, HAMR 0.7, TENS 2.8, DAMP 0.0
    {195, 194, 128, 128, 128, 165, 229, 0},   // PI-SD: PTCH 13.9, DEC 13.8, HARD 0.3, RING 1.9, TENS 0.7, RVOL 10.0, RDEC 19.2
    {157, 256, 128, 128, 128, 169, 215, 128},   // PI-MT: PTCH 9.0, DEC 28.6, HARD 0.0, HAMR 2.0, TUNE 1.3, DAMP 10.4, SIZE 17.0, POS 0.3
    {201, 256, 229, 128, 0, 0, 0, 0},   // PI-ML: PTCH 14.8, DEC 57.4, HARD 19.3, TENS 2.9
    {211, 256, 170, 0, 168, 128, 0, 0},   // PI-MA: GRNS 16.3, DEC 36.8, GLEN 10.6, SIZE 10.3, HARD 5.1
    {164, 171, 128, 128, 185, 210, 0, 0},   // PI-RS: PTCH 9.8, DEC 10.8, HARD 0.2, RING 0.8, RVOL 12.6, RDEC 16.1
    {214, 256, 224, 128, 211, 169, 195, 144},   // PI-RC: PTCH 16.8, DEC 28.4, HARD 18.4, RING 0.0, AG 16.3, AU 10.4, BR 14.0, GRAB 7.6
    {128, 256, 256, 128, 128, 128, 128, 256},   // PI-CC: PTCH 1.4, DEC 44.6, HARD 58.5, RING 4.4, AG 0.0, AU 0.0, BR 3.2, GRAB 27.7
    {128, 256, 128, 128, 128, 169, 128, 256},   // PI-HH: PTCH 5.4, DEC 42.0, CLSN 5.1, RING 3.4, AG 2.1, AU 10.5, BR 6.0, CLOS 26.9
    {256, 227, 256, 256, 0, 0, 0, 0},   // GND-SN: PTCH 33.9, DEC 18.8, RAMP 82.4, RDEC 41.8
    {256, 0, 0, 0, 0, 0, 0, 0},   // GND-NS: DEC 123.6
    {256, 256, 256, 214, 0, 0, 0, 0},   // GND-IM: UP 73.7, UVAL 40.7, DOWN 45.9, DVAL 16.8
};
// travel:end

// tools/art/fig.mjs's parts, in its order, in the common nodes' places.
enum Part : u32 { ring_r0 = 4, ring_l0, ring_r1, ring_l1, ring_r2, ring_l2, rod, clutch, hand, clamp_t, clamp_b, felt,
                  speaker, mic, stand, front0, front1, front2, front3, bead0,
                  // after the instruments' place: each family's and machine's hardware
                  lug_l = bead0 + 8, lug_r, coil, magnet, rope_l, rope_r, mount, leg_l, leg_r, pillow, felt2,
                  spark0, echo0 = spark0 + 8, frontb0 = echo0 + 4, frontc0 = frontb0 + 4, hanger = frontc0 + 4, part_end };
static const u8 part_shade[part_end - ring_r0] = {mid, mid, mid, mid, dim, dim, dim, outline2, outline2, outline2, outline2,
    outline2, outline2, outline2, mid, thin, mid, mid, mid, fill4, fill4, fill4, fill4, fill4, fill4, fill4, fill4,
    outline2, outline2, mid, outline2, mid, mid, outline2, mid, mid, outline2, outline2,
    mid, mid, mid, mid, mid, mid, mid, mid, dim, dim, dim, dim, thin, mid, mid, mid, thin, mid, mid, mid, dim};
// The hardware parts sit after the common nodes, which begin() does not hide: each view hides them first.
template <class F>
inline void begin(F& f) { for (u32 n = lug_l; n < part_end; ++n) hide(f, n); }

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
    // 0 .. travel (Q16), from nothing at 0: for an amount that starts from none (a
    // spray, teeth, a clamp), which must not jump in at the middle of its range.
    i32 a(int slot) const {
        if (slot < 0) return 0;
        return (static_cast<i32>(raw(slot)) * 516 * static_cast<i32>(k[slot])) >> 8;
    }
};
// From a to b (Q10 pixels, or any value within 2^21) by t (Q16).
inline i32 lerp(i32 a, i32 b, i32 t) { return a + (((b - a) * (t >> 4)) >> 12); }
inline i32 scaleq(i32 a, i32 t) { return (a * (t >> 4)) >> 12; }   // a t, t Q16, a within 2^19
// A shade between dark and bright (Q16), a palette grey at a time: a control drawn
// as a shade fades up through them, never jumping from dark to white.
inline u32 grey(i32 t) {
    static const u8 ramp[6] = {faint, dim, dash, label, mid, thin};
    const i32 i = (md::clamp(t, 0, 65536) * 5 + 32768) >> 16;
    return ramp[i];
}

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
        if (ribbon) {
            // A line is a real thin closed shape: both edges a quarter of a (local)
            // pixel apart, so it never retraces itself. A line drawn out and back
            // over itself broke up on the device where it turned sharply.
            Point both[64];
            const i32 h = unit / 8;
            for (u32 i = 0; i < n; ++i) {
                const Point& a = local[i ? i - 1 : i];
                const Point& b = local[i + 1 < n ? i + 1 : i];
                i32 dx = b.x - a.x, dy = b.y - a.y;
                if (dx > -unit / 8 && dx < unit / 8 && dy > -unit / 8 && dy < unit / 8) { dx = local[i].x - a.x; dy = local[i].y - a.y; }
                const i32 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
                const i32 len = (ax > ay ? ax + ay * 3 / 8 : ay + ax * 3 / 8) | 1;
                const i32 nx = -dy * h / len, ny = dx * h / len;
                both[i].x = local[i].x + nx; both[i].y = local[i].y + ny;
                both[2 * n - 1 - i].x = local[i].x - nx; both[2 * n - 1 - i].y = local[i].y - ny;
            }
            outline(f.paths[path], both, 2 * n, key);
        } else outline(f.paths[path], local, n, key);
        f.paths[path].key = key;
    }
    place(f, path, ox + shift, stage_y * unit + oy, 2 * unit, tone, stroke);
    if (tenths) opxy::draw::turn(f, path, rotation(tenths));
}
// A fixed part at (x, y) on the stage (Q10), scaled and turned, in its own shade or another.
template <class F>
inline void put(F& f, u32 part, i32 x, i32 y, i32 scale = unit, i32 tenths = 0) {
    place(f, part, x + shift, stage_y * unit + y, scale, part_shade[part - ring_r0],
          part_shade[part - ring_r0] >= fill1 ? fill_and_stroke : stroke);
    if (tenths) opxy::draw::turn(f, part, rotation(tenths));
}
template <class F>
inline void put_in(F& f, u32 part, i32 x, i32 y, i32 scale, i32 tenths, u32 tone) {
    place(f, part, x + shift, stage_y * unit + y, scale, tone, stroke);
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

// The hit, eased, a little more each frame: `pose` Q16 rises to 1 over the first
// 33 ms (the swing landing) and eases back out; `left` Q16 falls from 1 to 0; `arc`
// Q16 runs out to 1 and comes back over the last quarter, so nothing snaps home.
// `step` 0..15 is for what jumps (a shaker's beads), -1 at rest.
struct Hit {
    i32 left, pose, arc, step, ph;
    explicit Hit(const view::Moment& m, rig::Timing t = rig::drum) {
        ph = m.phase(t.length, t.speed);
        step = ph < 0 ? -1 : ph >> 6;
        if (ph < 0) { left = pose = arc = 0; return; }
        const i32 u = 1024 - ph;
        left = (((u * u) >> 10) * u) >> 4;
        if (ph < 64) pose = ph << 10;
        else { const i32 w = (u * 1024) / 960; pose = (((w * w) >> 10) * w) >> 4; }
        arc = 65536 - left;
        if (ph > 768) arc = scaleq(arc, (1024 - ph) << 8);
    }
    bool on() const { return step >= 0; }
};

// ---- Membranes: a drum's head across its shell, in section ----
enum Striker { beater, stick, mallet, rimstick };
struct Mem {
    int ptch, dec, dent, dentw, lift, ball, hold, clip, clipk, felt, harm, hfrq, hreach, kink, wires, wtune, wlift, wkink, fan, pos, size, hpf;
    Striker striker;
    int depth;        // the shell's depth: where its bottom head is, stage y
    bool spring;      // an FM drum's exciter under the head
    int trait;        // the family: 0 TRX (tube lugs), 1 EFM (exciter coil), 2 P-I (rope)
    bool rack, floor, pillow, slack;   // a rack tom's mount, a floor tom's legs, a kick's pillow, a head tuned down
};
inline Mem mem(Striker s, int depth) {
    Mem c;
    c.ptch = c.dec = c.dent = c.dentw = c.lift = c.ball = c.hold = c.clip = c.clipk = c.felt = c.harm = c.hfrq = c.hreach = c.kink = -1;
    c.wires = c.wtune = c.wlift = c.wkink = c.fan = c.pos = c.size = c.hpf = -1;
    c.striker = s; c.depth = depth; c.spring = false;
    c.trait = 0; c.rack = c.floor = c.pillow = c.slack = false;
    return c;
}

// A striker: a beater or mallet swinging from a pivot above, a stick from a
// fulcrum at its butt, or a stick lying across the rim. Its tip lands at (tx, ty).
template <class F>
// style: a family's striker, 0 TRX's (a felt ball, a round tip), 1 EFM's (a square
// head, a flat nylon tip), 2 P-I's (a big felt head, a mallet stick's ball).
inline void striker(F& f, u32 path, Striker kind, i32 tx, i32 ty, i32 raise16, i32 ball, i32 pose, i32 x0, int style = 0) {
    if (style == 2) ball = ball * 13 / 10;
    Pts q;
    if (kind == beater || kind == mallet) {
        // Hung from (px, 4): the ball's centre is the tip less its radius.
        const i32 len = ty - ball - 4 * px;
        const i32 tenths = lerp(lerp(60, 450, raise16), 0, pose) * (kind == beater ? 1 : -1);
        const Turn t = rotation(tenths);
        const i32 pivx = tx, pivy = 4 * px;
        const i32 w = (kind == beater ? 11 : 10) * px / 10;
        // Down the shaft's left edge, around the ball, up its right edge, turned about the pivot.
        i32 xs[24], ys[24];
        u32 n = 0;
        xs[n] = -w; ys[n++] = 0;
        xs[n] = -w; ys[n++] = len - ball + px;
        if (style == 1) {
            // A square head, corner on.
            for (u32 k = 0; k < 4; ++k) {
                const Turn a = rotation(static_cast<i32>(1350 + k * 900));
                xs[n] = (a.cos * ball) >> 10; ys[n++] = len + ((-a.sin * ball) >> 10);
            }
        } else
            for (u32 k = 0; k <= 16; ++k) {
                const Turn a = rotation(static_cast<i32>(1060 + k * 3280 / 16));   // round the ball from the shaft's left: the outline must not cross itself
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
    const i32 tenths = kind == stick ? -lerp(lerp(30, 300, raise16), 0, pose) : -lerp(0, 50, pose);
    const Turn t = rotation(slope + tenths);
    // Along +x from the butt: 3.2 px wide there, 1.4 at the tip, and a round tip `ball` across.
    i32 xs[16], ys[16];
    u32 n = 0;
    xs[n] = 0; ys[n++] = -32 * px / 10;
    xs[n] = len - ball; ys[n++] = -14 * px / 10;
    if (style == 1) { xs[n] = len; ys[n++] = -ball; xs[n] = len; ys[n++] = ball; }   // a flat nylon tip
    else
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
    i32 yb = m.depth * px;
    if (m.hpf >= 0) yb -= scaleq(26 * px, c.v(m.hpf));   // a high-pass thins the shell
    // Where it is struck, and how deep the dent (RAMP, BUMP, TENS) and how wide.
    const i32 xs = m.pos >= 0 ? x0 + scaleq(L, lerp(4000, 32000, c.v(m.pos))) : x0 + L / 5;
    i32 depth = m.dent >= 0 ? lerp(2 * px, 24 * px, c.v(m.dent)) : 3 * px;
    depth = lerp(depth * 7 / 8, depth, h.pose);
    const u32 wj = m.dentw >= 0 ? static_cast<u32>(c.v(m.dentw)) >> 10 : 20;
    // The clamps: closer as distortion rises, shown only when it is up.
    const bool clamped = m.clip >= 0 && c.raw(m.clip, 0) > 0;
    const i32 gap = clamped ? lerp(46 * px, 8 * px, c.a(m.clip)) : 64 * px;
    // The head's waves: PTCH half-waves, carried DEC's reach from the strike, an
    // overtone riding them, a hit's swell falling back in sixteen steps.
    const i32 amp = 9 * px + scaleq(8 * px, h.pose);
    const i32 half = lerp(2 * 256, (m.slack ? 5 : 8) * 256, c.v(m.ptch));                       // Q8 half-waves across
    const i32 inv = per_reach[static_cast<u32>(c.v(m.dec)) >> 10];
    i32 hamp = 0, hmul = 3 * 256 + 180;
    if (m.harm >= 0) hamp = scaleq(scaleq(amp, 44000), c.a(m.harm));
    else if (m.hfrq >= 0) hamp = scaleq(amp, 12000);
    if (m.hfrq >= 0) hmul = lerp(384, 6 * 256, c.v(m.hfrq));
    if ((half * hmul >> 8) > 24 * 256) hmul = (24 * 256 << 8) / (half >> 4) >> 4;
    const i32 hinv = m.hreach >= 0 ? per_reach[static_cast<u32>(c.v(m.hreach)) >> 10] : inv;
    const i32 jag = m.kink >= 0 ? scaleq(9 * px, c.a(m.kink)) : 0;
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
    if (m.hold >= 0) raise = lerp(raise, 0, c.a(m.hold));
    const i32 ball = m.ball >= 0 ? lerp(m.striker == stick || m.striker == rimstick ? 4 * px : 10 * px,
                                        m.striker == stick || m.striker == rimstick ? 2 * px : 5 * px, c.v(m.ball))
                                 : (m.striker == stick || m.striker == rimstick ? 5 * px / 2 : 8 * px);
    const i32 press = m.hold >= 0 && c.a(m.hold) > h.pose ? c.a(m.hold) : h.pose;   // HOLD rests it in the dent
    const i32 contact = yh + scaleq(depth, press);
    if (m.striker == rimstick) striker(f, md::view::comb, rimstick, x0 + 80 * px, yh - 2 * px, 0, ball, h.pose, x0, m.trait);
    else striker(f, md::view::comb, m.striker, xs, contact - 2 * px, raise, ball, h.pose, x0, m.trait);
    // The family's hardware on the shell, and the machine's own.
    const i32 mid_y = (yh + yb) / 2;
    if (m.trait == 0) { put(f, lug_l, x0 - 13 * px, mid_y); put(f, lug_r, x1 + 13 * px, mid_y); }
    else if (m.trait == 2) { put(f, rope_l, x0 - 13 * px, mid_y); put(f, rope_r, x1 + 13 * px, mid_y); }
    else if (!m.spring) { put(f, coil, cx + 44 * px, yb - 32 * px); put(f, magnet, cx + 44 * px, yb - 9 * px); }
    if (m.rack) put(f, mount, x0 - 24 * px, yh + 30 * px);
    if (m.floor) { put(f, leg_l, x0 - 7 * px, yb + 4 * px); put(f, leg_r, x1 + 7 * px, yb + 4 * px); }
    if (m.pillow) put(f, pillow, cx + 30 * px, yb - px);
    // The clamps, their tilt the distortion's character (DTYP).
    if (clamped) {
        const i32 tilt = m.clipk >= 0 ? lerp(-40, 40, c.v(m.clipk)) : 0;
        const i32 sc = (w * 1024) / (116 * px);   // their length follows the drum's
        put(f, clamp_t, cx, yh - gap - 3 * px / 2, sc < unit ? sc : unit, tilt);
        put(f, clamp_b, cx, yh + gap + 3 * px / 2, sc < unit ? sc : unit, -tilt);
    }
    // A felt damper inside the shell, wider as DAMP rises.
    if (m.felt >= 0) put(f, felt, x1 - 34 * px, yh + 8 * px, lerp(420, 1900, c.a(m.felt)));
    // Under the head: snare wires on their strainer, or an FM drum's exciter, or NOIS's spray.
    Pts x;
    if (m.wires >= 0) {
        const i32 on = c.a(m.wires);
        const i32 drop = lerp(18 * px, 2 * px, on) + scaleq(5 * px, h.pose);
        const i32 a = lerp(px, 5 * px, on);
        // TUNE winds them tighter; DEC's rattle (RDEC, NDEC) longer. They fit their 56 points
        // at any setting: a zigzag that ran out of points would end in a stray straight line.
        const i32 per = m.wtune >= 0 ? lerp(10 * px, 11 * px / 2, c.v(m.wtune)) : 13 * px / 2;
        const i32 reach = m.wlift >= 0 ? lerp(60 * px, 150 * px, c.v(m.wlift)) : 150 * px;
        const i32 jig = m.wkink >= 0 ? scaleq(5 * px, c.a(m.wkink)) : 0;
        const i32 wx0 = cx - reach / 2, wx1 = cx + reach / 2, y = yb + 4 * px + drop + a;
        x.add(wx0 - 7 * px, y - 3 * px); x.add(wx0, y - 3 * px);
        u32 k = 0;
        for (i32 xx = wx0; xx <= wx1 && x.n < 58; xx += per / 2, ++k)
            x.add(xx, y + (k & 1 ? a : -a) + (jig ? scaleq(jig, static_cast<i32>(trx_panel::rnd(k + 5))) - jig / 2 : 0));
        x.add(wx1, y - 3 * px); x.add(wx1 + 7 * px, y - 3 * px);
        x.add(wx1 + 7 * px, y + a + 3 * px); x.add(wx0 - 7 * px, y + a + 3 * px);
        draw(f, md::view::extra, x, cx, y, grey(on));   // TONE fades them up, a grey at a time
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
    }
    // NOIS: a spray of fixed strokes off the head beside the strike, more and longer as it rises.
    if (m.fan >= 0) {
        const i32 on = c.a(m.fan);
        const u32 n = static_cast<u32>((on * 8 + 32768) >> 16);
        const i32 sc = lerp(800, 1500, on);
        const i32 r0 = (12 + ((8 * h.pose) >> 16)) * px;
        for (u32 k = 0; k < n; ++k) {
            const i32 tenths = -150 - static_cast<i32>(k) * 1150 / 7;
            const Turn a = rotation(tenths);
            put(f, spark0 + k, xs + a.cos * (r0 >> 10), yh - 2 * px + (a.sin * (r0 >> 10)) * 4 / 5, sc, tenths);
        }
    }
}

// ---- Cymbals: a cymbal's section, bell and bow, on its rod ----
struct Cym {
    int size, dec, thick, teeth, period, reach, marks, ping, bell, gap, tilt, grab, ring, lift, spring, coils, rag;
    bool hats, inverse;   // inverse: the control is a pitch, so a bigger value draws a smaller cymbal
    int kind, trait;      // kind: 0 a ride (flat, big bell), 1 a crash (steep, small bell), 2 a china (edge turned up); trait: the family
    int toothpx, ragpx;   // how deep TEETH and RAG cut the rim at full travel
    bool upward;          // the cuts stand off the upper face: for deep ones
};
inline Cym cym(bool hats, bool inverse) {
    Cym c;
    c.size = c.dec = c.thick = c.teeth = c.period = c.reach = c.marks = c.ping = c.bell = c.gap = c.tilt = c.grab = c.ring = c.lift = c.spring = c.coils = c.rag = -1;
    c.hats = hats; c.inverse = inverse; c.kind = 0; c.trait = 0; c.toothpx = 4; c.ragpx = 7; c.upward = false;
    return c;
}
// One half (side -1 or 1) of a cymbal's section, or the whole (side 0), about
// (cx, cy): bell, bow out to the rim `bow` below (or above, flipped), the
// thickness, the teeth of its rim, hammer marks dented into the bow.
struct Profile { i32 r, bellh, bow, thick, teeth, from, rag, rb, flare, markd; u32 wave, marks; bool upward; };
inline void section(Pts& q, i32 cx, i32 cy, const Profile& p, int side, bool flip) {
    const i32 rb = p.rb, dir = flip ? -1 : 1;
    i32 top[32], bot[32], xs[32];
    for (u32 i = 0; i < 32; ++i) {
        i32 r, sgn;
        if (side) { r = 3 * px + (p.r - 3 * px) * static_cast<i32>(i) / 31; sgn = side; }
        else { const i32 x = -p.r + 2 * p.r * static_cast<i32>(i) / 31; r = x < 0 ? -x : x; sgn = x < 0 ? -1 : 1; if (r < 3 * px) r = 3 * px; }
        xs[i] = cx + sgn * r;
        i32 hgt;
        if (r < rb) { const i32 u = ((r >> 4) << 6) / (rb >> 4 ? rb >> 4 : 1); hgt = -p.bellh + p.bellh * u * u / 4096 * 9 / 10; }   // a dome: (r / rb)^2
        else hgt = -p.bellh / 10 + p.bow * ((r - rb) >> 6) / ((p.r - rb) >> 6 ? (p.r - rb) >> 6 : 1);
        // A china's edge turns up over its outer fifth.
        if (p.flare && r > p.r * 4 / 5) { const i32 e = ((r - p.r * 4 / 5) >> 6) * 1024 / ((p.r / 5) >> 6 ? (p.r / 5) >> 6 : 1); hgt -= p.flare * ((e * e) >> 10) >> 10; }
        // The rim's teeth, from `from` (a fraction of the radius, Q16) out.
        const i32 at = scaleq(p.r, p.from);
        i32 tooth = 0;
        if (side && r > at && p.teeth) {
            const i32 ramp = ((r - at) >> 6) * 1024 / ((p.r - at) >> 6 ? (p.r - at) >> 6 : 1);   // Q10
            tooth = (multiply(p.teeth, md::sine(i * p.wave + 0x40000000u)) * ramp) >> 10;
            if (p.rag) tooth += scaleq(p.rag, static_cast<i32>(trx_panel::rnd(i * 5 + 1))) - p.rag / 2;
            // Deep teeth stand off the rim's upper face, so the outline doesn't cross its lower one.
            if (p.upward) tooth -= ((p.teeth * ramp) >> 10) + p.rag / 2;
        }
        // Hammer marks: dents three samples apart from the bell out.
        if (side && p.marks && i >= 6 && i < 6 + 2 * p.marks && (i - 6) % 2 == 0) tooth += p.markd;
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
    const int kind = m.hats ? 3 : m.kind;
    static const i32 bells[4] = {15, 6, 9, 10}, bows[4] = {6, 30, 15, 12}, radii[4] = {23, 10, 14, 16}, flares[4] = {0, 0, 14, 0};
    p.bellh = m.bell >= 0 ? lerp(7 * px, 19 * px, c.v(m.bell)) : bells[kind] * px;
    p.bow = bows[kind] * px;
    p.rb = radii[kind] * px;
    p.flare = flares[kind] * px;
    if (!m.hats) p.bow += scaleq(10 * px, h.pose);   // the bow flexes as it is struck
    p.thick = m.thick >= 0 ? lerp(26 * px / 5, 8 * px / 5, c.v(m.thick)) : (kind == 1 ? 2 * px : 3 * px);
    p.upward = m.upward;
    p.teeth = m.teeth >= 0 ? scaleq(m.toothpx * px, c.a(m.teeth)) : 0;
    // How close the teeth: from one every four samples to one every other (a higher tune: finer).
    p.wave = m.period >= 0 ? 0x40000000u + (static_cast<u32>(c.v(m.period)) << 14) : 0x80000000u;
    p.from = m.reach >= 0 ? lerp(50000, 20000, c.v(m.reach)) : 36000;
    p.marks = m.marks >= 0 ? static_cast<u32>((c.a(m.marks) * 6 + 32768) >> 16) : 0;
    p.markd = m.marks >= 0 ? lerp(3 * px / 2, 5 * px, c.a(m.marks)) : 0;   // more marks, hammered deeper
    p.rag = m.rag >= 0 ? scaleq(m.ragpx * px, c.a(m.rag)) : 0;
    const i32 out = scaleq(7 * px, h.pose);
    const i32 dec = c.v(m.dec);
    const i32 rscale = ((m.ring >= 0 ? lerp(800, 1250, c.v(m.ring)) : unit) * lerp(760, 1500, dec)) >> 10;   // DEC: more and wider
    if (m.hats) {
        // The pair: the bottom hat's rim at y 112, the top's GAP (or CLOS) above it,
        // lifted on a hit; CLSN tips the top hat down onto the bottom one.
        const i32 rim = 112 * px;
        i32 gap = m.gap >= 0 ? lerp(2 * px, 32 * px, c.v(m.gap)) : 6 * px;
        gap += scaleq(7 * px, h.pose);
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
        if (m.trait == 2) { put(f, felt, cx, ct - p.bellh - 11 * px); put(f, felt2, cx, cb + p.bellh + 5 * px); }   // P-I's felt washers
        rings(f, cx - R - 3 * px, cx + R + 3 * px, rim - gap / 2, dec, out, 0, rscale);
        if (m.spring >= 0) {
            // EFM-HH's tremolo: a spring on the rod above the clutch, stretched by
            // TREM, wound by TFRQ.
            Pts s;
            const i32 top = 8 * px, bottom = ct - p.bellh - 8 * px;
            const i32 coils = m.coils >= 0 ? lerp(3 * 256, 10 * 256, c.v(m.coils)) : 5 * 256;   // Q8
            const i32 a = lerp(2 * px, 8 * px, c.v(m.spring));
            const i32 pitch = ((bottom - top) * 128) / coils;                                // a half turn's height
            s.add(cx, bottom);
            i32 k = 1;
            for (i32 y = bottom - pitch; y > top + pitch / 2 && s.n < 30; y -= pitch, ++k) s.add(cx + (k & 1 ? a : -a), y);
            s.add(cx, top);
            draw(f, md::view::extra, s, cx, (top + bottom) / 2, mid, true);
        }
    } else {
        // A ride or crash on its tilter, tilting as it is struck.
        const i32 cy = 92 * px;
        const i32 tenths = -((70 * (h.pose >> 4)) >> 12);
        Pts a, b;
        section(a, cx, cy, p, -1, false);
        section(b, cx, cy, p, 1, false);
        draw(f, md::view::contour, a, cx, cy, thin, false, tenths);
        draw(f, md::view::wave, b, cx, cy, thin, false, tenths);
        put(f, rod, cx, cy + p.bellh + 84 * px + 2 * px - p.bellh);
        put(f, clutch, cx, cy - p.bellh - 4 * px);
        if (m.trait == 2) { put(f, felt, cx, cy - p.bellh - 11 * px); put(f, felt2, cx, cy + p.thick + 4 * px); }   // P-I's felt washers
        if (m.trait == 1) put(f, coil, cx, cy - p.bellh - 28 * px);                                                 // EFM's exciter
        rings(f, cx - R - 3 * px, cx + R + 3 * px, cy + p.bow - 6 * px, dec, out, 0, rscale);
        if (m.ping >= 0) {
            // TOP: the bell's ping, as rings above it.
            const u32 n = static_cast<u32>((c.a(m.ping) * 4 + 32768) >> 16);
            Pts s;
            for (u32 k = 0; k < n; ++k) {
                const i32 r = (8 + 5 * static_cast<i32>(k)) * px + out / 2 + scaleq(10 * px, c.a(m.ping));
                for (u32 j = 0; j <= 6; ++j) {
                    const u32 jj = k & 1 ? 6 - j : j;
                    const Turn t = rotation(-1440 + static_cast<i32>(jj) * 1080 / 6);
                    s.add(cx + ((t.cos * (r >> 4)) >> 6), cy - p.bellh - 2 * px + ((t.sin * (r >> 4)) >> 6));
                }
            }
            if (n) draw(f, md::view::comb, s, cx, cy - p.bellh - 14 * px, mid, true);
        }
        if (m.lift >= 0) {
            // HARD: the stick over the bow, higher for a harder stroke.
            // A ride is played on its bow, a crash on its edge.
            const i32 at = m.kind == 1 ? 7 : 5;
            const i32 tx = cx + R * at / 8, ty = cy + p.bow * at / 8 - 3 * px;
            striker(f, md::view::comb, stick, tx, ty, c.v(m.lift), 5 * px / 2, h.pose, 0, m.trait);
        }
        if (m.grab >= 0) {
            // GRAB: a hand closing on the rim the sooner it grabs; far off, it never does.
            put(f, hand, cx + R + lerp(2 * px, 60 * px, c.v(m.grab)), cy + p.bow);
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
    const i32 by1 = vy + scaleq(w, 23600);
    i32 hh = scaleq(w, lerp(72000, 18000, c.v(r.ceiling)));   // RTUN: a lower ceiling for a higher tuning
    if (hh > by1 - 6 * px) hh = by1 - 6 * px;
    const i32 bx0 = vx - w / 2, bx1 = vx + w / 2, by0 = by1 - hh;
    Pts lw, rw, back;
    lw.add(fx0, fy0); lw.add(bx0, by0); lw.add(bx0, by1); lw.add(fx0, fy1);
    rw.add(fx1, fy0); rw.add(bx1, by0); rw.add(bx1, by1); rw.add(fx1, fy1);
    back.add(bx0, by0); back.add(bx1, by0); back.add(bx1, by1); back.add(bx0, by1);
    draw(f, md::view::contour, lw, (fx0 + bx0) / 2, 84 * px, dim);
    draw(f, md::view::wave, rw, (fx1 + bx1) / 2, 84 * px, dim);
    draw(f, md::view::comb, back, vx, (by0 + by1) / 2, mid);
    const i32 sx = 240 * px, sy = 104 * px, mx = 484 * px, my = 94 * px;
    // The loudspeaker: its size (HARD, or EFM's PTCH), and TONE (HPF) tips it up and thins the fronts' shade.
    put(f, speaker, sx, sy, r.cone >= 0 ? lerp(r.pitched ? 1500 : 650, r.pitched ? 650 : 1500, c.v(r.cone)) : unit,
        r.aim >= 0 ? lerp(220, -220, c.v(r.aim)) : 0);
    put(f, mic, mx, my);
    put(f, stand, mx, my);
    if (r.pitched) { put(f, coil, sx - 30 * px, sy); put(f, magnet, sx - 30 * px, sy + 22 * px); }   // EFM's exciter behind the cone
    // The hands: wavefronts RATE (CDEC) apart, thrown about by CLPY's loose timing,
    // wider and ringing with RICH, rounder or sharper with HARD; a hit sends them
    // across the room.
    const u32 n = r.count >= 0 ? 1 + static_cast<u32>((c.v(r.count) * 3 + 32768) >> 16) : 4;
    i32 spacing = lerp(6 * px, 30 * px, c.v(r.spacing));
    if (r.count >= 0) spacing = lerp(spacing * 3 / 2, spacing * 4 / 5, c.v(r.count));   // CLPS: more hands, closer
    const i32 travel_out = scaleq(70 * px, h.arc);
    const i32 sc = r.scale >= 0 ? lerp(700, 1500, c.v(r.scale)) : unit;
    const i32 jit = c.a(r.jitter);
    const u32 shape = r.cone >= 0 && !r.pitched ? static_cast<u32>((c.v(r.cone) * 2 + 32768) >> 16) : 0;
    const u32 tone = grey(r.aim >= 0 ? lerp(20000, 65536, c.v(r.aim)) : 65536);
    const u32 first = shape == 0 ? static_cast<u32>(front0) : shape == 1 ? static_cast<u32>(frontb0) : static_cast<u32>(frontc0);
    const i32 ring_a = c.a(r.scale);
    for (u32 k = 0; k < n && k < 4; ++k) {
        const i32 r1 = static_cast<i32>(trx_panel::rnd(k + 2)) - 32768, r2 = static_cast<i32>(trx_panel::rnd(k + 9)) - 32768, r3 = static_cast<i32>(trx_panel::rnd(k + 17)) - 32768;
        const i32 x = sx - 10 * px + travel_out + static_cast<i32>(k) * spacing + scaleq(scaleq(spacing * 2, jit), r1 * 2);
        const i32 y = sy + scaleq(scaleq(70 * px, jit), r2);
        const i32 tenths = scaleq(scaleq(700, jit), r3);
        put_in(f, first + k, x, y, sc, tenths, tone);
        if (ring_a > 8000) put_in(f, echo0 + k, x - lerp(3 * px, 14 * px, ring_a), y, sc, tenths, grey(ring_a));
    }
    // ROOM: reflections off the walls into the microphone.
    if (r.rays >= 0) {
        const i32 on = c.a(r.rays);
        const i32 at = on * 6;                                  // Q16 rays, the last one growing out
        const u32 rays = static_cast<u32>((at + 65535) >> 16);
        const i32 marks[6][2] = {{bx0 + w * 3 / 10, by0}, {bx1, by0 + hh * 2 / 5}, {bx0 + w * 7 / 10, by1}, {bx0, by0 + hh * 3 / 5}, {bx0 + w * 11 / 20, by0}, {bx0 + w * 2 / 5, by1}};
        Pts q;
        for (u32 k = 0; k < rays; ++k) {
            const i32 fx = k & 1 ? mx - 7 * px : sx + 6 * px, fy = k & 1 ? my - 3 * px : sy;
            const i32 grown = k + 1 == rays ? at - static_cast<i32>(k << 16) : 65536;
            q.add(fx, fy);
            q.add(lerp(fx, marks[k][0], grown), lerp(fy, marks[k][1], grown));
            if (k + 1 == rays) break;
        }
        if (rays) {
            draw(f, md::view::extra, q, vx, 84 * px, grey(lerp(on / 2, on, h.pose) + 16000), true);
        }
    }
    // FM: the leading front rippled (MOD deep, MFRQ often, MDEC along it).
    if (r.ripple >= 0) {
        const i32 a = scaleq(7 * px, c.a(r.ripple));
        const i32 waves = lerp(2 * 256, 8 * 256, c.v(r.rfrq));
        const i32 along = lerp(16 * 1024, 80 * 1024, c.v(r.rreach));   // Q10, of 64 along it
        i32 rr = 14 * px + travel_out + static_cast<i32>(n) * spacing;
        if (rr > 120 * px) rr = 120 * px;
        Pts q;
        for (u32 i = 0; i < 24; ++i) {
            const Turn t = rotation(-260 + static_cast<i32>(i) * 520 / 23);
            const i32 edge = along - static_cast<i32>(i) * 2849;   // fades out past MDEC's reach
            const i32 e = edge <= 0 ? 0 : edge >= 8192 ? a : (a >> 3) * edge >> 10;
            const i32 rad = rr + multiply(e, md::sine(static_cast<u32>(waves) * i * 11673000u));
            q.add(sx + 2 * px + (t.cos * (rad >> 10)), sy + (t.sin * (rad >> 10)));
        }
        draw(f, md::view::extra, q, sx + rr, sy, thin, true);
    }
}

// ---- A cowbell in section, on its mount ----
struct Bell { int size, dec, thick, dent, second, damp, lift, rag, ripple, rfrq, rreach; int trait; };
template <class F>
inline void cowbell(F& f, const Ctl& c, const Bell& b, const view::Moment& moment) {
    const Hit h(moment);
    const i32 k = lerp(81920, 49152, c.v(b.size));   // Q16 1.25 .. 0.75: a bigger bell is lower
    const i32 cx = (b.second >= 0 ? 382 : 362) * px, yt = (b.trait == 1 ? 44 : 32) * px;
    const i32 H = scaleq(76 * px, k), wt = scaleq(20 * px, k), wm = scaleq(38 * px, k), yb = yt + H;
    const i32 t = b.thick >= 0 ? lerp(3 * px, 14 * px, c.v(b.thick)) : 7 * px / 2;
    const i32 dent = b.dent >= 0 ? scaleq(24 * px, c.a(b.dent)) : 0;
    const i32 rag = b.rag >= 0 ? scaleq(6 * px, c.a(b.rag)) : 0;
    const i32 ra = b.ripple >= 0 ? scaleq(10 * px, c.a(b.ripple)) : 0;
    const i32 rf = b.rfrq >= 0 ? lerp(3 * 128, 5 * 256, c.v(b.rfrq)) : 3 * 256;   // Q8 cycles up the wall
    const i32 rr = b.rreach >= 0 ? lerp(2 * 256, 22 * 256, c.v(b.rreach)) : 16 * 256;   // Q8 samples from the strike
    Pts q;
    // Outer: up the left wall (struck low on it, from the left), across the top, down the right wall.
    for (u32 i = 0; i <= 12; ++i) {
        const i32 y = yb - H * static_cast<i32>(i) / 12, x = cx - wm + (wm - wt) * static_cast<i32>(i) / 12;
        i32 bump = (i == 3 ? dent : i == 2 || i == 4 ? dent / 2 : 0);
        if (ra) {
            const i32 d = (static_cast<i32>(i) > 3 ? static_cast<i32>(i) - 3 : 3 - static_cast<i32>(i)) << 8;
            if (d < rr) bump += multiply(((ra >> 2) * ((rr - d) * 64 / rr)) >> 4, md::sine(static_cast<u32>(rf) * i * 1398101u));
        }
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
    const i32 swing = (70 * (h.pose >> 4)) >> 12;
    draw(f, md::view::contour, q, cx, yt, thin, false, swing);
    // The mouth's back lip, seen through it: TONE's thicker wall stands it further in.
    Pts lip;
    for (u32 i = 0; i <= 8; ++i) {
        const Turn a = rotation(1800 + static_cast<i32>(i) * 1800 / 8);
        lip.add(cx + ((a.cos * ((wm - t / 2) >> 10))), yb + a.sin * (4 + (t >> 10)) - t / 2);
    }
    draw(f, md::view::wave, lip, cx, yt, mid, true, swing);
    put(f, clutch, cx, yt - 3 * px);
    // It hangs from a short strap (EFM: from its exciter), all of it under the header.
    if (b.trait == 1) put(f, coil, cx, yt - 24 * px);
    else put(f, hanger, cx, yt - 4 * px);
    if (b.second >= 0 && c.a(b.second) > 2000) {
        // ENH: a second bell behind, growing the whole way up.
        const i32 k2 = lerp(14000, 58000, c.a(b.second)), c2 = 282 * px;
        const i32 H2 = scaleq(76 * px, k2), wt2 = scaleq(20 * px, k2), wm2 = scaleq(38 * px, k2), yt2 = yt + 14 * px;
        Pts s2;
        s2.add(c2 - wm2, yt2 + H2); s2.add(c2 - wt2, yt2); s2.add(c2 + wt2, yt2); s2.add(c2 + wm2, yt2 + H2);
        draw(f, md::view::extra, s2, c2, yt2, mid);
    }
    // The stick, low on the left wall; SNAP lifts it.
    striker(f, md::view::comb, stick, cx - wm + (wm - wt) * 3 / 12 - 2 * px + dent, yb - H * 3 / 12, b.lift >= 0 ? c.v(b.lift) : 16384, 5 * px / 2, h.pose, 0, b.trait);
    if (b.damp >= 0) put(f, hand, cx + wm + lerp(44 * px, -8 * px, c.a(b.damp)), yb - 4 * px);
    const i32 dec = c.v(b.dec);
    rings(f, cx - wm - 3 * px, cx + wm + 3 * px, yb + 2 * px, dec, scaleq(7 * px, h.pose), 0, lerp(760, 1500, dec));
}

// ---- Claves: two hardwood sticks, one struck on the other ----
template <class F>
inline void claves(F& f, const Ctl& c, const view::Moment& moment) {
    const Hit h(moment);
    // PTCH: the struck clave shorter; DUAL: the second longer, struck with it;
    // TUNE: the second's angle; ENH: its grain; CLIC: the spark where they meet.
    const i32 L1 = lerp(200 * px, 110 * px, c.v(0)), cx = 352 * px, cy = 118 * px;
    const i32 L2 = lerp(40 * px, 124 * px, c.a(2)), ang2 = lerp(-200, -480, c.v(4));
    const i32 lift = lerp(12 * px, 0, h.pose);
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
    draw(f, md::view::comb, g, cx, cy, mid, true);
    // CLIC: a spark at the contact, fixed strokes, more and longer as it rises.
    const i32 clic = c.a(5);
    const u32 n = static_cast<u32>((clic * 6 + 32768) >> 16);
    const i32 r0 = (6 + ((6 * h.pose) >> 16)) * px, sc = lerp(700, 1400, clic);
    for (u32 k = 0; k < n; ++k) {
        const i32 tenths = -1500 - static_cast<i32>(k) * 1300 / 5;
        const Turn t = rotation(tenths);
        fig::put(f, spark0 + k, hx + t.cos * (r0 >> 10), hy - 6 * px + t.sin * (r0 >> 10), sc, tenths);
    }
    const i32 dec = c.v(1);
    rings(f, cx - L1 / 2 - 4 * px, cx + L1 / 2 + 4 * px, cy + 8 * px, dec, scaleq(7 * px, h.pose), 0, lerp(760, 1500, dec));
}

// ---- A gourd shaker in section, its beads inside ----
struct Shake { int count, size, bead, type, damp, wall, att, sus, rev, dec, spread; bool egg; };
template <class F>
inline void shaker(F& f, const Ctl& c, const Shake& s, const view::Moment& moment) {
    const Hit h(moment, rig::shaker);
    const i32 k = s.size >= 0 ? lerp(48000, 78000, c.v(s.size)) : 65536;
    const i32 cx = 380 * px, cy = 76 * px, rx = scaleq(58 * px, k), ry = scaleq(44 * px, k);
    // A hit rocks it to and fro, dying away, and rattles its beads.
    const i32 swing = h.on() ? ((((90 * h.left) >> 16) * (md::sine(static_cast<u32>(h.ph) * 0x1000000u) >> 16)) >> 15) : 0;
    Pts g;
    // The egg from its handle's right side round to its left, then the handle down and back.
    for (u32 i = 0; i <= 40; ++i) {
        const Turn t = rotation(1350 - 120 - static_cast<i32>(i) * 3360 / 40);
        g.add(cx + ((t.cos * (rx >> 4)) >> 6), cy - ((t.sin * (ry >> 4)) >> 6));
    }
    const Turn hl = rotation(1350);
    const i32 hx = cx + ((hl.cos * (rx >> 4)) >> 6) * 9 / 10, hy = cy + ((hl.sin * (ry >> 4)) >> 6) * 9 / 10;
    if (!s.egg) { g.add(hx - 3 * px, hy + 2 * px); g.add(hx - 42 * px, hy + 40 * px); g.add(hx - 36 * px, hy + 46 * px); g.add(hx + 3 * px, hy + 8 * px); }
    else for (u32 i = 1; i < 5; ++i) {   // P-I's egg shaker: closed round, no handle
        const Turn t = rotation(1350 - 120 - static_cast<i32>(40 + i) * 3360 / 40);
        g.add(cx + ((t.cos * (rx >> 4)) >> 6), cy - ((t.sin * (ry >> 4)) >> 6));
    }
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
    const i32 bs = s.bead >= 0 ? lerp(500, 2048, c.v(s.bead)) : unit;
    i32 sp = s.spread >= 0 ? lerp(18000, 60000, c.v(s.spread)) : 46000;
    if (s.count >= 0) sp = lerp(sp * 3 / 5, sp, c.v(s.count));   // more beads, further out
    const i32 ty = s.type >= 0 ? c.v(s.type) : 0, dp = s.damp >= 0 ? c.v(s.damp) : 0;
    for (u32 b = 0; b < n && b < 8; ++b) {
        const i32 rowx = cx - scaleq(rx, sp) + scaleq(2 * scaleq(rx, sp), static_cast<i32>(b) * 65536 / 7);
        const i32 rowy = cy + ry / 3;
        const i32 scx = cx + scaleq(scaleq(rx * 3 / 2, sp), static_cast<i32>(trx_panel::rnd(b * 3 + 1))) - scaleq(rx * 3 / 4, sp);
        const i32 scy = cy + scaleq(ry, static_cast<i32>(trx_panel::rnd(b * 3 + 2))) - ry / 2;
        i32 x = lerp(rowx, scx, ty), y = lerp(rowy, scy, ty);
        y = lerp(y, cy + ry * 2 / 3 - static_cast<i32>(b & 1) * 6 * px, dp);
        if (h.on()) {
            // Each its own way about its place, each at its own rate, dying away.
            const u32 rate = (3u + b % 4u) * 0x800000u, phase = trx_panel::rnd(b + 3) << 16;
            const i32 reach = scaleq(14 * px, h.left);
            x += multiply(reach, md::sine(static_cast<u32>(h.ph) * rate + phase));
            y += multiply(reach, md::sine(static_cast<u32>(h.ph) * rate * 2u + phase * 3u));
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
    if (s.dec >= 0) rings(f, cx - rx - 4 * px, cx + rx + 4 * px, cy, c.v(s.dec), scaleq(7 * px, h.pose), 0, lerp(760, 1500, c.v(s.dec)));
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
    splay += scaleq(splay * 2 / 5 + 4 * px, h.pose);
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
    const i32 dec = c.v(1);
    rings(f, cx - 12 * px - splay, cx + 12 * px + splay, tip, dec, scaleq(7 * px, h.pose), 0, lerp(760, 1500, dec));
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
    // DEC: how far forward the cone is held; a hit shakes it, dying away.
    const i32 jit = lerp(-6 * px, 8 * px, c.v(0)) + (h.on() ? multiply(scaleq(6 * px, h.left), md::sine(static_cast<u32>(h.ph) * 0x2000000u)) : 0);
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
    const i32 dec = c.v(0);
    rings(f, 0, 428 * px, 86 * px, dec, scaleq(8 * px, h.pose), 1, lerp(760, 1700, dec));
}

template <class F>
inline void cam(F& f, const Ctl& c, const view::Moment& moment) {
    const Hit h(moment);
    // An impulse as a cam's profile: a lobe UP long and UVAL high, then a dip
    // DOWN long and DVAL deep; a hit turns the cam under its follower.
    const i32 cx = 362 * px, cy = 98 * px, r0 = 30 * px;
    const i32 up = lerp(200, 1300, c.v(0)), uh = scaleq(22 * px, c.a(1)), dn = lerp(200, 1300, c.v(2)), dd = scaleq(18 * px, c.a(3));
    Pts q;
    for (u32 i = 0; i < 64; ++i) {
        const i32 a = static_cast<i32>(i) * 3600 / 64;   // tenths, clockwise from the top
        i32 r = r0;
        if (a < up) r += scaleq(uh / 2, 65536 - (md::sine(static_cast<u32>(a) * (0xffffffffu / static_cast<u32>(up)) + 0x40000000u) >> 15));
        else if (a >= up + 150 && a < up + 150 + dn) r -= scaleq(dd / 2, 65536 - (md::sine(static_cast<u32>(a - up - 150) * (0xffffffffu / static_cast<u32>(dn)) + 0x40000000u) >> 15));
        const Turn t = rotation(a - 900);
        q.add(cx + t.cos * (r >> 10), cy + t.sin * (r >> 10));
    }
    const i32 tenths = 150 - scaleq(3600, 65536 - h.left);
    draw(f, md::view::contour, q, cx, cy, thin, false, tenths);
    // The follower: a lever from its pivot at the left, its roller on the cam's top.
    i32 top = r0;
    {
        // The profile under the roller: the cam turned by `tenths`, so its angle -tenths.
        const i32 a = ((-tenths) % 3600 + 3600) % 3600;
        if (a < up) top += scaleq(uh / 2, 65536 - (md::sine(static_cast<u32>(a) * (0xffffffffu / static_cast<u32>(up)) + 0x40000000u) >> 15));
        else if (a >= up + 150 && a < up + 150 + dn) top -= scaleq(dd / 2, 65536 - (md::sine(static_cast<u32>(a - up - 150) * (0xffffffffu / static_cast<u32>(dn)) + 0x40000000u) >> 15));
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
    const i32 bow = lerp(lerp(px, 15 * px, c.v(3)), lerp(4 * px, 20 * px, c.v(3)), h.pose);
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
    striker(f, md::view::comb, mallet, cx + 10 * px, by - 4 * px + bow, 20000, lerp(10 * px, 5 * px, c.v(2)), h.pose, 0, 2);
    const i32 dec = c.v(1);
    rings(f, cx - half - 3 * px, cx + half + 3 * px, by, dec, scaleq(7 * px, h.pose), 0, lerp(760, 1500, dec));
}
}

// The machines' views, by their slots (kit.h), each with its row of travel.
#define MD_FIG_VIEW(ns, row, ...) namespace ns { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { \
    const fig::Ctl c = {s, fig::travel[row]}; fig::begin(f); __VA_ARGS__ } }
MD_FIG_VIEW(trx_bd, 0, fig::Mem k = fig::mem(fig::beater, 148); k.trait = 0; k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.lift = 4; k.fan = 5; k.harm = 6; k.clip = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_sd, 1, fig::Mem k = fig::mem(fig::stick, 112); k.trait = 0; k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.lift = 4; k.wires = 5; k.wtune = 6; k.clip = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_xt, 2, fig::Mem k = fig::mem(fig::stick, 122); k.trait = 0; k.rack = true; k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.felt = 4; k.clip = 5; k.clipk = 6; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_cp, 3, fig::Room r = {-1, 4, 0, 3, 1, 6, 7, 5, 2, -1, -1, -1, false}; fig::chamber(f, c, r, m);)
MD_FIG_VIEW(trx_rs, 4, fig::Mem k = fig::mem(fig::rimstick, 104); k.trait = 0; k.ptch = 0; k.dec = 1; k.clip = 2; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_cb, 5, fig::Bell b = {0, 1, 4, 5, 2, 3, -1, -1, -1, -1, -1, 0}; fig::cowbell(f, c, b, m);)
MD_FIG_VIEW(trx_ch, 6, fig::Cym k = fig::cym(true, false); k.gap = 0; k.dec = 1; k.thick = 2; k.teeth = 3; k.marks = 4; k.trait = 0; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(trx_oh, 7, fig::Cym k = fig::cym(true, false); k.gap = 0; k.dec = 1; k.thick = 2; k.teeth = 3; k.marks = 4; k.trait = 0; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(trx_cy, 8, fig::Cym k = fig::cym(false, false); k.marks = 0; k.dec = 1; k.ping = 2; k.bell = 3; k.size = 4; k.teeth = 5; k.kind = 0; k.trait = 0; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(trx_ma, 9, fig::Shake k = {4, -1, 7, 5, 3, 6, 0, 1, 2, -1, -1, false}; fig::shaker(f, c, k, m);)
MD_FIG_VIEW(trx_cl, 10, fig::claves(f, c, m);)
MD_FIG_VIEW(trx_xc, 11, fig::Mem k = fig::mem(fig::stick, 136); k.trait = 0; k.floor = true; k.slack = true; k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.felt = 4; k.clip = 5; k.clipk = 6; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(trx_b2, 12, fig::Mem k = fig::mem(fig::beater, 148); k.trait = 0; k.pillow = true; k.ptch = 0; k.dec = 1; k.dent = 2; k.hold = 3; k.ball = 4; k.fan = 5; k.kink = 6; k.clip = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_bd, 13, fig::Mem k = fig::mem(fig::beater, 148); k.trait = 1; k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.harm = 4; k.hfrq = 5; k.hreach = 6; k.kink = 7; k.spring = true; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_sd, 14, fig::Mem k = fig::mem(fig::stick, 112); k.trait = 1; k.ptch = 0; k.dec = 1; k.wires = 2; k.wlift = 3; k.harm = 4; k.hfrq = 5; k.hreach = 6; k.hpf = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_xt, 15, fig::Mem k = fig::mem(fig::stick, 122); k.trait = 1; k.rack = true; k.ptch = 0; k.dec = 1; k.dent = 2; k.dentw = 3; k.harm = 4; k.hfrq = 5; k.hreach = 6; k.ball = 7; k.spring = true; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_cp, 16, fig::Room r = {2, 3, -1, -1, 7, 1, -1, -1, 0, 4, 5, 6, true}; fig::chamber(f, c, r, m);)
MD_FIG_VIEW(efm_rs, 17, fig::Mem k = fig::mem(fig::rimstick, 104); k.trait = 1; k.ptch = 0; k.dec = 1; k.harm = 2; k.hpf = 3; k.wires = 4; k.wtune = 5; k.wlift = 6; k.wkink = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(efm_cb, 18, fig::Bell b = {0, 1, -1, -1, -1, -1, 2, 3, 4, 5, 6, 1}; fig::cowbell(f, c, b, m);)
MD_FIG_VIEW(efm_hh, 19, fig::Cym k = fig::cym(true, true); k.size = 0; k.dec = 1; k.spring = 2; k.coils = 3; k.teeth = 4; k.period = 5; k.reach = 6; k.rag = 7; k.trait = 1; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(efm_cy, 20, fig::Cym k = fig::cym(false, true); k.size = 0; k.dec = 1; k.rag = 2; k.thick = 3; k.teeth = 4; k.period = 5; k.reach = 6; k.kind = 2; k.trait = 1; k.toothpx = 10; k.ragpx = 14; k.upward = true; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(pi_bd, 21, fig::Mem k = fig::mem(fig::mallet, 148); k.trait = 2; k.ptch = 0; k.dec = 1; k.ball = 2; k.lift = 3; k.dent = 4; k.felt = 5; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(pi_sd, 22, fig::Mem k = fig::mem(fig::stick, 112); k.trait = 2; k.ptch = 0; k.dec = 1; k.ball = 2; k.harm = 3; k.dent = 4; k.wires = 5; k.wlift = 6; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(pi_mt, 23, fig::Mem k = fig::mem(fig::mallet, 122); k.trait = 2; k.rack = true; k.ptch = 0; k.dec = 1; k.ball = 2; k.lift = 3; k.hfrq = 4; k.felt = 5; k.size = 6; k.pos = 7; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(pi_ml, 24, fig::bar(f, c, m);)
MD_FIG_VIEW(pi_ma, 25, fig::Shake k = {0, 4, 5, -1, -1, -1, -1, -1, -1, 1, 2, true}; fig::shaker(f, c, k, m);)
MD_FIG_VIEW(pi_rs, 26, fig::Mem k = fig::mem(fig::rimstick, 104); k.trait = 2; k.ptch = 0; k.dec = 1; k.ball = 2; k.harm = 3; k.wires = 4; k.wlift = 5; fig::membrane(f, c, k, m);)
MD_FIG_VIEW(pi_rc, 27, fig::Cym k = fig::cym(false, true); k.size = 0; k.dec = 1; k.lift = 2; k.ring = 3; k.teeth = 4; k.marks = 5; k.thick = 6; k.grab = 7; k.kind = 0; k.trait = 2; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(pi_cc, 28, fig::Cym k = fig::cym(false, true); k.size = 0; k.dec = 1; k.lift = 2; k.ring = 3; k.teeth = 4; k.marks = 5; k.thick = 6; k.grab = 7; k.kind = 1; k.trait = 2; fig::cymbal(f, c, k, m);)
MD_FIG_VIEW(pi_hh, 29, fig::Cym k = fig::cym(true, true); k.size = 0; k.dec = 1; k.tilt = 2; k.ring = 3; k.teeth = 4; k.marks = 5; k.thick = 6; k.gap = 7; k.trait = 2; fig::cymbal(f, c, k, m);)
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
// the change it makes to the sound (the root of its share of 24 dB of band energy, at least 0.5).
static const opxy::u16 travel_e12[16][8] = {
    {256, 256, 128, 128, 252, 256, 256, 207},   // E12-BD: PTCH 26.5, DEC 77.6, SNAP 2.8, SPLN 5.5, STRT 23.3, RTRG 31.6, RTIM 31.4, BEND 15.7
    {256, 256, 237, 154, 256, 256, 193, 256},   // E12-SD: PTCH 53.0, DEC 87.7, HP 20.5, RING 8.7, STRT 33.3, RTRG 34.1, RTIM 13.7, BEND 27.7
    {256, 256, 256, 128, 235, 256, 256, 243},   // E12-HT: PTCH 35.8, DEC 91.9, HP 33.3, HPQ 3.3, STRT 20.2, RTRG 33.2, RTIM 30.8, BEND 21.6
    {256, 256, 256, 128, 256, 256, 255, 225},   // E12-LT: PTCH 32.6, DEC 99.0, HP 33.1, HPQ 3.2, STRT 34.3, RTRG 32.1, RTIM 23.8, BEND 18.5
    {256, 256, 241, 128, 175, 256, 174, 256},   // E12-CP: PTCH 64.1, DEC 65.7, HP 21.3, HPQ 3.3, STRT 11.2, RTRG 35.5, RTIM 11.1, BEND 34.4
    {256, 256, 252, 173, 256, 256, 256, 256},   // E12-RS: PTCH 49.2, DEC 86.4, HP 23.2, RATL 11.0, STRT 92.1, RTRG 34.8, RTIM 33.9, BEND 28.4
    {256, 256, 246, 128, 171, 256, 217, 256},   // E12-CB: PTCH 47.9, DEC 68.0, HP 22.1, HPQ 2.6, STRT 10.7, RTRG 31.9, RTIM 17.3, BEND 26.6
    {256, 256, 256, 128, 224, 256, 209, 256},   // E12-CH: PTCH 49.0, DEC 84.0, HP 25.7, HPQ 3.1, STRT 18.3, RTRG 40.3, RTIM 16.0, BEND 24.5
    {256, 256, 214, 256, 256, 181, 128, 256},   // E12-OH: PTCH 41.9, DEC 80.3, HP 16.8, STOP 58.3, STRT 82.0, RTRG 12.0, RTIM 5.7, BEND 27.5
    {256, 256, 203, 154, 191, 228, 128, 240},   // E12-RC: PTCH 35.9, DEC 76.5, HP 15.1, BELL 8.6, STRT 13.3, RTRG 19.1, RTIM 5.4, BEND 21.1
    {256, 256, 224, 128, 184, 174, 128, 256},   // E12-CC: PTCH 42.5, DEC 85.4, HP 18.4, HPQ 2.7, STRT 12.4, RTRG 11.1, RTIM 4.7, BEND 24.7
    {256, 256, 241, 176, 209, 256, 199, 256},   // E12-BR: PTCH 53.9, DEC 85.3, HP 21.4, REAL 11.3, STRT 16.0, RTRG 37.4, RTIM 14.5, BEND 27.1
    {256, 256, 195, 128, 164, 256, 158, 256},   // E12-TA: PTCH 35.3, DEC 62.6, HP 13.9, HPQ 2.6, STRT 9.9, RTRG 26.4, RTIM 9.2, BEND 32.7
    {256, 256, 214, 128, 172, 221, 128, 251},   // E12-TR: PTCH 34.4, DEC 68.5, HP 16.8, HPQ 2.6, STRT 10.8, RTRG 18.0, RTIM 4.0, BEND 23.1
    {256, 256, 194, 183, 134, 256, 179, 256},   // E12-SH: PTCH 32.3, DEC 65.6, HP 13.9, SLEW 12.3, STRT 6.6, RTRG 34.9, RTIM 11.7, BEND 31.0
    {256, 256, 242, 178, 201, 256, 256, 256},   // E12-BC: PTCH 43.8, DEC 84.9, HP 21.4, BC 11.6, STRT 14.8, RTRG 40.9, RTIM 34.4, BEND 25.0
};
// travel-e12:end

// tools/art/fig.mjs's E12 parts, in its order, in the common nodes' places.
enum Part : u32 { block = 4, ch_top, ch_bot, slot45, slot90, blade, patch0, diag = patch0 + 4, notch0, part_end = notch0 + 4 };
static const u8 part_shade[part_end - block] = {mid, dim, dim, dim, dim, outline2, outline2, outline2, outline2, outline2, thin, thin, thin, thin, thin};

// Each machine's recording, so each tape reads as its own: how far apart its stripes
// (Q8 of the pitch's spacing), how many come in a burst and how many steps' gap
// follows one, how much they vary (Q16), every how many a tall one rings out, and
// how many levels they are stepped to (0: smooth). A kick's few wide bands, a hat's
// fine dense stripes, a clap's bursts.
struct Take { u16 spacing; u8 burst, gap; u16 vary; u8 ping, levels; };
static const Take takes[16] = {
    {520, 0, 0, 6000, 0, 0},    // BD: few wide bands
    {256, 0, 0, 44000, 0, 0},   // SD: even, noisy
    {300, 0, 0, 10000, 3, 0},   // HT: tight, a louder one every third
    {460, 0, 0, 22000, 0, 0},   // LT: wide, uneven
    {230, 3, 2, 30000, 0, 0},   // CP: bursts of three
    {300, 2, 3, 20000, 0, 0},   // RS: two and a long gap
    {256, 0, 0, 4000, 2, 0},    // CB: two tones, tall and short
    {150, 0, 0, 30000, 0, 0},   // CH: dense and fine
    {150, 0, 0, 30000, 7, 0},   // OH: dense, the odd one ringing
    {175, 0, 0, 14000, 5, 0},   // RC: fine, a bell every fifth
    {160, 0, 0, 46000, 0, 0},   // CC: fine and wild
    {180, 5, 1, 50000, 0, 0},   // BR: swishes
    {190, 2, 2, 25000, 0, 0},   // TA: jingles in pairs
    {170, 0, 0, 0, 0, 0},       // TR: even and pure
    {170, 4, 2, 40000, 0, 0},   // SH: shakes of four
    {256, 0, 0, 20000, 0, 4},   // BC: stepped
};
template <class F>
inline void put(F& f, u32 part, i32 x, i32 y) {
    const u8 s = part_shade[part - block];
    place(f, part, x + fig::shift, stage_y * unit + y, unit, s, s >= fill1 ? fill_and_stroke : stroke);
}

enum Own { crease, second, stop, snap };
template <class F>
inline void tape(F& f, const Ctl& c, i32 decay16, Own own, u32 take, const view::Moment& moment) {
    const Hit h(moment);
    const i32 slide = -scaleq(26 * px, h.arc);   // the tape runs left through the block, and back
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
    const i32 crease_a = own == crease ? scaleq(6 * px, c.a(3)) : 0;
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
    const Take& tk = takes[take & 15];
    const i32 spacing0 = (lerp(10 * px, 16 * px / 5, c.v(0)) * tk.spacing) >> 8;
    const i32 bend = c.v(7) - 32768;                                                       // -32768 .. 32768
    const i32 kd = scaleq((decay16 >> 4) + 8000, 65536 - c.v(1));                                    // the fall a step, Q12
    const i32 s0 = scaleq(14 * 256, c.a(4)) >> 8;                                          // STRT: steps cut away
    const i32 low = own == snap ? 65536 : 65536 - scaleq(56000, c.a(2));                    // HP thins the lower halves
    const i32 drive = own == snap ? lerp(65536, 5 * 32768, c.a(2)) : 65536;                 // SNAP: up to 3 times, clipped
    const i32 driven = own == snap ? lerp(1 * 256, 6 * 256, c.v(3)) : 0;   // Q8 steps                              // for SPLN's steps
    const i32 joins = 1 + ((c.a(5) * 3 + 32768) >> 16);               // RTRG: the splices
    const i32 every = scaleq(lerp(24 * px, 76 * px, c.v(6)), lerp(76000, 52000, c.a(5)));   // RTRG: more splices, closer
    const i32 second_at = own == second ? lerp(end - 4 * px, cut + 14 * px, c.a(3)) : end + 64 * px;
    // The stripes as one line, up and down in turn. A burst's gap is left bare: the
    // line runs along the tape's lower edge there, where the outline already is.
    Pts all[2];
    u32 side = 0, part = 0;
    i32 x = cut + 3 * px, step = 0, join = 1;
    bool wrote = false;
    while (x < end - 10 * px) {
        if (all[part].n >= 30) { if (part) break; part = 1; side = 0; wrote = false; }
        if (join < joins && x >= cut + join * every) { step = 0; ++join; }
        const i32 s = s0 + step;
        // BEND: the first steps crowded (or spread), drifting back.
        const i32 crowd = scaleq(bend, md::exp_neg(step * 65536 / 8) >> 15);
        const i32 sp = scaleq(spacing0, 65536 - crowd);
        const u32 seed = (x >= second_at ? static_cast<u32>(s) + 40u : static_cast<u32>(s)) + take * 7u;   // each machine its own recording
        i32 a = scaleq(18 * px, md::exp_neg(s * kd >> 2) >> 15);
        a = scaleq(a, 65536 - tk.vary + scaleq(tk.vary, static_cast<i32>(trx_panel::rnd(seed * 3 + 1))));
        if (tk.ping && step % tk.ping == 0) a = a * 8 / 5;
        else if (tk.ping) a = a * 4 / 5;
        if (tk.levels) { const i32 q = 18 * px / tk.levels; a = (a + q / 2) / q * q; }
        if ((step << 8) < driven) a = scaleq(a, drive);
        else if ((step << 8) < driven + 256) a = scaleq(a, lerp(drive, 65536, ((step << 8) + 256 - driven) << 8));   // the last one between
        if (a > 18 * px) a = 18 * px;
        if (a < px) a = px;
        i32 lo = scaleq(a, low);
        if (lo < px) lo = px;
        all[part].add(x, yc + (side ? lo : -a));
        all[part].add(x, yc + (side ? -a : lo));
        side ^= 1;
        wrote = true;
        ++step;
        const i32 next = sp < 3 * px ? 3 * px : sp;
        x += next;
        if (tk.burst && step % tk.burst == 0) {
            // To the lower edge, along it past the gap, and up into the next burst.
            all[part].add(x - next, yc + half);
            x += tk.gap * next;
            if (x < end - 10 * px) all[part].add(x, yc + half);
            side = 0;
        }
    }
    (void)wrote;
    for (u32 p = 0; p < 2; ++p)
        if (all[p].n >= 2) fig::draw(f, p ? md::view::wave : md::view::comb, all[p], (all[p].p[0].x + all[p].p[all[p].n - 1].x) / 2, yc, thin, true);
    // The splicing-tape patches: the leader's join, then RTRG's.
    for (i32 k = 0; k < joins && k < 4; ++k) put(f, patch0 + static_cast<u32>(k), cut + k * every, yc);
    if (own == second && c.raw(3, 0) > 0) put(f, diag, second_at, yc);
    if (own == snap) put(f, diag, cut + 3 * px + ((driven >> 4) * (spacing0 >> 4) >> 0), yc);   // SPLN: where the saturated start ends
    // The block's mark: the machine's number, plus one, as notches filed in its right end.
    for (u32 k = 0; k < 4; ++k) if ((take + 1) >> k & 1) put(f, notch0 + k, cx + (140 - 7 * static_cast<i32>(k)) * px, yc - 38 * px);
    // STRT: the cut-off start, lying above the block.
    if (c.a(4) > 2000) {
        const i32 len = lerp(6 * px, 100 * px, c.a(4));
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
