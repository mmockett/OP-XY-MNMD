// What the EFM and GND voices' checks share: play a voice at middle C and
// measure it. Each test includes its module, then this, then names the module:
//   #include "../efm_bd.hpp"
//   #include "efm_check.hpp"
//   #define PLAY check::play<md::efm_bd::State, md::efm_bd::reset, md::efm_bd::trigger, md::efm_bd::render>
#include <cmath>
#include <cstdio>

namespace check {
int failures = 0;
inline void expect(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;   // 261.63 Hz at 44.1 kHz
static opxy::i32 out[kFrames * 3];      // up to three seconds

inline opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }   // 0..127 -> Q31

// Plays one hit of `seconds` (1..3) into out[].
template <class State, void (*Reset)(State&, md::Common&, opxy::u32), void (*Trigger)(State&, md::Common&),
          void (*Render)(State&, md::Common&, opxy::Output, const opxy::i32*, opxy::Pitch)>
void play(const int* values, unsigned seconds = 1, opxy::u32 increment = kMiddleC) {
    static State voice;
    md::Common shared = {};
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = increment;
    Reset(voice, shared, 1);
    Trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks * seconds; ++b) Render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
}

inline double rms(unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / count) / 2147483648.0;
}
inline double db(double x) { return 20 * std::log10(x + 1e-12); }
inline double ms(double t) { return t * 44.1; }   // milliseconds -> samples

inline double peak(unsigned from, unsigned count) {
    double p = 0;
    for (unsigned i = from; i < from + count; ++i) p = std::fmax(p, std::fabs(out[i] / 2147483648.0));
    return p;
}

inline bool bounded(unsigned seconds = 1) {
    for (unsigned i = 0; i < kFrames * seconds; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}

// Zero crossings per second over a window: twice the frequency of a sine.
inline double frequency(unsigned from, unsigned count) {
    unsigned crossings = 0;
    for (unsigned i = from + 1; i < from + count; ++i) crossings += (out[i - 1] < 0) != (out[i] < 0);
    return crossings * 44100.0 / count / 2;
}

// The level of one frequency in a window, by correlation (Goertzel), as an
// amplitude: what a pure sine of amplitude A at that frequency reads as A.
inline double tone(double hz, unsigned from, unsigned count) {
    const double w = 2 * 3.14159265358979 * hz / 44100;
    double re = 0, im = 0;
    for (unsigned i = 0; i < count; ++i) {
        const double hann = 0.5 - 0.5 * std::cos(2 * 3.14159265358979 * i / count);
        const double x = out[from + i] / 2147483648.0 * hann;
        re += x * std::cos(w * i);
        im += x * std::sin(w * i);
    }
    return 4 * std::sqrt(re * re + im * im) / count;
}

// A window's brightness: the frequency of a sine with the same slope-to-level
// ratio, in Hz (a sine reads as its own frequency; noise and bright partials read high).
inline double brightness(unsigned from, unsigned count) {
    double low = 0, high = 0, prev = 0;
    for (unsigned i = from; i < from + count; ++i) {
        const double x = out[i] / 2147483648.0;
        low += x * x;
        high += (x - prev) * (x - prev);
        prev = x;
    }
    return std::sqrt(high / (low + 1e-20)) * 44100 / (2 * 3.14159265358979);
}

inline int finish(const char* name) {
    if (!failures) std::printf("PASS %s\n", name);
    return failures ? 1 : 0;
}
}
