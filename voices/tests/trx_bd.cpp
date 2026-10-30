// Sound checks for MD Kick, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_bd.hpp"
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
    static md::trx_bd::State voice;
    md::Common shared = {};
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::trx_bd::reset(voice, shared, 1);
    md::trx_bd::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::trx_bd::render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
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

// Zero crossings per second over a window: twice the frequency of a sine.
double frequency(const opxy::i32* out, unsigned from, unsigned count) {
    unsigned crossings = 0;
    for (unsigned i = from + 1; i < from + count; ++i) crossings += (out[i - 1] < 0) != (out[i] < 0);
    return crossings * 44100.0 / count / 2;
}
}

int main() {
    static opxy::i32 out[kFrames];

    // PTCH 40 at middle C: 30 + 150 * 40/127 Hz in octaves, about 63 Hz once the sweep settles.
    const int round[8] = {40, 90, 44, 20, 0, 0, 0, 0};
    play(round, out);
    check("sounds", rms(out, 0, 4410) > 0.01);
    check("stays within the declared peak", bounded(out));
    check("decays", rms(out, 30870, 4410) < rms(out, 0, 4410) * 0.5);
    const double settled = frequency(out, 8820, 8820);
    std::printf("settled pitch %.1f Hz\n", settled);
    check("settles near its pitch", settled > 45 && settled < 90);

    // A deep, slow RAMP starts well above the settled pitch.
    const int swept[8] = {40, 90, 100, 80, 0, 0, 0, 0};
    play(swept, out);
    const double early = frequency(out, 0, 882);
    std::printf("first 20 ms of a deep sweep %.1f Hz\n", early);
    check("sweeps down from above", early > settled * 2);

    // A short decay is quiet after a quarter second; a long one is not.
    const int tight[8] = {40, 10, 44, 20, 0, 0, 0, 0};
    play(tight, out);
    check("short decay ends", rms(out, 11025, 4410) < 0.001);

    // Full CLIP and HARM still respect the ceiling.
    const int hot[8] = {60, 127, 127, 127, 127, 127, 127, 127};
    play(hot, out);
    check("hot settings stay bounded", bounded(out));
    check("hot settings sound", rms(out, 0, 4410) > 0.05);

    // Fitted to the Machinedrum (reference/fit_bd.py): the base pitch runs 30.6 Hz
    // at PTCH 0 to 120 Hz at 127, measured well after the sweeps have settled.
    const int low[8] = {0, 100, 0, 0, 0, 0, 0, 0};
    play(low, out);
    const double floor_hz = frequency(out, 13230, 8820);
    std::printf("PTCH 0 settles at %.1f Hz\n", floor_hz);
    check("PTCH 0 is the Machinedrum's 30.6 Hz", floor_hz > 28 && floor_hz < 33);
    const int high[8] = {127, 100, 0, 0, 0, 0, 0, 0};
    play(high, out);
    const double top_hz = frequency(out, 13230, 8820);
    std::printf("PTCH 127 settles at %.1f Hz\n", top_hz);
    check("PTCH 127 is the Machinedrum's 120 Hz", top_hz > 114 && top_hz < 127);

    // As the Machinedrum's: the click, then a dip well under it before the body
    // arrives (reference/fit_bd.py), peak at 3.6 ms.
    const int lands[8] = {40, 100, 0, 0, 0, 0, 0, 0};
    play(lands, out);
    double top = 0;
    for (unsigned i = 0; i < 2205; ++i) top = std::fmax(top, std::fabs(out[i] / 2147483648.0));
    const double dip = 20 * std::log10(rms(out, 220, 330) / top);
    std::printf("5-12.5 ms: %.1f dB under the peak\n", dip);
    check("the click stands over a dip", dip < -13 && dip > -22);
    check("the click's peak falls at 3 to 4 ms", std::fabs(out[160] / 2147483648.0) > top * 0.9);

    if (!failures) std::puts("PASS trx_bd");
    return failures ? 1 : 0;
}
