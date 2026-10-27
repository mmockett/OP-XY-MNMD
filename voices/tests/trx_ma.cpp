// Sound checks for TRX-MA, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_ma.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;
opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }

void play(const int* values, opxy::i32* out) {
    static md::trx_ma::State voice;
    md::Common shared = {};
    opxy::i32 p[8];
    for (int i = 0; i < 8; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::trx_ma::reset(voice, shared, 1);
    md::trx_ma::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::trx_ma::render(voice, shared, out + b * opxy::block_frames, p, pitch);
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
double frequency(const opxy::i32* out, unsigned from, unsigned count) {
    unsigned crossings = 0;
    for (unsigned i = from + 1; i < from + count; ++i) crossings += (out[i - 1] < 0) != (out[i] < 0);
    return crossings * 44100.0 / count / 2;
}
double db(double x) { return 20 * std::log10(x + 1e-12); }
}

int main() {
    static opxy::i32 out[kFrames];
    // Fitted to the Machinedrum (reference/fit_ma.py): at SUS 40 a bright burst
    // about 70 ms long, gated off.
    const int shake[8] = {0, 40, 0, 0, 40, 0, 64, 64};
    play(shake, out);
    check("sounds", rms(out, 0, 2205) > 0.02);
    check("stays within the declared peak", bounded(out));
    check("is bright", frequency(out, 0, 2205) > 4000);
    check("is gated off by 100 ms", db(rms(out, 4410, 2205)) < db(rms(out, 0, 2205)) - 40);
    // SUS 127 runs about 180 ms.
    const int longer[8] = {0, 127, 0, 0, 40, 0, 64, 64};
    play(longer, out);
    check("SUS 127 still sounds at 150 ms", db(rms(out, 6615, 441)) > db(rms(out, 0, 2205)) - 10);
    // ATT 127 rises over about 220 ms.
    const int slow[8] = {127, 40, 0, 0, 40, 0, 64, 64};
    play(slow, out);
    check("ATT rises", db(rms(out, 0, 441)) < db(rms(out, 8820, 441)) - 15);
    // REV 64: the return shake, after a gap.
    const int back[8] = {0, 40, 64, 0, 40, 0, 64, 64};
    play(back, out);
    check("REV shakes back", db(rms(out, 5292, 441)) > db(rms(out, 3970, 220)) + 15);
    // TONE moves the band up.
    const int dark[8] = {0, 40, 0, 0, 40, 0, 0, 64}, bright[8] = {0, 40, 0, 0, 40, 0, 127, 64};
    play(dark, out);
    const double low = frequency(out, 0, 2205);
    play(bright, out);
    check("TONE brightens", frequency(out, 0, 2205) > low * 1.2);
    for (int extreme = 0; extreme < 2; ++extreme) {
        int v[8];
        for (int i = 0; i < 8; ++i) v[i] = extreme * 127;
        play(v, out);
        check(extreme ? "bounded at 127" : "bounded at 0", bounded(out));
    }
    if (!failures) std::puts("PASS trx_ma");
    return failures ? 1 : 0;
}
