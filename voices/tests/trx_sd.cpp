// Sound checks for MD Snare, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_sd.hpp"
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
    static md::trx_sd::State voice;
    md::Common shared = {};
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::trx_sd::reset(voice, shared, 1);
    md::trx_sd::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::trx_sd::render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
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

    const int crack[8] = {50, 50, 40, 30, 60, 64, 40, 0};
    play(crack, out);
    check("sounds", rms(out, 0, 4410) > 0.01);
    check("stays within the declared peak", bounded(out));
    check("decays", rms(out, 22050, 4410) < rms(out, 0, 4410) * 0.2);

    // Fitted to the Machinedrum (reference/fit_sd.py): the body sits at 339 .. 345 Hz
    // whatever PTCH is; TONE 127 adds the wires, a 6.98 kHz ring.
    const int body[8] = {64, 90, 0, 30, 0, 0, 64, 0};
    play(body, out);
    const double tone = frequency(out, 2205, 4410);
    std::printf("body pitch %.1f Hz\n", tone);
    check("body is the Machinedrum's 342 Hz", tone > 335 && tone < 350);
    const int wires[8] = {64, 90, 0, 30, 0, 127, 64, 0};
    play(wires, out);
    const double hiss = frequency(out, 2205, 4410);
    std::printf("wires crossings as Hz %.1f\n", hiss);
    check("wires are noisy", hiss > 1000);

    const int hot[8] = {127, 127, 127, 127, 127, 64, 127, 127};
    play(hot, out);
    check("hot settings stay bounded", bounded(out));

    if (!failures) std::puts("PASS trx_sd");
    return failures ? 1 : 0;
}
