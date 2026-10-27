// Sound checks for GND-NOIS, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Monomachine's, from reference/fit_mgnd.py.
#include "../gnd_nois.hpp"
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

// The share of the energy below 400 Hz, from a crude two-band split.
double low_share(const opxy::i32* out) {
    double low = 0, all = 0, state = 0;
    const double a = 1 - std::exp(-2 * M_PI * 400 / 44100.0);
    for (unsigned i = 4410; i < kFrames; ++i) {
        const double x = out[i] / 2147483648.0;
        state += a * (x - state);
        low += state * state; all += x * x;
    }
    return low / all;
}

int main() {
    static opxy::i32 out[kFrames];
    static md::gnd_nois::State voice;
#define NOIS voice, md::gnd_nois::reset, md::gnd_nois::trigger, md::gnd_nois::render
    // ST RED STON - - - - TUNE
    const int white[8] = {0, 0, 0, 0, 0, 0, 0, 64}, pinkish[8] = {0, 64, 0, 0, 0, 0, 0, 64}, red[8] = {0, 127, 0, 0, 0, 0, 0, 64};
    play(NOIS, white, out);
    const double w = rms(out, 4410, kFrames - 4410), w_low = low_share(out);
    play(NOIS, pinkish, out);
    const double p = rms(out, 4410, kFrames - 4410);
    play(NOIS, red, out);
    const double r = rms(out, 4410, kFrames - 4410), r_low = low_share(out);
    std::printf("white %.4f, RED 64 %.4f, RED 127 %.4f rms; below 400 Hz %.3f, %.3f of the energy\n", w, p, r, w_low, r_low);
    // The Monomachine's 0.047 rms x the engine's 1.2; RED 64 1.45 x, RED 127 0.47 x.
    check("the fitted level", w > 0.052 && w < 0.061);
    check("RED is loudest at its middle", p / w > 1.3 && p / w < 1.6);
    check("RED 127 is quieter", r / w > 0.38 && r / w < 0.55);
    check("RED 127 is red", r_low > 0.55 && w_low < 0.2);
    // ST: +2.9 dB at 127 with STON off, nothing with it on.
    const int st[8] = {127, 0, 0, 0, 0, 0, 0, 64}, st_on[8] = {127, 0, 127, 0, 0, 0, 0, 64};
    play(NOIS, st, out);
    const double s = rms(out, 4410, kFrames - 4410) / w;
    play(NOIS, st_on, out);
    const double s_on = rms(out, 4410, kFrames - 4410) / w;
    std::printf("ST 127: %.2f x, with STON %.2f x\n", s, s_on);
    check("ST raises the level", s > 1.3 && s < 1.5);
    check("STON leaves the level", s_on > 0.95 && s_on < 1.05);
    // Every setting stays inside the ceiling.
    bool ok = true;
    for (int setting = 0; setting < 3; ++setting) {
        int values[8];
        for (int i = 0; i < 8; ++i) values[i] = setting == 0 ? 0 : setting == 1 ? 64 : 127;
        play(NOIS, values, out);
        ok = ok && bounded(out);
    }
    const int loud[8] = {127, 64, 0, 0, 0, 0, 0, 64};
    play(NOIS, loud, out);
    check("stays within the declared peak", ok && bounded(out));
    if (!failures) std::puts("PASS gnd_nois");
    return failures ? 1 : 0;
}
