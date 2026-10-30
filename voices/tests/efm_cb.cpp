// Sound checks for EFM-CB, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted measures are the Machinedrum's (reference/fit_efm_cb.py).
#include "../efm_cb.hpp"
#include "efm_check.hpp"
#define PLAY check::play<md::efm_cb::State, md::efm_cb::reset, md::efm_cb::trigger, md::efm_cb::render>

using namespace check;

int main() {
    // PTCH: two tones, 410 and 607 Hz at 64, 172 and 255 at 0, 960 and 1421 at 127.
    const int bells[3][8] = {{0, 96, 0, 0, 0, 64, 64, 0}, {64, 96, 0, 0, 0, 64, 64, 0}, {127, 96, 0, 0, 0, 64, 64, 0}};
    const double low[3] = {172, 410, 960};
    for (int k = 0; k < 3; ++k) {
        PLAY(bells[k]);
        const double a = tone(low[k], ms(20), ms(200)), b = tone(low[k] * 1.4805, ms(20), ms(200));
        const double off = tone(low[k] * 1.2, ms(20), ms(200));
        std::printf("PTCH %d: %.0f Hz %.1f dB, %.0f Hz %.1f dB\n", bells[k][0], low[k], db(a), low[k] * 1.4805, db(b));
        expect("PTCH sets the two tones", std::fabs(db(a) - db(b)) < 1.5 && off < 0.1 * a);
    }
    // The level: the Machinedrum's -21.0 dB RMS (first 5 ms), 2 dB up.
    PLAY(bells[1]);
    // It starts from a zero crossing: the first sound is 0.06 of the peak.
    expect("the onset starts small", std::fabs(out[1] / 2147483648.0) < 0.15 * peak(0, ms(10)));
    const double level = db(rms(0, ms(5)));
    std::printf("level %.1f dB RMS\n", level);
    expect("the level sits near -19 dB RMS", std::fabs(level + 19) < 1.0);
    const double plain = tone(820, ms(20), ms(200)) / tone(410, ms(20), ms(200));

    // DEC 64 falls 13 dB in 100 ms.
    const int mid[8] = {64, 64, 0, 0, 0, 64, 64, 0};
    PLAY(mid);
    const double fall = db(rms(0, ms(5))) - db(rms(ms(100), ms(5)));
    expect("DEC 64 falls about 13 dB in 100 ms", fall > 11 && fall < 15);

    // SNAP 127 takes 12 dB away by 100 ms, over DEC's own fall.
    const int snapped[8] = {64, 96, 127, 0, 0, 64, 64, 0};
    PLAY(snapped);
    const double snap = db(rms(0, ms(5))) - db(rms(ms(100), ms(5)));
    PLAY(bells[1]);
    const double unsnapped = db(rms(0, ms(5))) - db(rms(ms(100), ms(5)));
    std::printf("SNAP 127 falls %.1f dB by 100 ms, SNAP 0 %.1f\n", snap, unsnapped);
    expect("SNAP 127 takes about 12 dB away", snap - unsnapped > 9 && snap - unsnapped < 15);

    // FB 64: second harmonics 8 dB under the tones.
    const int fed[8] = {64, 96, 0, 64, 0, 64, 64, 0};
    PLAY(fed);
    const double second = tone(820, ms(20), ms(200)) / tone(410, ms(20), ms(200));
    std::printf("FB 64: second harmonic %.1f dB (FB 0 %.1f)\n", db(second), db(plain));
    expect("FB 64 adds second harmonics near -8 dB", db(second) > -11 && db(second) < -5);

    // MOD 64 at MFRQ 64: the modulators at 4 times, half EFM-BD's index: the
    // third harmonic (the lower sideband) 12 dB under the tone.
    const int fm[8] = {64, 96, 0, 0, 64, 64, 127, 0};
    PLAY(fm);
    const double side = db(tone(1230, ms(20), ms(200))) - db(tone(410, ms(20), ms(200)));
    std::printf("MOD 64 sideband %.1f dB\n", side);
    expect("MOD 64 sideband near -12 dB", side > -15 && side < -9);

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
    return finish("efm_cb");
}
