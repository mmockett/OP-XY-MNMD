#pragma once
// The silent stand-in for the E12 samples (docs/FAMILIES.md). The samples are
// captured from the owner's own Machinedrum ROM by tools/capture_e12.py into
// ~/Library/Application Support/OPXY-MD/private/e12/private_e12.hpp, which has
// this shape and is never committed. Without it every E12 machine is silent.
#include <opxy/types.hpp>

namespace md {
namespace e12_data {
const opxy::u32 captured = 0;
template <int> struct samples {      // the captured header's shape: one copy, linked
    static const signed char high[1];
    static const unsigned char low[1];
};
template <int N> const signed char samples<N>::high[1] = {0};
template <int N> const unsigned char samples<N>::low[1] = {0};
static const signed char (&high)[1] = samples<0>::high;
static const unsigned char (&low)[1] = samples<0>::low;
const opxy::u32 start[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const opxy::i32 level[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const opxy::u32 strt[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}
}
