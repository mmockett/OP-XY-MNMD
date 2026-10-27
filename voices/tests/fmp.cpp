// Sound checks for FM+STAT, FM+PAR and FM+DYN, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Monomachine's (reference/fit_fmp.py, reference/README.md).
#include "../fmp.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const double kMiddleC = 261.63;

opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }   // 0..127 -> Q31

template <class State, class Reset, class Trigger, class Render>
void play(Reset reset, Trigger trigger, Render render, const int* values, opxy::i32* out, int note = 60) {
    static State voice;
    md::Common shared = {};
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i)
        pitch[i] = static_cast<opxy::u32>(25479004.0 * std::pow(2.0, (note - 60) / 12.0));
    reset(voice, shared, 1);
    trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
}
void stat(const int* v, opxy::i32* out, int note = 60) {
    play<md::fmpstat::State>(md::fmpstat::reset, md::fmpstat::trigger, md::fmpstat::render, v, out, note);
}
void par(const int* v, opxy::i32* out, int note = 60) {
    play<md::fmppar::State>(md::fmppar::reset, md::fmppar::trigger, md::fmppar::render, v, out, note);
}
void dyn(const int* v, opxy::i32* out, int note = 60) {
    play<md::fmpdyn::State>(md::fmpdyn::reset, md::fmpdyn::trigger, md::fmpdyn::render, v, out, note);
}

bool bounded(const opxy::i32* out) {
    for (unsigned i = 0; i < kFrames; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}
// The level of a frequency over a window (Hann), relative to full scale.
double level(const opxy::i32* out, unsigned from, unsigned count, double hz) {
    double re = 0, im = 0;
    for (unsigned i = 0; i < count; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / count);
        const double x = out[from + i] / 2147483648.0 * w;
        re += x * std::cos(2 * M_PI * hz * i / 44100);
        im += x * std::sin(2 * M_PI * hz * i / 44100);
    }
    return std::sqrt(re * re + im * im) * 4 / count;
}
double db(double ratio) { return 20 * std::log10(ratio + 1e-12); }
// A spectral centroid over 64 bands of 125 Hz to 8 kHz, Hz.
double centroid(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0, weighted = 0;
    for (int k = 1; k <= 64; ++k) {
        const double l = level(out, from, count, k * 125.0);
        sum += l;
        weighted += l * k * 125.0;
    }
    return weighted / sum;
}
// Harmonic k against the fundamental, dB, at middle C, over 200 ms from `from`.
double harmonic(const opxy::i32* out, unsigned from, int k, double f0 = kMiddleC) {
    return db(level(out, from, 8820, k * f0) / level(out, from, 8820, f0));
}
}

