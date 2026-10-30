// Sound checks for TRX-CH and TRX-OH, compiled for the host and run by
//   python3 tools/test_voices.py
#include "../trx_hat.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;
opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }

md::Common shared = {};
struct Hat { md::trx_hat::State state; };

// Plays a hat, closed or open, with its controls (0..127 in slot order) for `blocks`.
void play(Hat& voice, bool open, const int* values, opxy::i32* out, unsigned blocks, bool begin = true, opxy::u32 owner = 1) {
    opxy::i32 p[8] = {};
    for (int i = 0; i < 7; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    if (begin) {
        if (open) { md::trx_oh::reset(voice.state, shared, owner); md::trx_oh::trigger(voice.state, shared); }
        else { md::trx_ch::reset(voice.state, shared, owner); md::trx_ch::trigger(voice.state, shared); }
    }
    for (unsigned b = 0; b < blocks; ++b) {
        if (open) md::trx_oh::render(voice.state, shared, out + b * opxy::block_frames, p, pitch);
        else md::trx_ch::render(voice.state, shared, out + b * opxy::block_frames, p, pitch);
    }
}

double rms(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / count) / 2147483648.0;
}
bool bounded(const opxy::i32* out, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}
double frequency(const opxy::i32* out, unsigned from, unsigned count) {
    unsigned crossings = 0;
    for (unsigned i = from + 1; i < from + count; ++i) crossings += (out[i - 1] < 0) != (out[i] < 0);
    return crossings * 44100.0 / count / 2;
}
}

int main() {
    static opxy::i32 out[kFrames];
    static Hat voice, other;

    // TRX-CH and TRX-OH, fitted to the Machinedrum (reference/fit_hh.py): a closed
    // hat holds about 110 ms at DEC 64 and is gone by a quarter second; the open
    // hat holds about 360 ms; a closed hat chokes an open one.
    const int hat[7] = {0, 64, 0, 127, 64, 0, 0};
    {
        play(voice, false, hat, out, kBlocks);
        check("CH sounds", rms(out, 0, 2205) > 0.005);
        check("CH stays within the declared peak", bounded(out, kFrames));
        check("CH holds", rms(out, 3528, 441) > rms(out, 0, 2205) * 0.5);
        check("CH is gone by a quarter second", rms(out, 11025, 2205) < rms(out, 0, 2205) * 0.001);
        check("CH is bright", frequency(out, 0, 2205) > 2500);
    }
    {
        play(voice, true, hat, out, kBlocks);
        check("OH still rings at 300 ms", rms(out, 13230, 2205) > rms(out, 0, 2205) * 0.5);
        play(voice, true, hat, out, 69);
        play(other, false, hat, out + 69 * 64, 1, true, 2);
        play(voice, true, hat, out, 69, false);
        check("a closed hat chokes an open one", rms(out, 2205, 2205) < 0.0001);
    }

    // The oscillators run free and the noise is its own: two hits differ.
    {
        static opxy::i32 first[kFrames];
        play(voice, false, hat, first, 10, true, 1);
        play(voice, false, hat, out, 10, true, 9);
        double difference = 0;
        for (unsigned i = 0; i < 640; ++i) difference += std::fabs(static_cast<double>(first[i]) - out[i]);
        check("each hit is its own", difference > 0);
    }

    // Both: bounded at both ends of every control.
    for (int open = 0; open < 2; ++open) {
        for (int extreme = 0; extreme < 2; ++extreme) {
            int v[7];
            for (int i = 0; i < 7; ++i) v[i] = extreme ? 127 : 0;
            play(voice, open, v, out, kBlocks);
            if (!bounded(out, kFrames)) { std::printf("FAIL %s above the declared peak at %d\n", open ? "TRX-OH" : "TRX-CH", extreme * 127); ++failures; }
        }
    }

    if (!failures) std::puts("PASS trx_hat");
    return failures ? 1 : 0;
}
