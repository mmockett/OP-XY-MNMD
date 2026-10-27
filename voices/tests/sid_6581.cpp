// Sound checks for MNM SID, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Monomachine's (reference/fit_sid_6581.py).
#include "../sid_6581.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;   // 261.63 Hz at 44.1 kHz
const double kPi = 3.14159265358979;

opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }   // 0..127 -> Q31
opxy::u32 note(int n) { return static_cast<opxy::u32>(440.0 * std::pow(2.0, (n - 69) / 12.0) / 44100.0 * 4294967296.0); }

//                    PW PWAD PWRS WAVE MOD MSRC MFRQ TUNE
const int kSaw[8] = {32, 0, 0, 40, 0, 0, 64, 64};

md::Common shared;
void play(const int* values, opxy::i32* out, opxy::u32 increment = kMiddleC, bool fresh = true) {
    static md::sid_6581::State voice;
    opxy::i32 parameters[8];
    for (int i = 0; i < 8; ++i) parameters[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = increment;
    if (fresh) { shared = md::Common(); md::sid_6581::reset(voice, shared, 1); }
    md::sid_6581::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::sid_6581::render(voice, shared, out + b * opxy::block_frames, parameters, pitch);
}
int with(const int* base, int slot, int value, int* into) {
    for (int i = 0; i < 8; ++i) into[i] = base[i];
    into[slot] = value;
    return 0;
}

double rms(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / count) / 2147483648.0;
}
double db(double a, double b) { return 20 * std::log10(a / b); }

bool bounded(const opxy::i32* out) {
    for (unsigned i = 0; i < kFrames; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}

// A DFT's magnitude at f Hz over a Hann-windowed second half.
double magnitude(const opxy::i32* out, double f) {
    double re = 0, im = 0;
    const unsigned from = kFrames / 2, n = kFrames / 2;
    for (unsigned i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2 * kPi * i / n), x = out[from + i] * w;
        re += x * std::cos(2 * kPi * f * i / 44100.0);
        im -= x * std::sin(2 * kPi * f * i / 44100.0);
    }
    return std::sqrt(re * re + im * im);
}
// The strongest frequency near `guess`, to 0.1 Hz.
double pitch_near(const opxy::i32* out, double guess) {
    double best = guess, most = 0;
    for (double f = guess * 0.95; f < guess * 1.05; f += guess * 0.0005) {
        const double m = magnitude(out, f);
        if (m > most) { most = m; best = f; }
    }
    return best;
}
// The share of the cycle a pulse is high, from its first two harmonics: |h2 / h1| = |cos(pi d)|.
double duty(const opxy::i32* out, double f) {
    const double r = magnitude(out, 2 * f) / magnitude(out, f);
    return 1 - std::acos(r < 1 ? r : 1) / kPi;   // the wider side
}
// Energy away from the harmonics of f against energy on them, dB: the aliasing.
double aliasing(const opxy::i32* out, double f) {
    double on = 0, off = 0;
    for (double g = 20; g < 20000; g += 7.3) {
        const double k = g / f, near = std::fabs(k - std::floor(k + 0.5)) * f;
        const double m = magnitude(out, g);
        (near < 15 ? on : off) += m * m;
    }
    return 10 * std::log10(off / on);
}
}

