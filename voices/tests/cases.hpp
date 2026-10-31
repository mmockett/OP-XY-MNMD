#pragma once
// The kit members' fixed cases (tools/test_voices.py writes the program that
// includes this): each machine at its defaults, then each named control at 0
// and at 127, the others at the defaults, struck once and left to ring. Each
// case prints a line of JSON: its peak and level, its band energies in
// windows, and how long it lasts. tools/test_voices.py compares them with
// voices/tests/cases.json, so a change to a voice's code can't drift its sound
// unnoticed, and fails a voice that doesn't fall silent.
#include <opxy/audio.hpp>
#include <cmath>
#include <cstdio>

namespace cases {
const unsigned kRate = 44100;
const unsigned kLongest = 24 * kRate / opxy::block_frames;   // blocks rendered at most
const unsigned kQuietTail = 2 * kRate / opxy::block_frames;  // blocks of silence a voice must keep
// Silence: every sample under 2^-15 of full scale (-90 dB).
const opxy::i32 kFloor = 1 << 16;
const unsigned kWindow = 2048, kWindows = 16;               // the first 0.74 s
const unsigned kBands = 8;
// Band edges in Hz: octaves and a half from 40 Hz, the last to Nyquist.
const double kEdges[kBands + 1] = {40, 110, 300, 800, 1800, 3500, 6500, 11000, 22050};

inline opxy::i32 q(int v127) { return v127 >= 127 ? 0x7fffffff : static_cast<opxy::i32>(v127 * 16909320); }

inline void fft(double* re, double* im, unsigned n) {
    for (unsigned i = 1, j = 0; i < n; ++i) {
        unsigned bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (unsigned len = 2; len <= n; len <<= 1) {
        const double a = -2 * M_PI / len;
        for (unsigned i = 0; i < n; i += len)
            for (unsigned k = 0; k < len / 2; ++k) {
                const double wr = std::cos(a * k), wi = std::sin(a * k);
                double* xr = re + i + k; double* xi = im + i + k;
                const double yr = xr[len / 2] * wr - xi[len / 2] * wi, yi = xr[len / 2] * wi + xi[len / 2] * wr;
                xr[len / 2] = *xr - yr; xi[len / 2] = *xi - yi; *xr += yr; *xi += yi;
            }
    }
}

inline double db(double power) { return power > 1e-30 ? 10 * std::log10(power) : -300; }

// One case: render, then print its measures as JSON.
template <class State, class Reset, class Trigger, class Render>
void measure(const char* machine, const char* label, const int* values, Reset reset, Trigger trigger, Render render) {
    static State voice;
    static opxy::i32 audio[kWindow * kWindows];
    md::Common shared = md::Common();
    opxy::i32 p[8];
    for (int i = 0; i < 8; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = 25479004u;   // middle C
    reset(voice, shared, 1);
    trigger(voice, shared);
    opxy::i32 block[opxy::block_frames];
    double peak = 0, sum = 0;
    unsigned last = 0, b = 0;   // the last block above the floor, plus one
    for (; b < kLongest; ++b) {
        render(voice, shared, block, p, pitch);
        bool loud = false;
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            const opxy::i32 s = block[i];
            const double a = std::fabs(static_cast<double>(s));
            if (a > peak) peak = a;
            if (s >= kFloor || s <= -kFloor) loud = true;
            const unsigned at = b * opxy::block_frames + i;
            if (at < kWindow * kWindows) { audio[at] = s; sum += a * a; }
        }
        if (loud) last = b + 1;
        if (b >= last + kQuietTail && b * opxy::block_frames >= kWindow * kWindows) break;
    }
    const bool silent = b < kLongest;
    std::printf("{\"machine\": \"%s\", \"case\": \"%s\", \"peak\": %.3f, \"level\": %.3f, \"lasts\": %.3f, \"silent\": %s, \"bands\": [",
                machine, label, db(peak * peak / 4.611686e18), db(sum / (kWindow * kWindows) / 4.611686e18),
                last * double(opxy::block_frames) / kRate, silent ? "true" : "false");
    static double re[kWindow], im[kWindow];
    for (unsigned w = 0; w < kWindows; ++w) {
        for (unsigned i = 0; i < kWindow; ++i) {
            const double hann = 0.5 - 0.5 * std::cos(2 * M_PI * i / kWindow);
            re[i] = audio[w * kWindow + i] / 2147483648.0 * hann;
            im[i] = 0;
        }
        fft(re, im, kWindow);
        std::printf("%s[", w ? ", " : "");
        for (unsigned band = 0; band < kBands; ++band) {
            double power = 0;
            for (unsigned k = 1; k < kWindow / 2; ++k) {
                const double hz = k * double(kRate) / kWindow;
                if (hz >= kEdges[band] && hz < kEdges[band + 1]) power += re[k] * re[k] + im[k] * im[k];
            }
            std::printf("%s%.1f", band ? ", " : "", db(power / kWindow));
        }
        std::printf("]");
    }
    std::printf("]}\n");
}
}
