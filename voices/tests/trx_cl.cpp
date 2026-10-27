// Sound checks for TRX-CL, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_cl.hpp"
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
    static md::trx_cl::State voice;
    md::Common shared = {};
    opxy::i32 p[8] = {};
    for (int i = 0; i < 6; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::trx_cl::reset(voice, shared, 1);
    md::trx_cl::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::trx_cl::render(voice, shared, out + b * opxy::block_frames, p, pitch);
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
    // Fitted to the Machinedrum (reference/fit_cl.py), at middle C: the tone is
    // 942 Hz + 2188 Hz x PTCH; DEC 64 falls 60 dB in 378 ms.
    const int ptch[3] = {0, 64, 127};
    const double want[3] = {942.8, 2045.0, 3129.7};
    for (int k = 0; k < 3; ++k) {
        const int v[6] = {ptch[k], 100, 0, 40, 0, 0};
        play(v, out);
        const double hz = frequency(out, 2205, 4410);
        std::printf("PTCH %3d: %.1f Hz (Machinedrum %.1f)\n", ptch[k], hz, want[k]);
        check("the tone is the Machinedrum's pitch", std::fabs(hz / want[k] - 1) < 0.01);
    }
    const int clave[6] = {64, 64, 0, 40, 0, 0};
    play(clave, out);
    check("sounds", rms(out, 0, 441) > 0.05);
    check("stays within the declared peak", bounded(out));
    const double fall = db(rms(out, 8820, 441)) - db(rms(out, 441, 441));
    std::printf("DEC 64: %.1f dB at 200 ms (Machinedrum about -30)\n", fall);
    check("DEC 64 falls as the Machinedrum's", fall < -25 && fall > -35);
    // ENH strikes harder: about 4 dB.
    const int soft[6] = {64, 64, 0, 0, 0, 0}, hard[6] = {64, 64, 0, 127, 0, 0};
    play(soft, out);
    const double s = db(rms(out, 441, 1323));
    play(hard, out);
    check("ENH strikes 4 dB harder", std::fabs(db(rms(out, 441, 1323)) - s - 3.9) < 1.5);
    // DUAL: a quiet first strike, the main one 15 ms later.
    const int dual[6] = {64, 64, 127, 40, 0, 0};
    play(dual, out);
    const double before = db(rms(out, 220, 330)), after = db(rms(out, 750, 330));
    std::printf("DUAL 127: %.1f dB before the main strike, %.1f after\n", before, after);
    check("DUAL strikes twice", after - before > 6);
    // TUNE: a second tone, 22 dB under the first at PTCH 0.
    const int tuned[6] = {0, 100, 0, 40, 127, 0};
    play(tuned, out);
    const double second = db(line(out, 2205, 4410, 3129.7)) - db(line(out, 2205, 4410, 942.3));
    std::printf("TUNE 127: the second tone %.1f dB under the first\n", second);
    check("TUNE adds a tone 22 dB under", std::fabs(second + 22) < 3);
    for (int extreme = 0; extreme < 2; ++extreme) {
        int v[6];
        for (int i = 0; i < 6; ++i) v[i] = extreme * 127;
        play(v, out);
        check(extreme ? "bounded at 127" : "bounded at 0", bounded(out));
    }
    if (!failures) std::puts("PASS trx_cl");
    return failures ? 1 : 0;
}
