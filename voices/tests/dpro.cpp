// Sound checks for DPRO-WAVE, DPRO-DDRW and DPRO-DENS, compiled for the host and run by
//   python3 tools/test_voices.py
// With the waves captured from the owner's ROM (tools/capture_dpro.py) it checks the
// fitted measures (reference/fit_dpro.py, reference/README.md); without them, that
// every machine stays silent and bounded. It says which ran.
#include "../dpro.hpp"
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 345;   // half a second
const unsigned kFrames = kBlocks * opxy::block_frames;
const opxy::u32 kMiddleC = 25479004u;

opxy::i32 q(int v127) { return static_cast<opxy::i32>(v127 * 16909320); }

template <class State, class Reset, class Trigger, class Render>
void play(State& voice, Reset reset, Trigger trigger, Render render, const int* values, opxy::i32* out,
          unsigned blocks = kBlocks, bool fresh = true) {
    md::Common shared = {};
    opxy::i32 p[8];
    for (int i = 0; i < 8; ++i) p[i] = q(values[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    if (fresh) reset(voice, shared, 1);
    trigger(voice, shared);
    for (unsigned b = 0; b < blocks; ++b) render(voice, shared, out + b * opxy::block_frames, p, pitch);
}

double rms(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return std::sqrt(sum / count) / 2147483648.0;
}
bool bounded(const opxy::i32* out, unsigned n = kFrames) {
    for (unsigned i = 0; i < n; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}
bool silent(const opxy::i32* out, unsigned n = kFrames) {
    for (unsigned i = 0; i < n; ++i)
        if (out[i]) return false;
    return true;
}
// The amplitude of one frequency (Goertzel), as a fraction of full scale.
double tone(const opxy::i32* out, unsigned from, unsigned count, double hz) {
    const double w = 2 * M_PI * hz / 44100, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (unsigned i = from; i < from + count; ++i) {
        const double s0 = out[i] / 2147483648.0 + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return 2 * std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / count;
}
// The strongest frequency between lo and hi Hz, to 0.25 Hz.
double strongest(const opxy::i32* out, unsigned from, unsigned count, double lo, double hi) {
    double best = lo, level = 0;
    for (double hz = lo; hz <= hi; hz += 0.25) {
        const double a = tone(out, from, count, hz);
        if (a > level) { level = a; best = hz; }
    }
    return best;
}
// The lag between shortest and longest where the output best repeats itself.
unsigned repeat(const opxy::i32* out, unsigned from, unsigned count, unsigned shortest, unsigned longest) {
    unsigned best = 0;
    double most = -1;
    for (unsigned lag = shortest; lag < longest; ++lag) {
        double ab = 0, aa = 0, bb = 0;
        for (unsigned i = from; i < from + count; ++i) {
            const double a = out[i], b = out[i + lag];
            ab += a * b; aa += a * a; bb += b * b;
        }
        const double r = aa > 0 && bb > 0 ? ab / std::sqrt(aa * bb) : 0;
        if (r > most) { most = r; best = lag; }
    }
    return best;
}
}

int main() {
    static opxy::i32 out[kFrames], other[kFrames];
    static md::dpro_wave::State wave;
    static md::dpro_ddrw::State ddrw;
    static md::dpro_dens::State dens;
    // Every control at both ends, one at a time, on each machine: bounded always.
    for (int slot = 0; slot < 8; ++slot)
        for (int end = 0; end < 2; ++end) {
            int v[8] = {41, 64, 0, 0, 0, 64, 0, 64};
            v[slot] = end ? 127 : 0;
            play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, v, out);
            check("DPRO-WAVE stays within the declared peak", bounded(out));
            int d[8] = {0, 64, 8, 0, 0, 64, 0, 64};
            d[slot] = end ? 127 : 0;
            play(ddrw, md::dpro_ddrw::reset, md::dpro_ddrw::trigger, md::dpro_ddrw::render, d, out);
            check("DPRO-DDRW stays within the declared peak", bounded(out));
            int e[8] = {127, 127, 127, 8, 0, 127, 0, 64};   // four voices on SQR, the loudest wave
            e[slot] = end ? 127 : 0;
            play(dens, md::dpro_dens::reset, md::dpro_dens::trigger, md::dpro_dens::render, e, out);
            check("DPRO-DENS stays within the declared peak", bounded(out));
        }

    if (!md::dpro_data::captured) {
        const int v[8] = {41, 64, 64, 0, 0, 64, 0, 64};
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, v, out);
        check("DPRO-WAVE is silent without the captured waves", silent(out));
        const int d[8] = {0, 64, 8, 0, 0, 64, 0, 64};
        play(ddrw, md::dpro_ddrw::reset, md::dpro_ddrw::trigger, md::dpro_ddrw::render, d, out);
        check("DPRO-DDRW is silent without the captured waves", silent(out));
        const int e[8] = {86, 101, 62, 32, 0, 40, 40, 64};
        play(dens, md::dpro_dens::reset, md::dpro_dens::trigger, md::dpro_dens::render, e, out);
        check("DPRO-DENS is silent without the captured waves", silent(out));
        if (!failures) std::puts("PASS dpro (silent: no captured waves on this Mac, so bounds and silence only)");
        return failures ? 1 : 0;
    }

    // DPRO-WAVE. S02 is a sine-like set: one cycle a note, at middle C.
    {
        const int sine[8] = {5, 0, 0, 0, 0, 64, 0, 64};
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, sine, out);
        check("DPRO-WAVE sounds", rms(out, 0, kFrames) > 0.01);
        check("DPRO-WAVE plays the note: S02 at 261.6 Hz", std::fabs(strongest(out, 4410, 8820, 250, 275) - 261.6) < 1.0);
        const int low[8] = {5, 0, 0, 0, 0, 64, 0, 0}, high[8] = {5, 0, 0, 0, 0, 64, 0, 127};
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, low, out);
        check("DPRO-WAVE TUNE 0 is 2 semitones down", std::fabs(strongest(out, 4410, 8820, 225, 245) - 233.1) < 1.5);
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, high, out);
        check("DPRO-WAVE TUNE 127 is 2 semitones up", std::fabs(strongest(out, 4410, 8820, 285, 305) - 293.7) < 1.5);
        // WP 127 is the next set at WP 0, as measured.
        const int end[8] = {41, 127, 0, 0, 0, 64, 0, 64}, next[8] = {45, 0, 0, 0, 0, 64, 0, 64};
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, end, out);
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, next, other);
        double diff = 0;
        for (unsigned i = 0; i < kFrames; ++i) diff = std::fmax(diff, std::fabs((out[i] - static_cast<double>(other[i])) / 2147483648.0));
        check("DPRO-WAVE WP 127 is the next set", diff < 0.002);
        // WP moves the shape: S11 at WP 64 is not S11 at WP 0.
        const int mid[8] = {41, 64, 0, 0, 0, 64, 0, 64}, start[8] = {41, 0, 0, 0, 0, 64, 0, 64};
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, mid, out);
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, start, other);
        diff = 0;
        for (unsigned i = 0; i < kFrames; ++i) diff = std::fmax(diff, std::fabs((out[i] - static_cast<double>(other[i])) / 2147483648.0));
        check("DPRO-WAVE WP changes the wave", diff > 0.02);
        // WPM sweeps WP: the wave a quarter second in is not the wave at the start; WPRS restarts it.
        const int sweep[8] = {1, 0, 127, 127, 0, 64, 0, 64};   // S01, which WP moves far
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, sweep, out);
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, sweep, other, kBlocks, false);
        double early = 0;
        for (unsigned i = 0; i < 2048; ++i) early = std::fmax(early, std::fabs((out[i] - static_cast<double>(other[i])) / 2147483648.0));
        check("DPRO-WAVE WPRS restarts the sweep at each note", early < 0.002);
        // The table's harmonics (130.8 Hz apart) at the start and 0.3 s on, as WP sweeps.
        double change = 0, total = 0;
        for (int k = 1; k <= 8; ++k) {
            const double a = tone(out, 0, 4410, 130.81 * k), b = tone(out, 13230, 4410, 130.81 * k);
            change += std::fabs(a - b); total += a + b;
        }
        check("DPRO-WAVE WPM moves WP", change > 0.1 * total);
        // SYNC SFRQ 64 restarts the wave at 51 Hz, whatever the note: 864 samples.
        const int sync[8] = {41, 0, 0, 0, 64, 64, 0, 64};
        play(wave, md::dpro_wave::reset, md::dpro_wave::trigger, md::dpro_wave::render, sync, out);
        const unsigned lag = repeat(out, 4410, 2000, 700, 1000);
        check("DPRO-WAVE SFRQ 64 restarts at 51 Hz", lag > 850 && lag < 880);
    }

    // DPRO-DDRW: SINE (D01) and SQR (D05).
    {
        const int one[8] = {0, 0, 8, 0, 0, 64, 0, 64};
        play(ddrw, md::dpro_ddrw::reset, md::dpro_ddrw::trigger, md::dpro_ddrw::render, one, out);
        const double sine = tone(out, 4410, 8820, 261.63);
        check("DPRO-DDRW MIX 0 is WAV1: SINE at the original's level (0.18)", sine > 0.16 && sine < 0.20);
        check("DPRO-DDRW MIX 0 has none of WAV2", tone(out, 4410, 8820, 784.9) < 0.002);
        const int half[8] = {0, 64, 8, 0, 0, 0, 0, 64};
        play(ddrw, md::dpro_ddrw::reset, md::dpro_ddrw::trigger, md::dpro_ddrw::render, half, out);
        check("DPRO-DDRW MIX 64 crossfades: SQR's third harmonic at half (0.032)", std::fabs(tone(out, 4410, 8820, 784.9) - 0.032) < 0.008);
        // WID 127 at MIX 64: the two waves 53 cents apart each way (as measured: 126.9 and 135.0 Hz at 130.8).
        const int wide[8] = {0, 64, 8, 0, 0, 127, 0, 64};
        play(ddrw, md::dpro_ddrw::reset, md::dpro_ddrw::trigger, md::dpro_ddrw::render, wide, out, 690);
        const double up = strongest(out, 4410, 30000, 3 * 266, 3 * 274) / 3;
        check("DPRO-DDRW WID 127 lifts WAV2 53 cents", std::fabs(1200 * std::log2(up / 261.63) - 53.5) < 4);
        // BR1 127: one bit left of WAV1.
        const int crushed[8] = {2, 0, 2, 0, 127, 64, 0, 64};
        play(ddrw, md::dpro_ddrw::reset, md::dpro_ddrw::trigger, md::dpro_ddrw::render, crushed, out);
        int levels = 0;
        opxy::i32 seen[8];
        for (unsigned i = 0; i < kFrames && levels <= 4; ++i) {
            bool known = false;
            for (int k = 0; k < levels; ++k) known = known || seen[k] == out[i];
            if (!known && levels < 8) seen[levels++] = out[i];
        }
        check("DPRO-DDRW BR1 127 leaves two levels", levels == 2);
        // D08 RND drifts: each cycle a little unlike the last (0.96 on the original at note 24), two sets apart unrelated.
        static md::dpro::Noise noise;
        noise.fill(12345);
        static double cycle[64][md::dpro::table_size];
        for (unsigned c = 0; c < 64; ++c)
            for (unsigned i = 0; i < md::dpro::table_size; ++i)
                cycle[c][i] = md::dpro::rnd(noise, i << md::dpro::index_shift, c, 0);
        const auto correlation = [](const double* a, const double* b) {
            double ab = 0, aa = 0, bb = 0;
            for (unsigned i = 0; i < md::dpro::table_size; ++i) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
            return ab / std::sqrt(aa * bb);
        };
        double next = 0, apart = 0;
        for (unsigned c = 0; c < 48; ++c) {
            next += correlation(cycle[c], cycle[c + 1]) / 48;
            apart = std::fmax(apart, std::fabs(correlation(cycle[c], cycle[c + 16])));
        }
        check("DPRO-DDRW D08 RND: a cycle 0.95 to 0.995 like the last", next > 0.95 && next < 0.995);
        check("DPRO-DDRW D08 RND: sets two apart unrelated", apart < 0.25);
    }

    // DPRO-DENS: SINE (D01) voices, a quarter of DDRW's level each.
    {
        const int alone[8] = {62, 62, 62, 0, 0, 0, 0, 64};
        play(dens, md::dpro_dens::reset, md::dpro_dens::trigger, md::dpro_dens::render, alone, out);
        const double one = tone(out, 4410, 8820, 261.63);
        check("DPRO-DENS with PCH2..4 OFF (62) is one voice, a quarter of DDRW's (0.045)", one > 0.040 && one < 0.050);
        const int fifth[8] = {101, 62, 62, 0, 0, 0, 0, 64};   // +7
        play(dens, md::dpro_dens::reset, md::dpro_dens::trigger, md::dpro_dens::render, fifth, out);
        check("DPRO-DENS PCH2 101 adds a fifth", tone(out, 4410, 8820, 392.0) > 0.04);
        const int just[8] = {78, 62, 62, 0, 0, 0, 0, 64};     // 6/5, not the tempered minor third
        play(dens, md::dpro_dens::reset, md::dpro_dens::trigger, md::dpro_dens::render, just, out, 690);
        check("DPRO-DENS PCH2 78 adds a just minor third (6/5)", std::fabs(strongest(out, 4410, 30000, 305, 320) - 313.96) < 0.8);
        const int below[8] = {0, 62, 62, 0, 0, 0, 0, 64};     // -12
        play(dens, md::dpro_dens::reset, md::dpro_dens::trigger, md::dpro_dens::render, below, out);
        check("DPRO-DENS PCH2 0 adds the octave below", tone(out, 4410, 8820, 130.81) > 0.04);
        const int chorus[8] = {62, 62, 62, 0, 0, 127, 0, 64};
        play(dens, md::dpro_dens::reset, md::dpro_dens::trigger, md::dpro_dens::render, chorus, out, 690);
        double lo = 1, hi = 0;
        for (unsigned w = 1; w < 40; ++w) { const double r = rms(out, w * 1000, 1000); lo = std::fmin(lo, r); hi = std::fmax(hi, r); }
        check("DPRO-DENS CHRL 127 beats", hi > lo * 1.3);
    }
    if (!failures) std::puts("PASS dpro (captured waves: fitted measures checked)");
    return failures ? 1 : 0;
}
