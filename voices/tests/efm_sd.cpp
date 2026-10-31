// Sound checks for EFM-SD, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted measures are the Machinedrum's (reference/fit_efm_sd.py).
#include "../efm_sd.hpp"
#include "efm_check.hpp"
#define PLAY check::play<md::efm_sd::State, md::efm_sd::reset, md::efm_sd::trigger, md::efm_sd::render>

using namespace check;

int main() {
    // PTCH: 121.9 Hz at 0, 290.3 at 64, 679.8 at 127, the tone alone (NOIS 0, MOD 0).
    const int tones[3][8] = {{0, 96, 0, 64, 0, 64, 64, 0}, {64, 96, 0, 64, 0, 64, 64, 0}, {127, 96, 0, 64, 0, 64, 64, 0}};
    const double hz[3] = {121.9, 290.3, 679.8};
    for (int k = 0; k < 3; ++k) {
        PLAY(tones[k]);
        // It starts just past a zero crossing: the first sample 0.07 of the peak.
        expect("the onset starts small", std::fabs(out[0] / 2147483648.0) < 0.12 * peak(0, ms(20)));
        const double f = frequency(ms(50), ms(200));
        std::printf("PTCH %d: %.1f Hz\n", tones[k][0], f);
        expect("PTCH sets the Machinedrum's pitch", std::fabs(f / hz[k] - 1) < 0.03);
    }
    // The tone's level: the Machinedrum's -20.7 dB RMS, 2 dB up.
    PLAY(tones[1]);
    const double body = db(rms(ms(50), ms(50)));
    std::printf("tone %.1f dB RMS\n", body);
    expect("the tone sits near -18.7 dB RMS", std::fabs(body + 18.7) < 1.0);
    // HPF 127 takes the 290 Hz tone down about 23 dB.
    const int cut[8] = {64, 96, 0, 64, 0, 64, 64, 127};
    PLAY(cut);
    const double cutoff = db(rms(ms(50), ms(50))) - body;
    std::printf("HPF 127: %.1f dB\n", cutoff);
    expect("HPF 127 takes the tone down 20 to 26 dB", cutoff < -20 && cutoff > -26);

    // DEC 0 is down 27 dB by 10 ms, then a quiet tail.
    const int tight[8] = {64, 0, 0, 64, 0, 64, 64, 0};
    PLAY(tight);
    const double at10 = db(rms(ms(10), ms(10))) - db(rms(0, ms(10)));
    std::printf("DEC 0 at 10 ms: %.1f dB\n", at10);
    expect("DEC 0 is short", at10 < -22 && at10 > -33);
    // DEC 64 falls about 27 dB in 200 ms.
    const int mid[8] = {64, 64, 0, 64, 0, 64, 64, 0};
    PLAY(mid);
    const double fall = db(rms(0, ms(10))) - db(rms(ms(200), ms(10)));
    expect("DEC 64 falls about 27 dB in 200 ms", fall > 24 && fall < 30);

    // The noise alone (DEC 0, NOIS 127, NDEC 127): -28.4 dB RMS at 50 ms on the
    // Machinedrum, and bright.
    const int hiss[8] = {64, 0, 127, 127, 0, 64, 64, 0};
    PLAY(hiss);
    const double noise = db(rms(ms(50), ms(10)));
    const double bright = brightness(ms(50), ms(10));
    std::printf("noise %.1f dB RMS, brightness %.0f Hz\n", noise, bright);
    expect("the noise sits near -26.4 dB RMS", std::fabs(noise + 26.4) < 1.5);
    expect("the noise is bright", bright > 6000);
    const int short_hiss[8] = {64, 0, 127, 64, 0, 64, 64, 0};
    PLAY(short_hiss);
    const double ndec = db(rms(ms(20), ms(10))) - db(rms(ms(100), ms(10)));
    std::printf("NDEC 64 falls %.1f dB from 20 to 100 ms\n", ndec);
    expect("NDEC 64 falls about 28 dB from 20 to 100 ms", ndec > 23 && ndec < 33);
    const int none[8] = {64, 0, 0, 127, 0, 64, 64, 0};
    PLAY(none);
    expect("NOIS 0 has no noise", db(rms(ms(50), ms(10))) < noise - 20);

    // MOD spreads the tone: at MOD 32 the band +-25 Hz around it holds lines
    // within 6 dB of the carrier, where a clean FM's would be far down.
    const int rough[8] = {64, 96, 0, 64, 32, 64, 127, 0};
    PLAY(rough);
    const double centre = tone(hz[1], ms(20), ms(180));
    const double beside = std::fmax(tone(hz[1] - 22, ms(20), ms(180)), tone(hz[1] + 22, ms(20), ms(180)));
    std::printf("MOD 32: lines 22 Hz off at %.1f dB\n", db(beside / centre));
    expect("MOD spreads the tone", beside > 0.25 * centre);

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
    return finish("efm_sd");
}
