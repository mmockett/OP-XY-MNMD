#pragma once
// The silent stand-in for DPRO-BBOX's samples (docs/FAMILIES.md). The real
// header, with the samples captured from the owner's own Monomachine ROM, is
// written by tools/capture_dpro.py into
// ~/Library/Application Support/OPXY-MD/private/dpro/ and copied into the
// engine by tools/make_engine.py. Without it mmbbox builds and passes its
// checks, silent. It holds no data.
#include <opxy/types.hpp>

namespace md {
namespace bbox_data {
const bool captured = false;
const opxy::u32 samples = 1;
const opxy::u32 frames = 1;
static const opxy::u32 start[1] = {0};
static const opxy::u32 length[1] = {0};
static const opxy::i16 data[1] = {0};
}
}
