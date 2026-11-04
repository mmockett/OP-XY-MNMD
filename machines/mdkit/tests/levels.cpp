// MD KIT's output levels (tools/catalog.py LEVELS): each default key's peak,
// one note at its stored controls and neutral track offsets, as a share of the
// SDK's half-scale ceiling. The quieter machines were raised to sit near the
// louder ones; the others, already at the ceiling, are unchanged. A key may
// not drift more than 0.02 from its figure, nor any key pass the ceiling.
#include "../engine.cpp"
#include <cstdio>

namespace {
// Keys 53..76: BD SD HT MT LT CP RS CB CH OH RC CC M1..M4, BD2 SD2 LC HC CP2 PH CB2 B22.
const double expected[24] = {0.757, 0.659, 0.624, 0.625, 0.626, 0.845, 0.938, 0.823, 0.710, 0.865, 0.676, 0.887,
                             0.766, 0.667, 0.722, 1.000, 0.757, 0.892, 0.884, 0.890, 0.783, 0.856, 0.850, 1.000};
// Before the levels: the same keys' peaks (TRX-XT, -CB, -CY and -XC were raised).
const double before[24] = {0.757, 0.659, 0.516, 0.516, 0.517, 0.845, 0.938, 0.735, 0.710, 0.865, 0.400, 0.525,
                           0.766, 0.667, 0.390, 1.000, 0.757, 0.892, 0.478, 0.481, 0.783, 0.856, 0.759, 1.000};
mdkit::Voice voice;
mdkit::Shared shared;
}

int main() {
    int failures = 0;
    for (opxy::u32 k = 0; k < 24; ++k) {
        opxy::u8 model, raw[8];
        mdkit::drum_defaults(53 + k, &model, raw);
        opxy::i32 values[8];
        for (int c = 0; c < 8; ++c) values[c] = raw[c] == 127 ? 0x7fffffff : static_cast<opxy::i32>(raw[c]) << 24;
        voice = mdkit::Voice();
        shared = mdkit::Shared();
        mdkit::initialize(voice, shared, 1);
        mdkit::start(voice, shared, 1);
        opxy::i32 out[opxy::block_frames];
        opxy::u32 pitch[opxy::block_frames];
        for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = ::md::kit::middle_c;
        opxy::i32 peak = 0;
        for (unsigned b = 0; b < 1380; ++b) {
            mdkit::drum_select(voice, shared, 1, model);
            mdkit::render(voice, shared, 1, out, values, pitch);
            for (unsigned i = 0; i < opxy::block_frames; ++i) {
                const opxy::i32 a = out[i] < 0 ? -out[i] : out[i];
                if (a > peak) peak = a;
            }
        }
        const double share = peak / 1073741824.0;
        if (peak > 1073741824) { std::printf("FAIL key %u passes the ceiling\n", 53 + k); ++failures; }
        if (share < expected[k] - 0.02 || share > expected[k] + 0.02) {
            std::printf("FAIL key %u peaks at %.3f, not %.3f\n", 53 + k, share, expected[k]); ++failures;
        }
        if (share < before[k] - 0.02) { std::printf("FAIL key %u is quieter than before\n", 53 + k); ++failures; }
    }
    return failures ? 1 : 0;
}
