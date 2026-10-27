// Sound checks for TRX-CB, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_cb.hpp"
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
    static md::trx_cb::State voice;
    md::Common shared = {};
    opxy::i32 p[8] = {};
    for (int i = 0; i < 6; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::trx_cb::reset(voice, shared, owner);
    md::trx_cb::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::trx_cb::render(voice, shared, out + b * opxy::block_frames, p, pitch);
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
// The level of one frequency over a window (a single-bin DFT), as a sine's amplitude.
double line(const opxy::i32* out, unsigned from, unsigned count, double hz) {
    double re = 0, im = 0;
    for (unsigned i = 0; i < count; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / count);
        const double x = out[from + i] / 2147483648.0 * w;
        re += x * std::cos(2 * M_PI * hz * i / 44100);
        im += x * std::sin(2 * M_PI * hz * i / 44100);
    }
    return 4 * std::sqrt(re * re + im * im) / count;
}
double db(double x) { return 20 * std::log10(x + 1e-12); }
}

int main() {
    static opxy::i32 out[kFrames];
    // Fitted to the Machinedrum (reference/fit_cb.py): a 1117 Hz bell with a
    // 1051 Hz voice 7 dB under it at ENH 40, a line at 1752 Hz 22 dB under.
    const int bell[6] = {64, 100, 40, 0, 64, 0};
    play(bell, out);
    check("sounds", rms(out, 0, 4410) > 0.02);
    check("stays within the declared peak", bounded(out));
    const double a = db(line(out, 882, 1764, 1117)), b = db(line(out, 882, 1764, 1051)), c = db(line(out, 882, 1764, 1752));
    std::printf("1117 Hz %.1f dB, 1051 Hz %.1f under, 1752 Hz %.1f under\n", a, b - a, c - a);
    check("the bell is at 1117 Hz, near -14 dB", a > -16 && a < -12.5);
    check("the second voice is about 7 dB under", b - a > -10 && b - a < -4);
    check("the line is about 22 dB under", c - a > -26 && c - a < -18);
    // ENH 80 brings the second voice level with the bell; DAMP 127 takes it away.
    const int enhanced[6] = {64, 100, 80, 0, 64, 0};
    play(enhanced, out);
    check("ENH 80 levels the voices", std::fabs(db(line(out, 882, 1764, 1051)) - db(line(out, 882, 1764, 1117))) < 3);
    const int damped[6] = {64, 100, 40, 127, 64, 0};
    play(damped, out);
    check("DAMP takes the second voice away", db(line(out, 882, 1764, 1051)) - db(line(out, 882, 1764, 1117)) < -20);
    // TONE brightens: the 3rd harmonic from 26 to 12 dB under the fundamental.
    const int dark[6] = {64, 100, 40, 0, 0, 0}, bright[6] = {64, 100, 40, 0, 127, 0};
    play(dark, out);
    const double d3 = db(line(out, 882, 1764, 3351)) - db(line(out, 882, 1764, 1117));
    play(bright, out);
    const double b3 = db(line(out, 882, 1764, 3351)) - db(line(out, 882, 1764, 1117));
    std::printf("3rd harmonic: TONE 0 %.1f dB, TONE 127 %.1f dB\n", d3, b3);
    check("TONE brightens", d3 < -22 && b3 > -15);
    // DEC 64: -60 dB in about 173 ms; at 200 ms it is well down.
    const int decay[6] = {64, 64, 40, 0, 64, 0};
    play(decay, out);
    check("DEC 64 has fallen 40 dB by 200 ms", db(rms(out, 8820, 441)) < db(rms(out, 441, 441)) - 40);
    // The oscillators run free: two hits differ.
    static opxy::i32 other[kFrames];
    play(bell, out, 1);
    play(bell, other, 2);
    double diff = 0;
    for (unsigned i = 0; i < 4410; ++i) diff += std::fabs(static_cast<double>(out[i]) - other[i]);
    check("each hit is its own", diff > 0);
    for (int extreme = 0; extreme < 2; ++extreme) {
        int v[6];
        for (int i = 0; i < 6; ++i) v[i] = extreme * 127;
        play(v, out);
        check(extreme ? "bounded at 127" : "bounded at 0", bounded(out));
    }
    if (!failures) std::puts("PASS trx_cb");
    return failures ? 1 : 0;
}
