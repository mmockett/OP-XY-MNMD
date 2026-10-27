// Sound checks for SWAVE-SAW, SWAVE-PULSE and SWAVE-ENS, compiled for the host
// and run by
//   python3 tools/test_voices.py
// The measures are the Monomachine's, from reference/fit_swave.py.
#include "../swave.hpp"
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

// Every control at both ends and the middle, across four octaves, stays inside the ceiling.
template <class State, class Reset, class Trigger, class Render>
void extremes(const char* name, State& voice, Reset reset, Trigger trigger, Render render, opxy::i32* out) {
    bool ok = true, sounds = true;
    for (int octave = -2; octave <= 2; ++octave) {
        for (int setting = 0; setting < 3; ++setting) {
            int values[8];
            for (int i = 0; i < 8; ++i) values[i] = setting == 0 ? 0 : setting == 1 ? 64 : 127;
            play(voice, reset, trigger, render, values, out, kMiddleC * std::pow(2.0, octave));
            ok = ok && bounded(out);
            if (setting == 1 && octave == 0) sounds = rms(out, 4410, 4410) > 0.005;
        }
    }
    char what[96];
    std::snprintf(what, sizeof what, "%s stays within the declared peak", name);
    check(what, ok);
    std::snprintf(what, sizeof what, "%s sounds at the middle setting", name);
    check(what, sounds);
}
}

