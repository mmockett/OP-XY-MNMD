// Sound checks for MD Clap, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_cp.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;   // 261.63 Hz at 44.1 kHz

opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }   // 0..127 -> Q31

void play(const int* values, opxy::i32* out) {
    static md::trx_cp::State voice;
    md::Common shared = {};
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::trx_cp::reset(voice, shared, 1);
    md::trx_cp::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::trx_cp::render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
}

double rms(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / count) / 2147483648.0;
}

bool bounded(const opxy::i32* out) {
    for (unsigned i = 0; i < kFrames; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}

// Peaks of the level in 1 ms windows over the first 200 ms: one per hand.
unsigned hands(const opxy::i32* out) {
    double previous = 0, before = 0;
    unsigned peaks = 0;
    for (unsigned w = 0; w < 200; ++w) {
        const double now = rms(out, w * 44, 44);
        if (previous > before && previous > now * 1.6 && previous > 0.01) ++peaks;
        before = previous; previous = now;
    }
    return peaks;
}
}

int main() {
    static opxy::i32 out[kFrames];

    const int clap[8] = {80, 64, 64, 40, 64, 0, 40, 64};
    play(clap, out);
    check("sounds", rms(out, 0, 4410) > 0.005);
    check("stays within the declared peak", bounded(out));
    const unsigned counted = hands(out);
    std::printf("hands heard %u\n", counted);
    check("several hands", counted >= 3);
    check("dry clap ends", rms(out, 22050, 4410) < 0.001);

    const int roomy[8] = {80, 64, 64, 40, 64, 127, 100, 64};
    play(roomy, out);
    std::printf("room at 250 ms %.4f, start %.4f\n", rms(out, 11025, 4410), rms(out, 0, 4410));
    // As the Machinedrum measures (reference/fit_cp.py): a long room is about
    // 45 dB under the hands a quarter of a second on, where a dry clap is silent.
    check("the room rings on", rms(out, 11025, 4410) > rms(out, 0, 4410) * 0.003);

    // The room's skirt: an octave and more above it, 25 dB and more under its band.
    const int band_room[8] = {80, 64, 64, 40, 64, 127, 100, 64};
    play(band_room, out);
    {
        double low1 = 0, low2 = 0, sum = 0, all = 0;
        const double a = 1 - std::exp(-2 * M_PI * 4800 / 44100);
        for (unsigned i = 0; i < 13230; ++i) {
            const double x = out[i] / 2147483648.0;
            low1 += a * (x - low1); low2 += a * (x - low1 - low2);
            if (i >= 6615) { const double high = x - low1 - low2; sum += high * high; all += x * x; }
        }
        check("the room has no bright skirt", sum < all * 0.003);
    }

    const int hot[8] = {127, 127, 127, 127, 0, 127, 127, 127};
    play(hot, out);
    check("hot settings stay bounded", bounded(out));
    const int one_hand[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    play(one_hand, out);
    check("one hand stays bounded", bounded(out));

    if (!failures) std::puts("PASS trx_cp");
    return failures ? 1 : 0;
}
