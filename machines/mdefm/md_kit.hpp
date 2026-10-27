#pragma once
// Notes as keys: the pitch the voices are voiced at, and the key of 24 a note
// plays (the drum kits' keys, and DPRO-BBOX's slices).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_tables.hpp"

namespace md_mdefm {} namespace md = md_mdefm; namespace md_mdefm {
namespace kit {
const u32 keys = 24;
const u32 middle_c = 25479004u;   // 261.63 Hz at 44.1 kHz, where the voices are voiced

// The MIDI note nearest a phase increment.
inline u32 note_of(u32 increment) {
    u32 note = 0;
    for (u32 step = 64; step; step >>= 1)
        if (note + step < 128 && increment >= note_edge_table[note + step]) note += step;
    return note;
}

// The key a note plays: notes wrap every two octaves, from C.
inline u32 key_of(u32 increment) {
    u32 note = note_of(increment);
    while (note >= keys) note -= keys;
    return note;
}
}
}
