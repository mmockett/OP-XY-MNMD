// Sound checks for EFM-BD, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted measures are the Machinedrum's (reference/fit_efm_bd.py).
#include "../efm_bd.hpp"
#include "efm_check.hpp"
#define PLAY check::play<md::efm_bd::State, md::efm_bd::reset, md::efm_bd::trigger, md::efm_bd::render>

using namespace check;

int main() {
    // PTCH: 25.7 Hz at 0, 102.5 at 64, 401 at 127, with RAMP 16 (flat) and no MOD.
    const int flat[3][8] = {{0, 96, 16, 64, 0, 64, 64, 0}, {64, 96, 16, 64, 0, 64, 64, 0}, {127, 96, 16, 64, 0, 64, 64, 0}};
    const double hz[3] = {25.7, 102.5, 401};
    for (int k = 0; k < 3; ++k) {
        PLAY(flat[k]);
        const double f = frequency(ms(100), ms(400));
        std::printf("PTCH %d: %.1f Hz\n", flat[k][0], f);
        expect("PTCH sets the Machinedrum's pitch", std::fabs(f / hz[k] - 1) < 0.03);
    }
    expect("stays within the declared peak", bounded());
    // The Machinedrum's level, -15.5 dB, 2 dB up as the TRX machines are.
    const double top = db(peak(0, ms(30)));
    std::printf("peak %.1f dB\n", top);
    expect("peaks near -13.5 dB", std::fabs(top + 13.5) < 1.0);

    // DEC 64: the body falls about 135 dB a second.
    const int body[8] = {64, 64, 16, 64, 0, 64, 64, 0};
    PLAY(body);
    const double fall = db(rms(ms(10), ms(20))) - db(rms(ms(210), ms(20)));
    std::printf("DEC 64 falls %.1f dB in 200 ms\n", fall);
    expect("DEC 64 falls about 27 dB in 200 ms", fall > 24 && fall < 30);
    // DEC 0 ends quickly, onto the quiet tail.
    const int tight[8] = {64, 0, 16, 64, 0, 64, 64, 0};
    PLAY(tight);
    expect("DEC 0 is down 40 dB by 100 ms", db(rms(ms(100), ms(20))) - db(rms(0, ms(10))) < -36);

    // RAMP 127: the pitch starts 7.9 times the settled one and falls at RDEC's rate.
    const int swept[8] = {64, 96, 127, 64, 0, 64, 64, 0};
    PLAY(swept);
    const double early = frequency(0, ms(5)), late = frequency(ms(600), ms(200));
    std::printf("RAMP 127: %.0f Hz falling to %.1f Hz\n", early, late);
    expect("RAMP sweeps from far above", early > late * 6);
    // RAMP 0 rises from below.
    const int rising[8] = {64, 96, 0, 64, 0, 64, 64, 0};
    PLAY(rising);
    expect("RAMP 0 rises from below", frequency(ms(20), ms(30)) < 60);

    // MOD 64 at MFRQ 64: the modulator at 4 times the carrier, index 1, so the
    // first sidebands (at 5 times) stand -5 dB to the carrier.
    const int fm[8] = {64, 96, 16, 64, 64, 64, 127, 0};
    PLAY(fm);
    const double side = db(tone(512.5, ms(50), ms(300))) - db(tone(102.5, ms(50), ms(300)));
    std::printf("MOD 64 sideband %.1f dB\n", side);
    expect("MOD 64 is index 1", side > -7 && side < -3);
    // MDEC 64: the index falls with a 110 ms time constant, so the sideband falls away.
    const int fading[8] = {64, 112, 16, 64, 64, 64, 64, 0};
    PLAY(fading);
    const double now = tone(512.5, 0, ms(30)) / tone(102.5, 0, ms(30));
    const double later = tone(512.5, ms(300), ms(30)) / tone(102.5, ms(300), ms(30));
    std::printf("MDEC 64 sideband %.2f then %.2f\n", now, later);
    expect("MDEC 64 takes the modulation away", later < now * 0.2);

    // MFB 16: the late feedback pulls the carrier down to about 87 Hz.
    const int fed[8] = {64, 96, 16, 64, 64, 64, 127, 16};
    PLAY(fed);
    expect("MFB pulls the carrier down", tone(87, ms(50), ms(300)) > 2 * tone(102.5, ms(50), ms(300)));

    // Every control's ends stay bounded.
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
    return finish("efm_bd");
}
