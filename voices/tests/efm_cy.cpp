// Sound checks for EFM-CY, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted measures are the Machinedrum's (reference/fit_efm_cy.py).
#include "../efm_cy.hpp"
#include "efm_check.hpp"
#define PLAY check::play<md::efm_cy::State, md::efm_cy::reset, md::efm_cy::trigger, md::efm_cy::render>

using namespace check;

int main() {
    // The plain cymbal (MOD 0, FB 0): partials at 580, 818, 1044, 1567 Hz at PTCH 64,
    // and 244, 343, 439, 658 at PTCH 0.
    const int plain[8] = {64, 64, 0, 0, 0, 64, 64, 0};
    PLAY(plain);
    const double partials[4] = {580, 818, 1044, 1567};
    const double first = tone(partials[0], ms(10), ms(90));
    for (int k = 0; k < 4; ++k) {
        const double level = db(tone(partials[k], ms(10), ms(90))) - db(first);
        std::printf("partial %.0f Hz: %.1f dB\n", partials[k], level);
        expect("the four partials stand level", std::fabs(level) < 2);
    }
    expect("nothing between them", tone(700, ms(10), ms(90)) < 0.1 * first);
    const double level = db(rms(0, ms(5)));
    std::printf("level %.1f dB RMS\n", level);
    expect("the level: the Machinedrum's -25.1 dB RMS, 2 dB up", std::fabs(level + 23.1) < 1.5);
    const int low[8] = {0, 64, 0, 0, 0, 64, 64, 0};
    PLAY(low);
    expect("PTCH 0 puts the lowest at 244 Hz", tone(244, ms(10), ms(90)) > 4 * tone(300, ms(10), ms(90)));

    // DEC 64 falls 13 dB in 100 ms, as EFM-BD's.
    PLAY(plain);
    const double fall = db(rms(0, ms(5))) - db(rms(ms(100), ms(5)));
    expect("DEC 64 falls about 13 dB in 100 ms", fall > 11 && fall < 15);

    // MOD 64 at MFRQ 64: the wash, brightness up from about 1.5 kHz to near 10 kHz.
    const double plain_bright = brightness(ms(10), ms(90));
    const int wash[8] = {64, 64, 0, 0, 64, 64, 127, 0};
    PLAY(wash);
    const double washed = brightness(ms(10), ms(90));
    std::printf("MOD 64: brightness %.0f -> %.0f Hz\n", plain_bright, washed);
    expect("MOD 64 washes it bright", washed > 4 * plain_bright);
    // HPF 127 takes the wash down 1 to 3 dB.
    const double before = db(rms(0, ms(50)));
    const int cut[8] = {64, 64, 0, 127, 64, 64, 127, 0};
    PLAY(cut);
    const double after = db(rms(0, ms(50))) - before;
    std::printf("HPF 127: %.1f dB\n", after);
    expect("HPF 127 trims the wash", after < -0.5 && after > -4);
    // FB 96: a buzz at the top of the band.
    const int buzz[8] = {64, 64, 96, 0, 0, 64, 64, 0};
    PLAY(buzz);
    expect("FB 96 buzzes", brightness(ms(10), ms(90)) > 3 * plain_bright);

    for (int s = 0; s < 7; ++s)
        for (int end = 0; end < 2; ++end) {
            int p[8] = {64, 64, 64, 64, 64, 64, 64, 0};
            p[s] = end ? 127 : 0;
            PLAY(p);
            expect("bounded at a control's end", bounded());
        }
    const int hot[8] = {127, 127, 127, 127, 127, 127, 127, 0};
    PLAY(hot);
    expect("hot settings stay bounded", bounded());
    expect("hot settings sound", rms(0, ms(100)) > 0.02);
    return finish("efm_cy");
}
