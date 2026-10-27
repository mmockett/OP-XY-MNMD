// Sound checks for GND-SIN, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Monomachine's, from reference/fit_mgnd.py.
#include "../gnd_sin.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const double kMiddleC = 261.6256;

opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }   // 0..127 -> Q31
opxy::u32 increment(double hz) { return static_cast<opxy::u32>(hz / 44100.0 * 4294967296.0); }

// Plays one machine's voice for a second at a note; `again` keeps the voice's
// state from the last note, as the next note on the same voice would.
template <class State, class Reset, class Trigger, class Render>
void play(State& voice, Reset reset, Trigger trigger, Render render, const int* values, opxy::i32* out,
          double hz = kMiddleC, bool again = false) {
    md::Common shared = {};
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = hz == kMiddleC ? 25479004u : increment(hz);
    if (!again) reset(voice, shared, 1);
    trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
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

// The amplitude of one frequency over a window, Hann-weighted (a single DFT bin).
double line(const opxy::i32* out, unsigned from, unsigned count, double hz) {
    double re = 0, im = 0, weight = 0;
    for (unsigned i = 0; i < count; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / count);
        const double x = out[from + i] / 2147483648.0;
        re += w * x * std::cos(2 * M_PI * hz * i / 44100);
        im += w * x * std::sin(2 * M_PI * hz * i / 44100);
        weight += w;
    }
    return 2 * std::sqrt(re * re + im * im) / weight;
}
// The strongest frequency near a guess, to 0.1 Hz.
double peak(const opxy::i32* out, unsigned from, unsigned count, double guess) {
    double best = guess, most = 0;
    for (double f = guess * 0.97; f < guess * 1.03; f += 0.1) {
        const double a = line(out, from, count, f);
        if (a > most) { most = a; best = f; }
    }
    return best;
}
double cents(double f, double ref) { return 1200 * std::log2(f / ref); }

}

int main() {
    static opxy::i32 out[kFrames];
    static md::gnd_sin::State voice;
#define SIN voice, md::gnd_sin::reset, md::gnd_sin::trigger, md::gnd_sin::render
    const int rest[8] = {0, 0, 0, 0, 0, 0, 0, 64};
    for (int octave = -3; octave <= 3; ++octave) {
        const double hz = kMiddleC * std::pow(2.0, octave);
        play(SIN, rest, out, hz);
        const double at = peak(out, 13230, 30870, hz);
        std::printf("octave %+d: %.2f Hz, peak %.3f\n", octave, at, [&] { double m = 0; for (unsigned i = 0; i < kFrames; ++i) m = std::max(m, std::fabs(out[i] / 2147483648.0)); return m; }());
        check("plays the key", std::fabs(cents(at, hz)) < 2);
        check("stays within the declared peak", bounded(out));
    }
    // The Monomachine's 0.183 peak, times the engine's 2.
    play(SIN, rest, out);
    double most = 0;
    for (unsigned i = 0; i < kFrames; ++i) most = std::max(most, std::fabs(out[i] / 2147483648.0));
    check("the fitted level", most > 0.36 && most < 0.372);
    check("a pure sine", line(out, 13230, 30870, 2 * kMiddleC) < 0.001 * line(out, 13230, 30870, kMiddleC));
    check("starts at zero", std::fabs(out[0] / 2147483648.0) < 0.001);
    // TUNE: +-2 semitones about 64.
    const int low[8] = {0, 0, 0, 0, 0, 0, 0, 0}, high[8] = {0, 0, 0, 0, 0, 0, 0, 127};
    play(SIN, low, out);
    const double down = cents(peak(out, 13230, 30870, kMiddleC * 0.89), kMiddleC);
    play(SIN, high, out);
    const double up = cents(peak(out, 13230, 30870, kMiddleC * 1.12), kMiddleC);
    std::printf("TUNE 0 %.1f cents, 127 %.1f cents\n", down, up);
    check("TUNE 0 is -2 semitones", std::fabs(down + 200) < 3);
    check("TUNE 127 is +1.97 semitones", std::fabs(up - 196.9) < 3);
    if (!failures) std::puts("PASS gnd_sin");
    return failures ? 1 : 0;
}
