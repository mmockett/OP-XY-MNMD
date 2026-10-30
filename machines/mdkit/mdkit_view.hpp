#pragma once
// The other Machinedrum families' panels and their machines' views, after the
// design's pEfm, pPi, pE12, pNoise and pImpulse (docs/design/Main.dc.html), for
// the drum kits' per-pad screens. The instrument on the left and each panel's
// fixed parts come from the rig (md_rig.hpp); these draw what changes shape, on
// the four paths, with trx_view.hpp's helpers.
#include "trx_view.hpp"

namespace md_mdkit {} namespace md = md_mdkit; namespace md_mdkit {
namespace kit_panel {
using namespace md::view;
using namespace md::trx_panel;
const i32 PX = panel_x, PW = panel_w;

// A control 0..127 as Q31 0..1; `otherwise` (0..127) when the machine lacks it.
inline i32 frac(const i32* s, int slot, u32 otherwise) {
    const u32 v = raw(s, slot, otherwise);
    return v >= 127 ? 0x7fffffff : static_cast<i32>(v * 16909320u);
}
// The phase through a hit (Q10, -1 when idle), as rig::draw times it.
inline i32 phase(const view::Moment& m, rig::Timing timing = rig::drum) { return m.phase(timing.length, timing.speed); }

// pEfm's modulator ratio, 2^(4 MFRQ - 1) clamped to 0.5..8, Q16, by MFRQ 0..127.
static const i32 ratio16[128] = {32768, 33491, 34230, 34986, 35758, 36547, 37354, 38178, 39021, 39882, 40763, 41662, 42582, 43522, 44482, 45464, 46468, 47493, 48541, 49613, 50708, 51827, 52971, 54140, 55335, 56556, 57805, 59080, 60384, 61717, 63079, 64472, 65895, 67349, 68836, 70355, 71908, 73495, 75117, 76775, 78469, 80201, 81971, 83781, 85630, 87520, 89452, 91426, 93444, 95506, 97614, 99769, 101971, 104221, 106522, 108873, 111276, 113732, 116242, 118808, 121430, 124110, 126849, 129649, 132511, 135435, 138425, 141480, 144602, 147794, 151056, 154390, 157798, 161281, 164840, 168478, 172197, 175998, 179882, 183852, 187910, 192058, 196297, 200629, 205058, 209583, 214209, 218937, 223769, 228708, 233756, 238916, 244189, 249578, 255087, 260717, 266472, 272353, 278364, 284508, 290788, 297206, 303765, 310470, 317323, 324326, 331485, 338801, 346279, 353922, 361733, 369717, 377877, 386218, 394742, 403455, 412360, 421461, 430763, 440271, 449988, 459920, 470071, 480446, 491050, 501889, 512966, 524288};

// Spikes from a base (Q10), split over paths `first` .. `first + paths - 1`, ten each.
template <class F>
inline void spread(F& frame, u32 first, u32 paths, const Spike* sp, u32 n, i32 base, u32 key, u32 tone) {
    for (u32 p = 0; p < paths; ++p) {
        const u32 from = p * 10, size = n > from ? (n - from < 10 ? n - from : 10) : 0;
        spikes(frame, first + p, sp + from, size, base, key + p * 13, tone);
    }
}

// pEfm: the modulator's wave flowing out of its operator along the arrows (one
// per quarter of MOD) into the carrier it bends; below, the carrier's line at
// PTCH and the sidebands MOD spreads about it, RATIO apart; and one of the
// machine's own marks: the noise of SD, the click of XT and CB, the claps of CP
// or the snare of RS. A hit plays it: the modulation falls away, so the
// carrier straightens and its sidebands draw in, and the marks jump.
enum Extra { none, noise, click, claps, snare };
struct Efm { int mod, mfrq, a, b; Extra extra; bool modulator; };
// sqrt(22^2 - (2.5 k)^2) px, Q10: where the arrows meet the operators' circles.
static const i32 rim[5] = {22528, 22383, 21938, 21180, 20067};
template <class F>
inline void efm(F& frame, const i32* s, const Efm& c, const view::Moment& m) {
    const i32 mod = frac(s, c.mod, 0), ph = phase(m), f = fade(m);
    const i32 rv = ratio16[raw(s, c.mfrq, 64)];
    const i32 live = ph > 0 ? mod - multiply(mod, static_cast<i32>(static_cast<u32>(ph) << 20)) : mod;   // 1 - ph / 2
    const i32 mx = PX + 34, cxr = PX + PW - 34, oy = 40;
    i32 xs[32], ys[32];
    u32 n = 0;
    // The modulator's wave, then the arrows from its circle to the carrier's,
    // each turning back along a circle's edge to the next.
    if (c.modulator && rv <= 4 * 65536) {
        for (u32 i = 0; i < 22; ++i, ++n) {
            xs[n] = (mx - 15) * unit + static_cast<i32>(i) * (30 * unit / 21);
            const u32 u = static_cast<u32>(rv) * i * 3121u;                      // rv i / 21 cycles
            ys[n] = oy * unit - multiply(md::sine(u), 9 * unit);
        }
    } else if (c.modulator) {
        // Above a ratio of 4 the 22 points would alias: its peaks, as a zigzag.
        const u32 peaks = 1 + (static_cast<u32>(rv) * 2u >> 16);                // whole half cycles across, 9 .. 17
        const i32 step = mul16(30 * unit, static_cast<u32>(over[peaks > 40 ? 40 : peaks]));
        xs[n] = (mx - 15) * unit; ys[n++] = oy * unit;
        for (u32 i = 0; i < peaks && n < 20; ++i, ++n) {
            xs[n] = (mx - 15) * unit + step / 2 + static_cast<i32>(i) * step;
            ys[n] = oy * unit + (i & 1 ? 9 * unit : -9 * unit);
        }
        xs[n] = (mx + 15) * unit; ys[n++] = oy * unit;
    }
    const u32 lines = 1 + scaled(raw(s, c.mod, 0), 4);
    for (u32 i = 0; i < lines; ++i) {
        const i32 k = 2 * static_cast<i32>(i) - static_cast<i32>(lines - 1);  // -4 .. 4, in 2.5 px
        const i32 y = oy * unit + k * (unit * 5 / 2), in = rim[k < 0 ? -k : k];
        const bool right = !(i & 1);
        xs[n] = right ? mx * unit + in : cxr * unit - in; ys[n++] = y;
        xs[n] = right ? cxr * unit - in : mx * unit + in; ys[n++] = y;
    }
    while (n < 32) { xs[n] = xs[n - 1] + 1; ys[n] = ys[n - 1]; ++n; }
    draw32(frame, contour, xs, ys, (mx + cxr) / 2, oy, key_of(rv << 8, mod, c.modulator ? 1 : 0, 21), mod > 0x028f5c29 ? view::mid : dim);
    for (u32 i = 0; i < 32; ++i) {
        xs[i] = (cxr - 15) * unit + static_cast<i32>(i) * (30 * unit / 31);
        const u32 u = i * 138547332u;                                            // i / 31 cycles
        const i32 bend = multiply(live, md::sine(static_cast<u32>(rv) * i * 2114u));
        const u32 at = u + static_cast<u32>(bend) + static_cast<u32>(multiply(bend, 1270265472));   // 5 / (2 pi) a cycle at full MOD
        ys[i] = oy * unit - multiply(md::sine(at), 9 * unit);
    }
    draw32(frame, wave, xs, ys, cxr, oy, key_of(rv << 8, live, 0, 22), line);
    // The spectrum: the carrier at PTCH, 1 + 7 MOD sidebands a side, RATIO apart,
    // each e^(-|k| / (0.6 + 3 MOD)) as tall; fewer when the machine's mark needs a path.
    const i32 base = 150 * unit, fc = (PX + 24) * unit + static_cast<i32>(raw(s, 0, 64) * 120 * unit / 127);
    i32 sp = 6 * unit + mul16(7 * unit, static_cast<u32>(rv));
    if (sp > 46 * unit) sp = 46 * unit;
    const i32 most = c.extra == none ? 7 : 4;
    i32 sides = (multiply(live, 7 * 256) + 128) >> 8;
    if (sides > most) sides = most;
    // 1 / (0.6 + 3 MOD), Q16, by MOD in sixteenths.
    static const i32 falloff[17] = {109227, 83220, 67216, 56375, 48545, 42625, 37992, 34267, 31208, 28650, 26479,
                                    24614, 22995, 21576, 20321, 19205, 18204};
    const i32 per_side = falloff[(static_cast<u32>(live) + 0x04000000u) >> 27];
    Spike band[15] = {};
    u32 nb = 0;
    for (i32 k = -sides; k <= sides; ++k) {
        const i32 x = fc + k * sp;
        if (x < PX * unit || x > (PX + PW) * unit) continue;
        const i32 h = k ? multiply(46 * unit, md::exp_neg((k < 0 ? -k : k) * per_side)) : 50 * unit - multiply(live, 16 * unit);
        band[nb].x = x; band[nb].h = h; band[nb].w = 0; ++nb;
    }
    const u32 bkey = key_of(s[0], live, rv << 8, 28);
    spread(frame, comb, c.extra == none ? 2 : 1, band, nb, base, bkey, view::mid);
    place(frame, handle_a, fc, sy(0) + base - (50 * unit - multiply(live, 16 * unit)), unit, fill5, fill_and_stroke);
    if (c.extra == none) return;
    // The machine's own mark, on the fourth path, jumping with the hit.
    const i32 lift = 65536 + (static_cast<u32>(f) >> 16);                       // Q16: 1 .. 1.5
    Spike mk[10];
    u32 k = 0;
    if (c.extra == noise) {
        const u32 count = scaled(raw(s, c.a, 30), 24), height = raw(s, c.a, 30);
        for (u32 i = 0; i < count && k < 10; i += count > 10 ? 3 : 1, ++k) {
            mk[k].x = PX * unit + mul16(PW * unit, rnd(i + 5));
            mk[k].h = mul16(2 * unit + mul16(static_cast<i32>(height * 30 * unit / 127), rnd(i + 9)), static_cast<u32>(lift));
            mk[k].w = unit;
        }
        spikes(frame, extra, mk, k, base, key_of(s[c.a], f, 0, 24), dim);
    } else if (c.extra == click) {
        mk[0].x = (PX + 3) * unit; mk[0].h = mul16(6 * unit + static_cast<i32>(raw(s, c.a, 0) * 44 * unit / 127), static_cast<u32>(lift)); mk[0].w = unit;
        spikes(frame, extra, mk, 1, base, key_of(s[c.a], f, 0, 25), view::thin);
    } else if (c.extra == claps) {
        const u32 count = 2 + scaled(raw(s, c.a, 64), 5);
        const i32 step = 6 * unit + static_cast<i32>(raw(s, c.b, 50) * 8 * unit / 127);
        const u32 now = ph < 0 ? 99u : static_cast<u32>(ph) * count >> 10;     // the clap sounding
        for (u32 i = 0; i < count && k < 10; ++i, ++k) {
            mk[k].x = (PX + PW - 8) * unit - static_cast<i32>(count - i) * step + unit * 3 / 2;
            mk[k].h = i == now ? 15 * unit : 10 * unit; mk[k].w = unit;
        }
        spikes(frame, extra, mk, k, 108 * unit, key_of(s[c.a], s[c.b], 0, 26) ^ (now << 26), view::mid);
    } else if (c.extra == snare) {
        const i32 sx = PX * unit + mul16(PW * unit, 19661 + mul16(42598, static_cast<u32>(raw(s, c.b, 64)) * 516u));
        const i32 tall = mul16(static_cast<i32>(raw(s, c.a, 0) * 36 * unit / 127), static_cast<u32>(lift));
        if (tall < unit) return;                                                // no snare: no wires on the axis
        for (u32 i = 0; i < 6; ++i, ++k) {
            mk[k].x = sx + static_cast<i32>(i) * 3 * unit;
            mk[k].h = tall - multiply(tall, static_cast<i32>(i) * 257698038);   // 1 - 0.12 i
            mk[k].w = unit / 2;
        }
        spikes(frame, extra, mk, k, base, key_of(s[c.a], s[c.b], f, 27), view::thin);
    }
}

// pPi: the modes as lines from the axis, spaced by PTCH and SIZE, tallest for
// the lowest, steeper for soft HARD and a heavy HAMR, the off-axis modes
// quieter for POS; metal's upper modes follow RING. RVOL adds the rattle.
struct Pi { int ptch, size, hard, hamr, tens, pos, ring, rvol; bool metal; };
template <class F>
inline void pi(F& frame, const i32* s, const Pi& c, const view::Moment& m) {
    static const i32 plate[14] = {65536, 96338, 119931, 144835, 171704, 193987, 224133, 251003, 275907, 304742, 340787, 386662, 419430, 465306};
    static const i32 drum[10] = {65536, 104202, 140247, 150733, 173670, 191365, 207094, 229376, 235930, 266076};
    static const bool axis[10] = {true, false, false, true, false, false, false, false, true, false};
    const i32 ph = phase(m);
    const i32 h = ph >= 0 ? md::one - static_cast<i32>(static_cast<u32>(ph) << 21) : 0;   // the strike, fading
    const i32 base = (c.rvol >= 0 ? 94 : 140) * unit, tall = (c.rvol >= 0 ? 70 : 110) * unit;
    const i32 f0 = (PX + 10) * unit + static_cast<i32>(raw(s, c.ptch, 64) * 50 * unit / 127);
    // (22 or 46) (1.25 - SIZE / 2) px: twice 0.375 + (1 - SIZE) / 4.
    const i32 sc = multiply((c.metal ? 22 : 46) * unit, 0x30000000 + multiply(0x7fffffff - frac(s, c.size, 64), 0x20000000)) * 2;
    // Q16 decay per mode: 0.55 - 0.4 HARD + 0.35 HAMR.
    const i32 k = 36045 - mul16(26214, static_cast<u32>(raw(s, c.hard, 64)) * 516u) + (c.hamr >= 0 ? mul16(22938, static_cast<u32>(raw(s, c.hamr, 38)) * 516u) : 0);
    const i32 pos = frac(s, c.pos, 64), ring = frac(s, c.ring, 38), stretch = multiply(frac(s, c.tens, 0), multiply(h, 0x0a3d70a4));
    const i32* ratios = c.metal ? plate : drum;
    const u32 count = c.metal ? 14 : 10;
    Spike sp[14];
    u32 n = 0;
    for (u32 i = 0; i < count; ++i) {
        const i32 x = f0 + mul16(sc + multiply(sc, stretch), static_cast<u32>(ratios[i] - 65536));
        if (x > (PX + PW - 4) * unit) break;
        i32 a = md::exp_neg(static_cast<i32>(i) * (k > 0 ? k : 0));
        if (c.pos >= 0 && !c.metal && !axis[i]) a = multiply(a, 0x0ccccccd + multiply(pos, 0x73333333));
        if (c.metal && i > 2) a = multiply(a, 0x33333333 + multiply(ring, 0x4ccccccd));
        sp[n].x = x; sp[n].w = unit / 2;
        sp[n].h = multiply(tall, a) + multiply(multiply(tall, a), multiply(h, 0x26666666));
        ++n;
    }
    const u32 key = key_of(s[c.ptch < 0 ? 0 : c.ptch], k << 12, ring, 31) ^ (static_cast<u32>(h) >> 26);
    spread(frame, contour, 2, sp, n, base, key, line);
    if (n) place(frame, handle_a, sp[0].x, sy(0) + base - sp[0].h, unit, fill5, fill_and_stroke);   // the fundamental's node
    if (c.rvol >= 0) {
        // The rattle shakes with the hit.
        const u32 shake = ph < 0 ? 0u : static_cast<u32>(ph) >> 5;
        const u32 grains = scaled(raw(s, c.rvol, 64), 30), band = 2 + scaled(raw(s, c.rvol, 64), 24);
        Spike g[20];
        u32 count2 = 0;
        for (u32 i = 0; i < grains && count2 < 20; i += grains > 20 ? 2 : 1) {
            g[count2].x = PX * unit + mul16(PW * unit, rnd(i + 13));
            g[count2].h = static_cast<i32>(mul16(static_cast<i32>(band) * unit, rnd(i + 31 + shake * 5)));
            g[count2].w = unit;
            ++count2;
        }
        spread(frame, comb, 2, g, count2, 146 * unit, key_of(s[c.rvol], 0, 0, 33) + (ph < 0 ? 0u : (shake + 1) << 26), view::dim);
    }
}

// pE12: the sample as a wave, each class decaying as its own, dim before where
// STRT starts it; STRT's and BEND's node markers; BEND's curve above; and the
// machine's own control (RING, RATL, BELL, REAL, SLEW, BC) as a ripple, or
// E12-BD's SPLN as the length of sample it plays. A hit plays the sample: the
// part already played falls quiet, from STRT to the end, then all of it returns.
template <class F>
inline void e12(F& frame, const i32* s, i32 decay16, int extra_slot, int splen_slot, const view::Moment& m) {
    const i32 cy = 106, A = 32;
    const u32 st = raw(s, 4, 0);
    const i32 sx = PX * unit + static_cast<i32>(st * (PW * unit / 2) / 127);
    const u32 first = (st * 28 + 253) / 254;                                       // bars before s0 = STRT / 2 are pre
    const i32 ph = phase(m);
    const i32 played = ph < 0 ? 0 : sx + mul16((PX + PW) * unit - sx, static_cast<u32>(ph) << 6);
    // One point a bar, above and below the axis in turn, from the axis and back:
    // the skipped start on one path, the rest on another.
    for (u32 part = 0; part < 2; ++part) {
        const u32 from = part ? first : 0, to = part ? 28 : first;
        i32 xs[32], ys[32];
        u32 n = 0;
        xs[n] = (PX + static_cast<i32>(from) * 8) * unit; ys[n++] = cy * unit;
        for (u32 i = from; i < to; ++i, ++n) {
            const i32 a = multiply(md::exp_neg(static_cast<i32>(mul16(decay16, i * 2341u))),   // t = i / 28
                                   0x46666666 + multiply(0x39999999, static_cast<i32>(rnd(i + 11) << 15)));   // 0.55 + 0.45 RND
            const i32 x = (PX + static_cast<i32>(i) * 8 + 4) * unit;
            i32 h = multiply(A * unit, a);
            if (part && x < played) h = multiply(h, 0x2ccccccd);                    // played: 0.35
            xs[n] = x; ys[n] = cy * unit + (i & 1 ? h : -h);
        }
        xs[n] = (PX + static_cast<i32>(to) * 8) * unit; ys[n++] = cy * unit;
        if (to == from) { hide(frame, part ? wave : contour); continue; }
        while (n < 32) { xs[n] = xs[n - 1] + 1; ys[n] = ys[n - 1]; ++n; }
        const u32 at = played ? static_cast<u32>(played - PX * unit) >> 13 : 0;     // 8 px steps of the play position
        draw32(frame, part ? wave : contour, xs, ys, PX + PW / 2, cy, key_of(s[4], decay16 << 12, static_cast<i32>(at << 24), 41 + part), part ? view::mid : dim);
    }
    place(frame, handle_a, sx, sy(cy - A - 6), unit, fill5, fill_and_stroke);
    // BEND: 32 - (BEND - 0.5) 40 e^(-(x - PX) / 40).
    i32 xs[32], ys[32];
    const i32 bend = static_cast<i32>(raw(s, 7, 64)) - 64;                          // -64 .. 63
    for (u32 i = 0; i < 32; ++i) {
        xs[i] = PX * unit + static_cast<i32>(i) * (PW * unit / 31);
        const i32 e = md::exp_neg(static_cast<i32>(i) * 11835);                      // (PW / 31) / 40 a step
        ys[i] = 32 * unit - multiply(bend * 40 * unit / 127, e);
    }
    draw32(frame, comb, xs, ys, PX + PW / 2, 32, key_of(s[7], 0, 0, 42), view::mid);
    place(frame, handle_b, xs[0], sy(0) + ys[0], unit, fill5, fill_and_stroke);
    if (splen_slot >= 0) {
        // SPLN: a bar from STRT along the top of the sample, 4 + 30 SPLN long.
        const i32 len = 4 * unit + static_cast<i32>(raw(s, splen_slot, 38) * 30 * unit / 127), top = (cy - A) * unit;
        const i32 px[5] = {sx, sx + len, sx + len, sx, sx}, py[5] = {top, top, top + 4 * unit, top + 4 * unit, top};
        for (u32 i = 0; i < 32; ++i) { xs[i] = px[i < 5 ? i : 4] + (i >= 5 ? static_cast<i32>(i) : 0); ys[i] = py[i < 5 ? i : 4]; }
        draw32(frame, extra, xs, ys, PX + PW / 2, cy - A, key_of(s[splen_slot], s[4], 0, 44), view::thin);
    } else if (extra_slot >= 0) {
        const i32 ex = static_cast<i32>(raw(s, extra_slot, 38)) * 10 * unit / 127;
        for (u32 i = 0; i < 32; ++i) {
            xs[i] = sx + static_cast<i32>(i) * (((PX + PW) * unit - sx) / 31);
            const i32 e = md::exp_neg(static_cast<i32>(i) * 4228);                   // e^(-2 t)
            ys[i] = (cy - A / 2) * unit - multiply(multiply(ex, md::sine(i * 1680000000u)), e);
        }
        draw32(frame, extra, xs, ys, PX + PW / 2, cy - A / 2, key_of(s[extra_slot], s[4], 0, 43), view::thin);
    }
}

// pNoise: white noise as lines about the axis, moving through the hit.
template <class F>
inline void noise_view(F& frame, const view::Moment& m) {
    const i32 ph = phase(m);
    const u32 shift = ph > 0 ? static_cast<u32>(ph) * 50 / 1024 : 0;
    Spike sp[40];
    for (u32 i = 0; i < 40; ++i) {
        const i32 r = static_cast<i32>(rnd(i + shift)) - 32768;
        sp[i].x = PX * unit + static_cast<i32>(i) * (PW * unit / 40);
        sp[i].h = (r * 56 / 32) * unit / 1024;
        sp[i].w = unit / 2;
    }
    spread(frame, contour, 4, sp, 40, 80 * unit, 51 + shift * 4, view::mid);
}

// pImpulse: up to UVAL for UP, down to DVAL for DOWN, then back to the axis.
template <class F>
inline void impulse(F& frame, const i32* s) {
    const i32 cy = 80 * unit, x1 = (PX + 20) * unit;
    const i32 x2 = x1 + 8 * unit + static_cast<i32>(raw(s, 0, 64) * 90 * unit / 127);
    const i32 x3 = x2 + 8 * unit + static_cast<i32>(raw(s, 2, 64) * 90 * unit / 127);
    const i32 up = cy - static_cast<i32>(raw(s, 1, 127) * 60 * unit / 127), down = cy + static_cast<i32>(raw(s, 3, 0) * 60 * unit / 127);
    const i32 px[8] = {PX * unit, x1, x1, x2, x2, x3, x3, (PX + PW) * unit};
    const i32 py[8] = {cy, cy, up, up, down, down, cy, cy};
    i32 xs[32], ys[32];
    for (u32 i = 0; i < 32; ++i) { xs[i] = px[i < 8 ? i : 7] + (i >= 8 ? static_cast<i32>(i) : 0); ys[i] = py[i < 8 ? i : 7]; }
    draw32(frame, contour, xs, ys, PX + PW / 2, 80, key_of(s[0], s[1], s[2], s[3] >> 8 | 1), line);
    place(frame, handle_a, x2, sy(0) + up, unit, fill5, fill_and_stroke);
    place(frame, handle_b, x3, sy(0) + down, unit, fill5, fill_and_stroke);
}
}

// The machines' views: which slots feed the panels.
namespace gnd_sn { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_panel::voice(f, s, {0, 2, 3, -1, -1, -1}, m); } }
namespace gnd_ns { template <class F> inline void view(F& f, const i32*, const view::Moment& m) { kit_panel::noise_view(f, m); } }
namespace gnd_im { template <class F> inline void view(F& f, const i32* s, const view::Moment&) { kit_panel::impulse(f, s); } }

