#pragma once
// VO-6's view, after the design's vowel chart, syllable and consonant timeline
// (docs/design/Main.dc.html, mnmView). The rig (md_rig.hpp) draws the chart's
// box, its vowels and labels, and the timeline; this draws what changes shape:
// the crosshair and handle at the vowel's own formants, the syllable it sings,
// the consonant's burst and the vowel's wave. The handle and crosshair sit
// where the formants really are on the design's chart (its first formant
// 250 .. 900 Hz across, its second 700 .. 2500 Hz up), so the chart's vowels
// name what is heard.
#include "md_view.hpp"
#include "md_rig.hpp"
#include "live_view.hpp"

namespace md {
namespace vo_panel {
using namespace md::view;

const i32 half = 0x40000000;
enum { VOC1, VOC2, VSW, VOIC, CONS, CLEN, CVOL, TUNE };

// The handle on the chart, Q10 pixels, at VOC1 and VOC2 0, 16, ..., 112, 127:
// the first formant 195 .. 708 Hz across, the second 499 .. 2675 Hz up.
const i32 handle_x[9] = {26624, 26624, 26624, 35507, 51382, 70057, 92025, 117868, 146220};
const i32 handle_y[9] = {139264, 139264, 139264, 130205, 113760, 94085, 69605, 38666, 24576};
// The formants in Hz at the same steps (voices/vo_6.hpp), for naming the vowel.
const i32 f1_hz[9] = {195, 229, 270, 317, 373, 439, 517, 608, 708};
const i32 f2_hz[9] = {499, 627, 782, 957, 1176, 1438, 1764, 2176, 2675};
// The design's vowels: letter, first and second formant.
struct Vowel { char letter; i32 f1, f2; };
const Vowel vowels[5] = {{'i', 270, 2300}, {'e', 530, 1850}, {'a', 730, 1100}, {'o', 570, 840}, {'u', 300, 870}};
// The consonants' names, as CONS steps.
const char* const names[21] = {"", "b", "d", "f", "g", "h", "j", "k", "l", "m", "n", "p", "r", "rr", "s", "sj", "t",
                               "tj", "th", "v", "z"};

// Single-stroke lower case for the syllable: the stroke font's letters
// (tools/art/font.mjs) redrawn so each is one line, in half grid units (x 0..8,
// x-height 4, baseline 12, descender 16). The dots of i and j are left out.
const signed char strokes[] = {
    2, 5, 4, 4, 6, 4, 8, 6, 8, 12, 8, 8, 2, 8, 0, 10, 0, 11, 2, 12, 6, 12, 8, 10,   // a: 12
    0, 0, 0, 12, 0, 10, 2, 12, 6, 12, 8, 10, 8, 6, 6, 4, 2, 4, 0, 6,                 // b: 10
    8, 6, 6, 4, 2, 4, 0, 6, 0, 10, 2, 12, 6, 12, 8, 10, 8, 0, 8, 12,                 // d: 10
    0, 8, 8, 8, 8, 6, 6, 4, 2, 4, 0, 6, 0, 10, 2, 12, 6, 12, 8, 11,                  // e: 10
    3, 12, 3, 4, 0, 4, 7, 4, 3, 4, 3, 2, 5, 0, 7, 0,                                 // f: 8
    8, 12, 2, 12, 0, 10, 0, 6, 2, 4, 8, 4, 8, 14, 6, 16, 1, 16,                      // g: 9
    0, 0, 0, 12, 0, 6, 2, 4, 6, 4, 8, 6, 8, 12,                                      // h: 7
    2, 4, 4, 4, 4, 12, 2, 12, 6, 12,                                                 // i: 5
    5, 4, 5, 14, 3, 16, 1, 16,                                                       // j: 4
    0, 0, 0, 12, 0, 9, 8, 4, 3, 7, 8, 12,                                            // k: 6
    3, 0, 3, 10, 5, 12, 7, 12,                                                       // l: 4
    0, 12, 0, 4, 0, 6, 1, 4, 3, 4, 4, 6, 4, 12, 4, 6, 5, 4, 7, 4, 8, 6, 8, 12,       // m: 12
    0, 12, 0, 4, 0, 6, 2, 4, 6, 4, 8, 6, 8, 12,                                      // n: 7
    2, 4, 6, 4, 8, 6, 8, 10, 6, 12, 2, 12, 0, 10, 0, 6, 2, 4,                        // o: 9
    0, 16, 0, 4, 0, 6, 2, 4, 6, 4, 8, 6, 8, 10, 6, 12, 2, 12, 0, 10,                 // p: 10
    0, 4, 0, 12, 0, 7, 3, 4, 7, 4,                                                   // r: 5
    0, 11, 2, 12, 6, 12, 8, 10, 6, 8, 2, 8, 0, 6, 2, 4, 6, 4, 8, 5,                  // s: 10
    7, 12, 5, 12, 3, 10, 3, 1, 3, 4, 0, 4, 7, 4,                                     // t: 7
    0, 4, 0, 10, 2, 12, 6, 12, 8, 10, 8, 4, 8, 12,                                   // u: 7
    0, 4, 4, 12, 8, 4,                                                               // v: 3
    0, 4, 8, 4, 0, 12, 8, 12,                                                        // z: 4
};
struct Glyph { char letter; unsigned char first, count; };   // first: a point's index in strokes
const Glyph glyphs[21] = {{'a', 0, 12}, {'b', 12, 10}, {'d', 22, 10}, {'e', 32, 10}, {'f', 42, 8}, {'g', 50, 9},
                          {'h', 59, 7}, {'i', 66, 5}, {'j', 71, 4}, {'k', 75, 6}, {'l', 81, 4}, {'m', 85, 12},
                          {'n', 97, 7}, {'o', 104, 9}, {'p', 113, 10}, {'r', 123, 5}, {'s', 128, 10}, {'t', 138, 7},
                          {'u', 145, 7}, {'v', 152, 3}, {'z', 155, 4}};
inline const Glyph* glyph(char c) {
    for (u32 k = 0; k < 21; ++k) if (glyphs[k].letter == c) return &glyphs[k];
    return 0;
}

// The design's RND(i), as a fixed table: a noisy wave and burst that stay still.
static const i32 noise[32] = {
    1203450987, -804712311, 431876543, -1876543210, 987654321, -123456789, 1678901234, -1543210987,
    345678901, -987123456, 1456789012, -345612789, 765432109, -1987654321, 234567890, -654321098,
    1890123456, -1098765432, 567890123, -876543210, 1345678901, -234567891, 912345678, -1678901235,
    123456789, -1456789013, 1789012345, -567890124, 678901234, -1234567890, 1567890123, -789012345,
};

// Writes a polyline of up to 32 points (stage Q10) into a path about (ox, oy),
// at `scale` 1 or 2, unless the path already holds this key.
template <class F>
inline void lines(F& frame, u32 path, const i32* xs, const i32* ys, u32 n, i32 ox, i32 oy, u32 scale, u32 key, u32 tone) {
    typename F::path_type& p = frame.paths[path];
    if (p.key != key) {
        Point points[32];
        for (u32 i = 0; i < n; ++i) {
            points[i].x = xs[i] - ox * unit;
            points[i].y = ys[i] - oy * unit;
            if (scale == 2) { points[i].x = opxy::q31::half(points[i].x); points[i].y = opxy::q31::half(points[i].y); }
        }
        polyline(p, points, n, key);
        md::live::unfold(p);
    }
    show_path(frame, path, ox, oy, tone, static_cast<i32>(scale) * unit);
}

template <class F>
inline void draw(F& frame, const i32* s, const Moment& moment) {
    // The rig's own handle, and the wave it would strike, give way to this view's.
    for (u32 n = art_first; n < F::node_count; ++n)
        if (frame.nodes[n].shade == fill5 && frame.nodes[n].paint == fill_and_stroke) hide(frame, n);
        else if (frame.nodes[n].shade == line) hide(frame, n);
    const u32 cons = (static_cast<u32>(md::positive(s[CONS])) >> 16) * 21u >> 15;
    const bool sing = s[VSW] >= half || cons == 0;
    const i32 voic = md::positive(s[VOIC]), clen = md::positive(s[CLEN]), cvol = md::positive(s[CVOL]);

    // The chart: the crosshair and handle at the vowel's formants.
    const i32 px = md::table9(handle_x, s[VOC1]), py = md::table9(handle_y, s[VOC2]);
    {
        // A closed plus, its arms as thin as a ribbon: walking one line out and
        // back along another turns straight back, which the device draws as streaks.
        typename F::path_type& cross = frame.paths[contour];
        const u32 key = md::live::sealed(md::live::mix(md::live::mix(1u, px), py));
        if (cross.key != key) {
            const i32 w = apart / 2, cx = (px - 106 * unit) / 2, cy = (py - 80 * unit) / 2;
            const i32 l = -45 * unit, r = 45 * unit, t = -33 * unit, b = 33 * unit;
            const Point plus[12] = {{l, cy - w}, {cx - w, cy - w}, {cx - w, t}, {cx + w, t}, {cx + w, cy - w}, {r, cy - w},
                                    {r, cy + w}, {cx + w, cy + w}, {cx + w, b}, {cx - w, b}, {cx - w, cy + w}, {l, cy + w}};
            outline(cross, plus, 12, key);
        }
        show_path(frame, contour, 106, 80, dim, 2 * unit);
        place(frame, handle_a, sx(0) + px, sy(0) + py, unit, fill5, fill_and_stroke);
    }

    // The syllable on one path: the consonant's letters, then the nearest vowel.
    char letter = 0;
    if (sing) {
        const i32 f1 = md::table9(f1_hz, s[VOC1]), f2 = md::table9(f2_hz, s[VOC2]);
        i32 best = 0x7fffffff;
        for (u32 k = 0; k < 5; ++k) {
            const i32 d1 = vowels[k].f1 - f1, d2 = multiply(vowels[k].f2 - f2, 0x33333333);   // the design's 0.4
            const i32 d = d1 * d1 + d2 * d2;
            if (d < best) { best = d; letter = vowels[k].letter; }
        }
    }
    {
        char text[4] = {0, 0, 0, 0};
        u32 length = 0;
        for (const char* c = names[cons]; *c; ++c) text[length++] = *c;
        if (letter) text[length++] = letter;
        // 40 px type, centred on x 330: a half grid unit is 40 / 18 px (2276 Q10), a letter 22.2 px on.
        const i32 left = 330 * unit - static_cast<i32>(length) * 11378;
        u32 dots = 0;
        i32 xs[32], ys[32];
        u32 n = 0;
        for (u32 k = 0; k < length; ++k) {
            const Glyph* g = glyph(text[k]);
            if (!g) continue;
            const i32 x0 = left + static_cast<i32>(k) * 22756;
            for (u32 i = 0; i < g->count && n < 32; ++i, ++n) {
                xs[n] = x0 + strokes[2 * (g->first + i)] * 2276;
                ys[n] = 17067 + strokes[2 * (g->first + i) + 1] * 2276;     // 10 + 1.5 grid units
            }
            // The dots of i and j: two of the readout's bars, small.
            if ((text[k] == 'i' || text[k] == 'j') && dots < 2) {
                const i32 dx = text[k] == 'i' ? 4 : 5;
                place(frame, digit_first + 3 * dots, x0 + dx * 2276, sy(0) + 17067 + 2276, 400, label, stroke);
                ++dots;
            }
        }
        if (n >= 2) lines(frame, comb, xs, ys, n, 330, 30, 1, 1u + cons + 32u * (static_cast<u32>(letter) & 31u), label);
        else hide(frame, comb);
    }

    // The timeline, after the design's: the consonant a burst of grains standing
    // on the line, CLEN long and CVOL tall, then the vowel's wave (when it
    // sings), four cycles of the first formant's wave with the second's ripple
    // at their ratio (up to 4) and noise as VOIC whispers; the wave rolls on
    // while a note plays.
    const i32 tx = 220 * unit, ty = 118 * unit;
    const i32 width = cons ? 10 * unit + multiply(clen, 80 * unit) : 0;
    if (cons) {
        // Ten grains, each up from the line and back, a tenth of the width apart.
        i32 xs[32], ys[32];
        const i32 pitch = multiply(width, 0x0ccccccd);   // 1 / 10
        // A thin tall grain is a turn nearly straight back, whose stroke escapes as a long
        // line, and at CLEN 0 its points sit at the device's merging distance. So a tall
        // grain widens as it needs to, keeping a flat foot, and is no taller than 10 pitches.
        const i32 tall = md::clamp(2 * unit + multiply(cvol, 28 * unit), 0, 10 * pitch);
        const i32 side = md::clamp(multiply(tall, 0x03d70a3d), pitch / 4, multiply(pitch, 0x26666666));   // 0.03 of its height, up to 0.3 pitch
        for (u32 g = 0; g < 10; ++g) {
            const i32 x = tx + static_cast<i32>(g) * pitch;
            xs[3 * g] = x; ys[3 * g] = ty;
            xs[3 * g + 1] = x + side;
            ys[3 * g + 1] = ty - multiply(tall, 0x33333333 + multiply(0x4ccccccd, half + opxy::q31::half(noise[(g * 7 + cons * 5) & 31])));
            xs[3 * g + 2] = x + 2 * side; ys[3 * g + 2] = ty;
        }
        xs[30] = tx + width; ys[30] = ty;
        lines(frame, extra, xs, ys, 31, 270, 104, 2, md::live::sealed(md::live::mix(md::live::mix(cons, clen), cvol)), mid);
    } else {
        hide(frame, extra);
    }
    // A cycle over the hit, then one a second for as long as the note is held;
    // the wave is a function of its phase, so it rolls without a seam.
    const u32 roll = md::live::turning(md::live::of(moment), 8073200u, rig::drum.length, md::live::one_hz);
    if (sing) {
        i32 xs[32], ys[32];
        const i32 x0 = tx + width + (cons ? 4 * unit : 0), x1 = 470 * unit;
        const i32 f1 = md::table9(f1_hz, s[VOC1]), f2 = md::table9(f2_hz, s[VOC2]);
        const u32 step = (554189328u >> 7) * md::live::stretch(s[TUNE]);        // four cycles over 31 steps, at TUNE's middle
        i32 ratio = 1;   // F2 / F1 in quarters, without dividing
        while (ratio < 16 && (ratio + 1) * f1 <= f2 * 4) ++ratio;
        for (u32 i = 0; i < 32; ++i) {
            xs[i] = x0 + multiply(x1 - x0, static_cast<i32>(i) * 0x04210842);       // 1 / 31 of the width
            const u32 u = i * step + roll;
            i32 v = multiply(md::sine(u), 0x4ccccccd);                              // 0.6
            v += multiply(multiply(md::one - voic, 0x33333333), md::sine((u >> 2) * static_cast<u32>(ratio)));
            v += multiply(multiply(voic, 0x4ccccccd), noise[i & 31]);
            ys[i] = 100 * unit - multiply(v, 14 * unit);
        }
        lines(frame, wave, xs, ys, 32, static_cast<i32>(static_cast<u32>(x0 + x1) >> 11), 100, 2,
              md::live::sealed(md::live::mix(md::live::mix(md::live::mix(md::live::mix(md::live::mix(cons, s[VOC1]), s[VOC2]), voic), clen), s[TUNE]) + roll), line);
    } else {
        hide(frame, wave);
    }
}
}

namespace vo_6 { template <class F> inline void view(F& f, const i32* s, const view::Moment& m) { vo_panel::draw(f, s, m); } }
}
