// MD E12's pad controls in the voice (tools/make_engine.py pad_fx_code), written
// by tools/make_engine.py: at 0 a key sounds exactly as without them; FILTER's
// low-pass takes the highs and its high-pass the lows; SRR holds each sample
// for its hold; at their heaviest, every machine stays under the SDK's
// half-scale ceiling and still ends.
// Built without the captured data (docs/FAMILIES.md): silent, so what FILTER does to the sound goes unchecked.
#include "../engine.cpp"
#include <cstdio>
#include <cstring>

namespace {
int failures = 0;
const bool captured = false;
mde12::Voice voice;
mde12::Shared shared;

// One note of a key's machine at its kit controls, a second at most, with
// the pad controls given; returns the blocks rendered until it ended.
unsigned play(opxy::u32 key, const opxy::i32* pads, opxy::i32* out, unsigned frames, opxy::i32& peak) {
    opxy::u8 model, raw[8];
    mde12::drum_defaults(key, &model, raw);
    opxy::i32 values[8];
    for (int c = 0; c < 8; ++c) values[c] = raw[c] == 127 ? 0x7fffffff : static_cast<opxy::i32>(raw[c]) << 24;
    std::memset(&voice, 0, sizeof voice);
    shared = mde12::Shared();
    mde12::initialize(voice, shared, 1);
    mde12::start(voice, shared, 1);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = ::md::kit::middle_c;
    peak = 0;
    unsigned blocks = 0;
    for (; blocks < 44100 * 20 / opxy::block_frames && !mde12::finished(voice, shared); ++blocks) {
        mde12::drum_select(voice, shared, 1, model);
        if (pads) mde12::drum_pad(voice, shared, 1, pads);
        opxy::i32 block[opxy::block_frames];
        mde12::render(voice, shared, 1, block, values, pitch);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            const opxy::i32 a = block[i] < 0 ? -block[i] : block[i];
            if (a > peak) peak = a;
            const unsigned at = blocks * opxy::block_frames + i;
            if (out && at < frames) out[at] = block[i];
        }
    }
    return blocks;
}

// The energy of a signal's first differences (its highs) and of a running
// average (its lows), on 16-bit samples.
double highs(const opxy::i32* x, unsigned n) {
    double e = 0;
    for (unsigned i = 1; i < n; ++i) { const double d = (x[i] - x[i - 1]) / 65536.0; e += d * d; }
    return e;
}
double lows(const opxy::i32* x, unsigned n) {
    double e = 0, avg = 0;
    for (unsigned i = 0; i < n; ++i) { avg += (x[i] / 65536.0 - avg) / 32; e += avg * avg; }
    return e;
}

const unsigned frames = 8192;
opxy::i32 plain[frames], with[frames];
}

int main() {
    const opxy::i32 neutral[2] = {0, 0};
    // At 0 every key sounds exactly as without the hook.
    for (opxy::u32 key = 53; key < 77; ++key) {
        opxy::i32 peak_a, peak_b;
        const unsigned a = play(key, nullptr, plain, frames, peak_a);
        const unsigned b = play(key, neutral, with, frames, peak_b);
        if (a != b || peak_a != peak_b || std::memcmp(plain, with, sizeof plain)) {
            std::printf("FAIL key %u sounds different with its pad controls at 0\n", key); ++failures;
        }
    }
    // Low-pass at its lowest takes most of a hat's highs; high-pass at its
    // highest most of a kick's lows. Key 61 is CH, key 53 BD.
    if (captured) {
        opxy::i32 peak;
        play(61, neutral, plain, frames, peak);
        const opxy::i32 low[2] = {-16, 0};
        play(61, low, with, frames, peak);
        if (!(highs(with, frames) < 0.05 * highs(plain, frames))) {
            std::printf("FAIL FILTER L150 leaves %.3f of CH's highs\n", highs(with, frames) / highs(plain, frames)); ++failures;
        }
        play(53, neutral, plain, frames, peak);
        const opxy::i32 high[2] = {15, 0};
        play(53, high, with, frames, peak);
        if (!(lows(with, frames) < 0.1 * lows(plain, frames))) {
            std::printf("FAIL FILTER H4.0k leaves %.3f of BD's lows\n", lows(with, frames) / lows(plain, frames)); ++failures;
        }
    }
    // SRR holds each sample: at 15, runs of 64 equal samples from the note's start.
    {
        opxy::i32 peak;
        const opxy::i32 crushed[2] = {0, 15};
        play(53, crushed, with, frames, peak);
        for (unsigned i = 0; i < 4096; ++i)
            if (with[i] != with[i - i % 64]) { std::printf("FAIL SRR 15 changes inside a hold at %u\n", i); ++failures; break; }
        const opxy::i32 light[2] = {0, 1};
        play(53, light, with, frames, peak);
        for (unsigned i = 0; i < 4096; i += 2)
            if (with[i] != with[i + 1]) { std::printf("FAIL SRR 1 does not hold pairs at %u\n", i); ++failures; break; }
    }
    // At their heaviest, every machine stays under the ceiling and ends.
    const opxy::i32 heavy[][2] = {{-16, 15}, {-1, 0}, {1, 0}, {15, 15}, {-8, 4}, {-1, 15}};
    for (opxy::u32 key = 53; key < 77; ++key)
        for (const auto& pads : heavy) {
            opxy::i32 peak;
            const unsigned blocks = play(key, pads, nullptr, 0, peak);
            if (peak > ::md::half_scale) {
                std::printf("FAIL key %u passes the ceiling at FILTER %d SRR %d\n", key, pads[0], pads[1]); ++failures;
            }
            if (blocks >= 44100 * 20 / opxy::block_frames && !mde12::waiting(voice)) {
                std::printf("FAIL key %u never ends at FILTER %d SRR %d\n", key, pads[0], pads[1]); ++failures;
            }
        }
    // The values arrive clamped to their fields.
    const opxy::i32 wild[2] = {99, 99}, under[2] = {-99, -9};
    mde12::drum_pad(voice, shared, 1, wild);
    if (voice.fx.filter != 15 || voice.fx.srr != 15) { std::printf("FAIL pad values unclamped\n"); ++failures; }
    mde12::drum_pad(voice, shared, 1, under);
    if (voice.fx.filter != -16 || voice.fx.srr != 0) { std::printf("FAIL pad values unclamped\n"); ++failures; }
    if (!failures) std::puts("PASS mde12 pad controls");
    return failures ? 1 : 0;
}