int main() {
    static opxy::i32 out[kFrames];
    int v[8];

    // Saw at middle C: its pitch, its 1/k harmonics, its level as the reference.
    play(kSaw, out);
    check("sounds", rms(out, 0, 4410) > 0.05);
    check("stays within the declared peak", bounded(out));
    const double saw = rms(out, 22050, 22050);
    const double f = pitch_near(out, 261.63);
    std::printf("saw at middle C %.2f Hz, rms %.3f\n", f, saw);
    check("plays middle C", std::fabs(f - 261.63) < 0.5);
    const double h2 = magnitude(out, 2 * f) / magnitude(out, f), h3 = magnitude(out, 3 * f) / magnitude(out, f);
    check("saw harmonics fall as 1/k", std::fabs(h2 - 0.5) < 0.05 && std::fabs(h3 - 0.333) < 0.05);

    // TUNE: +-2 semitones (233.0 and 293.1 Hz on the Monomachine).
    with(kSaw, md::sid_6581::TUNE, 0, v); play(v, out);
    const double down = pitch_near(out, 233.0);
    with(kSaw, md::sid_6581::TUNE, 127, v); play(v, out);
    const double up = pitch_near(out, 293.1);
    std::printf("TUNE 0 %.1f Hz, 127 %.1f Hz\n", down, up);
    check("TUNE spans +-2 semitones", std::fabs(down - 233.0) < 1 && std::fabs(up - 293.1) < 1);

    // Pitch across the keyboard, and aliasing at the top (the saw's edges are band-limited).
    for (int n = 24; n <= 108; n += 12) {
        play(kSaw, out, note(n));
        const double want = 440.0 * std::pow(2.0, (n - 69) / 12.0), got = pitch_near(out, want);
        if (std::fabs(1200 * std::log2(got / want)) > 3) { std::printf("note %d: %.2f Hz, wants %.2f\n", n, got, want); check("tracks the key", false); }
        check("bounded across the keyboard", bounded(out));
    }
    play(kSaw, out, note(96));
    const double alias96 = aliasing(out, 2093.0);
    play(kSaw, out, note(108));
    const double alias108 = aliasing(out, 4186.0);
    std::printf("saw aliasing: note 96 %.1f dB, note 108 %.1f dB\n", alias96, alias108);
    check("saw aliasing stays low", alias96 < -30 && alias108 < -25);

    // Levels against the saw: TRI, PULS (PW 0) the same amplitude; MIX 10 dB under; NOIS about the same.
    with(kSaw, md::sid_6581::WAVE, 10, v); play(v, out);
    const double tri = db(rms(out, 22050, 22050), saw);
    with(kSaw, md::sid_6581::WAVE, 90, v); play(v, out);
    const double mix = db(rms(out, 22050, 22050), saw);
    with(kSaw, md::sid_6581::WAVE, 115, v); play(v, out);
    const double noise = db(rms(out, 22050, 22050), saw);
    std::printf("levels re saw: tri %+.1f, mix %+.1f, noise %+.1f dB\n", tri, mix, noise);
    check("tri as loud as the saw", std::fabs(tri - 0.1) < 1);
    check("mix 10 dB under the saw", std::fabs(mix + 10.1) < 1.5);
    check("noise about the saw's level", std::fabs(noise + 0.8) < 1.5);

    // PW: duty 1 - |PW - 64| / 128, PW 64 silent.
    const int widths[4] = {0, 32, 48, 96};
    const double duties[4] = {0.5, 0.75, 0.875, 0.75};
    for (int k = 0; k < 4; ++k) {
        int p[8];
        with(kSaw, md::sid_6581::WAVE, 64, v); with(v, md::sid_6581::PW, widths[k], p); play(p, out);
        const double d = duty(out, f);
        std::printf("PW %d: duty %.3f\n", widths[k], d);
        check("PW sets the duty", std::fabs(d - duties[k]) < 0.02);
    }
    { int p[8]; with(kSaw, md::sid_6581::WAVE, 64, v); with(v, md::sid_6581::PW, 64, p); play(p, out);
      check("PW 64 is silent", rms(out, 22050, 22050) < saw * 0.01); }

    // PWAD sweeps the width: from PW at the note with PWRS on, 185 steps a second at 64.
    {
        int p[8] = {32, 64, 127, 64, 0, 0, 64, 64};
        play(p, out);
        // After 0.1 s the width is 32 + 18.5 steps: duty 1 - |50.5 - 64| / 128 = 0.895.
        static opxy::i32 early[kFrames];
        for (unsigned i = 0; i < kFrames; ++i) early[i] = i < 1764 ? out[3528 + i] : 0;
        double re1 = 0, im1 = 0, re2 = 0, im2 = 0;
        for (unsigned i = 0; i < 1764; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2 * kPi * i / 1764), x = early[i] * w;
            re1 += x * std::cos(2 * kPi * f * i / 44100); im1 -= x * std::sin(2 * kPi * f * i / 44100);
            re2 += x * std::cos(4 * kPi * f * i / 44100); im2 -= x * std::sin(4 * kPi * f * i / 44100);
        }
        const double r = std::sqrt(re2 * re2 + im2 * im2) / std::sqrt(re1 * re1 + im1 * im1);
        const double d = 1 - std::acos(r < 1 ? r : 1) / kPi;
        std::printf("PWAD 64, 80..120 ms after the note: duty %.3f\n", d);
        check("PWAD sweeps from PW", std::fabs(d - 0.895) < 0.03);
    }

    // RING ignores WAVE: tri and saw ring alike, 8.4 dB under the saw, with the modulator at 311 Hz.
    int ring_tri[8] = {32, 0, 0, 10, 40, 0, 40, 64}, ring_saw[8] = {32, 0, 0, 40, 40, 0, 40, 64};
    play(ring_tri, out);
    const double ring = db(rms(out, 22050, 22050), saw), m311 = magnitude(out, 311.1) / magnitude(out, f);
    play(ring_saw, out);
    const double ring2 = db(rms(out, 22050, 22050), saw);
    std::printf("ring: %+.1f dB (saw selected %+.1f), modulator at 311 Hz %.2f of the note\n", ring, ring2, m311);
    check("ring level", std::fabs(ring + 8.4) < 1.5);
    check("ring ignores WAVE", std::fabs(ring - ring2) < 0.2);
    check("ring has the modulator in it", m311 > 0.7);
    // The modulator is the same on every key.
    play(ring_tri, out, note(72));
    check("the modulator stays at 311 Hz an octave up", magnitude(out, 311.1) / magnitude(out, 523.25) > 0.7);

    // SYNC: the heard wave at the modulator's pitch, restarted each cycle of the note: still the note's pitch.
    int synced[8] = {32, 0, 0, 40, 80, 0, 56, 64};
    play(synced, out);
    const double s2 = magnitude(out, 2 * f) / magnitude(out, f);
    std::printf("sync MFRQ 56: h2 / h1 %.2f (2.55 on the Monomachine)\n", s2);
    check("sync brightens the second harmonic", s2 > 2 && s2 < 3.2);

    // PRCH: the previous note is the modulator.
    {
        int first[8] = {32, 0, 0, 10, 0, 0, 64, 64}, second[8] = {32, 0, 0, 10, 40, 127, 40, 64};
        play(first, out, note(48));
        play(second, out, kMiddleC, false);
        const double m = magnitude(out, 130.81) / magnitude(out, f);
        std::printf("PRCH after C3: 130.8 Hz at %.2f of the note\n", m);
        check("PRCH rings with the previous note", m > 0.5);
    }

    // Every step of WAVE, MOD and MSRC at the ends of the others, high and low on the keyboard, stays bounded.
    for (int wave = 0; wave < 5; ++wave)
        for (int mod = 0; mod < 4; ++mod)
            for (int end = 0; end < 2; ++end) {
                int p[8] = {end ? 127 : 0, end ? 127 : 0, end ? 127 : 0, wave * 26 + 13, mod * 32 + 16, end ? 127 : 0, end ? 127 : 0, end ? 127 : 0};
                play(p, out, note(end ? 120 : 12));
                check("every setting stays bounded", bounded(out));
            }

    if (!failures) std::puts("PASS sid_6581");
    return failures ? 1 : 0;
}
