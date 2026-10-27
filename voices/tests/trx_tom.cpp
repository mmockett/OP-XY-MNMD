// Sound checks for TRX-XT and TRX-XC, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_tom.hpp"
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

void play(bool conga, const int* values, opxy::i32* out, unsigned blocks = kBlocks) {
    static md::trx_tom::State voice;
    md::Common shared = {};
    opxy::i32 p[8] = {};
    for (int i = 0; i < 7; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    if (conga) { md::trx_xc::reset(voice, shared, 1); md::trx_xc::trigger(voice, shared); }
    else { md::trx_xt::reset(voice, shared, 1); md::trx_xt::trigger(voice, shared); }
    for (unsigned b = 0; b < blocks; ++b) {
        if (conga) md::trx_xc::render(voice, shared, out + b * opxy::block_frames, p, pitch);
        else md::trx_xt::render(voice, shared, out + b * opxy::block_frames, p, pitch);
    }
}

double rms(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / count) / 2147483648.0;
}
double peak(const opxy::i32* out, unsigned from, unsigned count) {
    double top = 0;
    for (unsigned i = from; i < from + count; ++i) top = std::fmax(top, std::fabs(out[i] / 2147483648.0));
    return top;
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
// Seconds for the level, in 10 ms windows from 10 ms on, to fall 40 dB.
double fall40(const opxy::i32* out) {
    const double start = rms(out, 441, 441);
    for (unsigned w = 2; w < kFrames / 441; ++w)
        if (rms(out, w * 441, 441) < start * 0.01) return w * 0.01 - 0.01;
    return 1.0;
}
double db(double x) { return 20 * std::log10(x + 1e-12); }
}

int main() {
    static opxy::i32 out[kFrames];
    // Fitted to the Machinedrum (reference/fit_tom.py), at middle C.
    for (int conga = 0; conga < 2; ++conga) {
        const char* name = conga ? "TRX-XC" : "TRX-XT";
        char what[128];
        // Base pitch: the tom 72.5 Hz x 2^(1.99 PTCH), the conga 86.5 Hz x 2^(1.98 PTCH).
        const double low = conga ? 86.8 : 72.0, middle = conga ? 172.9 : 144.7, high = conga ? 341.1 : 287.3;
        const int ptch[3] = {0, 64, 127};
        const double want[3] = {low, middle, high};
        for (int k = 0; k < 3; ++k) {
            const int v[7] = {ptch[k], 100, 0, 64, 0, 0, 0};
            play(conga, v, out);
            const double hz = frequency(out, 8820, 8820);
            std::printf("%s PTCH %3d: %.1f Hz (Machinedrum %.1f)\n", name, ptch[k], hz, want[k]);
            std::snprintf(what, sizeof what, "%s PTCH %d is the Machinedrum's pitch", name, ptch[k]);
            check(what, std::fabs(hz / want[k] - 1) < 0.03);
        }
        // RAMP adds about 356 Hz (conga 366) at its start, falling with a 48 ms (44 ms) time constant.
        const int swept[7] = {64, 100, 127, 64, 0, 0, 0};
        play(conga, swept, out);
        const double early = frequency(out, 441, 441), later = frequency(out, 4410, 882);
        std::printf("%s RAMP 127: %.0f Hz at 10 ms, %.0f at 100 ms\n", name, early, later);
        std::snprintf(what, sizeof what, "%s sweeps down from above", name);
        check(what, early > middle + 200 && early < middle + 360 && later < middle + 60);
        // DEC 64 falls 40 dB in about 0.55 s (tom) or 0.27 s (conga), as measured (-60 dB in 821 / 402 ms).
        const int decay[7] = {64, 64, 0, 64, 0, 0, 0};
        play(conga, decay, out);
        const double t = fall40(out), expect = conga ? 0.268 : 0.547;
        std::printf("%s DEC 64: -40 dB after %.3f s (Machinedrum %.3f)\n", name, t, expect);
        std::snprintf(what, sizeof what, "%s DEC 64 decays as the Machinedrum's", name);
        check(what, std::fabs(t / expect - 1) < 0.15);
        // The level: the body peaks near -11 dB of full scale, as the Machinedrum's.
        const int clean[7] = {64, 127, 0, 64, 0, 0, 0};
        play(conga, clean, out);
        const double top = db(peak(out, 0, 4410));
        std::printf("%s peak %.1f dB\n", name, top);
        std::snprintf(what, sizeof what, "%s peaks as the Machinedrum's", name);
        check(what, top > -12.5 && top < -10);
        // DAMP 127 halves the body.
        const double body = rms(out, 2205, 2205);
        const int damped[7] = {64, 127, 0, 64, 127, 0, 0};
        play(conga, damped, out);
        std::snprintf(what, sizeof what, "%s DAMP halves the body", name);
        check(what, std::fabs(db(rms(out, 2205, 2205) / body) + 6) < 1);
        // DIST 127 clips it about 9 dB (conga 7 dB) down.
        const int hot[7] = {64, 127, 0, 64, 0, 127, 0};
        play(conga, hot, out);
        const double drop = db(peak(out, 441, 4410)) - top;
        std::printf("%s DIST 127: peak %.1f dB under the clean one\n", name, drop);
        std::snprintf(what, sizeof what, "%s DIST clips it down", name);
        check(what, drop < (conga ? -4.5 : -6.5) && drop > -11);
        // Bounded at both ends of every control.
        for (int extreme = 0; extreme < 2; ++extreme) {
            int v[7];
            for (int i = 0; i < 7; ++i) v[i] = extreme ? 127 : 0;
            play(conga, v, out);
            std::snprintf(what, sizeof what, "%s bounded with every control at %d", name, extreme * 127);
            check(what, bounded(out));
        }
    }
    if (!failures) std::puts("PASS trx_tom");
    return failures ? 1 : 0;
}
