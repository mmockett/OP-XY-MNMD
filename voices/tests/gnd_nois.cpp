// Sound checks for GND-NOIS, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Monomachine's, from reference/fit_mgnd.py.
#include "../gnd_nois.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const double kMiddleC = 261.6256;

opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }   // 0..127 -> Q31
opxy::u32 increment(double hz) { return static_cast<opxy::u32>(hz / 44100.0 * 4294967296.0); }

// Plays one machine's voice for a second at a note; `again` keeps the voice's
// state from the last note, as the next note on the same voice would.
template <class State, class Reset, class Trigger, class Render>
void play(State& voice, Reset reset, Trigger trigger, Render render, const int* values, opxy::i32* out,
          double hz = kMiddleC, bool again = false) {
    md::Common shared = {};
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = hz == kMiddleC ? 25479004u : increment(hz);
    if (!again) reset(voice, shared, 1);
    trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
}

double rms(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / count) / 2147483648.0;
}

bool bounded(const opxy::i32* out) {
    for (unsigned i = 0; i < kFrames; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}

// The amplitude of one frequency over a window, Hann-weighted (a single DFT bin).
double line(const opxy::i32* out, unsigned from, unsigned count, double hz) {
    double re = 0, im = 0, weight = 0;
    for (unsigned i = 0; i < count; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / count);
        const double x = out[from + i] / 2147483648.0;
        re += w * x * std::cos(2 * M_PI * hz * i / 44100);
        im += w * x * std::sin(2 * M_PI * hz * i / 44100);
        weight += w;
    }
    return 2 * std::sqrt(re * re + im * im) / weight;
}
// The strongest frequency near a guess, to 0.1 Hz.
double peak(const opxy::i32* out, unsigned from, unsigned count, double guess) {
    double best = guess, most = 0;
    for (double f = guess * 0.97; f < guess * 1.03; f += 0.1) {
        const double a = line(out, from, count, f);
        if (a > most) { most = a; best = f; }
    }
    return best;
}
double cents(double f, double ref) { return 1200 * std::log2(f / ref); }
// The power-weighted frequency within a semitone of a guess: where a narrow band of
// noise sits. Sums may run over several seconds, for the narrow bands of low notes.
void band_sums(const opxy::i32* out, unsigned from, unsigned count, double guess, double& sum, double& weight) {
    for (int c = -100; c <= 100; c += 4) {
        const double f = guess * std::pow(2.0, c / 1200.0), a = line(out, from, count, f);
        sum += a * a * f; weight += a * a;
    }
}
double band_centre(const opxy::i32* out, unsigned from, unsigned count, double guess) {
    double sum = 0, weight = 0;
    band_sums(out, from, count, guess, sum, weight);
    return sum / weight;
}
// The share of the energy within a semitone of a frequency (Hann-weighted DFT bins, 1 Hz apart).
double near_share(const opxy::i32* out, unsigned from, unsigned count, double hz) {
    double in = 0;
    for (double f = hz * 0.944; f <= hz * 1.059; f += 44100.0 / count) { const double a = line(out, from, count, f); in += a * a; }
    double all = 0;
    for (unsigned i = from; i < from + count; ++i) { const double x = out[i] / 2147483648.0; all += x * x; }
    return in / 4 / (all / count);   // a line's amplitude a carries a^2 / 2 of power; the Hann window halves the bins' sum again
}
unsigned at_ceiling(const opxy::i32* out) {
    unsigned n = 0;
    for (unsigned i = 0; i < kFrames; ++i) n += out[i] >= 1073741824 || out[i] <= -1073741824;
    return n;
}

}

// The share of the energy below 400 Hz, from a crude two-band split.
double low_share(const opxy::i32* out) {
    double low = 0, all = 0, state = 0;
    const double a = 1 - std::exp(-2 * M_PI * 400 / 44100.0);
    for (unsigned i = 4410; i < kFrames; ++i) {
        const double x = out[i] / 2147483648.0;
        state += a * (x - state);
        low += state * state; all += x * x;
    }
    return low / all;
}

// Zero crossings a second, after the first tenth of a second.
double crossings(const opxy::i32* out) {
    unsigned n = 0;
    for (unsigned i = 4411; i < kFrames; ++i) n += (out[i] < 0) != (out[i - 1] < 0);
    return n * 44100.0 / (kFrames - 4411);
}

