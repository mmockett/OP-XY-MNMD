#pragma once
// The silent stand-in for the DigiPRO waves (docs/FAMILIES.md). The real
// header, with the waves captured from the owner's own Monomachine ROM, is
// written by tools/capture_dpro.py into
// ~/Library/Application Support/OPXY-MD/private/dpro/ and copied into each
// engine by tools/make_engine.py. Without it the DigiPRO engines build and
// pass their checks, silent. It holds no data.
#include <opxy/types.hpp>

namespace md {
namespace dpro_data {
const bool captured = false;
const opxy::u32 table_bits = 9;                 // 512 samples a table
const opxy::u32 d_waves = 1, s_waves = 1;       // D01..D64 and S01..S32 when captured
const opxy::u32 s_steps = 5;                    // each S-set at WP 0, 16, 48, 80, 112
template <int> struct waves {                  // the captured header's shape: one copy, linked
    static const opxy::i16 d[1][1u << table_bits];
    static const opxy::i16 s[1][s_steps][1u << table_bits];
    static const opxy::i16 end[1u << table_bits];
};
template <int N> const opxy::i16 waves<N>::d[1][1u << table_bits] = {};
template <int N> const opxy::i16 waves<N>::s[1][s_steps][1u << table_bits] = {};
template <int N> const opxy::i16 waves<N>::end[1u << table_bits] = {};
static const opxy::i16 (&d_table)[1][1u << table_bits] = waves<0>::d;
static const opxy::i16 (&s_table)[1][s_steps][1u << table_bits] = waves<0>::s;
static const opxy::i16 (&s_end)[1u << table_bits] = waves<0>::end;   // where S32 arrives at WP 127
static const opxy::i32 d_norm[1] = {0};          // for the views: 2^30 / each table's peak
static const opxy::i32 s_norm[1] = {0};
static const opxy::i16 d_peak[1] = {256};        // each D-wave's peak, for DDRW's bit reduction
}
}