namespace efm_bd { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::efm(f, s, {4, 5, -1, -1, kit_panel::none, true}, m); } }
namespace efm_sd { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::efm(f, s, {4, 5, 2, 3, kit_panel::noise, true}, m); } }
namespace efm_xt { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::efm(f, s, {4, 5, 7, -1, kit_panel::click, true}, m); } }
namespace efm_cp { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::efm(f, s, {4, 5, 2, 3, kit_panel::claps, true}, m); trx_panel::clap(f, m); } }
namespace efm_rs { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::efm(f, s, {2, -1, 4, 5, kit_panel::snare, false}, m); } }
namespace efm_cb { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::efm(f, s, {4, 5, 2, -1, kit_panel::click, true}, m); } }
namespace efm_hh { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::efm(f, s, {4, 5, -1, -1, kit_panel::none, true}, m); } }
namespace efm_cy { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::efm(f, s, {4, 5, -1, -1, kit_panel::none, true}, m); } }

namespace pi_bd { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::pi(f, s, {0, -1, 2, 3, 4, -1, -1, -1, false}, m); } }
namespace pi_sd { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::pi(f, s, {0, -1, 2, -1, 4, -1, 3, 5, false}, m); } }
namespace pi_mt { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::pi(f, s, {0, 6, 2, 3, -1, 7, -1, -1, false}, m); } }
namespace pi_ml { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::pi(f, s, {0, -1, 2, -1, 3, -1, -1, -1, true}, m); } }
namespace pi_rs { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::pi(f, s, {0, -1, 2, -1, -1, -1, 3, 4, false}, m); } }
namespace pi_rc { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::pi(f, s, {0, -1, 2, -1, -1, -1, 3, -1, true}, m); } }
namespace pi_cc { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { pi_rc::view(f, s, m); } }
namespace pi_hh { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::pi(f, s, {0, -1, -1, -1, -1, -1, 3, -1, true}, m); } }
namespace pi_ma {
template <class F> inline void view(F& f, const i32* s, const view::Moment& m) {
    trx_panel::grains(f, s, -1, -1, 0, -1, m);
}
}

// E12: each class's decay along the strip (the design's kmap, Q16) and its own control.
namespace e12_bd { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 196608, -1, 3, m); } }
namespace e12_sd { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 262144, 3, -1, m); } }
namespace e12_ht { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 196608, -1, -1, m); } }
namespace e12_lt { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 196608, -1, -1, m); } }
namespace e12_cp { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 327680, -1, -1, m); trx_panel::clap(f, m); } }
namespace e12_rs { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 589824, 3, -1, m); } }
namespace e12_cb { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 262144, -1, -1, m); } }
namespace e12_ch { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 524288, -1, -1, m); } }
namespace e12_oh { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 163840, -1, -1, m); } }
namespace e12_rc { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 91750, 3, -1, m); } }
namespace e12_cc { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 91750, -1, -1, m); } }
namespace e12_br { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 262144, 3, -1, m); } }
namespace e12_ta { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 327680, -1, -1, m); } }
namespace e12_tr { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 262144, -1, -1, m); } }
namespace e12_sh { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 327680, 3, -1, m); } }
namespace e12_bc { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { kit_panel::e12(f, s, 196608, 3, -1, m); } }
}
