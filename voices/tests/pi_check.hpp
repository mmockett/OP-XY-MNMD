#pragma once
// Shared by the P-I machines' sound checks (voices/tests/pi_*.cpp): plays one
// hit of a voice module at middle C and measures it.
#include <cmath>
#include <cstdio>
#include <opxy/audio.hpp>

namespace check {
int failures = 0;
inline void expect(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 1380;   // two seconds
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;   // 261.63 Hz at 44.1 kHz

inline opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }   // 0..127 -> Q31

// One hit of module V with these eight 0..127 values, into out (kFrames).
template <class State, class Reset, class Trigger, class Render>
void play(Reset reset, Trigger trigger, Render render, const int* values, opxy::i32* out) {
    static State voice;
    md::Common shared = {};
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    reset(voice, shared, 1);
    trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
}

inline unsigned at(double seconds) { return static_cast<unsigned>(seconds * 44100); }

inline double rms(const opxy::i32* out, double from, double seconds) {
    double sum = 0;
    const unsigned a = at(from), n = at(seconds);
    for (unsigned i = a; i < a + n; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / n) / 2147483648.0;
}
inline double db(double x) { return 20 * std::log10(x + 1e-12); }

inline bool bounded(const opxy::i32* out) {
    for (unsigned i = 0; i < kFrames; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}

// Zero crossings per second over a window: twice the frequency of a sine.
inline double frequency(const opxy::i32* out, double from, double seconds) {
    unsigned crossings = 0;
    const unsigned a = at(from), n = at(seconds);
    for (unsigned i = a + 1; i < a + n; ++i) crossings += (out[i - 1] < 0) != (out[i] < 0);
    return crossings * 44100.0 / n / 2;
}

// The level of one frequency over a window (Goertzel), relative units.
inline double tone(const opxy::i32* out, double hz, double from, double seconds) {
    const unsigned a = at(from), n = at(seconds);
    const double w = 2 * std::cos(2 * 3.14159265358979 * hz / 44100);
    double s1 = 0, s2 = 0;
    for (unsigned i = a; i < a + n; ++i) {
        const double s = out[i] / 2147483648.0 + w * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return std::sqrt(s1 * s1 + s2 * s2 - w * s1 * s2) / n;
}

// The strongest of `count` frequencies from low to high (log steps) over a window.
inline double strongest(const opxy::i32* out, double low, double high, double from, double seconds, int count = 200) {
    double best = 0, at_hz = low;
    for (int k = 0; k < count; ++k) {
        const double hz = low * std::pow(high / low, k / (count - 1.0));
        const double level = tone(out, hz, from, seconds);
        if (level > best) { best = level; at_hz = hz; }
    }
    return at_hz;
}
}