int main() {
    static opxy::i32 out[kFrames];
    static md::gnd_nois::State voice;
#define NOIS voice, md::gnd_nois::reset, md::gnd_nois::trigger, md::gnd_nois::render
    // ST RED STON - / - RES - TUNE
    const int white[8] = {0, 0, 0, 0, 0, 0, 0, 64}, pinkish[8] = {0, 64, 0, 0, 0, 0, 0, 64}, red[8] = {0, 127, 0, 0, 0, 0, 0, 64};
    play(NOIS, white, out);
    const double w = rms(out, 4410, kFrames - 4410), w_low = low_share(out);
    play(NOIS, pinkish, out);
    const double p = rms(out, 4410, kFrames - 4410);
    play(NOIS, red, out);
    const double r = rms(out, 4410, kFrames - 4410), r_low = low_share(out);
    std::printf("white %.4f, RED 64 %.4f, RED 127 %.4f rms; below 400 Hz %.3f, %.3f of the energy\n", w, p, r, w_low, r_low);
    // The Monomachine's 0.047 rms x the engine's 1.2; RED 64 1.45 x, RED 127 0.47 x.
    check("the fitted level", w > 0.052 && w < 0.061);
    check("RED is loudest at its middle", p / w > 1.3 && p / w < 1.6);
    check("RED 127 is quieter", r / w > 0.38 && r / w < 0.55);
    check("RED 127 is red", r_low > 0.55 && w_low < 0.2);
    // ST: +2.9 dB at 127 with STON off, nothing with it on.
    const int st[8] = {127, 0, 0, 0, 0, 0, 0, 64}, st_on[8] = {127, 0, 127, 0, 0, 0, 0, 64};
    play(NOIS, st, out);
    const double s = rms(out, 4410, kFrames - 4410) / w;
    play(NOIS, st_on, out);
    const double s_on = rms(out, 4410, kFrames - 4410) / w;
    std::printf("ST 127: %.2f x, with STON %.2f x\n", s, s_on);
    check("ST raises the level", s > 1.3 && s < 1.5);
    check("STON leaves the level", s_on > 0.95 && s_on < 1.05);
    // The key and TUNE: the red part passes a two-pole high-pass an octave under the
    // played note, so a higher note is a brighter, thinner rush. On the Monomachine
    // at RED 127 the zero-crossing rate is about 930 a second at C2, 1580 at C4 and
    // 2990 at C6 (ours, with the white top following the key: 1010, 1810, 3160);
    // the level falls 2.1 x from C2 to C4 (ours 2.0 x); TUNE 0 .. 127 moves the
    // rate 1.16 x (ours 1.18 x). Up to v1.2.3 the key and TUNE did nothing here.
    play(NOIS, red, out, 65.406);
    const double c2 = crossings(out), c2_rms = rms(out, 4410, kFrames - 4410);
    play(NOIS, red, out);
    const double c4 = crossings(out), c4_rms = rms(out, 4410, kFrames - 4410);
    play(NOIS, red, out, 1046.502);
    const double c6 = crossings(out);
    const int red_down[8] = {0, 127, 0, 0, 0, 0, 0, 0}, red_up[8] = {0, 127, 0, 0, 0, 0, 0, 127};
    play(NOIS, red_down, out);
    const double t0 = crossings(out);
    play(NOIS, red_up, out);
    const double t127 = crossings(out);
    play(NOIS, white, out, 65.406);
    const double w2 = crossings(out), w2_rms = rms(out, 4410, kFrames - 4410);
    play(NOIS, white, out, 1046.502);
    const double w6 = crossings(out), w6_rms = rms(out, 4410, kFrames - 4410);
    std::printf("RED 127 crossings/s C2 %.0f, C4 %.0f, C6 %.0f; level C2/C4 %.2f; TUNE 0 %.0f, 127 %.0f; white C2 %.0f, C6 %.0f (level %.2f)\n",
                c2, c4, c6, c2_rms / c4_rms, t0, t127, w2, w6, w6_rms / w2_rms);
    check("the key moves RED's noise up", c4 / c2 > 1.35 && c6 / c4 > 1.6);
    check("the key's level falls as on the original", c2_rms / c4_rms > 1.5 && c2_rms / c4_rms < 2.4);
    check("TUNE moves RED's noise", t127 / t0 > 1.08 && t127 / t0 < 1.3);
    // White noise darkens as the key rises, its level held: on the Monomachine the
    // crossings fall from 9640 a second at C2 to 6400 at C6 (0.66 x; ours 0.69 x).
    check("the key darkens white noise", w6 / w2 > 0.55 && w6 / w2 < 0.78);
    check("the key keeps white noise's level", w6_rms / w2_rms > 0.9 && w6_rms / w2_rms < 1.1);

    // RES: 0 is the original, above; turned up it narrows the noise to the note.
    // At middle C its level stays within 2.5 dB of RES 0's and the share within a
    // semitone of the note grows at every step, for white noise and red alike.
    for (int colour = 0; colour < 2; ++colour) {
        double first = 0, last_share = 0;
        bool held = true, rising = true;
        std::printf("RED %d, RES 0..127 at C4: level / share near the note:", colour ? 127 : 0);
        for (int r = 0; r <= 128; r += 32) {
            const int v = r > 127 ? 127 : r;
            const int values[8] = {0, colour ? 127 : 0, 0, 0, 0, v, 0, 64};
            play(NOIS, values, out);
            const double level = rms(out, 4410, kFrames - 4410), share = near_share(out, 4410, kFrames - 4410, kMiddleC);
            std::printf(" %.3f / %.2f", level, share);
            if (!v) first = level;
            held = held && std::fabs(20 * std::log10(level / first)) < 2.5;
            rising = rising && (v == 0 || share > last_share * 1.15);
            last_share = share;
        }
        std::puts("");
        check(colour ? "RES holds red noise's level" : "RES holds white noise's level", held);
        check(colour ? "RES narrows red noise to the note" : "RES narrows white noise to the note", rising);
    }
    // At RES 127 the band sits on the note, within 10 cents, from C1 to C7, and its
    // level stays within 4 dB of middle C's (the original falls 18 dB at RED 127).
    {
        const int pitched[8] = {0, 64, 0, 0, 0, 127, 0, 64};
        play(NOIS, pitched, out);
        const double c4_level = rms(out, 4410, kFrames - 4410);
        double worst = 0, spread = 0;
        for (int octave = -3; octave <= 3; ++octave) {
            const double hz = kMiddleC * std::pow(2.0, octave);
            play(NOIS, pitched, out, octave ? hz : kMiddleC);
            spread = std::max(spread, std::fabs(20 * std::log10(rms(out, 4410, kFrames - 4410) / c4_level)));
            // Eight seconds: at C1 the band is under 2 Hz wide.
            double sum = 0, weight = 0;
            for (int second = 0; second < 8; ++second) {
                if (second) play(NOIS, pitched, out, octave ? hz : kMiddleC, true);
                band_sums(out, 0, kFrames, hz, sum, weight);
            }
            worst = std::max(worst, std::fabs(cents(sum / weight, hz)));
        }
        std::printf("RES 127: the band within %.1f cents of the note C1..C7, the level within %.1f dB of C4's\n", worst, spread);
        check("RES plays the key in tune", worst < 10);
        check("RES holds the level across the keys", spread < 4);
    }
    // TUNE, with RES up, moves the pitch +-2 semitones.
    {
        double moved[2];
        for (int end = 0; end < 2; ++end) {
            const int values[8] = {0, 64, 0, 0, 0, 96, 0, end ? 127 : 0};
            play(NOIS, values, out);
            const double want = kMiddleC * std::pow(2.0, (end ? 2 : -2) / 12.0);
            moved[end] = cents(band_centre(out, 4410, kFrames - 4410, want), kMiddleC);
        }
        std::printf("RES 96: TUNE 0 %+.0f cents, 127 %+.0f cents\n", moved[0], moved[1]);
        check("TUNE moves RES's pitch two semitones down", moved[0] > -225 && moved[0] < -175);
        check("TUNE moves RES's pitch two semitones up", moved[1] > 175 && moved[1] < 225);
    }
    // Pitched settings never reach the ceiling, on any key: the presets and RES's end.
    {
        const int settings[4][8] = {{0, 40, 0, 0, 0, 56, 0, 64}, {0, 16, 0, 0, 0, 112, 0, 64}, {0, 80, 0, 0, 0, 112, 0, 64},
                                    {127, 64, 0, 0, 0, 127, 0, 127}};
        unsigned clamped = 0;
        for (int i = 0; i < 4; ++i)
            for (int octave = -3; octave <= 4; ++octave) {
                play(NOIS, settings[i], out, kMiddleC * std::pow(2.0, octave));
                clamped += at_ceiling(out);
            }
        std::printf("RES's presets and ends, C1..C8: %u samples at the ceiling\n", clamped);
        check("RES never reaches the ceiling", clamped == 0);
    }
    // Every setting stays inside the ceiling.
    bool ok = true;
    for (int setting = 0; setting < 3; ++setting) {
        int values[8];
        for (int i = 0; i < 8; ++i) values[i] = setting == 0 ? 0 : setting == 1 ? 64 : 127;
        play(NOIS, values, out);
        ok = ok && bounded(out);
    }
    const int loud[8] = {127, 64, 0, 0, 0, 0, 0, 64};
    play(NOIS, loud, out);
    check("stays within the declared peak", ok && bounded(out));
    // Stereo (render_stereo). STON off, or ST 0, is the mono noise on both sides.
    // At full width (STON on, ST 127) the sides are two noises, each within 0.5 dB
    // of the mono noise's level, and nothing passes the declared peak.
    {
        using namespace md;
        static gnd_nois::State a, b;
        static opxy::i32 mono[opxy::block_frames], both[2 * opxy::block_frames];
        md::Common shared = {};
        opxy::u32 pitch[opxy::block_frames];
        for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = 25479004u;   // middle C
        const int reds[4] = {0, 48, 127, 16}, ress[4] = {0, 0, 0, 64};
        for (int n = 0; n < 4; ++n) {
            opxy::i32 off[8] = {}, zero[8] = {}, on[8] = {};
            off[1] = zero[1] = on[1] = q(reds[n]);
            off[5] = zero[5] = on[5] = q(ress[n]);
            off[7] = zero[7] = on[7] = q(64);
            off[0] = q(127);
            zero[2] = on[2] = q(127);
            on[0] = 0x7fffffff;
            const opxy::i32* const mono_cases[2] = {off, zero};
            for (const opxy::i32* mono_case : mono_cases) {
                gnd_nois::reset(a, shared, 1); gnd_nois::reset(b, shared, 1);
                bool same = true;
                for (unsigned k = 0; k < 40; ++k) {
                    gnd_nois::render(a, shared, mono, mono_case, pitch);
                    gnd_nois::render_stereo(b, shared, both, mono_case, pitch);
                    for (unsigned i = 0; i < opxy::block_frames; ++i) same = same && both[2 * i] == mono[i] && both[2 * i + 1] == mono[i];
                }
                check(mono_case == off ? "stereo: STON off is the mono noise on both sides" : "stereo: ST 0 is the mono noise on both sides", same);
            }
            double m = 0, left = 0, right = 0, cross = 0;
            bool bounded = true;
            gnd_nois::reset(a, shared, 1); gnd_nois::reset(b, shared, 1);
            for (unsigned k = 0; k < 2760; ++k) {
                gnd_nois::render(a, shared, mono, on, pitch);
                gnd_nois::render_stereo(b, shared, both, on, pitch);
                for (unsigned i = 0; i < opxy::block_frames; ++i) {
                    const opxy::i32 l = both[2 * i], r = both[2 * i + 1];
                    bounded = bounded && l >= -1073741824 && l <= 1073741824 && r >= -1073741824 && r <= 1073741824;
                    if (k > 80) {
                        m += static_cast<double>(mono[i]) * mono[i];
                        left += static_cast<double>(l) * l;
                        right += static_cast<double>(r) * r;
                        cross += static_cast<double>(l) * r;
                    }
                }
            }
            const double to_left = 10 * std::log10(left / m), to_right = 10 * std::log10(right / m);
            std::printf("stereo, RED %d RES %d: left %+.2f dB, right %+.2f dB against mono, correlation %.2f\n",
                        reds[n], ress[n], to_left, to_right, cross / std::sqrt(left * right));
            check("stereo: above the declared peak", bounded);
            check("stereo: each side within 0.5 dB of mono", std::fabs(to_left) <= 0.5 && std::fabs(to_right) <= 0.5);
            // A narrower band (RES) has fewer independent samples in four seconds.
            check("stereo: full width is two noises", std::fabs(cross / std::sqrt(left * right)) < (ress[n] ? 0.3 : 0.1));
        }
    }
    if (!failures) std::puts("PASS gnd_nois");
    return failures ? 1 : 0;
}
