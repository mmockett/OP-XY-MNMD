// Sound checks for EFM-CP, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted measures are the Machinedrum's (reference/fit_efm_cp.py).
#include "../efm_cp.hpp"
#include "efm_check.hpp"
#define PLAY check::play<md::efm_cp::State, md::efm_cp::reset, md::efm_cp::trigger, md::efm_cp::render>

using namespace check;

// The claps' onsets: 1 ms windows that rise 6 dB over the one before.
int onsets(unsigned to_ms) {
    int count = 0;
    double before = db(rms(0, 44));
    for (unsigned w = 1; w < to_ms; ++w) {
        const double now = db(rms(w * 44, 44));
        count += now > before + 6;
        before = now;
    }
    return count;
}

int main() {
    // PTCH: the tail's pitch, 121.9 Hz at 0, 820 at 64, 5351 at 127 (MOD 0).
    const int tones[3][8] = {{0, 96, 64, 64, 0, 64, 64, 0}, {64, 96, 64, 64, 0, 64, 64, 0}, {127, 96, 64, 64, 0, 64, 64, 0}};
    const double hz[3] = {121.9, 819.9, 5351.4};
    for (int k = 0; k < 3; ++k) {
        PLAY(tones[k]);
        const double f = frequency(ms(100), ms(200));
        std::printf("PTCH %d: %.1f Hz\n", tones[k][0], f);
        expect("PTCH sets the Machinedrum's pitch", std::fabs(f / hz[k] - 1) < 0.03);
    }

    // CLPS 64, CDEC 64: eight claps 6.6 ms apart, then the tail: eight onsets after the first.
    const int base[8] = {64, 64, 64, 64, 0, 64, 64, 0};
    PLAY(base);
    const int eight = onsets(100);
    std::printf("CLPS 64: %d onsets after the first\n", eight);
    expect("CLPS 64 claps eight times", eight == 8);
    // Each clap falls about 16 dB over its 6.6 ms; the tail holds.
    const double clap = db(rms(ms(4.5), ms(1))) - db(rms(0, ms(1)));
    std::printf("a clap falls %.1f dB in 4.5 ms\n", clap);
    expect("a clap falls 10 to 18 dB in 4.5 ms", clap < -10 && clap > -18);
    const double tail = db(rms(ms(57), ms(1))) - db(rms(ms(61), ms(1)));
    expect("the tail holds after the claps", std::fabs(tail) < 2);
    // The level: the Machinedrum's -25.6 dB RMS over the first 50 ms, 2 dB up.
    const double level = db(rms(0, ms(50)));
    std::printf("level %.1f dB RMS\n", level);
    expect("the level sits near -23.6 dB RMS", std::fabs(level + 23.6) < 1.0);

    // CLPS 127 claps sixteen times; CLPS 16 twice.
    const int many[8] = {64, 64, 127, 64, 0, 64, 64, 0};
    PLAY(many);
    expect("CLPS 127 claps sixteen times", onsets(150) == 16);
    const int two[8] = {64, 64, 16, 64, 0, 64, 64, 0};
    PLAY(two);
    expect("CLPS 16 claps twice", onsets(150) == 2);
    // CDEC 127 spaces them 41 ms apart: the second at 41 ms.
    const int slow[8] = {64, 64, 16, 127, 0, 64, 64, 0};
    PLAY(slow);
    expect("CDEC 127 spaces claps 41 ms", db(rms(ms(41.5), ms(1))) > db(rms(ms(39.5), ms(1))) + 10);

    // DEC 64: the tail falls about 13 dB per 100 ms.
    PLAY(base);
    const double tail_fall = db(rms(ms(100), ms(10))) - db(rms(ms(200), ms(10)));
    std::printf("DEC 64 tail falls %.1f dB from 100 to 200 ms\n", tail_fall);
    expect("DEC 64 tail falls about 13 dB per 100 ms", tail_fall > 10 && tail_fall < 16);

    // HPF 127 takes the default (MOD 64) down about 10 dB and brightens it.
    const int open[8] = {64, 64, 64, 64, 64, 64, 64, 0};
    const int cut[8] = {64, 64, 64, 64, 64, 64, 64, 127};
    PLAY(open);
    const double before = db(rms(0, ms(50))), dull = brightness(0, ms(50));
    PLAY(cut);
    const double after = db(rms(0, ms(50))), bright = brightness(0, ms(50));
    std::printf("HPF 127: %.1f dB, brightness %.0f to %.0f Hz\n", after - before, dull, bright);
    expect("HPF 127 takes it down 8 to 13 dB", after - before < -8 && after - before > -13);
    expect("HPF brightens it", bright > dull * 1.5);

    for (int s = 0; s < 8; ++s)
        for (int end = 0; end < 2; ++end) {
            int p[8] = {64, 64, 64, 64, 64, 64, 64, 64};
            p[s] = end ? 127 : 0;
            PLAY(p);
            expect("bounded at a control's end", bounded());
        }
    const int hot[8] = {127, 127, 127, 127, 127, 127, 127, 0};
    PLAY(hot);
    expect("hot settings stay bounded", bounded());
    expect("hot settings sound", rms(0, ms(100)) > 0.02);
    return finish("efm_cp");
}
