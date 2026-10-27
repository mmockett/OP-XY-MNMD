// Sound checks for PI-SD, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Machinedrum's (reference/fit_pi_sd.py).
#include "../pi_sd.hpp"
#include "pi_check.hpp"

namespace {
using namespace check;
void play(const int* values, opxy::i32* out) {
    check::play<md::pi_sd::State>(md::pi_sd::reset, md::pi_sd::trigger, md::pi_sd::render, values, out);
}
// Above 1.5 kHz, roughly: the signal less a one-pole low-pass of it.
double bright(const opxy::i32* out, double from, double seconds) {
    double low = 0, sum = 0;
    for (unsigned i = 0; i < at(from + seconds); ++i) {
        const double x = out[i] / 2147483648.0;
        low += (x - low) * 0.19;
        if (i >= at(from)) sum += (x - low) * (x - low);
    }
    return std::sqrt(sum / at(seconds));
}
}

int main() {
    static opxy::i32 out[kFrames];
    //                    PTCH DEC HARD RING TENS RVOL RDEC
    const int design[8] = {64, 50, 64, 0, 40, 64, 40, 0};
    play(design, out);
    expect("sounds", rms(out, 0, 0.1) > 0.03);
    expect("stays within the declared peak", bounded(out));

    // The head's fundamental: 86 Hz at PTCH 0, 178 at 64, 340 at 127.
    const int low[8] = {0, 127, 64, 0, 40, 0, 0, 0}, mid[8] = {64, 127, 64, 0, 40, 0, 0, 0},
              high[8] = {127, 127, 64, 0, 40, 0, 0, 0};
    play(low, out);
    const double low_hz = strongest(out, 60, 130, 0.0, 0.15);
    play(mid, out);
    const double mid_hz = strongest(out, 130, 200, 0.0, 0.15);
    play(high, out);
    const double high_hz = strongest(out, 250, 400, 0.0, 0.15);
    std::printf("head at PTCH 0, 64, 127: %.1f %.1f %.1f Hz\n", low_hz, mid_hz, high_hz);
    expect("PTCH 0 is 86 Hz", low_hz > 82 && low_hz < 90);
    expect("PTCH 64 is 178 Hz", mid_hz > 171 && mid_hz < 184);
    expect("PTCH 127 is 340 Hz", high_hz > 326 && high_hz < 354);

    // DEC: -60 dB in 138 ms at 64, so about 22 dB down from 20 to 70 ms.
    const int ring[8] = {64, 64, 64, 0, 40, 0, 0, 0};
    play(ring, out);
    const double fall = db(rms(out, 0.02, 0.01)) - db(rms(out, 0.07, 0.01));
    std::printf("DEC 64 falls %.1f dB from 20 to 70 ms\n", fall);
    expect("DEC 64 rings about 0.14 s", fall > 16 && fall < 28);

    // RING: the mode at 2.75 times the fundamental comes up and rings on.
    const int ringing[8] = {64, 50, 64, 127, 40, 0, 0, 0}, dry[8] = {64, 50, 64, 0, 40, 0, 0, 0};
    play(ringing, out);
    const double rung = tone(out, 488, 0.1, 0.1);
    play(dry, out);
    const double unrung = tone(out, 488, 0.1, 0.1);
    std::printf("488 Hz at 100 ms, RING 127 over RING 0: %.1f dB\n", db(rung) - db(unrung));
    expect("RING rings on at 2.75 f", db(rung) - db(unrung) > 25);

    // The wires: RVOL 127 and RDEC 127 rattle on, -40 dB under the start by about 0.3 s.
    const int wires[8] = {64, 50, 64, 0, 40, 127, 127, 0}, tight[8] = {64, 50, 64, 0, 40, 127, 64, 0};
    play(wires, out);
    const double long_wires = bright(out, 0.2, 0.02);
    play(tight, out);
    const double short_wires = bright(out, 0.2, 0.02);
    std::printf("wires at 200 ms, RDEC 127 over 64: %.1f dB\n", db(long_wires) - db(short_wires));
    expect("RDEC lengthens the wires", db(long_wires) - db(short_wires) > 25);

    for (int c = 0; c < 7; ++c)
        for (int end = 0; end < 2; ++end) {
            int values[8] = {64, 50, 64, 0, 40, 64, 40, 0};
            values[c] = end ? 127 : 0;
            play(values, out);
            expect("bounded at a control's end", bounded(out));
        }
    const int hot[8] = {0, 127, 127, 127, 127, 127, 127, 0};
    play(hot, out);
    expect("hot settings stay bounded", bounded(out));

    if (!failures) std::puts("PASS pi_sd");
    return failures ? 1 : 0;
}
