// Sound checks for PI-MT, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Machinedrum's (reference/fit_pi_mt.py).
#include "../pi_mt.hpp"
#include "pi_check.hpp"

namespace {
using namespace check;
void play(const int* values, opxy::i32* out) {
    check::play<md::pi_mt::State>(md::pi_mt::reset, md::pi_mt::trigger, md::pi_mt::render, values, out);
}
}

int main() {
    static opxy::i32 out[kFrames];
    //                    PTCH DEC HARD HAMR TUNE DAMP SIZE POS
    const int design[8] = {64, 50, 64, 64, 64, 0, 64, 0};
    play(design, out);
    expect("sounds", rms(out, 0, 0.1) > 0.05);
    expect("stays within the declared peak", bounded(out));

    // The head: 69 Hz at PTCH 0 to 276 Hz at 127, two octaves.
    const int low[8] = {0, 100, 64, 64, 64, 0, 127, 0}, high[8] = {127, 100, 64, 64, 64, 0, 127, 0};
    play(low, out);
    const double low_hz = strongest(out, 60, 80, 0.1, 0.4);
    play(high, out);
    const double high_hz = strongest(out, 250, 300, 0.1, 0.4);
    std::printf("head at PTCH 0 and 127: %.1f, %.1f Hz\n", low_hz, high_hz);
    expect("PTCH 0 is 69 Hz", low_hz > 66.5 && low_hz < 71.5);
    expect("PTCH 127 is 276 Hz", high_hz > 266 && high_hz < 286);

    // SIZE: the shell's pitch, 941 Hz at 0, 317 at 64, 190 at 127, whatever PTCH is.
    const int small[8] = {0, 64, 64, 64, 64, 0, 0, 0}, mid[8] = {127, 64, 64, 64, 64, 0, 64, 0},
              big[8] = {0, 64, 64, 64, 64, 0, 127, 0};
    play(small, out);
    const double small_hz = strongest(out, 800, 1100, 0.1, 0.3);
    play(mid, out);
    const double mid_hz = strongest(out, 290, 345, 0.1, 0.3);
    play(big, out);
    const double big_hz = strongest(out, 175, 205, 0.1, 0.3);
    std::printf("shell at SIZE 0, 64, 127: %.0f %.0f %.0f Hz\n", small_hz, mid_hz, big_hz);
    expect("SIZE 0 is 941 Hz", small_hz > 900 && small_hz < 980);
    expect("SIZE 64 is 317 Hz", mid_hz > 305 && mid_hz < 330);
    expect("SIZE 127 is 190 Hz", big_hz > 182 && big_hz < 198);

    // DEC: the shell falls 60 dB in 2.9 s at DEC 64; DAMP 127 cuts it to half a second.
    const int ring[8] = {64, 64, 64, 64, 64, 0, 64, 0}, damped[8] = {64, 64, 64, 64, 64, 127, 64, 0};
    play(ring, out);
    const double fall = db(tone(out, 317, 0.2, 0.05)) - db(tone(out, 317, 0.8, 0.05));
    play(damped, out);
    const double damped_fall = db(tone(out, 317, 0.2, 0.05)) - db(tone(out, 317, 0.8, 0.05));
    std::printf("the shell from 0.2 to 0.8 s: %.1f dB, damped %.1f dB\n", fall, damped_fall);
    expect("DEC 64 rings the shell about 3 s", fall > 9 && fall < 16);
    expect("DAMP shortens the shell", damped_fall > fall + 20);

    // POS: TUNE's mode (218 Hz at TUNE 64) comes up 10 dB from the middle to the edge.
    const int centre[8] = {64, 64, 64, 64, 64, 0, 64, 0}, edge[8] = {64, 64, 64, 64, 64, 0, 64, 127};
    play(centre, out);
    const double centre_level = tone(out, 216, 0.0, 0.2);
    play(edge, out);
    const double edge_level = tone(out, 216, 0.0, 0.2);
    std::printf("TUNE's mode, POS 127 over 0: %.1f dB\n", db(edge_level) - db(centre_level));
    expect("POS brings up TUNE's mode", db(edge_level) - db(centre_level) > 7);

    for (int c = 0; c < 8; ++c)
        for (int end = 0; end < 2; ++end) {
            int values[8] = {64, 50, 64, 64, 64, 0, 64, 0};
            values[c] = end ? 127 : 0;
            play(values, out);
            expect("bounded at a control's end", bounded(out));
        }
    const int hot[8] = {127, 127, 127, 0, 127, 0, 127, 127};
    play(hot, out);
    expect("hot settings stay bounded", bounded(out));

    if (!failures) std::puts("PASS pi_mt");
    return failures ? 1 : 0;
}
