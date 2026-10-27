#pragma once
// TRX's panels and the TRX machines' views, after the design's pTrxVoice,
// pTrxHat and pTrxRoom (docs/design/Main.dc.html). The instrument on the left
// and the panels' fixed parts come from the rig (md_rig.hpp); these draw what
// changes shape: the pitch contour, the wave, the partials and the clap train.
#include "md_view.hpp"
#include "md_rig.hpp"

namespace md_mdefm {} namespace md = md_mdefm; namespace md_mdefm {
namespace trx_panel {
using namespace md::view;

// A slot's value 0..1 (Q31), or `otherwise` when the machine has no such control.
inline i32 get(const i32* slots, int slot, i32 otherwise) { return slot < 0 ? otherwise : md::positive(slots[slot]); }
inline i32 q(i32 fraction_q31, i32 pixels) { return multiply(fraction_q31, pixels * unit); }   // Q10 pixels
const i32 half = 0x40000000;

// 2^16 / (31 (0.03 + 0.4 r / 127)), r = 0..127: the design's 1 / tau for the sweep,
// per step of the contour's 31 points.
static const i32 step_over_tau[128] = {
    70469, 63773, 58240, 53590, 49628, 46211, 43234, 40618, 38300, 36233, 34377, 32702,
    31183, 29799, 28532, 27369, 26296, 25305, 24386, 23531, 22734, 21989, 21292, 20637,
    20021, 19441, 18894, 18377, 17887, 17423, 16982, 16563, 16164, 15784, 15421, 15075,
    14744, 14427, 14123, 13832, 13553, 13285, 13027, 12779, 12540, 12310, 12089, 11875,
    11668, 11469, 11276, 11090, 10910, 10735, 10566, 10402, 10244, 10090, 9940, 9795,
    9654, 9517, 9384, 9255, 9129, 9007, 8887, 8771, 8658, 8548, 8440, 8335,
    8233, 8133, 8036, 7941, 7848, 7757, 7669, 7582, 7498, 7415, 7334, 7254,
    7177, 7101, 7027, 6954, 6882, 6813, 6744, 6677, 6611, 6547, 6484, 6422,
    6361, 6301, 6242, 6185, 6128, 6073, 6019, 5965, 5912, 5861, 5810, 5760,
    5711, 5663, 5616, 5569, 5523, 5478, 5434, 5390, 5347, 5305, 5263, 5222,
    5182, 5142, 5103, 5065, 5027, 4990, 4953, 4916,
};

// The design's RND(i), as a fixed table: a noisy wave that stays still.
static const i32 noise[32] = {
    1203450987, -804712311, 431876543, -1876543210, 987654321, -123456789, 1678901234, -1543210987,
    345678901, -987123456, 1456789012, -345612789, 765432109, -1987654321, 234567890, -654321098,
    1890123456, -1098765432, 567890123, -876543210, 1345678901, -234567891, 912345678, -1678901235,
    123456789, -1456789013, 1789012345, -567890124, 678901234, -1234567890, 1567890123, -789012345,
};

// Writes a polyline of 32 points into a path, about its origin (ox, oy), at scale 2.
template <class F>
inline void draw32(F& frame, u32 path, const i32* xs, const i32* ys, i32 ox, i32 oy, u32 key, u32 tone) {
    typename F::path_type& p = frame.paths[path];
    if (p.key != key) {
        Point points[32];
        for (u32 i = 0; i < 32; ++i) { points[i].x = (xs[i] - ox * unit) / 2; points[i].y = (ys[i] - oy * unit) / 2; }
        polyline(p, points, 32, key);
    }
    show_path(frame, path, ox, oy, tone, 2 * unit);
}

// How much of a hit is left: Q31 1 as it lands, falling to 0 by the animation's
// end, 0 at rest. Every view animates its own drawing by it; none adds a mark.
inline i32 fade(const view::Moment& m, rig::Timing timing = rig::drum) {
    const i32 ph = m.phase(timing.length, timing.speed);
    return ph >= 0 ? md::one - ph * 2097152 : 0;
}

// pTrxVoice: the pitch falling from the sweep's top to PTCH's level, its two
// node markers, then the wave, its harmonics and noise. Slots of the controls
// that feed it, or -1. A hit plays the wave: it swells, and with a RAMP its
// cycles crowd at the top of the sweep and spread as the pitch falls along the
// contour, back to the drawing at rest.
struct Voice { int ptch, ramp, rdec, harm, tone, nois; };

template <class F>
inline void voice(F& frame, const i32* s, const Voice& c, const view::Moment& m) {
    const i32 p = get(s, c.ptch, half), ramp = get(s, c.ramp, 0), harm = get(s, c.harm, 0);
    const i32 tone = get(s, c.tone, half) - half, nz = get(s, c.nois, 0);
    const u32 r = static_cast<u32>(get(s, c.rdec, 0x26666666)) >> 24;    // 0.3 when absent
    // Contour: x 248..472; y = max(10, lvl - ramp (lvl - 12) e^(-t / tau)), lvl = 86 - 60 p.
    i32 xs[32], ys[32];
    const i32 lvl = 86 * unit - q(p, 60);
    for (u32 i = 0; i < 32; ++i) {
        xs[i] = panel_x * unit + static_cast<i32>(i) * (panel_w * unit / 31);   // i / 31 of the width
        const i32 e = md::exp_neg(step_over_tau[r] * static_cast<i32>(i));
        const i32 y = lvl - multiply(multiply(ramp, e), lvl - 12 * unit);
        ys[i] = y < 10 * unit ? 10 * unit : y;
    }
    draw32(frame, contour, xs, ys, 360, 48, key_of(s[c.ptch < 0 ? 0 : c.ptch], ramp, static_cast<i32>(r) << 24, 1), line);
    place(frame, handle_a, xs[0], sy(0) + ys[0], unit, fill5, fill_and_stroke);
    place(frame, handle_b, (panel_x + panel_w) * unit, sy(0) + lvl, unit, fill5, fill_and_stroke);
    // The hit: t through the animation along the contour's width, the pitch's
    // rise above its level there (Q8, up to 3 times the cycles) and the swell.
    const i32 ph = m.phase(rig::drum.length, rig::drum.speed), f = fade(m);
    const i32 rise = ph < 0 ? 0 : multiply(multiply(ramp, f), md::exp_neg(static_cast<i32>((static_cast<u32>(ph * 31) >> 5) * static_cast<u32>(step_over_tau[r]) >> 5)));
    const u32 cycles = 256u + (static_cast<u32>(rise) >> 22);                  // Q8: 1 .. 3
    const i32 amp = 35 * unit + multiply(f, 9 * unit);                         // a quarter more as it lands
    // Wave: three cycles over the width, on two paths of 32 points each.
    for (u32 side = 0; side < 2; ++side) {
        for (u32 i = 0; i < 32; ++i) {
            const u32 k = side * 31 + i;                                     // 0 .. 62 along the width
            xs[i] = panel_x * unit + static_cast<i32>(k) * (panel_w * unit / 62);   // k / 62
            const u32 u = static_cast<u32>(k) * (811800u * cycles);          // 3 cycles over 62 steps, at rest
            i32 v = opxy::q31::half(md::sine(u));
            v += multiply(multiply(harm, 0x1ccccccd), md::sine(u * 3));      // harm 0.45, at half
            v += multiply(multiply(tone, 0x4ccccccd), md::sine(u * 2));      // (tone - 0.5) 0.6, at half
            v += multiply(multiply(nz, 0x20000000), noise[k & 31]);          // noise 0.5, at half
            v = md::clamp(v, -0x53333333, 0x53333333);                       // +-1.3, at half
            ys[i] = 128 * unit - multiply(v, amp);                           // 22 px x 0.8, at half: 35
        }
        draw32(frame, side ? extra : wave, xs, ys, side ? 416 : 304, 128,
               key_of(harm, tone + half, nz, 2 + side) ^ (cycles << 9) ^ (static_cast<u32>(f) >> 26 << 18), line);
    }
}

// pTrxHat: six metal partials, as tall as MTAL makes them, on one comb.
template <class F>
inline void hat(F& frame, const i32* s, int mtal, int gap, i32 struck) {
    static const i32 at[6] = {268, 309, 336, 368, 406, 456};   // 248 + 20 + (r - 1) 120, r = 1, 1.34 ...
    const i32 mt = get(s, mtal, half);
    const i32 tall = 30 * unit + q(mt, 70);
    i32 xs[32], ys[32];
    u32 n = 0;
    for (u32 i = 0; i < 6; ++i) {
        const i32 h = multiply(tall + multiply(tall, multiply(struck, 0x26666666)), md::one - static_cast<i32>(i) * 0x07ae147b);
        xs[n] = at[i] * unit; ys[n++] = 120 * unit;
        xs[n] = at[i] * unit; ys[n++] = 120 * unit - h;
        xs[n] = at[i] * unit + 1; ys[n++] = 120 * unit;
    }
    while (n < 32) { xs[n] = xs[n - 1] + 1; ys[n] = ys[n - 1]; ++n; }
    draw32(frame, comb, xs, ys, 360, 80, key_of(mt, struck, 3, 3), line);
    // The design's "gap NN": the word is art (tools/art/build.mjs), the number the readout.
    if (gap >= 0) number(frame, panel_x + 21, 127, static_cast<u32>(md::positive(s[gap])) >> 24);
}

// pTrxRoom: the clap's hands as a train of spikes, RATE apart, with the echo of
// RICH. A hit claps along the train: each hand's spike jumps in its turn.
template <class F>
inline void room(F& frame, const i32* s, int rate, int tone, int hard, int rich, u32 hands, const view::Moment& m) {
    const i32 rt = get(s, rate, half), tn = get(s, tone, half), hd = get(s, hard, half), rc = get(s, rich, 0x33333333);
    const i32 spacing = 10 * unit + q(rt, 30);
    i32 width = 3 * unit + q(md::one - hd, 8);
    if (width > spacing - unit) width = spacing - unit;                     // hands side by side, never over each other
    const i32 ph = m.phase(rig::drum.length, rig::drum.speed);
    const u32 at = ph < 0 ? 99u : static_cast<u32>(ph) * hands >> 10;        // the hand clapping now
    i32 xs[32], ys[32];
    u32 n = 0;
    for (u32 i = 0; i < hands && n + 3 <= 32; ++i) {
        const i32 x = (panel_x + 10) * unit + static_cast<i32>(i) * spacing;
        const i32 h = multiply((100 - static_cast<i32>(i) * 8) * unit, half + opxy::q31::half(tn)) * (i == at ? 5 : 4) / 4;
        xs[n] = x; ys[n++] = 130 * unit;
        xs[n] = x + unit * 3 / 2; ys[n++] = 130 * unit - h;
        xs[n] = x + width; ys[n++] = 130 * unit;
    }
    while (n < 32) { xs[n] = xs[n - 1] + 1; ys[n] = ys[n - 1]; ++n; }
    draw32(frame, comb, xs, ys, 360, 80, key_of(rt, tn, hd, 4) ^ (at << 26), line);
    // The echo, when RICH is above a fifth.
    if (rc > 0x1999999a) {
        n = 0;
        for (u32 i = 0; i < hands && n + 3 <= 32; ++i) {
            const i32 x = (panel_x + 10) * unit + static_cast<i32>(i) * spacing + multiply(width, 0x4ccccccd);
            const i32 h = multiply(multiply((100 - static_cast<i32>(i) * 8) * unit, half + opxy::q31::half(tn)), multiply(rc, 0x4ccccccd));
            xs[n] = x; ys[n++] = 130 * unit;
            xs[n] = x + unit * 3 / 2; ys[n++] = 130 * unit - h;
            xs[n] = x + width; ys[n++] = 130 * unit;
        }
        while (n < 32) { xs[n] = xs[n - 1] + 1; ys[n] = ys[n - 1]; ++n; }
        draw32(frame, extra, xs, ys, 360, 80, key_of(rt, tn, rc, 5), dim);
    }
}

// The clap instrument's hands light up as they clap (its only mid-shade parts).
template <class F>
inline void clap(F& frame, const view::Moment& m) {
    if (fade(m) > 0x40000000) view::restyle(frame, view::mid, view::line);
}

// 2^16 / n, n = 0..40, and (i / 64)^0.8 in Q16: the panels' counts and the
// cymbal's spacing without runtime division.
static const opxy::u16 over[41] = {0, 65535, 32768, 21845, 16384, 13107, 10923, 9362, 8192, 7282, 6554, 5958, 5461, 5041,
    4681, 4369, 4096, 3855, 3641, 3449, 3277, 3121, 2979, 2849, 2731, 2621, 2521, 2427, 2341, 2260, 2185, 2114, 2048,
    1986, 1928, 1872, 1820, 1771, 1725, 1680, 1638};
static const opxy::u16 pow08[65] = {0, 2353, 4096, 5665, 7132, 8525, 9864, 11159, 12417, 13644, 14843, 16020, 17174, 18310,
    19428, 20531, 21619, 22693, 23755, 24805, 25844, 26873, 27892, 28901, 29902, 30895, 31880, 32857, 33827, 34790,
    35746, 36697, 37641, 38579, 39511, 40438, 41360, 42276, 43188, 44095, 44997, 45895, 46788, 47677, 48562, 49443,
    50320, 51193, 52063, 52929, 53791, 54650, 55506, 56358, 57207, 58053, 58896, 59736, 60573, 61407, 62238, 63067,
    63892, 64716, 65535};

// A control 0..127, and a count 0..127 * n / 127 rounded.
inline u32 raw(const i32* s, int slot, u32 otherwise) { return slot < 0 ? otherwise : static_cast<u32>(md::positive(s[slot])) >> 24; }
inline u32 scaled(u32 v, u32 n) { return (v * n * 516u + 32768u) >> 16; }
// A fixed pseudo-random 0..65535, the design's RND.
inline u32 rnd(u32 i) { return static_cast<u32>(noise[i & 31]) >> 16; }
// (a * b) >> 16 for a up to 2^20 and b up to 2^17, in unsigned arithmetic.
inline i32 mul16(i32 a, u32 b) { return static_cast<i32>(((static_cast<u32>(a) >> 4) * (b >> 4)) >> 8); }

// Spikes along an axis at y = base (Q10): up to ten per path, each from the axis
// to its peak (h above the axis, Q10, negative below) and back.
struct Spike { i32 x, h, w; };
template <class F>
inline void spikes(F& frame, u32 path, const Spike* sp, u32 n, i32 base, u32 key, u32 tone) {
    if (!n) return;
    i32 xs[32], ys[32];
    u32 k = 0;
    for (u32 i = 0; i < n && k + 3 <= 32; ++i) {
        xs[k] = sp[i].x - sp[i].w; ys[k++] = base;
        xs[k] = sp[i].x; ys[k++] = base - sp[i].h;
        xs[k] = sp[i].x + sp[i].w; ys[k++] = base;
    }
    while (k < 32) { xs[k] = xs[k - 1] + 1; ys[k] = ys[k - 1]; ++k; }
    draw32(frame, path, xs, ys, 360, 80, key, tone);
}

// pTrxCym: 8 + 18 RICH partials, the upper half lifted by TOP and moved by TTUN,
// spread by SIZE, narrowed by PEAK.
template <class F>
inline void cymbal(F& frame, const i32* s, i32 struck) {
    const u32 rich = raw(s, 0, 64), top = raw(s, 2, 64), tt = raw(s, 3, 64), size = raw(s, 4, 64), peak = raw(s, 5, 64);
    const u32 n = 8 + scaled(rich, 18);
    const i32 base = 140 * unit, w = unit + static_cast<i32>(scaled(127 - peak, 5 * unit));
    const u32 spread = 45875 + scaled(size, 19661), lift = 19661 + scaled(top, 58982);
    // In the keys' sixteen steps, so a path's kept shape is always its key's.
    const u32 bloom_step = static_cast<u32>(struck) >> 27, bloom = 65536 + (bloom_step << 9);
    const i32 shift = static_cast<i32>(scaled(tt, 20 * unit)) - 10 * unit;
    Spike lo[13], hi[13];
    u32 nl = 0, nh = 0;
    for (u32 i = 0; i < n; ++i) {
        const u32 t = i * over[n];                                           // Q16
        const bool upper = t > 32768;
        i32 x = (panel_x + 6) * unit + mul16(mul16(217907, pow08[t >> 10]), spread);
        if (upper) x += shift;
        if (x > (panel_x + panel_w) * unit) continue;
        i32 h = mul16(mul16(mul16(112640, 65536 - ((t * 39322u) >> 16)), upper ? lift : 65536), 39322 + ((rnd(i) * 26214u) >> 16));
        h = mul16(h, bloom);
        Spike& sp = upper ? hi[nh++] : lo[nl++];
        sp.x = x; sp.h = h; sp.w = w;
    }
    const u32 key = key_of(s[0], s[4], s[5], 6) ^ bloom_step;
    const u32 lo_first = nl < 10 ? nl : (nl + 1) / 2, hi_first = nh < 10 ? nh : (nh + 1) / 2;
    spikes(frame, wave, lo, lo_first, base, key, view::mid);
    spikes(frame, comb, lo + lo_first, nl - lo_first, base, key + 1, view::mid);
    spikes(frame, contour, hi, hi_first, base, key_of(s[2], s[3], s[4], 7) ^ bloom_step, line);
    spikes(frame, extra, hi + hi_first, nh - hi_first, base, key_of(s[2], s[3], s[5], 8) ^ bloom_step, line);
}

// pGrains: 10 + 30 RATL grains about the axis, fading out (or in, with REV),
// thinned by DAMP, scattered by RTYP; the arrow shows the direction. Each grain
// is a spike from the axis to its place, as the device's paths are one line.
// A hit shakes them: each grain jumps about its place, settling as it fades.
template <class F>
inline void grains(F& frame, const i32* s, int rev_slot, int damp_slot, int rate_slot, int type_slot, const view::Moment& m) {
    const i32 ph = m.phase(rig::shaker.length, rig::shaker.speed), f = fade(m, rig::shaker);
    const u32 shake = ph < 0 ? 0u : static_cast<u32>(ph) >> 5;               // a new shake every 35 ms
    const bool rev = raw(s, rev_slot, 0) > 63, scatter = raw(s, type_slot, 0) > 63;
    const u32 damp = raw(s, damp_slot, 0), n = 10 + scaled(raw(s, rate_slot, 64), 30);
    const i32 cy = 80 * unit;
    Spike g[30];
    u32 count = 0;
    for (u32 i = 0; i < n && count < 30; ++i) {
        if (rnd(i + 77) < damp * 413) continue;                             // DAMP 0.8
        const u32 t = scatter ? rnd(i + 3) : i * over[n];
        const u32 e = rev ? 16384 + ((t * 49152u) >> 16) : 65536 - ((t * 49152u) >> 16);
        const u32 r = rnd(i + 400);
        const i32 mag = mul16(static_cast<i32>(((r >= 32768 ? r - 32768 : 32768 - r) * 57344u) >> 15), e);
        g[count].x = panel_x * unit + mul16(panel_w * unit, t);
        const i32 jolt = ph < 0 ? 0 : multiply(f, (static_cast<i32>(rnd(i + shake * 7)) - 32768) * 3 / 8);   // +-12 px at the landing
        g[count].h = (r >= 32768 ? mag : -mag) + jolt;
        g[count].w = 2 * unit;
        ++count;
    }
    const u32 key = key_of(static_cast<i32>(raw(s, rate_slot, 64) << 24), static_cast<i32>(damp << 24),
                           scatter ? 0x7f000000 : 0, 9) + (rev ? 1u : 0u) + (ph < 0 ? 0u : (shake + 1) << 26);
    for (u32 p = 0; p < 3; ++p) {
        const u32 first = p * 10, size = count > first ? (count - first < 10 ? count - first : 10) : 0;
        spikes(frame, p == 0 ? contour : p == 1 ? wave : comb, g + first, size, cy, key + p * 7, view::thin);
    }
    // The direction: a line with its arrowhead at the end the grains grow towards.
    const i32 mx = (panel_x + panel_w / 2) * unit, y = (80 + 66) * unit, s1 = rev ? -1 : 1;
    i32 xs[32], ys[32];
    const i32 tip = mx + s1 * 26 * unit, back = mx + s1 * 20 * unit;
    const i32 px[6] = {mx - s1 * 20 * unit, tip, back, back, tip, tip};
    const i32 py[6] = {y, y, y - 4 * unit, y + 4 * unit, y, y};
    for (u32 i = 0; i < 32; ++i) { xs[i] = px[i < 6 ? i : 5] + (i >= 6 ? static_cast<i32>(i) : 0); ys[i] = py[i < 6 ? i : 5]; }
    draw32(frame, extra, xs, ys, 360, 146, rev ? 11 : 12, view::mid);
}
}

// The machines' views: which slots feed the panels (the design's pTrx* and
// pGrains, by the names each machine has), animated by the hit.
namespace trx_bd { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_panel::voice(f, s, {0, 2, 3, 6, -1, 5}, m); } }
namespace trx_sd { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_panel::voice(f, s, {0, 2, 3, -1, 5, 4}, m); } }
namespace trx_xt { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_panel::voice(f, s, {0, 2, 3, -1, -1, -1}, m); } }
namespace trx_xc { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_xt::view(f, s, m); } }
namespace trx_rs { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_panel::voice(f, s, {0, -1, -1, -1, -1, -1}, m); } }
namespace trx_cb { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_panel::voice(f, s, {0, 5, -1, 2, 4, -1}, m); } }
namespace trx_cl { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_panel::voice(f, s, {0, -1, -1, 3, 4, -1}, m); } }
namespace trx_b2 { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_panel::voice(f, s, {0, 2, -1, -1, -1, 5}, m); } }
namespace trx_cp {
template <class F> inline void view(F& f, const i32* s, const view::Moment& m) {
    const u32 hands = 1 + trx_panel::scaled(trx_panel::raw(s, 0, 64), 5);    // the design's max(2, 1 + CLPY 5)
    trx_panel::room(f, s, 4, 1, 2, 3, hands < 2 ? 2 : hands, m);
    trx_panel::clap(f, m);
}
}
namespace trx_ch {
template <class F> inline void view(F& f, const i32* s, const view::Moment& m) {
    trx_panel::hat(f, s, 4, -1, trx_panel::fade(m));   // the partials jump, then settle
}
}
namespace trx_oh { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { trx_ch::view(f, s, m); } }
namespace trx_cy {
template <class F> inline void view(F& f, const i32* s, const view::Moment& m) {
    trx_panel::cymbal(f, s, trx_panel::fade(m));
}
}
namespace trx_ma {
template <class F> inline void view(F& f, const i32* s, const view::Moment& m) {
    trx_panel::grains(f, s, 2, 3, 4, 5, m);
}
}
}
