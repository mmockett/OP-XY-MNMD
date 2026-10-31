// Sound checks for TRX-CY, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_cy.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 2070;   // three seconds
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;
opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }

void play(const int* values, opxy::i32* out, opxy::u32 owner = 1) {
    static md::trx_cy::State voice;
    md::Common shared = {};
    opxy::i32 p[8] = {};
    for (int i = 0; i < 6; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::trx_cy::reset(voice, shared, owner);
    md::trx_cy::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::trx_cy::render(voice, shared, out + b * opxy::block_frames, p, pitch);
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
// Energy in a band, by a single-pole pair: the signal high-passed at lo, low-passed at hi.
double band(const opxy::i32* out, unsigned from, unsigned count, double lo, double hi) {
    const double a = 1 - std::exp(-2 * M_PI * lo / 44100), b = 1 - std::exp(-2 * M_PI * hi / 44100);
    double l1 = 0, l2 = 0, h1 = 0, h2 = 0, sum = 0;
    for (unsigned i = 0; i < from + count; ++i) {
        const double x = out[i] / 2147483648.0;
        l1 += a * (x - l1); l2 += a * (x - l1 - l2);
        const double hp = x - l1 - l2;
        h1 += b * (hp - h1); h2 += b * (h1 - h2);
        if (i >= from) sum += h2 * h2;
    }
    return std::sqrt(sum / count);
}
double db(double x) { return 20 * std::log10(x + 1e-12); }
}

int main() {
    static opxy::i32 out[kFrames];
    // Fitted to the Machinedrum (reference/fit_cy.py): a body and a top near 5.6 kHz
    // about level in these bands (-55.9 and -55.3 dB there); DEC 50 is down about 22 dB at half a second.
    const int cymbal[6] = {64, 50, 64, 64, 64, 64};
    play(cymbal, out);
    check("sounds", rms(out, 0, 4410) > 0.01);
    check("stays within the declared peak", bounded(out));
    const double body = db(band(out, 882, 4410, 700, 1400)), top = db(band(out, 882, 4410, 4000, 8000));
    std::printf("body %.1f dB, top %.1f dB\n", body, top);
    check("the top stands near the body", top > body - 8);
    const double late = db(rms(out, 22050, 2205)) - db(rms(out, 882, 2205));
    std::printf("level at half a second: %.1f dB\n", late);
    check("DEC 50 falls as the Machinedrum's", late < -20 && late > -36);
    // SIZE moves the body: small is thin.
    const int big[6] = {64, 50, 64, 64, 0, 64}, small[6] = {64, 50, 64, 64, 127, 64};
    play(big, out);
    const double big_low = db(band(out, 882, 4410, 300, 900));
    play(small, out);
    // 11 dB on the Machinedrum, in this band.
    check("SIZE 127 thins the body", db(band(out, 882, 4410, 300, 900)) < big_low - 10);
    // TOP: the top 15 dB between its ends; less in this band, which holds some of the body.
    const int dull[6] = {64, 50, 0, 64, 64, 64}, sharp[6] = {64, 50, 127, 64, 64, 64};
    play(dull, out);
    const double t0 = db(band(out, 882, 4410, 4500, 8000));
    play(sharp, out);
    const double t1 = db(band(out, 882, 4410, 4500, 8000));
    std::printf("TOP 0 to 127: the top %+.1f dB\n", t1 - t0);
    check("TOP raises the top", t1 - t0 > 8 && t1 - t0 < 20);
    // DEC 127 still rings at two seconds.
    const int crash[6] = {64, 127, 64, 64, 64, 64};
    play(crash, out);
    check("DEC 127 rings on", db(rms(out, 88200, 4410)) - db(rms(out, 882, 2205)) > -40);
    // The squares run free: two hits differ.
    static opxy::i32 other[kFrames];
    play(cymbal, other, 9);
    play(cymbal, out, 1);
    double diff = 0;
    for (unsigned i = 0; i < 4410; ++i) diff += std::fabs(static_cast<double>(out[i]) - other[i]);
    check("each hit is its own", diff > 0);
    for (int extreme = 0; extreme < 2; ++extreme) {
        int v[6];
        for (int i = 0; i < 6; ++i) v[i] = extreme * 127;
        play(v, out);
        check(extreme ? "bounded at 127" : "bounded at 0", bounded(out));
    }
    if (!failures) std::puts("PASS trx_cy");
    return failures ? 1 : 0;
}
