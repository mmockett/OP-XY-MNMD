// Sound checks for TRX-RS, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_rs.hpp"
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

void play(const int* values, opxy::i32* out, opxy::u32 owner = 1) {
    static md::trx_rs::State voice;
    md::Common shared = {};
    opxy::i32 p[8] = {};
    for (int i = 0; i < 3; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::trx_rs::reset(voice, shared, owner);
    md::trx_rs::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::trx_rs::render(voice, shared, out + b * opxy::block_frames, p, pitch);
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
// The level of the signal's first difference: weighs the highs.
double edges(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from + 1; i < from + count; ++i) { const double d = static_cast<double>(out[i]) - out[i - 1]; sum += d * d; }
    return std::sqrt(sum / count) / 2147483648.0;
}
double db(double x) { return 20 * std::log10(x + 1e-12); }
}

int main() {
    static opxy::i32 out[kFrames];
    // Fitted to the Machinedrum (reference/fit_rs.py), at middle C: the tone is
    // 172 Hz + 171 Hz x PTCH; the rim is gone in 25 ms, the tone rings for DEC.
    const int ptch[3] = {0, 64, 127};
    const double want[3] = {172.3, 258.4, 343.2};
    for (int k = 0; k < 3; ++k) {
        const int v[3] = {ptch[k], 100, 0};
        play(v, out);
        const double hz = frequency(out, 4410, 8820);
        std::printf("PTCH %3d: tone %.1f Hz (Machinedrum %.1f)\n", ptch[k], hz, want[k]);
        check("the tone is the Machinedrum's pitch", std::fabs(hz / want[k] - 1) < 0.02);
    }
    const int rim[3] = {64, 64, 0};
    play(rim, out);
    check("sounds", rms(out, 0, 441) > 0.03);
    check("stays within the declared peak", bounded(out));
    const double burst = db(rms(out, 0, 265)), tone = db(rms(out, 2205, 441));
    std::printf("rim %.1f dB, tone at 50 ms %.1f dB\n", burst, tone);
    check("the rim stands over the tone", burst > tone + 5);
    check("the rim is bright", frequency(out, 0, 265) > 400);
    // DEC: the tone falls 60 dB in 601 ms at DEC 64; 40 dB after about 0.4 s.
    check("DEC 64 rings past a quarter second", db(rms(out, 11025, 441)) > tone - 30);
    check("DEC 64 is gone by a second", db(rms(out, 40000, 2000)) < tone - 40);
    const int shortest[3] = {64, 0, 0};
    play(shortest, out);
    check("DEC 0 is a click", db(rms(out, 8820, 441)) < tone - 50);
    // Two hits differ in detail: the rim's phase runs free.
    static opxy::i32 other[kFrames];
    play(rim, other, 7);
    double diff = 0;
    for (unsigned i = 0; i < 265; ++i) diff += std::fabs(static_cast<double>(out[i]) - other[i]);
    check("each hit is its own", diff > 0);
    // DIST adds odd harmonics but keeps the tone's level.
    const int hot[3] = {64, 100, 127};
    play(hot, out);
    const double hot_tone = db(rms(out, 2205, 4410));
    const int clean[3] = {64, 100, 0};
    play(clean, other);
    check("DIST keeps the level", std::fabs(hot_tone - db(rms(other, 2205, 4410))) < 2);
    check("DIST adds edges", edges(out, 2205, 4410) > edges(other, 2205, 4410) * 3);
    for (int extreme = 0; extreme < 2; ++extreme) {
        const int v[3] = {extreme * 127, extreme * 127, extreme * 127};
        play(v, out);
        check(extreme ? "bounded at 127" : "bounded at 0", bounded(out));
    }
    if (!failures) std::puts("PASS trx_rs");
    return failures ? 1 : 0;
}
