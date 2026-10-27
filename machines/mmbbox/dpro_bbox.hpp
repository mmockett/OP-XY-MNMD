#pragma once
// DPRO-BBOX: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_kit.hpp"
#include "private_bbox.hpp"

// The Monomachine's DPRO-BBOX: 24 drum samples on 24 keys, played from the
// owner's own ROM (captured by tools/capture_dpro.py into a private header;
// without it the engine builds and plays silence, docs/FAMILIES.md).
//   PTCH  STRT  -     -
//   RTRG  RTIM  -     -
// As measured on the original: the keys repeat every two octaves from C, and
// each key also raises the pitch a quarter semitone, so a sample two octaves up
// plays 6 semitones higher. PTCH moves it about +-21 semitones, a third of a
// semitone a step. STRT starts it later, by up to 0.37 of the sample at 127 (measured
// on two samples, a 57 ms one and a 295 ms one, a step 5.7 and 47.5 frames).
// RTRG restarts it that many times, RTIM apart: about 1000 ms / (RTIM + 1).
namespace md {
namespace dpro_bbox {
struct State {
    u32 sample;     // which of the 24
    u32 position;   // frames into it, Q16
    u32 started;    // a note has started: render reads its key
    u32 repeats;    // restarts still to come
    u32 until;      // samples to the next
};
namespace {
using md::multiply;
enum { PTCH, STRT, SPARE3, SPARE4, RTRG, RTIM };

// PTCH: Q16 octaves a step from 64, as measured: -20 semitones at 0 (-10.7 at 32),
// +21.8 at 127 (+11.2 at 96).
inline i32 ptch_octaves(i32 slot) {
    const i32 v = static_cast<i32>(static_cast<u32>(md::positive(slot)) >> 24) - 64;
    return v < 0 ? v * 1707 : v * 1892;
}
// STRT: the start, in frames, for a sample of `length` frames: length x STRT x 3 / 1024.
inline u32 start_of(u32 length, i32 slot) { return (length * (static_cast<u32>(md::positive(slot)) >> 24) * 3) >> 10; }
// RTIM: the restart interval in samples, about 1000 ms / (RTIM + 1): 990 ms at
// 0, 110 at 8, 58 at 16, 15.6 at 64, 8 at 127, as measured.
static const opxy::u16 intervals[128] = {
    42975, 21700, 14515, 10904, 8732, 7282, 6244, 5466, 4860, 4375, 3978, 3647, 3367, 3127, 2918, 2736,
    2575, 2433, 2305, 2190, 2085, 1991, 1904, 1825, 1752, 1685, 1622, 1564, 1511, 1460, 1413, 1369,
    1328, 1289, 1252, 1217, 1184, 1153, 1123, 1095, 1069, 1043, 1019, 996, 974, 953, 932, 913,
    894, 876, 859, 843, 827, 811, 797, 782, 769, 756, 743, 730, 718, 707, 696, 685,
    674, 664, 654, 644, 635, 626, 617, 609, 600, 592, 584, 577, 569, 562, 555, 548,
    541, 534, 528, 522, 516, 510, 504, 498, 492, 487, 482, 476, 471, 466, 461, 457,
    452, 447, 443, 438, 434, 430, 425, 421, 417, 413, 410, 406, 402, 398, 395, 391,
    388, 384, 381, 378, 375, 371, 368, 365, 362, 359, 356, 353, 351, 348, 345, 342
};
inline u32 interval(u32 rtim) { return intervals[rtim & 127u]; }
}

inline void reset(State& s, Common&, u32) { s.sample = 0; s.position = 0xffffffffu; s.started = 0; s.repeats = 0; s.until = 0; }
inline void trigger(State& s, Common&) { s.started = 1; }

inline void render(State& s, Common&, opxy::Output out, const i32* p, opxy::Pitch pitch) {
    if (s.started) {
        const u32 key = md::kit::key_of(pitch[0]);
        s.sample = key < bbox_data::samples ? key : 0;
    }
    const u32 strt = start_of(bbox_data::length[s.sample], p[STRT]) << 16;
    if (s.started) {
        s.position = strt;
        s.repeats = static_cast<u32>(md::positive(p[RTRG])) >> 24;
        s.until = interval(static_cast<u32>(md::positive(p[RTIM])) >> 24);
        s.started = 0;
    }
    // The key's quarter semitones from note 60, then PTCH: a Q16 frame step.
    const i32 key = static_cast<i32>(md::kit::note_of(pitch[0])) - 60;
    const u32 step = md::transpose(65536u, key * 1365 + ptch_octaves(p[PTCH]));
    const opxy::i16* data = bbox_data::data + bbox_data::start[s.sample];
    const u32 end = bbox_data::length[s.sample] << 16;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 x = 0;
        if (s.position + 65536u < end) {
            const u32 f = s.position >> 16;
            const i32 a = data[f], b = data[f + 1];
            x = a * 65536 + (b - a) * static_cast<i32>(s.position & 0xffffu);
            s.position += step;
        }
        out[i] = md::finish(x);
        if (s.repeats && --s.until == 0) {
            --s.repeats;
            s.position = strt;
            s.until = interval(static_cast<u32>(md::positive(p[RTIM])) >> 24);
        }
    }
}
}
}
