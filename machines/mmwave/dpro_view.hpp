#pragma once
// The DigiPRO machines' views, after the design's mnmView (docs/design/Main.dc.html):
// DPRO-WAVE's iso wavetable, DDRW's two waves and mix fader, DENS's chord
// keyboard and BBOX's sample strip over its key map. The fixed parts (plates,
// fader, black keys, key map, labels) come from the rig (md_rig.hpp); these draw
// what changes shape on the four paths, from the controls and the captured waves
// themselves (private_dpro.hpp: flat lines without it). voices/trx_view.hpp is
// the worked example.
#include "md_view.hpp"
#include "md_rig.hpp"
#include "dpro.hpp"
#include "chord_view.hpp"

namespace md_mmwave {} namespace md = md_mmwave; namespace md_mmwave {
namespace dpro_view {
using namespace md::view;
using opxy::i16;

// 2^32 / 31: one turn of phase over a 32-point row.
const u32 turn_per_point = 138547332u;

// Point i of n + 1 across x0..x1 (stage px), Q10, without a divide: i / 31 as i x 2^20 / 31.
inline i32 across(i32 x0, i32 x1, u32 i) {
    return x0 * unit + (((x1 - x0) * unit) * static_cast<i32>((i * 33825u) >> 10) >> 10);
}

// A table's sample at a phase, Q15 at its peak (norm from private_dpro.hpp).
inline i32 level(const i16* table, i32 norm, u32 phase) {
    return md::clamp(multiply(table[phase >> dpro::index_shift] * 65536, norm), -32767, 32767);
}

// 32 points of a wave row: across x0..x1 at height y, `amplitude` px at the table's
// peak, `step` of phase a point, its samples' bits cut to `keep` (DDRW's BR).
inline void row(Point* out, const i16* table, i32 norm, i32 x0, i32 x1, i32 y, i32 amplitude, u32 step,
                u32 keep = 0xffffffffu, u32 phase = 0) {
    for (u32 i = 0; i < 32; ++i) {
        const i32 v = static_cast<i32>(static_cast<u32>(level(table, norm, phase) * 65536) & keep) / 65536;
        out[i].x = across(x0, x1, i);
        out[i].y = y * unit - v * amplitude / 32;   // v Q15 x amplitude px, Q10
        phase += step;
    }
}

// Shows 64 points (stage Q10) as path `path` about (ox, oy) at scale 2: the
// device's paths reach +-64 px, so +-128 at scale 2. Rebuilt only when `key` changes.
template <class F>
inline void show(F& frame, u32 path, const Point* points, i32 ox, i32 oy, u32 key, u32 tone, Paint paint = stroke) {
    typename F::path_type& p = frame.paths[path];
    if (p.key != key) {
        Point local[64];
        for (u32 i = 0; i < 64; ++i) {   // within reach, leaving room for outline's eighth of a pixel
            local[i].x = md::clamp((points[i].x - ox * unit) / 2, -63 * unit, 63 * unit);
            local[i].y = md::clamp((points[i].y - oy * unit) / 2, -63 * unit, 63 * unit);
        }
        outline(p, local, 64, key);
    }
    place(frame, path, sx(ox), sy(oy), 2 * unit, tone, paint);
}

// One row out and back (a line), or two rows joined at their ends (a pair).
template <class F>
inline void single(F& frame, u32 path, const Point* row32, i32 ox, i32 oy, u32 key, u32 tone) {
    Point p[64];
    for (u32 i = 0; i < 32; ++i) { p[i] = row32[i]; p[63 - i].x = row32[i].x; p[63 - i].y = row32[i].y + 1; }
    show(frame, path, p, ox, oy, key, tone);
}
template <class F>
inline void pair(F& frame, u32 path, const Point* a, const Point* b, i32 ox, i32 oy, u32 key, u32 tone) {
    Point p[64];
    for (u32 i = 0; i < 32; ++i) { p[i] = a[i]; p[63 - i] = b[i]; }
    show(frame, path, p, ox, oy, key, tone);
}

inline u32 key_of_slots(const i32* s, u32 a, u32 b, u32 c, u32 salt) {
    return md::view::key_of(s[a], s[b], s[c], static_cast<i32>(salt << 25));
}
}

// DPRO-WAVE: five plates in iso, the set and the four after it, each with its
// wave; the played wave slides from the front plate to the next as WP goes from
// 0 to 127 (where it is the next set). The header names the set.
namespace dpro_wave {
template <class F>
inline void view(F& frame, const i32* s, const view::Moment&) {
    using namespace dpro_view;
    const u32 set = dpro::step(s[0]) >> 2;
    const i32 wp = md::positive(s[1]);
    const u32 two = 2 * turn_per_point;                      // the table: two cycles a row
    restyle(frame, thin, dim);                               // the plates, behind the waves
    // The plates' waves, front to back: x0 = 40 + 18 k, 236 px across a 300 px plate, y = 118 - 16 k.
    Point rows[5][32];
    for (u32 k = 0; k < 5; ++k) {
        const u32 w = (set + k) % dpro_data::s_waves;
        const i32 x0 = 72 + 18 * static_cast<i32>(k);
        row(rows[k], dpro_data::s_table[w][0], dpro_data::s_norm[w], x0, x0 + 236, 118 - 16 * static_cast<i32>(k), 22, two);
    }
    const u32 base = set + 1;
    single(frame, contour, rows[0], 190, 118, key_of_slots(s, 0, 0, 0, 1) + base, mid);
    pair(frame, comb, rows[1], rows[2], 217, 94, key_of_slots(s, 0, 0, 0, 2) + base, dim);
    pair(frame, extra, rows[3], rows[4], 253, 62, key_of_slots(s, 0, 0, 0, 3) + base, dim);
    // The played wave: the set crossfaded along WP, moved with it towards plate 1.
    Point cur[32], next[32];
    const u32 step_index = static_cast<u32>(md::clamp(multiply(wp, 5 * 65536) >> 16, 0, 4));
    const u32 own = set % dpro_data::s_waves;
    const i16* a = dpro_data::s_table[own][step_index];
    const i16* b = step_index + 1 < dpro_data::s_steps ? dpro_data::s_table[own][step_index + 1]
                 : own + 1 < dpro_data::s_waves ? dpro_data::s_table[own + 1][0] : dpro_data::s_end;
    const i32 dx = multiply(wp, 18 * unit), dy = multiply(wp, 16 * unit);
    row(cur, a, dpro_data::s_norm[own], 72, 308, 118, 22, two);
    row(next, b, dpro_data::s_norm[own], 72, 308, 118, 22, two);
    const i32 fade = static_cast<i32>((static_cast<u32>(wp) * 5u) & 0x7fffffffu);
    for (u32 i = 0; i < 32; ++i) {
        cur[i].y += multiply(next[i].y - cur[i].y, fade) - dy;
        cur[i].x += dx;
    }
    single(frame, wave, cur, 190 + (dx >> 10), 118 - (dy >> 10), key_of_slots(s, 0, 1, 1, 4), line);
}
}

// DPRO-DDRW: WAV1 above, WAV2 below, each cut to its bits, the fader between
// (rigged), and their mix on the right.
namespace dpro_ddrw {
template <class F>
inline void view(F& frame, const i32* s, const view::Moment&) {
    using namespace dpro_view;
    const u32 one = dpro::d_index(s[0]), two = dpro::d_index(s[2]);
    const u32 w1 = one < dpro_data::d_waves ? one : 0, w2 = two < dpro_data::d_waves ? two : 0;
    const u32 bits1 = 16 - ((dpro::step(s[4]) * 15 + 64) >> 7), bits2 = 16 - ((dpro::step(s[6]) * 15 + 64) >> 7);
    const u32 keep1 = 0xffffffffu << (32 - bits1), keep2 = 0xffffffffu << (32 - bits2);
    const u32 cycles = 2 * turn_per_point;
    Point a[32], b[32], m[32], a2[32], b2[32];
    row(a, dpro_data::d_table[w1], dpro_data::d_norm[w1], 44, 296, 40, 26, cycles, keep1);
    row(b, dpro_data::d_table[w2], dpro_data::d_norm[w2], 44, 296, 120, 26, cycles, keep2);
    // Each wave bright while it is most of the mix, as the design's levels.
    single(frame, contour, a, 170, 40, key_of_slots(s, 0, 4, 4, 1), md::positive(s[1]) < 0x55555555 ? mid : dim);
    single(frame, wave, b, 170, 120, key_of_slots(s, 2, 6, 6, 2), md::positive(s[1]) > 0x2aaaaaaa ? mid : dim);
    // The mix: 360..472, at 80, 30 px.
    row(a2, dpro_data::d_table[w1], dpro_data::d_norm[w1], 360, 472, 80, 30, cycles, keep1);
    row(b2, dpro_data::d_table[w2], dpro_data::d_norm[w2], 360, 472, 80, 30, cycles, keep2);
    const i32 mix = md::positive(s[1]);
    for (u32 i = 0; i < 32; ++i) { m[i] = a2[i]; m[i].y += multiply(b2[i].y - a2[i].y, mix); }
    single(frame, comb, m, 416, 80, key_of_slots(s, 0, 1, 2, 3) ^ key_of_slots(s, 4, 6, 6, 5), line);
}
}

// DPRO-DENS: the chord on the keyboard (chord_view.hpp), and the wave, with
// its chorus copy above, dimmer, when CHRL is up.
namespace dpro_dens {
template <class F>
inline void view(F& frame, const i32* s, const view::Moment& m) {
    using namespace dpro_view;
    // The chord: the note and up to three intervals, in semitones (just ratios to the nearest), as dpro_dens,
    // from C at 12 (the middle of the keyboard's two octaves).
    static const signed char below[16] = {-12, -11, -10, -9, -8, -7, -7, -6, -5, -5, -4, -4, -3, -3, -2, -1};
    static const signed char above[16] = {1, 2, 3, 3, 4, 4, 5, 5, 6, 7, 7, 8, 9, 10, 11, 12};
    i32 keys[4] = {12, 0, 0, 0};
    u32 voices = 1;
    for (u32 v = 1; v < 4; ++v) {
        const u32 pch = dpro::step(s[v - 1]);
        if (pch == 62 || pch == 63) continue;   // off
        keys[voices++] = 12 + (pch < 62 ? below[(pch + 2) >> 2] : above[(pch - 64) >> 2]);
    }
    chord_view::keyboard(frame, contour, keys, voices);
    // The wave: 258..472 (214 px), at 78, 34 px, three cycles; the chorus copy 14 px above, CHRL deep.
    const u32 w = dpro::d_index(s[3]) < dpro_data::d_waves ? dpro::d_index(s[3]) : 0;
    Point a[32], b[32];
    row(a, dpro_data::d_table[w], dpro_data::d_norm[w], 258, 472, 78, 34, 3 * turn_per_point);
    single(frame, wave, a, 365, 78, key_of_slots(s, 3, 3, 3, 7), line);
    const i32 chrl = md::positive(s[5]);
    if (chrl > 0x02000000) {
        // While a note plays the copy sways down to the wave and back up.
        const i32 ph = m.phase(md::rig::drum.length, md::rig::drum.speed);
        const i32 idle = 4 * unit + multiply(md::positive(s[6]), 14 * unit);
        const i32 lift = idle - (ph > 0 ? multiply(idle, (ph < 512 ? ph : 1023 - ph) << 21) : 0);
        row(b, dpro_data::d_table[w], dpro_data::d_norm[w], 258, 472, 78, multiply(34, md::one - (chrl >> 2)), 3 * turn_per_point, 0xffffffffu, 0x08000000u);
        for (u32 i = 0; i < 32; ++i) b[i].y -= lift;
        single(frame, comb, b, 365, 64, key_of_slots(s, 3, 5, 6, 6) ^ static_cast<u32>(lift), dim);
    }
}
}

// DPRO-BBOX: the sample strip from STRT on, bars falling away with the sample
// (faster as PTCH raises it), dimmer before the start (the rig moves the start
// line and draws the key map and retrigger ticks); a handle on the start. The
// design fills the bars; the device never fills a mutable path, so they are
// outlines.
namespace dpro_bbox {
template <class F>
inline void view(F& frame, const i32* s, const view::Moment& m) {
    using namespace dpro_view;
    // The design's 30 bars across 14..470 (15.2 px), at 110, 34 px: e^-(t (3 + 3 PTCH)) (0.55 + 0.45 RND).
    static const i32 jitter[30] = {
        748, 910, 600, 862, 583, 961, 696, 661, 822, 922, 742, 597, 853, 909, 701,
        815, 633, 570, 997, 879, 625, 757, 968, 710, 590, 835, 942, 677, 783, 868,
    };   // (0.55 + 0.45 RND(i + 3)) Q10, a fixed draw
    const i32 ptch = md::positive(s[0]);
    const i32 start = md::positive(s[1]);
    const i32 sx_q10 = 14 * unit + multiply(start, 228 * unit);   // 14 + STRT x 456 x 0.5
    const i32 rate = 3 * 65536 + multiply(ptch, 3 * 65536);     // Q16
    Point bars[4][64];
    u32 count[4] = {0, 0, 0, 0};
    for (u32 i = 0; i < 30; ++i) {
        const i32 xl = 14 * unit + static_cast<i32>(i) * 15565, xr = xl + 12493;   // 15.2 and 12.2 px
        const i32 t = static_cast<i32>(i * 2185u);              // i / 30, Q16
        const i32 h = (multiply(md::exp_neg(multiply(t * 32768, rate)), 34 * unit) * jitter[i]) >> 10;
        const u32 group = (xl >= sx_q10 ? 2u : 0u) + (i >= 15 ? 1u : 0u);   // before or after start, left or right
        Point* p = bars[group];
        u32& c = count[group];
        if (c + 4 > 32) continue;
        // The tops left to right now, the bottoms on the way back.
        p[c].x = xl; p[c].y = 110 * unit - h; p[63 - c].x = xl; p[63 - c].y = 110 * unit + h; ++c;
        p[c].x = xr; p[c].y = 110 * unit - h; p[63 - c].x = xr; p[63 - c].y = 110 * unit + h; ++c;
    }
    // A left half runs on to the right half's first corners: the device closes
    // each path, and their two uprights then fall on one line at the seam.
    for (u32 g = 0; g < 4; g += 2) {
        if (!count[g] || !count[g + 1] || count[g] >= 32) continue;
        bars[g][count[g]] = bars[g + 1][0];
        bars[g][63 - count[g]] = bars[g + 1][63];
        ++count[g];
    }
    static const u32 paths[4] = {contour, wave, comb, extra};
    static const i32 centre[4] = {128, 356, 128, 356};
    for (u32 g = 0; g < 4; ++g) {
        if (!count[g]) { hide(frame, paths[g]); continue; }
        Point* p = bars[g];
        // Fill the unused middle with points down the last bar's right edge,
        // which the path draws anyway: nothing reaches past the bar.
        for (u32 i = count[g]; i < 32; ++i) {
            const i32 step = static_cast<i32>(i - count[g] + 1) * 16;   // 1/64 px apart: half a pixel at most
            p[i] = p[count[g] - 1]; p[i].y += step;
            p[63 - i] = p[63 - (count[g] - 1)]; p[63 - i].y -= step;
        }
        show(frame, paths[g], p, centre[g], 110, key_of_slots(s, 0, 1, 1, 8 + g), g < 2 ? dim : mid);
    }
    // The start's handle; while a note plays it runs on along the strip, as the playhead.
    const i32 ph = m.phase(md::rig::drum.length, md::rig::drum.speed);
    const i32 at = ph > 0 ? sx_q10 + multiply(470 * unit - sx_q10, ph << 21) : sx_q10;
    place(frame, handle_a, at, sy(110 - 40), unit, fill5, fill_and_stroke);
    // Its line (the rig's one part in the line shade) runs with it: one handle.
    for (u32 n = art_first; n < F::node_count; ++n)
        if (frame.nodes[n].shade == line && frame.nodes[n].x < hidden_x * unit) frame.nodes[n].x += at - sx_q10;
}
}
}
