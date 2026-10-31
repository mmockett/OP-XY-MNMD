// Sound checks for EFM-HH, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted measures are the Machinedrum's (reference/fit_efm_hh.py).
#include "../efm_hh.hpp"
#include "efm_check.hpp"
#define PLAY check::play<md::efm_hh::State, md::efm_hh::reset, md::efm_hh::trigger, md::efm_hh::render>

using namespace check;

int main() {
    // The plain hat (MOD 0, FB 0, TREM 0): the Machinedrum's -34.1 dB RMS over
    // 50 ms, 2 dB up, and bright.
    const int plain[8] = {64, 64, 0, 64, 0, 64, 64, 0};
    PLAY(plain);
    const double level = db(rms(0, ms(50))), bright = brightness(0, ms(50));
    std::printf("plain: %.1f dB RMS, brightness %.0f Hz\n", level, bright);
    expect("the plain hat sits near -32 dB RMS", std::fabs(level + 32) < 1.5);
    expect("the plain hat is bright", bright > 6000);
    // Its carrier's phase jumps every 32 samples, a click through the high-pass:
    // one sample in each 32 holds far more than its share of the energy. And so
    // no line stands at its pitch (690 Hz), against the band around it.
    expect("no line at the pitch", tone(690, ms(10), ms(190)) < 2 * tone(600, ms(10), ms(190)));
    double bins[32] = {0}, total = 0, most = 0;
    for (unsigned i = ms(5); i < ms(45); ++i) {
        const double x = out[i] / 2147483648.0;
        bins[i & 31] += x * x;
        total += x * x;
    }
    for (int k = 0; k < 32; ++k) most = std::fmax(most, bins[k]);
    std::printf("the loudest of 32 phases holds %.0f%% of the energy\n", 100 * most / total);
    expect("clicks every 32 samples", most / total > 0.25);

    // DEC 64 falls about 130 dB a second, as EFM-BD's.
    const double fall = db(rms(ms(20), ms(10))) - db(rms(ms(220), ms(10)));
    std::printf("DEC 64 falls %.1f dB from 20 to 220 ms\n", fall);
    expect("DEC 64 falls about 26 dB in 200 ms", fall > 21 && fall < 31);

    // MOD 127 raises the level about 6.6 dB on the Machinedrum (the sidebands pass
    // the high-pass) and darkens it.
    const int fm[8] = {64, 64, 0, 64, 127, 64, 127, 0};
    PLAY(fm);
    const double loud = db(rms(0, ms(50))) - level, dark = brightness(0, ms(50));
    std::printf("MOD 127: %+.1f dB, brightness %.0f Hz\n", loud, dark);
    expect("MOD 127 is louder", loud > 4 && loud < 9);
    expect("MOD 127 is darker", dark < bright);

    // TREM 127 at TFRQ 64: a 7.7 Hz tremolo, all the way down at its trough (65 ms).
    const int trem[8] = {64, 96, 127, 64, 0, 64, 64, 0};
    PLAY(trem);
    const double trough = db(rms(ms(60), ms(10))) - db(rms(ms(125), ms(10)));
    std::printf("TREM 127: trough %.1f dB under the crest\n", trough);
    expect("TREM 127 dips deep at 65 ms", trough < -15);

    // A short hat chokes an open one still ringing.
    {
        static md::efm_hh::State open, closed;
        md::Common shared = {};
        opxy::i32 p_open[8], p_closed[8];
        const int vo[8] = {64, 110, 0, 64, 0, 64, 64, 0}, vc[8] = {64, 20, 0, 64, 0, 64, 64, 0};
        for (int i = 0; i < 8; ++i) { p_open[i] = q(vo[i]); p_closed[i] = q(vc[i]); }
        opxy::u32 pitch[opxy::block_frames];
        for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
        md::efm_hh::reset(open, shared, 1);
        md::efm_hh::trigger(open, shared);
        for (unsigned b = 0; b < 10; ++b) md::efm_hh::render(open, shared, out, p_open, pitch);
        md::efm_hh::reset(closed, shared, 2);
        md::efm_hh::trigger(closed, shared);
        md::efm_hh::render(closed, shared, out, p_closed, pitch);
        for (unsigned b = 0; b < 300; ++b) md::efm_hh::render(open, shared, out + b * opxy::block_frames, p_open, pitch);
        const double choked = db(rms(ms(400), ms(20))) - db(rms(0, ms(20)));
        std::printf("open hat choked: %.1f dB 400 ms on\n", choked);
        expect("a closed hat chokes an open one", choked < -60);
    }

    for (int s = 0; s < 8; ++s)
        for (int end = 0; end < 2; ++end) {
            int p[8] = {64, 64, 64, 64, 64, 64, 64, 64};
            p[s] = end ? 127 : 0;
            PLAY(p);
            expect("bounded at a control's end", bounded());
        }
    const int hot[8] = {127, 127, 127, 127, 127, 127, 127, 127};
    PLAY(hot);
    expect("hot settings stay bounded", bounded());
    expect("hot settings sound", rms(0, ms(100)) > 0.005);
    return finish("efm_hh");
}
