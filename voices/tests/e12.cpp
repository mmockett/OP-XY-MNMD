// Sound checks for the sixteen E12 machines, compiled for the host and run by
//   python3 tools/test_voices.py
// With the samples captured from the owner's ROM (tools/capture_e12.py) they
// check the fitted measures; without them (CI, other checkouts) they check
// that every machine is silent and bounded. The last line says which ran.
#include "../e12.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // one second
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;
opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }

// The raw setting the capture uses: the sample as it is.
void raw(unsigned machine, int* v) {
    const int p[8] = {64, 127, 0, machine == md::e12::OH ? 127 : 0, 0, 0, 64, 64};
    for (int i = 0; i < 8; ++i) v[i] = p[i];
}

void play(unsigned machine, const int* values, opxy::i32* out, unsigned blocks = kBlocks) {
    static md::e12::State voice;
    md::Common shared = {};
    opxy::i32 p[8];
    for (int i = 0; i < 8; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::e12::reset(voice, shared, 1);
    md::e12::trigger(voice, shared);
    for (unsigned b = 0; b < blocks; ++b) md::e12::render(voice, shared, out + b * opxy::block_frames, p, pitch, machine);
}

double rms(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / count) / 2147483648.0;
}
double peak(const opxy::i32* out, unsigned count) {
    double most = 0;
    for (unsigned i = 0; i < count; ++i) most = std::fmax(most, std::fabs(out[i] / 2147483648.0));
    return most;
}
bool bounded(const opxy::i32* out, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}
// The last sample louder than 1/10000 of full scale.
unsigned last_sound(const opxy::i32* out, unsigned count) {
    unsigned last = 0;
    for (unsigned i = 0; i < count; ++i)
        if (std::fabs(out[i] / 2147483648.0) > 1e-4) last = i;
    return last;
}
// How unlike two renders are (0 the same), over `count` samples.
double unlike(const opxy::i32* a, const opxy::i32* b, unsigned count) {
    double d = 0, n = 0;
    for (unsigned i = 0; i < count; ++i) {
        d += std::pow((a[i] - static_cast<double>(b[i])) / 2147483648.0, 2);
        n += std::pow(b[i] / 2147483648.0, 2);
    }
    return std::sqrt(d / (n + 1e-30));
}

const char* const names[16] = {"E12-BD", "E12-SD", "E12-HT", "E12-LT", "E12-CP", "E12-RS", "E12-CB", "E12-CH",
                                "E12-OH", "E12-RC", "E12-CC", "E12-BR", "E12-TA", "E12-TR", "E12-SH", "E12-BC"};
}

