// Sound checks for EFM-RS, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted measures are the Machinedrum's (reference/fit_efm_rs.py).
#include "../efm_rs.hpp"
#include "efm_check.hpp"
#define PLAY check::play<md::efm_rs::State, md::efm_rs::reset, md::efm_rs::trigger, md::efm_rs::render>

using namespace check;

int main() {
    // PTCH: the rim alone (SNAR 0, MOD 0), 243.9 Hz at 0, 975.7 at 64, 3819 at 127.
    const int rims[3][8] = {{0, 64, 0, 0, 0, 64, 64, 0}, {64, 64, 0, 0, 0, 64, 64, 0}, {127, 64, 0, 0, 0, 64, 64, 0}};
    const double hz[3] = {243.9, 975.7, 3818.7};
    for (int k = 0; k < 3; ++k) {
        PLAY(rims[k]);
        const double f = frequency(ms(20), ms(200));
        std::printf("PTCH %d: %.1f Hz\n", rims[k][0], f);
        expect("PTCH sets the rim's pitch", std::fabs(f / hz[k] - 1) < 0.03);
    }
    // The rim's level, the Machinedrum's -19.0 dB RMS (first 5 ms), 2 dB up;
    // DEC 64 falls 13 dB in 100 ms, DEC 0 36 dB in 10 ms.
    PLAY(rims[1]);
    const double rim = db(rms(0, ms(5)));
    const double fall = rim - db(rms(ms(100), ms(5)));
    std::printf("rim %.1f dB RMS, DEC 64 falls %.1f dB in 100 ms\n", rim, fall);
    expect("the rim sits near -17 dB RMS", std::fabs(rim + 17) < 1.0);
    expect("DEC 64 falls about 13 dB in 100 ms", fall > 11 && fall < 15);
    const int tight[8] = {64, 0, 0, 0, 0, 64, 64, 0};
    PLAY(tight);
    expect("DEC 0 falls 30 dB in 10 ms", db(rms(ms(10), ms(5))) - db(rms(0, ms(5))) < -30);

    // SNAR: the snare at SPTC's pitch (290.7 Hz at 64, 121.9 at 0), in proportion.
    const int snare[8] = {64, 0, 0, 0, 127, 64, 96, 0};
    PLAY(snare);
    const double sf = frequency(ms(20), ms(200));
    const double full = db(rms(ms(20), ms(5)));
    std::printf("snare %.1f Hz, %.1f dB RMS at 20 ms\n", sf, full);
    expect("SPTC 64 sets the snare near 290.7 Hz", std::fabs(sf / 290.7 - 1) < 0.03);
    expect("SNAR 127 is as loud as the rim", std::fabs(full - rim) < 3);
    const int half[8] = {64, 0, 0, 0, 64, 64, 96, 0};
    PLAY(half);
    const double halved = db(rms(ms(20), ms(5))) - full;
    std::printf("SNAR 64: %.1f dB\n", halved);
    expect("SNAR 64 is 6 dB under 127", halved < -4.5 && halved > -7.5);
    const int low[8] = {64, 0, 0, 0, 127, 0, 96, 0};
    PLAY(low);
    expect("SPTC 0 sets the snare near 121.9 Hz", std::fabs(frequency(ms(20), ms(200)) / 121.9 - 1) < 0.03);

    // MOD: gentle; at 127 the rim's brightness rises by a fifth (centroid 976 -> 1173 Hz).
    PLAY(rims[1]);
    const double plain = brightness(ms(10), ms(90));
    const int rough[8] = {64, 64, 127, 0, 0, 64, 64, 0};
    PLAY(rough);
    const double moved = brightness(ms(10), ms(90));
    std::printf("MOD 127: brightness %.0f -> %.0f Hz\n", plain, moved);
    expect("MOD 127 brightens the rim gently", moved > plain * 1.1 && moved < plain * 2.5);

    // HPF 127 takes the default down about 12 dB.
    const int open[8] = {64, 96, 64, 0, 64, 64, 96, 64};
    const int cut[8] = {64, 96, 64, 127, 64, 64, 96, 64};
    PLAY(open);
    const double before = db(rms(0, ms(5)));
    PLAY(cut);
    const double after = db(rms(0, ms(5)));
    std::printf("HPF 127: %.1f dB\n", after - before);
    expect("HPF 127 takes it down 10 to 15 dB", after - before < -10 && after - before > -15);

    for (int s = 0; s < 8; ++s)
        for (int end = 0; end < 2; ++end) {
            int p[8] = {64, 64, 64, 64, 64, 64, 64, 64};
            p[s] = end ? 127 : 0;
            PLAY(p);
            expect("bounded at a control's end", bounded());
        }
    const int hot[8] = {127, 127, 127, 0, 127, 127, 127, 127};
    PLAY(hot);
    expect("hot settings stay bounded", bounded());
    expect("hot settings sound", rms(0, ms(100)) > 0.02);
    return finish("efm_rs");
}
