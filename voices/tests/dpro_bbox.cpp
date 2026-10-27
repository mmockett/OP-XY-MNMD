// Sound checks for DPRO-BBOX, compiled for the host and run by
//   python3 tools/test_voices.py
// With the samples captured from the owner's ROM (tools/capture_dpro.py) it checks
// the fitted measures (reference/fit_dpro.py); without them, that it stays silent
// and bounded. It says which ran.
#include "../dpro_bbox.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;   // a second
const unsigned kFrames = kBlocks * opxy::block_frames;

opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }
opxy::u32 note(int n) { return static_cast<opxy::u32>(440.0 * std::pow(2.0, (n - 69) / 12.0) / 44100.0 * 4294967296.0); }

void play(const int* values, int key, opxy::i32* out) {
    static md::dpro_bbox::State voice;
    md::Common shared = {};
    opxy::i32 p[8];
    for (int i = 0; i < 8; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = note(key);
    md::dpro_bbox::reset(voice, shared, 1);
    md::dpro_bbox::trigger(voice, shared);
    for (unsigned b = 0; b < kBlocks; ++b) md::dpro_bbox::render(voice, shared, out + b * opxy::block_frames, p, pitch);
}

bool bounded(const opxy::i32* out) {
    for (unsigned i = 0; i < kFrames; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}
// The sample's length: up to the last frame above 1% of its peak.
unsigned length(const opxy::i32* out) {
    double peak = 0;
    for (unsigned i = 0; i < kFrames; ++i) peak = std::fmax(peak, std::fabs(static_cast<double>(out[i])));
    unsigned last = 0;
    for (unsigned i = 0; i < kFrames; ++i)
        if (std::fabs(static_cast<double>(out[i])) > peak * 0.01) last = i;
    return last;
}
}

int main() {
    static opxy::i32 out[kFrames], other[kFrames];
    for (int key = 24; key < 96; key += 7)
        for (int slot = 0; slot < 8; ++slot)
            for (int end = 0; end < 2; ++end) {
                int v[8] = {64, 0, 0, 0, 0, 64, 0, 0};
                v[slot] = end ? 127 : 0;
                play(v, key, out);
                check("DPRO-BBOX stays within the declared peak", bounded(out));
            }
    if (!md::bbox_data::captured) {
        const int v[8] = {64, 0, 0, 0, 8, 64, 0, 0};
        play(v, 60, out);
        bool silent = true;
        for (unsigned i = 0; i < kFrames; ++i) silent = silent && !out[i];
        check("DPRO-BBOX is silent without the captured samples", silent);
        if (!failures) std::puts("PASS dpro_bbox (silent: no captured samples on this Mac, so bounds and silence only)");
        return failures ? 1 : 0;
    }

    const int plain[8] = {64, 0, 0, 0, 0, 64, 0, 0};
    // Each key from C plays its own sample: the 24 differ, and repeat two octaves up, 6 semitones higher.
    play(plain, 48, out);
    check("DPRO-BBOX sounds", length(out) > 1000);
    play(plain, 49, other);
    double diff = 0;
    for (unsigned i = 0; i < 4000; ++i) diff = std::fmax(diff, std::fabs((out[i] - static_cast<double>(other[i])) / 2147483648.0));
    check("DPRO-BBOX neighbouring keys play different samples", diff > 0.05);
    const unsigned at48 = length(out);
    play(plain, 72, out);
    const unsigned at72 = length(out);
    check("DPRO-BBOX two octaves up is the same sample, 6 semitones up (1.41 x shorter)",
          std::fabs(static_cast<double>(at48) / at72 - 1.414) < 0.05);
    // PTCH: about +-21 semitones (measured -20 at 0, +21.8 at 127).
    const int low[8] = {0, 0, 0, 0, 0, 64, 0, 0}, high[8] = {127, 0, 0, 0, 0, 64, 0, 0};
    play(low, 48, out);
    const double down = static_cast<double>(length(out)) / at48;
    play(high, 48, out);
    const double up = static_cast<double>(at48) / length(out);
    check("DPRO-BBOX PTCH 0 is about 20 semitones down", std::fabs(12 * std::log2(down) - 20) < 1.5);
    check("DPRO-BBOX PTCH 127 is about 22 semitones up", std::fabs(12 * std::log2(up) - 21.8) < 1.5);
    // STRT 127 skips 0.37 of the sample (length x 127 x 3 / 1024 frames at note 60's rate).
    const unsigned skip = md::bbox_data::length[12] * 127 * 3 / 1024;
    const int late[8] = {64, 127, 0, 0, 0, 64, 0, 0};
    play(plain, 60, out);
    play(late, 60, other);
    double best = 0;
    unsigned at = 0;
    for (unsigned lag = skip - 40; lag < skip + 40; ++lag) {
        double ab = 0, aa = 0, bb = 0;
        for (unsigned i = 0; i < 600; ++i) { ab += other[i] * static_cast<double>(out[i + lag]); aa += other[i] * static_cast<double>(other[i]); bb += out[i + lag] * static_cast<double>(out[i + lag]); }
        const double r = aa > 0 && bb > 0 ? ab / std::sqrt(aa * bb) : 0;
        if (r > best) { best = r; at = lag; }
    }
    check("DPRO-BBOX STRT 127 starts 0.37 of the sample in", best > 0.99 && at + 2 >= skip && at <= skip + 2);
    // RTRG 8, RTIM 64: eight restarts, 15.3 ms apart.
    const int roll[8] = {64, 0, 0, 0, 8, 64, 0, 0};
    play(roll, 60, other);
    unsigned restarts = 0;
    for (unsigned k = 1; k < 12; ++k) {
        const unsigned t = k * 674;
        double ab = 0, aa = 0, bb = 0;
        for (unsigned i = 0; i < 200; ++i) { ab += other[t + i] * static_cast<double>(out[i]); aa += other[t + i] * static_cast<double>(other[t + i]); bb += out[i] * static_cast<double>(out[i]); }
        if (aa > 0 && ab / std::sqrt(aa * bb) > 0.99) ++restarts;
    }
    check("DPRO-BBOX RTRG 8 restarts eight times, RTIM 64 apart (674 samples)", restarts == 8);
    if (!failures) std::puts("PASS dpro_bbox (captured samples: fitted measures checked)");
    return failures ? 1 : 0;
}