int main() {
    static opxy::i32 out[kFrames], other[kFrames], third[kFrames];
    const bool captured = md::e12_data::captured != 0;
    char what[160];

    // Every machine: bounded at both ends of every control, and at the raw setting;
    // it sounds with the capture and is silent without it.
    for (unsigned m = 0; m < 16; ++m) {
        int v[8];
        raw(m, v);
        play(m, v, out);
        std::snprintf(what, sizeof what, "%s %s at the raw setting", names[m], captured ? "sounds" : "is silent");
        check(what, captured ? rms(out, 0, 2205) > 0.002 : peak(out, kFrames) == 0);
        for (int slot = 0; slot < 8; ++slot) {
            for (int end = 0; end <= 127; end += 127) {
                raw(m, v);
                v[slot] = end;
                play(m, v, out);
                std::snprintf(what, sizeof what, "%s within the declared peak at slot %d = %d", names[m], slot + 1, end);
                check(what, bounded(out, kFrames));
            }
        }
        for (int end = 0; end <= 127; end += 127) {
            for (int i = 0; i < 8; ++i) v[i] = end;
            play(m, v, out);
            std::snprintf(what, sizeof what, "%s within the declared peak with every control at %d", names[m], end);
            check(what, bounded(out, kFrames));
        }
    }

    if (captured) {
        using namespace md::e12;
        int v[8];
        // Level: E12-BD peaks as the Machinedrum's, times 1.2 (tools/capture_e12.py), 0.345.
        raw(BD, v);
        play(BD, v, out);
        check("E12-BD peaks near 0.345", std::fabs(peak(out, kFrames) - 0.345) < 0.01);
        // PTCH: 32 steps an octave. At 96 the sample ends in half the time.
        raw(CB, v);
        play(CB, v, out);
        v[PTCH] = 96;
        play(CB, v, other);
        const double ratio = static_cast<double>(last_sound(other, kFrames)) / last_sound(out, kFrames);
        check("E12-CB at PTCH 96 plays twice as fast", ratio > 0.48 && ratio < 0.52);
        // DEC: none at 127 (E12-OH rings its 730 ms); at 64, -40 dB by 200 ms (reference/fit_e12.py).
        raw(OH, v);
        play(OH, v, out);
        check("E12-OH at DEC 127 rings 730 ms", last_sound(out, kFrames) > 31000 && last_sound(out, kFrames) < 33000);
        v[DEC] = 64;
        play(OH, v, other);
        const double fall = 20 * std::log10(rms(other, 8820, 441) / rms(out, 8820, 441));
        check("E12-OH at DEC 64 is 40 dB down at 200 ms", fall < -36 && fall > -44);
        // STOP: 5.8 ms a step; at 64 the open hat is gone by 380 ms and still sounds at 360.
        raw(OH, v);
        v[OWN] = 64;
        play(OH, v, other);
        check("E12-OH at STOP 64 stops near 372 ms", last_sound(other, kFrames) > 15900 && last_sound(other, kFrames) < 16800);
        // STRT: E12-CC at 64 starts 7165 frames in (the capture's own step).
        raw(CC, v);
        play(CC, v, out);
        v[STRT] = 64;
        play(CC, v, other);
        const unsigned offset = (64u * md::e12_data::strt[CC]) >> 8;
        check("E12-CC at STRT 64 starts 7165 frames in", offset > 7155 && offset < 7175 && unlike(other, out + offset, 20000) < 0.01);
        // RTRG / RTIM: at RTRG 4 the cowbell plays 3 more times, every 1344 samples at RTIM 64.
        raw(CB, v);
        play(CB, v, out);
        v[RTRG] = 4;
        play(CB, v, other);
        check("E12-CB retrigs at RTIM 64 every 1344 samples", unlike(other + 1344, out, 1344) < 0.01 && unlike(other + 4032, out, 1344) < 0.01);
        check("E12-CB retrigs only RTRG - 1 times", unlike(other + 4032, out, 4000) < 0.01);
        // The crossfades: at 64 E12-SD is half its sample and half the second.
        raw(SD, v);
        play(SD, v, out);
        v[OWN] = 127;
        play(SD, v, other);
        v[OWN] = 64;
        play(SD, v, third);
        for (unsigned i = 0; i < kFrames; ++i) other[i] = out[i] / 2 + other[i] / 2;
        check("E12-SD at RING 127 is another sample", unlike(third, out, 20000) > 0.3);
        check("E12-SD at RING 64 is half of each", unlike(third, other, 20000) < 0.02);
        // HP: at 64 (1.6 kHz, two poles) E12-SD loses its body: a third of its level.
        raw(SD, v);
        play(SD, v, out);
        v[HP] = 64;
        play(SD, v, other);
        check("E12-SD at HP 64 is thinner", rms(other, 0, 4410) < rms(out, 0, 4410) * 0.75);
        // SNAP: E12-BD driven into the clip at 0.36, louder throughout with SPLN 127.
        raw(BD, v);
        play(BD, v, out);
        v[HP] = 127;
        v[OWN] = 127;
        play(BD, v, other);
        check("E12-BD's SNAP clips at 0.36", peak(other, kFrames) < 0.362 && peak(other, kFrames) > 0.35);
        check("E12-BD's SNAP is louder", rms(other, 4410, 4410) > rms(out, 4410, 4410) * 2.5);
        // BEND: at 127 the hit starts close to an octave up and drifts back very slowly,
        // so the cowbell plays out in about half the time.
        raw(CB, v);
        play(CB, v, out);
        v[BEND] = 127;
        play(CB, v, other);
        const double bent = static_cast<double>(last_sound(other, kFrames)) / last_sound(out, kFrames);
        check("E12-CB at BEND 127 plays an octave up", bent > 0.49 && bent < 0.53);
    }

    if (!failures) std::puts(captured ? "PASS e12 (with the samples captured from the owner's ROM)"
                                      : "PASS e12 (silent: no captured samples, docs/FAMILIES.md)");
    return failures ? 1 : 0;
}