int main() {
    static opxy::i32 out[kFrames];
    char what[96];

    // ---- FM+STAT: 1FRQ 1FIN 1ENV 1FB / 2FRQ 2VOL TONE TUNE
    // No depth: a pure sine at the note, at a quarter of full scale.
    const int pure[8] = {82, 64, 0, 0, 82, 0, 64, 64};
    stat(pure, out);
    check("STAT sounds at a quarter of full scale", std::fabs(level(out, 22050, 8820, kMiddleC) - 0.25) < 0.02);
    check("STAT with no depth is a pure sine", harmonic(out, 22050, 2) < -60);

    // Static depths, as on the Monomachine at ratio 1 (TONE 64): 2VOL 32 gives the
    // second harmonic 13.6 dB under the fundamental, 2VOL 64 4.0 dB over it.
    const int light[8] = {82, 64, 0, 0, 82, 32, 64, 64};
    stat(light, out);
    std::printf("STAT 2VOL 32: H2 %.1f dB (Monomachine -13.6)\n", harmonic(out, 22050, 2));
    check("STAT 2VOL 32 matches", std::fabs(harmonic(out, 22050, 2) + 13.6) < 2);
    const int medium[8] = {82, 64, 0, 0, 82, 64, 64, 64};
    stat(medium, out);
    std::printf("STAT 2VOL 64: H2 %.1f dB, H3 %.1f dB (Monomachine +4.0, -4.1)\n", harmonic(out, 22050, 2), harmonic(out, 22050, 3));
    check("STAT 2VOL 64 matches", std::fabs(harmonic(out, 22050, 2) - 4.0) < 2.5 && std::fabs(harmonic(out, 22050, 3) + 4.1) < 2.5);

    // 1ENV above 64 decays: at 96 (tau 95 ms) the brightness is gone by half a second;
    // at 64 it holds.
    const int decaying[8] = {82, 64, 96, 0, 82, 0, 64, 64};
    stat(decaying, out);
    check("STAT 1ENV 96 decays", harmonic(out, 441, 2) > -15 && harmonic(out, 22050, 2) < -50);
    const int held[8] = {82, 64, 64, 0, 82, 0, 64, 64};
    stat(held, out);
    check("STAT 1ENV 64 holds", std::fabs(harmonic(out, 441, 2) - harmonic(out, 30870, 2)) < 1);

    // TONE low-passes the modulators: TONE 0 leaves the carrier nearly pure; TONE 64
    // passes ratio 1 at middle C but not ratio 4.
    const int dark[8] = {82, 64, 0, 0, 82, 64, 0, 64};
    stat(dark, out);
    check("STAT TONE 0 darkens", harmonic(out, 22050, 2) < -40);
    const int high[8] = {82, 64, 0, 0, 125, 64, 32, 64}, open_[8] = {82, 64, 0, 0, 125, 64, 127, 64};
    stat(high, out);
    const double closed = level(out, 22050, 8820, 5 * kMiddleC);
    stat(open_, out);
    check("STAT TONE opens a high ratio", level(out, 22050, 8820, 5 * kMiddleC) > closed * 4);

    // Listed ratios: 1FRQ 61 is 1/2, so the sidebands sit 130.8 Hz from the carrier.
    const int half[8] = {61, 64, 64, 0, 82, 0, 64, 64};
    stat(half, out);
    check("STAT 1FRQ 1/2 puts sidebands at half the note",
          db(level(out, 22050, 8820, 1.5 * kMiddleC) / level(out, 22050, 8820, kMiddleC)) > -20);
    // 1FIN 96 moves M1 by an eighth of the carrier: sidebands at 1.125 x.
    const int fine[8] = {82, 96, 32, 0, 82, 0, 64, 64};
    stat(fine, out);
    check("STAT 1FIN moves M1", db(level(out, 22050, 8820, 2.125 * kMiddleC) / level(out, 22050, 8820, kMiddleC)) > -30);

    // 1FB: up to 32 harmonics only; at 64 the carrier's pitch rises about 60 Hz
    // (the Monomachine: +61 Hz), from the fed-back wave's DC part.
    const int fed[8] = {82, 64, 64, 64, 82, 0, 64, 64};
    stat(fed, out);
    double peak = 0, at = 0;
    for (double hz = 200; hz < 400; hz += 1) { const double l = level(out, 13230, 16384, hz); if (l > peak) { peak = l; at = hz; } }
    std::printf("STAT 1FB 64: strongest partial near the note at %.0f Hz (Monomachine 323)\n", at);
    check("STAT 1FB 64 shifts the carrier up", at > 300 && at < 345);
    const int soft[8] = {82, 64, 64, 32, 82, 0, 64, 64};
    stat(soft, out);
    check("STAT 1FB 32 keeps the pitch", level(out, 22050, 8820, kMiddleC) > 3 * level(out, 22050, 8820, kMiddleC + 20));

    // Pitch: an octave up doubles; TUNE 127 is about +2 semitones.
    stat(pure, out, 72);
    check("STAT tracks the key", level(out, 22050, 8820, 2 * kMiddleC) > 0.2);
    const int tuned[8] = {82, 64, 0, 0, 82, 0, 64, 127};
    stat(tuned, out);
    check("STAT TUNE 127 is +2 semitones", level(out, 22050, 8820, kMiddleC * std::pow(2.0, 63 / 32.0 / 12)) > 0.2);

    // ---- FM+PAR: 1FRQ 1ENV 2FRQ 2ENV / 3FRQ 3ENV TONE TUNE
    const int par_pure[8] = {82, 0, 104, 0, 114, 0, 64, 64};
    par(par_pure, out);
    check("PAR with no depth is a pure sine", harmonic(out, 22050, 2) < -60);
    const int par_static[8] = {82, 64, 104, 0, 114, 0, 64, 64};
    par(par_static, out);
    check("PAR 1ENV 64 matches STAT's depth", std::fabs(harmonic(out, 22050, 2) - 4.0) < 2.5);
    // PAR's decays start from the depth curve: 1ENV 96 opens far brighter than 64.
    const int par_decay[8] = {82, 96, 104, 0, 114, 0, 64, 64};
    par(par_decay, out);
    std::printf("PAR 1ENV 96: centroid %.0f Hz at the start, %.0f Hz at half a second\n", centroid(out, 0, 1764), centroid(out, 22050, 1764));
    check("PAR 1ENV 96 opens bright and decays", centroid(out, 0, 1764) > 600 && centroid(out, 22050, 1764) < 400);

    // ---- FM+DYN: 1FRQ 1FEN 1VOL 1VEN / 2FRQ 2ENV 2FB TUNE
    const int dyn_pure[8] = {64, 64, 0, 64, 64, 0, 0, 64};
    dyn(dyn_pure, out);
    check("DYN with no depth is a pure sine", harmonic(out, 22050, 2) < -60);
    // 1FRQ is continuous: v / 64 of the carrier (32 is 1/2).
    const int dyn_half[8] = {32, 64, 32, 64, 64, 0, 0, 64};
    dyn(dyn_half, out);
    check("DYN 1FRQ 32 is a ratio of 1/2", db(level(out, 22050, 8820, 1.5 * kMiddleC) / level(out, 22050, 8820, kMiddleC)) > -25);
    // 1VEN above 64 lifts 1VOL at the start and falls back to it.
    const int lifted[8] = {64, 64, 32, 96, 64, 0, 0, 64};
    dyn(lifted, out);
    check("DYN 1VEN lifts the depth at the start", harmonic(out, 441, 3) > harmonic(out, 30870, 3) + 10);
    // 1FEN above 64 starts M1 high: early sidebands far from the carrier.
    const int swept[8] = {64, 96, 32, 64, 64, 0, 0, 64};
    dyn(swept, out);
    std::printf("DYN 1FEN 96: centroid %.0f Hz at the start, %.0f Hz at 0.8 s\n", centroid(out, 0, 1764), centroid(out, 35280, 1764));
    check("DYN 1FEN sweeps M1 down", centroid(out, 0, 1764) > 2 * centroid(out, 35280, 1764));

    // ---- Every machine: bounded at the controls' ends, across five octaves.
    for (int machine = 0; machine < 3; ++machine)
        for (int note = 36; note <= 108; note += 12)
            for (int setting = 0; setting < 3; ++setting) {
                int v[8];
                for (int i = 0; i < 8; ++i) v[i] = setting == 0 ? 0 : setting == 1 ? 127 : (i * 37 + 11) % 128;
                if (machine == 0) stat(v, out, note); else if (machine == 1) par(v, out, note); else dyn(v, out, note);
                std::snprintf(what, sizeof what, "machine %d note %d setting %d stays within the declared peak", machine, note, setting);
                check(what, bounded(out));
            }

    if (!failures) std::puts("PASS fmp");
    return failures ? 1 : 0;
}
