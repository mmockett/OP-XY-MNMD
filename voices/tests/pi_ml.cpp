// Sound checks for PI-ML, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Machinedrum's (reference/fit_pi_ml.py).
#include "../pi_ml.hpp"
#include "pi_check.hpp"

namespace {
using namespace check;
void play(const int* values, opxy::i32* out) {
    check::play<md::pi_ml::State>(md::pi_ml::reset, md::pi_ml::trigger, md::pi_ml::render, values, out);
}
// The summed level at the modes' ratios of a pitch, over a window.
double modes(const opxy::i32* out, double hz, double from, double seconds) {
    const double ratios[7] = {0.581, 0.689, 0.742, 1, 1.112, 1.970, 2.070};
    double sum = 0;
    for (int k = 0; k < 7; ++k) sum += tone(out, hz * ratios[k], from, seconds);
    return sum;
}
}

int main() {
    static opxy::i32 out[kFrames];
    //                    PTCH DEC HARD TENS
    const int design[8] = {64, 50, 64, 40, 0, 0, 0, 0};
    play(design, out);
    expect("sounds", rms(out, 0, 0.1) > 0.02);
    expect("stays within the declared peak", bounded(out));

    // The pitch: 372 Hz at PTCH 0, 743 at 64, 1474 at 127 (the modes' sum peaks there).
    const int pitches[3] = {0, 64, 127};
    const double expected[3] = {372, 743, 1474};
    for (int k = 0; k < 3; ++k) {
        const int values[8] = {pitches[k], 80, 64, 0, 0, 0, 0, 0};
        play(values, out);
        const double hz = expected[k];
        const double on = modes(out, hz, 0.1, 0.2);
        const double off = std::fmax(modes(out, hz * 1.05, 0.1, 0.2), modes(out, hz * 0.95, 0.1, 0.2));
        std::printf("PTCH %d: the modes at %.0f Hz over 5 %% off: %.1f dB\n", pitches[k], hz, db(on) - db(off));
        expect("the modes sit at the Machinedrum's pitch", db(on) - db(off) > 8);
    }

    // HARD 0 is far quieter than HARD 64.
    const int soft[8] = {64, 50, 0, 40, 0, 0, 0, 0};
    play(soft, out);
    const double soft_level = rms(out, 0, 0.1);
    play(design, out);
    std::printf("HARD 0 under HARD 64: %.1f dB\n", db(soft_level) - db(rms(out, 0, 0.1)));
    expect("HARD 0 is about 14 dB quieter", db(soft_level) - db(rms(out, 0, 0.1)) < -9);

    // TENS 127 starts near twice the pitch; TENS 0 does not bend.
    const int tight[8] = {64, 80, 64, 127, 0, 0, 0, 0}, slack[8] = {64, 80, 64, 0, 0, 0, 0, 0};
    play(tight, out);
    const double bent = modes(out, 743 * 1.95, 0.0, 0.01) / modes(out, 743, 0.0, 0.01);
    play(slack, out);
    const double straight = modes(out, 743 * 1.95, 0.0, 0.01) / modes(out, 743, 0.0, 0.01);
    std::printf("first 10 ms near twice the pitch, TENS 127 and 0: %.2f, %.2f\n", bent, straight);
    expect("TENS stretches the strike", bent > 1 && straight < 1);

    // DEC: -60 dB in 0.41 s at DEC 64.
    const int ring[8] = {64, 64, 64, 0, 0, 0, 0, 0};
    play(ring, out);
    const double fall = db(rms(out, 0.1, 0.05)) - db(rms(out, 0.3, 0.05));
    std::printf("DEC 64 falls %.1f dB from 0.1 to 0.3 s\n", fall);
    expect("DEC 64 rings about 0.4 s", fall > 22 && fall < 36);

    for (int c = 0; c < 4; ++c)
        for (int end = 0; end < 2; ++end) {
            int values[8] = {64, 50, 64, 40, 0, 0, 0, 0};
            values[c] = end ? 127 : 0;
            play(values, out);
            expect("bounded at a control's end", bounded(out));
        }
    if (!failures) std::puts("PASS pi_ml");
    return failures ? 1 : 0;
}
