#pragma once
// Draws a machine's instrument (and its panel's fixed parts) from the tables
// tools/art/build.mjs writes into each engine's art.h: every part sits at its
// pivot, moved, scaled and turned by the controls between the design's
// defaults and their ends, struck on a hit, and the hit's own parts (rings,
// sparks) play out over the animation. No floats, no runtime division.
#include "md_view.hpp"

namespace md_mmpar {} namespace md = md_mmpar; namespace md_mmpar {
namespace rig {
using opxy::u16;
using opxy::u8;
using opxy::draw::unit;

struct Pose { i32 dx, dy, scale, turn; };    // Q10 pixels, Q10 scale, tenths of a degree
struct Part {
    i32 x, y;                                // pivot, quarter pixels on the stage
    u8 shade, paint, kind, struck;           // kind 0 rest, 1 struck pose, 2 hit only; struck: replaced while struck
    u16 effect_first, effect_count;
    Pose motion;                             // a hit part's motion from early to late in the animation
    Pose strike;                             // a rest part's move when struck
};
// control: the machine's slot; absent: 1 gone low, 2 gone high, 4 shown only low, 8 only high
struct Effect { u8 control, absent; Pose low, high; };
struct Machine { u16 first, count; u8 defaults[8]; };

enum Kind { rest = 0, struck = 1, hit = 2 };

// 2^16 / n for n 1..127: the effects' interpolation without a divide.
static const u16 reciprocal[128] = {
    0, 65535, 32768, 21845, 16384, 13107, 10923, 9362, 8192, 7282, 6554, 5958, 5461, 5041, 4681, 4369,
    4096, 3855, 3641, 3449, 3277, 3121, 2979, 2849, 2731, 2621, 2521, 2427, 2341, 2260, 2185, 2114,
    2048, 1986, 1928, 1872, 1820, 1771, 1725, 1680, 1638, 1598, 1560, 1524, 1489, 1456, 1425, 1394,
    1365, 1337, 1311, 1285, 1260, 1237, 1214, 1192, 1170, 1150, 1130, 1111, 1092, 1074, 1057, 1040,
    1024, 1008, 993, 978, 964, 950, 936, 923, 910, 898, 886, 874, 862, 851, 840, 830,
    819, 809, 799, 790, 780, 771, 762, 753, 745, 736, 728, 720, 712, 705, 697, 690,
    683, 676, 669, 662, 655, 649, 643, 636, 630, 624, 618, 612, 607, 601, 596, 590,
    585, 580, 575, 570, 565, 560, 555, 551, 546, 542, 537, 533, 529, 524, 520, 516,
};

// Scales a pose by t (Q16, 0..1).
inline Pose part_of(const Pose& p, i32 t) {
    Pose out = {(p.dx * (t >> 4)) >> 12, (p.dy * (t >> 4)) >> 12, unit + (((p.scale - unit) * (t >> 4)) >> 12), (p.turn * (t >> 4)) >> 12};
    return out;
}

// The hit's timing: the design's animation runs 520 ms (1100 ms for shakers).
struct Timing { i32 length, speed; };        // Q10 seconds; 2^20 / length
const Timing drum = {532, 1971}, shaker = {1126, 931};

template <class F>
inline void draw(F& frame, const Part* parts, const Effect* effects, const u16* members, const Machine& m,
                 const i32* slots, const view::Moment& moment, Timing timing = drum) {
    using namespace opxy::draw;
    const i32 ph = moment.phase(timing.length, timing.speed);   // Q10 0..1, or -1
    const bool striking = ph >= 0 && ph < 410;                    // the first 40%
    const i32 strike = striking ? ((410 - ph) << 16) / 410 : 0;   // Q16, fading
    for (u32 k = 0; k < m.count; ++k) {
        const u32 index = members[m.first + k];
        const Part& part = parts[index];
        const u32 node = view::art_first + index;
        if ((part.kind == struck && !striking) || (part.kind == hit && ph < 0) || (part.kind == rest && part.struck && striking)) {
            hide(frame, node);
            continue;
        }
        // The controls: each effect between the default and the control's end.
        i32 dx = 0, dy = 0, scale = unit, turn = 0;
        bool shown = true;
        for (u32 e = 0; e < part.effect_count; ++e) {
            const Effect& fx = effects[part.effect_first + e];
            const i32 v = static_cast<i32>(static_cast<u32>(md::positive(slots[fx.control])) >> 24);
            const i32 d = m.defaults[fx.control];
            Pose p;
            if (((fx.absent & 4) && v >= (d >> 1)) || ((fx.absent & 8) && v <= d + ((127 - d) >> 1))) shown = false;
            if (v < d) {
                if ((fx.absent & 1) && v < (d >> 1)) shown = false;
                p = part_of(fx.low, (d - v) * reciprocal[d]);
            } else {
                if ((fx.absent & 2) && v > d + ((127 - d) >> 1)) shown = false;
                p = d < 127 ? part_of(fx.high, (v - d) * reciprocal[127 - d]) : part_of(fx.high, 0);
            }
            dx += p.dx; dy += p.dy; turn += p.turn;
            scale = (scale * p.scale) >> 10;
        }
        if (!shown) { hide(frame, node); continue; }
        if (part.kind == rest && striking) {
            const Pose s = part_of(part.strike, strike);
            dx += s.dx; dy += s.dy; turn += s.turn; scale = (scale * s.scale) >> 10;
        }
        u32 tone = part.shade;
        if (part.kind == hit) {
            // From the early pose (20% in) towards the late one (70%), fading at the end.
            const i32 t = ((ph - 205) << 16) / 512;
            const Pose s = part_of(part.motion, t);
            dx += s.dx; dy += s.dy; turn += s.turn; scale = (scale * s.scale) >> 10;
            if (ph > 717 && tone < view::label) tone = view::dim;
        }
        scale = md::clamp(scale, 64, 2 * unit);
        place(frame, node, part.x * 256 + dx, (view::stage_y * 4 + part.y) * 256 + dy, scale, tone, static_cast<Paint>(part.paint));
        if (turn) {
            const i32 tenths = ((turn % 3600) + 3600) % 3600;
            opxy::draw::turn(frame, node, rotation(tenths));
        }
    }
}

// Hides one machine's parts (another machine of a family engine is showing).
template <class F>
inline void hide_all(F& frame, const u16* members, const Machine& m) {
    for (u32 k = 0; k < m.count; ++k) opxy::draw::hide(frame, view::art_first + members[m.first + k]);
}
}
}
