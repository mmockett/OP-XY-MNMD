// Sound checks for PI-RS, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Machinedrum's (reference/fit_pi_rs.py).
#include "../pi_rs.hpp"
#include "pi_check.hpp"

namespace {
using namespace check;
void play(const int* values, opxy::i32* out) {
    check::play<md::pi_rs::State>(md::pi_rs::reset, md::pi_rs::trigger, md::pi_rs::render, values, out);
}
}

int main() {
    static opxy::i32 out[kFrames];
    //                    PTCH DEC HARD RING RVOL RDEC
    const int design[8] = {64, 50, 64, 0, 64, 40, 0, 0};
    play(design, out);
    expect("sounds", rms(out, 0, 0.1) > 0.02);
    expect("stays within the declared peak", bounded(out));

    // The rim rings at 299 Hz at PTCH 64 and 144 Hz at PTCH 32, whatever the head does.
    const int mid[8] = {64, 100, 64, 0, 0, 0, 0, 0}, low[8] = {32, 100, 64, 0, 0, 0, 0, 0};
    play(mid, out);
    const double mid_hz = strongest(out, 100, 800, 0.1, 0.2);
    play(low, out);
    const double low_hz = strongest(out, 100, 800, 0.1, 0.2);
    std::printf("strongest rim mode at PTCH 64 and 32: %.0f, %.0f Hz\n", mid_hz, low_hz);
    expect("PTCH 64 rings the 299 Hz mode", mid_hz > 292 && mid_hz < 306);
    expect("PTCH 32 rings the 144 Hz mode", low_hz > 140 && low_hz < 148);

    // DEC: -60 dB in 0.85 s at 64, so 21 dB down from 0.1 to 0.4 s.
    const int ring[8] = {64, 64, 64, 0, 0, 0, 0, 0};
    play(ring, out);
    const double fall = db(tone(out, 299, 0.1, 0.05)) - db(tone(out, 299, 0.4, 0.05));
    std::printf("DEC 64: the 299 Hz mode falls %.1f dB from 0.1 to 0.4 s\n", fall);
    expect("DEC 64 rings about 0.85 s", fall > 17 && fall < 26);

    // HARD: the stick's crack, louder with HARD.
    const int soft[8] = {64, 50, 0, 0, 0, 0, 0, 0}, hard[8] = {64, 50, 127, 0, 0, 0, 0, 0};
    play(soft, out);
    const double soft_crack = rms(out, 0, 0.005);
    play(hard, out);
    const double hard_crack = rms(out, 0, 0.005);
    std::printf("the crack, HARD 127 over 0: %.1f dB\n", db(hard_crack) - db(soft_crack));
    expect("HARD makes the crack louder", db(hard_crack) - db(soft_crack) > 5);

    // RING brings up the head's mode at 488 Hz (PTCH 64).
    const int ringing[8] = {64, 50, 64, 127, 0, 0, 0, 0};
    play(ringing, out);
    const double rung = tone(out, 488, 0.05, 0.1);
    play(soft, out);
    std::printf("488 Hz, RING 127 over 0: %.1f dB\n", db(rung) - db(tone(out, 488, 0.05, 0.1)));
    expect("RING rings at 488 Hz", db(rung) - db(tone(out, 488, 0.05, 0.1)) > 15);

    for (int c = 0; c < 6; ++c)
        for (int end = 0; end < 2; ++end) {
            int values[8] = {64, 50, 64, 0, 64, 40, 0, 0};
            values[c] = end ? 127 : 0;
            play(values, out);
            expect("bounded at a control's end", bounded(out));
        }
    const int hot[8] = {0, 127, 127, 127, 127, 127, 0, 0};
    play(hot, out);
    expect("hot settings stay bounded", bounded(out));

    if (!failures) std::puts("PASS pi_rs");
    return failures ? 1 : 0;
}
