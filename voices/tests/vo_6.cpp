// Sound checks for MNM VO-6, compiled for the host and run by
//   python3 tools/test_voices.py
// The fitted values are the Monomachine's own, measured by reference/fit_vo_6.py.
#include "../vo_6.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const double kPi = 3.14159265358979;

enum { VOC1, VOC2, VSW, VOIC, CONS, CLEN, CVOL, TUNE };
// A consonant's CONS value at the middle of its step, as presets place them.
int cons(int index) { return (index * 128 + 64) / 21; }
enum { B = 1, M = 9, S = 14 };

opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }   // 0..127 -> Q31
opxy::u32 increment(double note) { return static_cast<opxy::u32>(440.0 * std::pow(2.0, (note - 69) / 12) / 44100 * 4294967296.0); }

void play(const int* values, opxy::i32* out, double note = 60) {
    static md::vo_6::State voice;
    md::Common shared = {};
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = increment(note);
    md::vo_6::reset(voice, shared, 1);
    md::vo_6::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::vo_6::render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
}

double rms(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / count) / 2147483648.0;
}
double db(double x) { return 20 * std::log10(x + 1e-12); }
unsigned ms(double t) { return static_cast<unsigned>(t * 44.1); }

bool bounded(const opxy::i32* out) {
    for (unsigned i = 0; i < kFrames; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}

// Power at a frequency over a window (Goertzel, Hann-windowed).
double power(const opxy::i32* out, unsigned from, unsigned count, double hz) {
    const double w = 2 * kPi * hz / 44100, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (unsigned i = 0; i < count; ++i) {
        const double hann = 0.5 - 0.5 * std::cos(2 * kPi * i / count);
        const double s = out[from + i] / 2147483648.0 * hann + c * s1 - s2;
        s2 = s1; s1 = s;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

// The strongest frequency between low and high, in 1% steps, of a whisper's
// noise averaged over eight windows: where a formant sits.
double peak(const opxy::i32* out, double low, double high) {
    double best = 0, at = low;
    for (double hz = low; hz <= high; hz *= 1.01) {
        double p = 0;
        for (unsigned k = 0; k < 8; ++k) p += power(out, ms(100) + k * 4096, 4096, hz);
        if (p > best) { best = p; at = hz; }
    }
    return at;
}

// The fundamental, by autocorrelation with parabolic interpolation, Hz.
double fundamental(const opxy::i32* out, unsigned from, unsigned count, double low, double high) {
    const unsigned lo = static_cast<unsigned>(44100 / high), hi = static_cast<unsigned>(44100 / low);
    static double r[2048];
    for (unsigned lag = lo - 1; lag <= hi + 1; ++lag) {
        double sum = 0;
        for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i + lag];
        r[lag] = sum;
    }
    unsigned best = lo;
    for (unsigned lag = lo; lag <= hi; ++lag) if (r[lag] > r[best]) best = lag;
    const double a = r[best - 1], b = r[best], c = r[best + 1];
    const double shift = (a - 2 * b + c) != 0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0;
    return 44100 / (best + shift);
}
double cents(double hz, double want) { return 1200 * std::log2(hz / want); }
}

int main() {
    static opxy::i32 out[kFrames];
    const int ah[8] = {120, 58, 127, 0, 0, 30, 80, 64};

    play(ah, out);
    check("sounds", rms(out, ms(200), ms(200)) > 0.02);
    check("stays within the declared peak", bounded(out));
    const double vowel = rms(out, ms(300), ms(300));
    std::printf("vowel at %.1f dB\n", db(vowel));

    // Every control at both ends, across five octaves, stays bounded, and short of
    // the ceiling (0.45 of full scale), so nothing is clipped.
    double loudest = 0;
    for (int corner = 0; corner < 256; ++corner) {
        int v[8];
        for (int i = 0; i < 8; ++i) v[i] = (corner >> i & 1) ? 127 : 0;
        for (double note = 24; note <= 108; note += 21) {
            play(v, out, note);
            if (!bounded(out)) { std::printf("corner %d note %.0f: ", corner, note); check("bounded at the controls' ends", false); }
            for (unsigned i = 0; i < kFrames; ++i) {
                const double a = std::fabs(out[i] / 2147483648.0);
                loudest = a > loudest ? a : loudest;
            }
        }
    }
    std::printf("loudest at the controls' ends %.3f of full scale\n", loudest);
    check("headroom at the controls' ends", loudest < 0.45);

    // It plays chromatically: middle C is middle C, and every key follows.
    for (double note = 36; note <= 84; note += 12) {
        play(ah, out, note);
        const double want = 440 * std::pow(2.0, (note - 69) / 12);
        const double hz = fundamental(out, ms(300), 8192, want / 1.5, want * 1.5);
        std::printf("note %.0f plays %.2f Hz (%+.1f cents)\n", note, hz, cents(hz, want));
        check("tracks the keys within 2 cents", std::fabs(cents(hz, want)) < 2);
    }
    // TUNE: +-2 semitones at its ends, (TUNE - 64) / 32 semitones between.
    const double tunes[4][2] = {{0, -2}, {64, 0}, {96, 1}, {127, 63.0 / 32}};
    for (int k = 0; k < 4; ++k) {
        int v[8] = {120, 58, 127, 0, 0, 30, 80, static_cast<int>(tunes[k][0])};
        play(v, out);
        const double want = 261.63 * std::pow(2.0, tunes[k][1] / 12);
        const double hz = fundamental(out, ms(300), 8192, 150, 450);
        std::printf("TUNE %.0f plays %.2f Hz, want %.2f\n", tunes[k][0], hz, want);
        check("TUNE moves +-2 semitones", std::fabs(cents(hz, want)) < 2);
    }

    // The formants, whispered (VOIC 127) so the noise shows the filter: VOC1 moves
    // the first from 195 to 708 Hz, VOC2 the second from 499 to 2675 Hz.
    {
        int v[8] = {0, 64, 127, 127, 0, 30, 80, 64};
        play(v, out);
        const double f1_low = peak(out, 150, 320);
        v[VOC1] = 127; play(v, out);
        const double f1_high = peak(out, 450, 900);
        v[VOC1] = 0; v[VOC2] = 0; play(v, out);
        const double f2_low = peak(out, 400, 700);
        v[VOC2] = 64; play(v, out);
        const double f2_mid = peak(out, 900, 1500);
        v[VOC2] = 127; play(v, out);
        const double f2_high = peak(out, 2300, 3200);
        std::printf("F1 %.0f .. %.0f Hz, F2 %.0f / %.0f / %.0f Hz\n", f1_low, f1_high, f2_low, f2_mid, f2_high);
        check("VOC1 0 puts F1 at 195 Hz (a broad peak)", std::fabs(f1_low / 195 - 1) < 0.12);
        check("VOC1 127 puts F1 at 708 Hz", std::fabs(f1_high / 708 - 1) < 0.08);
        check("VOC2 0 puts F2 at 499 Hz", std::fabs(f2_low / 499 - 1) < 0.08);
        check("VOC2 64 puts F2 at 1176 Hz", std::fabs(f2_mid / 1176 - 1) < 0.08);
        check("VOC2 127 puts F2 at 2675 Hz", std::fabs(f2_high / 2675 - 1) < 0.08);
    }

    // The Monomachine keeps a vowel's level whatever its formants: within 4 dB
    // at the chart's corners and middle (it rises 2.9 dB across VOC1).
    {
        double least = 1e9, most = -1e9;
        const int corners[5][2] = {{0, 0}, {0, 127}, {127, 0}, {127, 127}, {64, 64}};
        for (int k = 0; k < 5; ++k) {
            int v[8] = {corners[k][0], corners[k][1], 127, 127, 0, 30, 80, 64};
            play(v, out);
            const double level = db(rms(out, ms(200), ms(600)));
            least = level < least ? level : least;
            most = level > most ? level : most;
        }
        std::printf("whispered vowels from %.1f to %.1f dB\n", least, most);
        check("vowels keep their level", most - least < 4);
    }

    // VOIC: from buzz to whisper. A buzz has its power at the harmonics.
    {
        int v[8] = {120, 58, 127, 0, 0, 30, 80, 64};
        double share[2];
        for (int k = 0; k < 2; ++k) {
            v[VOIC] = k ? 127 : 0;
            play(v, out, 48);
            double on = 0, off = 0;
            for (int h = 1; h <= 20; ++h) {
                on += power(out, ms(300), 16384, 130.81 * h);
                off += power(out, ms(300), 16384, 130.81 * (h + 0.5));
            }
            share[k] = on / (on + off);
        }
        std::printf("harmonic share: voiced %.2f, whispered %.2f\n", share[0], share[1]);
        check("VOIC 0 buzzes", share[0] > 0.95);
        check("VOIC 127 whispers", share[1] < 0.75);
    }

    // CONS: the definition's 21 steps (the original's v * 21 >> 7, as its LCD
    // reads) at the SDK's quantization, and presets' values at their steps' middles.
    for (int i = 0; i < 21; ++i) {
        const opxy::i32 middle = static_cast<opxy::i32>((2 * i + 1) * 0x7fffffffll / 42);
        const opxy::u32 index = (static_cast<opxy::u32>(middle) >> 16) * 21u >> 15;
        if (static_cast<int>(index) != i) { std::printf("step %d reads %u: ", i, index); check("CONS steps", false); }
        if (cons(i) * 21 >> 7 != i) { std::printf("step %d: ", i); check("cons() is the original's step", false); }
    }

    // S: hiss, then the vowel. CLEN 0 brings the vowel in by 110 ms, CLEN 127
    // holds the hiss past 400 ms; CVOL is the hiss's level.
    {
        int v[8] = {120, 58, 127, 0, cons(S), 0, 80, 64};
        play(v, out);
        const double hiss = rms(out, ms(30), ms(50)), sung = rms(out, ms(130), ms(100));
        std::printf("S at CLEN 0: hiss %.1f dB, vowel %.1f dB\n", db(hiss), db(sung));
        check("S hisses well under the vowel", db(hiss) < db(vowel) - 20 && db(hiss) > db(vowel) - 45);
        check("CLEN 0 sings by 110 ms", db(sung) > db(vowel) - 3);
        v[CLEN] = 127; play(v, out);
        check("CLEN 127 still hisses at 400 ms", db(rms(out, ms(350), ms(50))) < db(vowel) - 15);
        v[CLEN] = 30; v[CVOL] = 0; play(v, out);
        const double quiet = rms(out, ms(30), ms(50));
        v[CVOL] = 127; play(v, out);
        const double loud = rms(out, ms(30), ms(50));
        std::printf("S at CVOL 0 %.1f dB, 127 %.1f dB\n", db(quiet), db(loud));
        check("CVOL moves the hiss about 20 dB", db(loud) - db(quiet) > 15 && db(loud) - db(quiet) < 26);
        // V-SW off: the consonant alone.
        v[VSW] = 0; play(v, out);
        check("V-SW off: S alone, then silence", rms(out, ms(300), ms(200)) < 1e-4);
    }
    // M waits 50 ms, hums, then sings; B starts on the vowel at once.
    {
        int v[8] = {120, 58, 127, 0, cons(M), 30, 80, 64};
        play(v, out);
        check("M waits 50 ms", rms(out, 0, ms(45)) < 1e-5);
        check("M hums under the vowel", db(rms(out, ms(70), ms(40))) < db(vowel) - 3);
        v[CONS] = cons(B); play(v, out);
        check("B sings within 30 ms", db(rms(out, ms(30), ms(30))) > db(vowel) - 6);
    }

    if (!failures) std::puts("PASS vo_6");
    return failures ? 1 : 0;
}