int main() {
    static opxy::i32 out[kFrames];
    static opxy::i32 other[kFrames];
    using namespace md;

    // --- SWAVE-SAW: UNIL UNIW UNIX - SUBX SUB1 SUB2 TUNE
    static swave_saw::State saw;
#define SAW saw, swave_saw::reset, swave_saw::trigger, swave_saw::render
    extremes("saw", SAW, out);
    const int plain[8] = {0, 0, 0, 0, 0, 0, 0, 64};
    play(SAW, plain, out);
    const double main_line = line(out, 13230, 30870, kMiddleC);
    std::printf("saw: middle C at %.2f Hz, rms %.4f\n", peak(out, 13230, 30870, kMiddleC), rms(out, 13230, 30870));
    check("saw plays the key", std::fabs(cents(peak(out, 13230, 30870, kMiddleC), kMiddleC)) < 2);
    // Fitted level: the Monomachine's 0.0102 rms x the engine's 2.2.
    check("saw's level", rms(out, 13230, 30870) > 0.020 && rms(out, 13230, 30870) < 0.025);
    // The saw is thin at its fundamental: about 2 dB over the 2nd harmonic, not a pure saw's 6.
    const double thin = 20 * std::log10(main_line / line(out, 13230, 30870, 2 * kMiddleC));
    std::printf("saw: fundamental %.1f dB over the 2nd\n", thin);
    check("saw's thin fundamental", thin > 0.5 && thin < 3.5);
    // Two octaves up and down it still plays the key.
    play(SAW, plain, out, kMiddleC * 4);
    check("saw tracks two octaves up", std::fabs(cents(peak(out, 13230, 30870, kMiddleC * 4), kMiddleC * 4)) < 2);
    play(SAW, plain, out, kMiddleC / 4);
    check("saw tracks two octaves down", std::fabs(cents(peak(out, 13230, 30870, kMiddleC / 4), kMiddleC / 4)) < 2);
    // UNIW 127 at UNIL 127: a pair 53.7 cents either side, as loud as the main saw.
    const int wide[8] = {127, 127, 0, 0, 0, 0, 0, 64};
    play(SAW, wide, out);
    const double up = kMiddleC * std::pow(2.0, 53.7 / 1200), down = kMiddleC * std::pow(2.0, -53.7 / 1200);
    const double pair = line(out, 0, kFrames, up) / line(out, 0, kFrames, kMiddleC);
    std::printf("saw: the pair at +53.7 cents, %.2f x the main\n", pair);
    check("saw's outer pair at UNIW 127", pair > 0.75 && pair < 1.3 && line(out, 0, kFrames, down) > 0.75 * line(out, 0, kFrames, kMiddleC));
    // UNIX 127: the inner pair at half the detune.
    const int inner[8] = {127, 127, 127, 0, 0, 0, 0, 64};
    play(SAW, inner, out);
    check("saw's inner pair at half the detune", line(out, 0, kFrames, kMiddleC * std::pow(2.0, 26.85 / 1200)) > 0.6 * line(out, 0, kFrames, kMiddleC));
    // UNIW 0: the stack starts in phase, so the three saws add up.
    const int narrow[8] = {127, 0, 0, 0, 0, 0, 0, 64};
    play(SAW, narrow, other);
    check("saw's stack starts in phase", line(other, 13230, 30870, kMiddleC) > 2.6 * main_line);
    // SUB1 127: a sine an octave down at 4.33 x the main saw's fundamental; SUB2 127 two octaves down at 1.66 x.
    const int sub1[8] = {0, 0, 0, 0, 0, 127, 0, 64};
    play(SAW, sub1, out);
    const double s1 = line(out, 13230, 30870, kMiddleC / 2) / main_line;
    const int sub2[8] = {0, 0, 0, 0, 0, 0, 127, 64};
    play(SAW, sub2, out);
    const double s2 = line(out, 13230, 30870, kMiddleC / 4) / main_line;
    const int subx[8] = {0, 0, 0, 0, 127, 0, 0, 64};
    play(SAW, subx, out);
    const double sx = line(out, 13230, 30870, kMiddleC / 2) / main_line;
    std::printf("saw: SUB1 %.2f x, SUB2 %.2f x, SUBX %.2f x the main fundamental\n", s1, s2, sx);
    check("saw's SUB1 level", s1 > 4.0 && s1 < 5.2);
    check("saw's SUB2 level", s2 > 1.45 && s2 < 1.9);
    check("saw's SUBX level", sx > 2.0 && sx < 2.6);
    // TUNE: +-2 semitones about 64, continuous.
    const int sharp[8] = {0, 0, 0, 0, 0, 0, 0, 127};
    play(SAW, sharp, out);
    const double tuned = cents(peak(out, 13230, 30870, kMiddleC * 1.12), kMiddleC);
    std::printf("saw: TUNE 127 is %.1f cents\n", tuned);
    check("saw's TUNE 127 is +1.97 semitones", std::fabs(tuned - 196.9) < 3);

    // --- SWAVE-PULSE: UNIL UNIW SUB1 SUB2 PW PWAD PWRS TUNE
    static swave_pulse::State pulse;
#define PULSE pulse, swave_pulse::reset, swave_pulse::trigger, swave_pulse::render
    extremes("pulse", PULSE, out);
    const int square[8] = {0, 0, 0, 0, 64, 0, 127, 64};
    play(PULSE, square, out);
    const double square_line = line(out, 13230, 30870, kMiddleC);
    std::printf("pulse: rms %.4f, 2nd harmonic %.1f dB\n", rms(out, 13230, 30870),
                20 * std::log10(line(out, 13230, 30870, 2 * kMiddleC) / square_line));
    check("pulse's level", rms(out, 13230, 30870) > 0.027 && rms(out, 13230, 30870) < 0.034);
    check("pulse at PW 64 is square", line(out, 13230, 30870, 2 * kMiddleC) < 0.01 * square_line);
    check("pulse plays the key", std::fabs(cents(peak(out, 13230, 30870, kMiddleC), kMiddleC)) < 2);
    // PW 32: a duty of 0.255, its fundamental sin(pi 0.255) = 0.72 of the square's.
    const int quarter[8] = {0, 0, 0, 0, 32, 0, 127, 64};
    play(PULSE, quarter, out);
    const double duty = line(out, 13230, 30870, kMiddleC) / square_line;
    check("pulse's duty at PW 32", duty > 0.68 && duty < 0.76);
    // The pair is detuned half as far as SAW's: 26.9 cents at UNIW 127.
    const int pairs[8] = {127, 127, 0, 0, 64, 0, 127, 64};
    play(PULSE, pairs, out);
    check("pulse's pair at half SAW's detune", line(out, 0, kFrames, kMiddleC * std::pow(2.0, 26.85 / 1200)) > 0.75 * line(out, 0, kFrames, kMiddleC));
    // SUB1 127: a sine an octave down at 6 x the square's fundamental.
    const int psub[8] = {0, 0, 127, 0, 64, 0, 127, 64};
    play(PULSE, psub, out);
    const double ps1 = line(out, 13230, 30870, kMiddleC / 2) / square_line;
    std::printf("pulse: SUB1 %.2f x the square's fundamental\n", ps1);
    check("pulse's SUB1 level", ps1 > 5.2 && ps1 < 6.9);
    // PWAD 64 sweeps the duty through square every 0.38 s; PWRS restarts it at the note.
    const int swept[8] = {0, 0, 0, 0, 64, 64, 127, 64};
    play(PULSE, swept, out);
    double low_seen = 1, high_seen = 0;
    for (unsigned w = 0; w < 18; ++w) {
        const double r = line(out, 4410 + w * 2205, 2205, 2 * kMiddleC) / std::max(1e-9, line(out, 4410 + w * 2205, 2205, kMiddleC));
        low_seen = std::min(low_seen, r); high_seen = std::max(high_seen, r);
    }
    check("PWAD sweeps the width", low_seen < 0.2 && high_seen > 0.6);
    play(PULSE, swept, other, kMiddleC, true);
    // (After the filters' memory of the last note has gone, a few milliseconds.)
    double apart = 0;
    for (unsigned i = 2205; i < 4410; ++i) apart = std::max(apart, std::fabs(static_cast<double>(out[i]) - other[i]) / 2147483648.0);
    check("PWRS restarts the sweep at the note", apart < 1e-4);
    const int free_running[8] = {0, 0, 0, 0, 64, 64, 0, 64};
    play(PULSE, free_running, out);
    play(PULSE, free_running, other, kMiddleC, true);
    apart = 0;
    for (unsigned i = 2205; i < 4410; ++i) apart = std::max(apart, std::fabs(static_cast<double>(out[i]) - other[i]) / 2147483648.0);
    check("without PWRS the sweep runs on", apart > 0.01);

    // --- SWAVE-ENS: PCH2 PCH3 PCH4 WAVE PW CHRL CHRW TUNE
    static swave_ens::State ens;
#define ENS ens, swave_ens::reset, swave_ens::trigger, swave_ens::render
    extremes("ens", ENS, out);
    const int one[8] = {62, 62, 62, 0, 64, 0, 0, 64};
    play(ENS, one, out);
    const double one_rms = rms(out, 13230, 30870), one_line = line(out, 13230, 30870, kMiddleC);
    std::printf("ens: one voice rms %.4f\n", one_rms);
    check("ens's level", one_rms > 0.050 && one_rms < 0.062);
    check("ens plays the key", std::fabs(cents(peak(out, 13230, 30870, kMiddleC), kMiddleC)) < 2);
    // PCH2: +1 semitone at 66, the just fifth 3:2 at 105, -12 at 0; 61..63 is off.
    const int step[8] = {66, 62, 62, 0, 64, 0, 0, 64};
    play(ENS, step, out);
    check("ens's PCH2 66 is +1 semitone", line(out, 13230, 30870, kMiddleC * std::pow(2.0, 1 / 12.0)) > 0.8 * one_line);
    const int fifth[8] = {105, 62, 62, 0, 64, 0, 0, 64};
    play(ENS, fifth, out);
    check("ens's PCH2 105 is 3:2", line(out, 13230, 30870, kMiddleC * 1.5) > 0.8 * one_line);
    // (Thinner, as the Monomachine's lowest voices are: about half as loud at the fundamental.)
    const int under[8] = {0, 62, 62, 0, 64, 0, 0, 64};
    play(ENS, under, out);
    check("ens's PCH2 0 is an octave down", line(out, 13230, 30870, kMiddleC / 2) > 0.45 * one_line);
    // Four voices, each as loud as one: twice the rms.
    const int chord[8] = {64, 72, 84, 0, 64, 0, 0, 64};
    play(ENS, chord, out);
    const double four = rms(out, 13230, 30870) / one_rms;
    std::printf("ens: four voices %.2f x one\n", four);
    check("ens's voices add", four > 1.8 && four < 2.2);
    // WAVE 127, PW 64: a square an octave down, as loud as the saw.
    const int low_square[8] = {62, 62, 62, 127, 64, 0, 0, 64};
    play(ENS, low_square, out);
    check("ens's pulse at PW 64 is an octave down", line(out, 13230, 30870, kMiddleC / 2) > 4 * line(out, 13230, 30870, kMiddleC));
    check("ens's pulse is as loud as the saw", rms(out, 13230, 30870) > 0.85 * one_rms && rms(out, 13230, 30870) < 1.1 * one_rms);
    // WAVE 64: an even crossfade, the two parts uncorrelated: 0.69 of either.
    const int blend[8] = {62, 62, 62, 64, 64, 0, 0, 64};
    play(ENS, blend, out);
    const double mid = rms(out, 13230, 30870) / one_rms;
    check("ens's WAVE crossfades", mid > 0.62 && mid < 0.76);
    // CHRL 127: the dry voice gives way to its detuned copies.
    const int wet[8] = {62, 62, 62, 0, 64, 127, 64, 64};
    play(ENS, wet, out);
    check("ens's chorus replaces the dry voice", line(out, 0, kFrames, kMiddleC) < 0.6 * one_line && rms(out, 13230, 30870) > 0.7 * one_rms);

    if (!failures) std::puts("PASS swave");
    return failures ? 1 : 0;
}
