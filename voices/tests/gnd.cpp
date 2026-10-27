// Sound checks for GND-SN, GND-NS and GND-IM, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted measures are the Machinedrum's (reference/fit_gnd.py).
#include "../gnd_sn.hpp"
#include "../gnd_ns.hpp"
#include "../gnd_im.hpp"
#include "efm_check.hpp"
#define SN check::play<md::gnd_sn::State, md::gnd_sn::reset, md::gnd_sn::trigger, md::gnd_sn::render>
#define NS check::play<md::gnd_ns::State, md::gnd_ns::reset, md::gnd_ns::trigger, md::gnd_ns::render>
#define IM check::play<md::gnd_im::State, md::gnd_im::reset, md::gnd_im::trigger, md::gnd_im::render>

using namespace check;

int main() {
    // GND-SN: an octave every 16 steps, 43.1 Hz at 0, 689 at 64, 10.6 kHz at 127.
    const int sines[3][8] = {{0, 96, 0, 0, 0, 0, 0, 0}, {64, 96, 0, 0, 0, 0, 0, 0}, {127, 96, 0, 0, 0, 0, 0, 0}};
    const double hz[3] = {43.1, 689.1, 10612};
    for (int k = 0; k < 3; ++k) {
        SN(sines[k]);
        const double f = frequency(ms(100), ms(200));
        std::printf("SN PTCH %d: %.1f Hz\n", sines[k][0], f);
        expect("SN PTCH sets the Machinedrum's pitch", std::fabs(f / hz[k] - 1) < 0.03);
    }
    SN(sines[1]);
    const double sn = db(rms(0, ms(5)));
    std::printf("SN level %.1f dB RMS\n", sn);
    expect("SN sits 2 dB over the Machinedrum's -26.6 dB RMS", std::fabs(sn + 24.6) < 1.0);
    // DEC 64 falls 28 dB in 100 ms.
    const int sn_dec[8] = {64, 64, 0, 0, 0, 0, 0, 0};
    SN(sn_dec);
    const double fall = db(rms(0, ms(5))) - db(rms(ms(100), ms(5)));
    expect("SN DEC 64 falls about 28 dB in 100 ms", fall > 24 && fall < 32);
    // RAMP 32 starts the pitch at 5 times; RDEC 32 brings it back with a 52 ms time constant.
    const int sn_ramp[8] = {64, 96, 32, 32, 0, 0, 0, 0};
    SN(sn_ramp);
    const double early = frequency(0, ms(5)), late = frequency(ms(400), ms(200));
    std::printf("SN RAMP 32: %.0f Hz settling to %.0f Hz\n", early, late);
    expect("SN RAMP 32 starts near 3.4 kHz", std::fabs(early / 3445 - 1) < 0.1);
    expect("SN RDEC 32 settles back", std::fabs(late / 689.1 - 1) < 0.05);

    // GND-NS: white noise, the Machinedrum's -19 dB RMS 2 dB up; silent at DEC 0.
    const int hiss[8] = {96, 0, 0, 0, 0, 0, 0, 0};
    NS(hiss);
    const double ns = db(rms(0, ms(20)));
    std::printf("NS level %.1f dB RMS, brightness %.0f Hz\n", ns, brightness(0, ms(20)));
    expect("NS sits near -17 dB RMS", std::fabs(ns + 17) < 1.0);
    expect("NS is white", brightness(0, ms(20)) > 8000);
    const int quiet[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    NS(quiet);
    expect("NS DEC 0 is silent", rms(0, ms(20)) == 0);

    // GND-IM: UVAL 127 steps to -0.252 (2 dB up) for UP samples, then DVAL's for DOWN.
    const int im[8] = {64, 127, 64, 64, 0, 0, 0, 0};
    IM(im);
    const double first = out[0] / 2147483648.0, second = out[70] / 2147483648.0, after = out[140] / 2147483648.0;
    std::printf("IM: %.3f, %.3f at 70, %.3f at 140\n", first, second, after);
    expect("IM steps to UVAL's depth", std::fabs(first + 0.317) < 0.02);
    expect("IM steps up to DVAL's for DOWN", second > -0.05 && second < 0.05);
    expect("IM returns past zero, high-passed", after > 0.05);
    expect("IM is gone by 200 ms", peak(ms(200), ms(10)) < 0.001);

    const int ends[2] = {0, 127};
    for (int s = 0; s < 4; ++s)
        for (int e = 0; e < 2; ++e) {
            int p[8] = {64, 64, 64, 64, 0, 0, 0, 0};
            p[s] = ends[e];
            SN(p);
            expect("SN bounded at a control's end", bounded());
            NS(p);
            expect("NS bounded at a control's end", bounded());
            IM(p);
            expect("IM bounded at a control's end", bounded());
        }
    return finish("gnd");
}
