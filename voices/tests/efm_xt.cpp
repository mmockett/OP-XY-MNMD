// Sound checks for EFM-XT, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted measures are the Machinedrum's (reference/fit_efm_xt.py).
#include "../efm_xt.hpp"
#include "efm_check.hpp"
#define PLAY check::play<md::efm_xt::State, md::efm_xt::reset, md::efm_xt::trigger, md::efm_xt::render>

using namespace check;

int main() {
    // PTCH: 43.1 Hz at 0, 86.1 at 64, 170.9 at 127, with RAMP 16 (flat) and no MOD.
    const int flat[3][8] = {{0, 96, 16, 64, 0, 64, 64, 0}, {64, 96, 16, 64, 0, 64, 64, 0}, {127, 96, 16, 64, 0, 64, 64, 0}};
    const double hz[3] = {43.1, 86.1, 170.9};
    double level[3];
    for (int k = 0; k < 3; ++k) {
        PLAY(flat[k]);
        const double f = frequency(ms(100), ms(400));
        level[k] = db(rms(ms(50), ms(50)));
        std::printf("PTCH %d: %.1f Hz, %.1f dB\n", flat[k][0], f, level[k]);
        expect("PTCH sets the Machinedrum's pitch", std::fabs(f / hz[k] - 1) < 0.03);
    }
    // The high-pass: the Machinedrum's level climbs 8.2 dB from PTCH 0 to 127.
    expect("the high-pass thins low settings", level[2] - level[0] > 6.5 && level[2] - level[0] < 10);
    expect("the body sits 2 dB over the Machinedrum's -27.3 dB RMS", std::fabs(level[1] + 25.3) < 1.5);

    // DEC 64 falls about 26 dB in 200 ms, as EFM-BD's.
    const int body[8] = {64, 64, 16, 64, 0, 64, 64, 0};
    PLAY(body);
    const double fall = db(rms(ms(10), ms(20))) - db(rms(ms(210), ms(20)));
    std::printf("DEC 64 falls %.1f dB in 200 ms\n", fall);
    expect("DEC 64 falls about 26 dB in 200 ms", fall > 23 && fall < 29);
    // DEC 0 has no quiet tail: down 50 dB by 100 ms.
    const int tight[8] = {64, 0, 16, 64, 0, 64, 64, 0};
    PLAY(tight);
    expect("DEC 0 leaves no tail", db(rms(ms(100), ms(20))) - db(rms(0, ms(10))) < -50);

    // RAMP 127 starts 7.9 times the settled pitch.
    const int swept[8] = {64, 96, 127, 64, 0, 64, 64, 0};
    PLAY(swept);
    expect("RAMP sweeps from far above", frequency(0, ms(5)) > frequency(ms(600), ms(200)) * 5);

    // MOD 64 at MFRQ 64: the modulator at 4 times the carrier; the upper first
    // sideband (5 times) stands about -6 dB to the carrier on the Machinedrum.
    const int fm[8] = {64, 96, 16, 64, 64, 64, 127, 0};
    PLAY(fm);
    const double side = db(tone(430.5, ms(20), ms(80))) - db(tone(86.1, ms(20), ms(80)));
    std::printf("MOD 64 sideband %.1f dB\n", side);
    expect("MOD 64 sideband near -6 dB", side > -11 && side < -3);

    // MOD 110: the fed-back modulator's grit, jagged where MOD 0 is smooth (third differences).
    const int gritty[8] = {80, 64, 40, 40, 110, 48, 48, 0}, smooth[8] = {80, 64, 40, 40, 0, 48, 48, 0};
    double rough[2];
    for (int k = 0; k < 2; ++k) {
        PLAY(k ? smooth : gritty);
        double sum = 0;
        for (unsigned i = ms(10); i < ms(50); ++i) {
            const double d = (out[i] - 3.0 * out[i - 1] + 3.0 * out[i - 2] - out[i - 3]) / 2147483648.0;
            sum += d * d;
        }
        rough[k] = db(std::sqrt(sum / ms(40)));
    }
    std::printf("third differences: MOD 110 %.1f dB, MOD 0 %.1f dB\n", rough[0], rough[1]);
    expect("MOD 110 has grit", rough[0] > rough[1] + 30);

    // CLIC: 0 starts from a zero crossing, 127 from beyond the body's swing.
    const int soft[8] = {64, 96, 16, 64, 0, 64, 64, 0};
    PLAY(soft);
    const double swing = peak(ms(20), ms(30));
    const double gentle = std::fabs(out[0] / 2147483648.0);
    const int hard[8] = {64, 96, 16, 64, 0, 64, 64, 127};
    PLAY(hard);
    const double clicked = std::fabs(out[0] / 2147483648.0);
    std::printf("CLIC 0 starts at %.2f, 127 at %.2f of the swing\n", gentle / swing, clicked / swing);
    expect("CLIC 0 starts quietly", gentle < 0.05 * swing);
    expect("CLIC 127 starts beyond the swing", clicked > 1.3 * swing);

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
    expect("hot settings sound", rms(0, ms(100)) > 0.02);
    return finish("efm_xt");
}
