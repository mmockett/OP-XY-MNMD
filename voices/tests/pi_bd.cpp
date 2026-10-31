// Sound checks for PI-BD, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Machinedrum's (reference/fit_pi_bd.py).
#include "../pi_bd.hpp"
#include "pi_check.hpp"

namespace {
using namespace check;
// The strike's energy (rms of the tones every 50 Hz) between low and high Hz, in its first 4 ms.
double band(const opxy::i32* out, double low, double high) {
    double sum = 0;
    int n = 0;
    for (double hz = low; hz <= high; hz += 50, ++n) sum += tone(out, hz, 0.0, 0.004) * tone(out, hz, 0.0, 0.004);
    return std::sqrt(sum / n);
}
void play(const int* values, opxy::i32* out) {
    check::play<md::pi_bd::State>(md::pi_bd::reset, md::pi_bd::trigger, md::pi_bd::render, values, out);
}
}

int main() {
    static opxy::i32 out[kFrames];
    //                  PTCH DEC HARD HAMR TENS DAMP
    const int design[8] = {64, 50, 64, 64, 40, 0, 0, 0};
    play(design, out);
    expect("sounds", rms(out, 0, 0.1) > 0.05);
    expect("stays within the declared peak", bounded(out));

    // The settled pitch: 28.9 Hz at PTCH 0, 57.2 at 64, 113 at 127.
    const int low[8] = {0, 127, 64, 64, 40, 0, 0, 0}, mid[8] = {64, 127, 64, 64, 40, 0, 0, 0},
              high[8] = {127, 127, 64, 64, 40, 0, 0, 0};
    play(low, out);
    const double low_hz = frequency(out, 0.8, 1.0);
    play(mid, out);
    const double mid_hz = frequency(out, 0.8, 1.0);
    play(high, out);
    const double high_hz = frequency(out, 0.8, 1.0);
    std::printf("settled at PTCH 0, 64, 127: %.1f %.1f %.1f Hz\n", low_hz, mid_hz, high_hz);
    expect("PTCH 0 settles at 28.9 Hz", low_hz > 27.5 && low_hz < 30.5);
    expect("PTCH 64 settles at 57.2 Hz", mid_hz > 55 && mid_hz < 59.5);
    expect("PTCH 127 settles at 113 Hz", high_hz > 109 && high_hz < 117);

    // DEC: the fundamental falls 60 dB in 31.3 ms per step, so 15 dB per half second at 64.
    const int ring[8] = {64, 64, 64, 64, 40, 0, 0, 0};
    play(ring, out);
    const double fall = db(rms(out, 0.5, 0.1)) - db(rms(out, 1.0, 0.1));
    std::printf("DEC 64 falls %.1f dB from 0.5 to 1 s\n", fall);
    expect("DEC 64 rings -60 dB in 2 s", fall > 13 && fall < 17);
    const int dead[8] = {64, 0, 64, 64, 40, 0, 0, 0};
    play(dead, out);
    expect("DEC 0 is gone in half a second", rms(out, 0.5, 0.1) < 0.0005);

    // TENS: the strike stretches the head, x2.7 at TENS 127, relaxing in about 27 ms.
    const int tight[8] = {64, 127, 64, 64, 127, 0, 0, 0}, slack[8] = {64, 127, 64, 64, 0, 0, 0, 0};
    play(tight, out);
    const double stretched = strongest(out, 60, 300, 0.0, 0.012);
    play(slack, out);
    const double slack_hz = strongest(out, 30, 300, 0.0, 0.012);
    std::printf("first 12 ms at TENS 127 and 0: %.0f, %.0f Hz\n", stretched, slack_hz);
    expect("TENS 127 starts far above the settled pitch", stretched > 115);
    expect("TENS 0 barely bends", slack_hz < 75);

    // The strike's burst: noise spread evenly from about 1 to 6 kHz, as the
    // Machinedrum's, not one ringing tone; a heavy beater (HAMR) darkens it.
    play(design, out);
    const double near1k = band(out, 1000, 1800), near3k = band(out, 2200, 3800), near5k = band(out, 4000, 6000);
    const int heavy[8] = {64, 50, 64, 127, 40, 0, 0, 0};
    play(heavy, out);
    const double dull5k = band(out, 4000, 6000);
    std::printf("strike burst at 1, 3, 5 kHz: %.1f %.1f %.1f dB; 5 kHz at HAMR 127: %.1f dB\n",
                db(near1k), db(near3k), db(near5k), db(dull5k));
    expect("the strike is a band of noise, not a tone", std::fabs(db(near1k / near3k)) < 6 && std::fabs(db(near3k / near5k)) < 6);
    expect("HAMR darkens the strike", dull5k < near5k * 0.8);

    // Every control at both ends stays inside the SDK's ceiling.
    for (int c = 0; c < 6; ++c)
        for (int end = 0; end < 2; ++end) {
            int values[8] = {64, 50, 64, 64, 40, 0, 0, 0};
            values[c] = end ? 127 : 0;
            play(values, out);
            expect("bounded at a control's end", bounded(out));
        }
    const int hot[8] = {0, 127, 127, 0, 127, 0, 0, 0};
    play(hot, out);
    expect("hot settings stay bounded", bounded(out));

    if (!failures) std::puts("PASS pi_bd");
    return failures ? 1 : 0;
}
