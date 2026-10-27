// Sound checks for PI-MA, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Machinedrum's (reference/fit_pi_ma.py).
#include "../pi_ma.hpp"
#include "pi_check.hpp"

namespace {
using namespace check;
void play(const int* values, opxy::i32* out) {
    check::play<md::pi_ma::State>(md::pi_ma::reset, md::pi_ma::trigger, md::pi_ma::render, values, out);
}
// The brightness: the share of energy above 4 kHz, as a one-pole high-pass sees it.
double bright(const opxy::i32* out, double from, double seconds) {
    double low = 0, high = 0, all = 0;
    for (unsigned i = 0; i < at(from + seconds); ++i) {
        const double x = out[i] / 2147483648.0;
        low += (x - low) * 0.43;
        if (i >= at(from)) { high += (x - low) * (x - low); all += x * x; }
    }
    return high / (all + 1e-30);
}
// The envelope's roughness: the spread of 1 ms levels around their 15 ms average, dB.
double rough(const opxy::i32* out, double seconds) {
    const unsigned n = at(seconds) / 44;
    static double level[20000];
    for (unsigned k = 0; k < n; ++k) level[k] = db(rms(out, k * 44 / 44100.0, 44 / 44100.0));
    double sum = 0;
    unsigned count = 0;
    for (unsigned k = 7; k + 7 < n; ++k) {
        double mean = 0;
        for (unsigned j = k - 7; j <= k + 7; ++j) mean += level[j];
        mean /= 15;
        sum += (level[k] - mean) * (level[k] - mean);
        ++count;
    }
    return std::sqrt(sum / count);
}
}

int main() {
    static opxy::i32 out[kFrames];
    //                    GRNS DEC GLEN -  SIZE HARD
    const int design[8] = {64, 50, 40, 0, 64, 64, 0, 0};
    play(design, out);
    expect("sounds", rms(out, 0, 0.2) > 0.005);
    expect("stays within the declared peak", bounded(out));

    // More grains and longer ones are smoother.
    const int sparse[8] = {0, 50, 40, 0, 64, 64, 0, 0}, dense[8] = {127, 50, 40, 0, 64, 64, 0, 0};
    play(sparse, out);
    const double sparse_rough = rough(out, 0.3);
    play(dense, out);
    const double dense_rough = rough(out, 0.3);
    std::printf("roughness at GRNS 0 and 127: %.1f, %.1f dB\n", sparse_rough, dense_rough);
    expect("GRNS smooths the rattle", sparse_rough > dense_rough * 3);

    // DEC: -20 dB after 70 ms at DEC 0, over a second at DEC 127.
    const int short_shake[8] = {64, 0, 40, 0, 64, 64, 0, 0}, long_shake[8] = {64, 127, 40, 0, 64, 64, 0, 0};
    play(short_shake, out);
    const double short_tail = db(rms(out, 0.3, 0.1)) - db(rms(out, 0, 0.05));
    play(long_shake, out);
    const double long_tail = db(rms(out, 0.3, 0.1)) - db(rms(out, 0, 0.05));
    std::printf("level at 0.3 s under the start, DEC 0 and 127: %.1f, %.1f dB\n", short_tail, long_tail);
    expect("DEC 0 is over by 0.3 s", short_tail < -40);
    expect("DEC 127 still shakes at 0.3 s", long_tail > -12);

    // SIZE brightens it, HARD darkens it.
    const int small[8] = {64, 50, 40, 0, 0, 64, 0, 0}, big[8] = {64, 50, 40, 0, 127, 64, 0, 0},
              soft[8] = {64, 50, 40, 0, 64, 0, 0, 0}, hard[8] = {64, 50, 40, 0, 64, 127, 0, 0};
    play(small, out);
    const double small_bright = bright(out, 0, 0.2);
    play(big, out);
    const double big_bright = bright(out, 0, 0.2);
    play(soft, out);
    const double soft_bright = bright(out, 0, 0.2);
    play(hard, out);
    const double hard_bright = bright(out, 0, 0.2);
    std::printf("share above 4 kHz: SIZE 0 %.2f, 127 %.2f; HARD 0 %.2f, 127 %.2f\n", small_bright, big_bright,
                soft_bright, hard_bright);
    expect("SIZE brightens", big_bright > small_bright * 1.3);
    expect("HARD darkens", hard_bright < soft_bright * 0.7);

    for (int c = 0; c < 6; ++c)
        for (int end = 0; end < 2; ++end) {
            int values[8] = {64, 50, 40, 0, 64, 64, 0, 0};
            values[c] = end ? 127 : 0;
            play(values, out);
            expect("bounded at a control's end", bounded(out));
        }
    const int hot[8] = {127, 127, 127, 0, 127, 0, 0, 0};
    play(hot, out);
    expect("hot settings stay bounded", bounded(out));
    if (!failures) std::puts("PASS pi_ma");
    return failures ? 1 : 0;
}
