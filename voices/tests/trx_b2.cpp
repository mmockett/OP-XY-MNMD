// Sound checks for TRX-B2, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_b2.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 1380;   // two seconds
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;
opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }

void play(const int* values, opxy::i32* out) {
    static md::trx_b2::State voice;
    md::Common shared = {};
    opxy::i32 p[8];
    for (int i = 0; i < 8; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::trx_b2::reset(voice, shared, 1);
    md::trx_b2::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::trx_b2::render(voice, shared, out + b * opxy::block_frames, p, pitch);
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
    // Fitted to the Machinedrum (reference/fit_b2.py), at middle C: the base runs
    // 10 Hz at PTCH 0, 50.3 at 64 and 106.4 at 127.
    const int ptch[3] = {0, 64, 127};
    const double want[3] = {10.0, 50.3, 106.4};
    for (int k = 0; k < 3; ++k) {
        const int v[8] = {ptch[k], 127, 0, 0, 0, 0, 0, 0};
        play(v, out);
        const double hz = frequency(out, 8820, 35280);
        std::printf("PTCH %3d: %.1f Hz (Machinedrum %.1f)\n", ptch[k], hz, want[k]);
        check("the base is the Machinedrum's", std::fabs(hz / want[k] - 1) < 0.04);
    }
    // RAMP 127 adds about 457 Hz, falling with a 38 ms time constant.
    const int swept[8] = {64, 100, 127, 0, 0, 0, 0, 0};
    play(swept, out);
    const double early = frequency(out, 0, 441), later = frequency(out, 4410, 4410);
    std::printf("RAMP 127: %.0f Hz in the first 10 ms, %.0f at 100-200 ms\n", early, later);
    check("RAMP sweeps down from above", early > 350 && later < 80);
    const int kick[8] = {64, 64, 0, 0, 0, 0, 0, 0};
    play(kick, out);
    check("sounds", rms(out, 0, 4410) > 0.05);
    check("stays within the declared peak", bounded(out));
    // DEC 64: -60 dB in about half a second after a 50 ms shoulder.
    const double fall = db(rms(out, 13230, 2205)) - db(rms(out, 441, 882));
    std::printf("DEC 64: %.1f dB at 300 ms\n", fall);
    check("DEC 64 falls as the Machinedrum's", fall < -25 && fall > -45);
    // HOLD 64 holds about 190 ms, then DEC 32 drops it.
    const int held[8] = {64, 32, 0, 64, 0, 0, 0, 0};
    play(held, out);
    check("HOLD holds", db(rms(out, 7056, 441)) > db(rms(out, 882, 441)) - 3);
    check("then it falls", db(rms(out, 13230, 441)) < db(rms(out, 882, 441)) - 30);
    // DEC 127 does not fall.
    const int endless[8] = {64, 127, 0, 0, 0, 0, 0, 0};
    play(endless, out);
    check("DEC 127 holds", db(rms(out, 80000, 4410)) > db(rms(out, 4410, 4410)) - 1);
    // DIST squares it; DIRT 127 crumbles it to three levels.
    const int clean[8] = {64, 100, 0, 0, 0, 0, 0, 0}, square[8] = {64, 100, 0, 0, 0, 0, 0, 127};
    play(clean, out);
    const double plain = db(rms(out, 882, 4410));
    play(square, out);
    std::printf("DIST 127: %+.1f dB over the clean kick\n", db(rms(out, 882, 4410)) - plain);
    check("DIST squares it louder", db(rms(out, 882, 4410)) > plain + 2);
    const int crumbled[8] = {64, 100, 0, 0, 0, 0, 127, 0};
    play(crumbled, out);
    // Three levels under the envelope: within a few milliseconds, only a few values.
    unsigned levels = 0;
    opxy::i32 seen[16];
    for (unsigned i = 2205; i < 2265 && levels < 16; ++i) {
        bool found = false;
        for (unsigned k = 0; k < levels; ++k) found = found || std::abs(seen[k] - out[i]) < 400000;
        if (!found) seen[levels++] = out[i];
    }
    std::printf("DIRT 127: %u levels in a short stretch\n", levels);
    check("DIRT 127 crumbles it to a few levels", levels <= 5);
    // DIRT and DIST shape the sine before the envelope: however far a hit has
    // fallen it keeps sounding, at DEC's rate, as the Machinedrum's does.
    const int fading[8] = {64, 60, 0, 0, 0, 0, 0, 0}, fading_dirt[8] = {64, 60, 0, 0, 0, 0, 127, 0}, fading_dist[8] = {64, 60, 0, 0, 0, 0, 0, 127};
    play(fading, out);
    const double plain_late = db(rms(out, 8820, 4410));
    play(fading_dirt, out);
    std::printf("DIRT 127 at 200-300 ms: %+.1f dB over clean\n", db(rms(out, 8820, 4410)) - plain_late);
    check("DIRT 127 keeps falling with the clean kick", std::fabs(db(rms(out, 8820, 4410)) - plain_late) < 3);
    play(fading_dist, out);
    std::printf("DIST 127 at 200-300 ms: %+.1f dB over clean\n", db(rms(out, 8820, 4410)) - plain_late);
    check("DIST 127 keeps falling with the clean kick", std::fabs(db(rms(out, 8820, 4410)) - plain_late) < 5);
    for (int extreme = 0; extreme < 2; ++extreme) {
        int v[8];
        for (int i = 0; i < 8; ++i) v[i] = extreme * 127;
        play(v, out);
        check(extreme ? "bounded at 127" : "bounded at 0", bounded(out));
    }
    if (!failures) std::puts("PASS trx_b2");
    return failures ? 1 : 0;
}
