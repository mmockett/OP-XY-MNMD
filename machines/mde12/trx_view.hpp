#pragma once
// The drum kits' shared screen helpers: paths of 32 points, spikes along an
// axis, the hit's fade, and the clap instrument's lit hands. The machines'
// views are in mdkit_view.hpp.
#include "md_view.hpp"
#include "md_rig.hpp"

namespace md_mde12 {} namespace md = md_mde12; namespace md_mde12 {
namespace trx_panel {
using namespace md::view;

// A slot's value 0..1 (Q31), or `otherwise` when the machine has no such control.
inline i32 get(const i32* slots, int slot, i32 otherwise) { return slot < 0 ? otherwise : md::positive(slots[slot]); }
inline i32 q(i32 fraction_q31, i32 pixels) { return multiply(fraction_q31, pixels * unit); }   // Q10 pixels
const i32 half = 0x40000000;

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

// The clap instrument's hands light up as they clap (its only mid-shade parts,
// eight to a hand): one hand at a time, in turn, over the first half of the
// hit, so a frame changes eight or sixteen nodes, not all 32 at once.
template <class F>
inline void clap(F& frame, const view::Moment& m) {
    const i32 ph = m.phase(rig::drum.length, rig::drum.speed);
    if (ph < 0 || ph >= 512) return;
    const u32 hand = static_cast<u32>(ph) >> 7;   // 0 .. 3
    u32 seen = 0;
    for (u32 n = view::art_first; n < F::node_count; ++n)
        if (frame.nodes[n].shade == view::mid && frame.nodes[n].x < opxy::draw::hidden_x * opxy::draw::unit) {
            if (seen >> 3 == hand) opxy::draw::shade(frame, n, view::line, opxy::draw::stroke);
            ++seen;
        }
}

// 2^16 / n, n = 0..40: the panels' counts without runtime division.
static const opxy::u16 over[41] = {0, 65535, 32768, 21845, 16384, 13107, 10923, 9362, 8192, 7282, 6554, 5958, 5461, 5041,
    4681, 4369, 4096, 3855, 3641, 3449, 3277, 3121, 2979, 2849, 2731, 2621, 2521, 2427, 2341, 2260, 2185, 2114, 2048,
    1986, 1928, 1872, 1820, 1771, 1725, 1680, 1638};
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
// Scattered grains as one outline: left to right, so it never doubles back to
// close beside its start, and each at least a pixel tall, so none is a flat
// out-and-back (both of which the device draws as slivers and streaks).
inline void settle(Spike* g, u32 n) {
    for (u32 i = 1; i < n; ++i)
        for (u32 j = i; j > 0 && g[j].x < g[j - 1].x; --j) { const Spike t = g[j]; g[j] = g[j - 1]; g[j - 1] = t; }
    for (u32 i = 0; i < n; ++i)
        if (g[i].h > -unit && g[i].h < unit) g[i].h = g[i].h < 0 ? -unit : unit;
}
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

}
}
