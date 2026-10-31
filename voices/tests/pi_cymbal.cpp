// Sound checks for PI-RC, PI-CC and PI-HH, compiled for the host and run by
//   python3 tools/test_voices.py
// The measures are the Machinedrum's (reference/fit_pi_cymbal.py).
#include "../pi_cymbal.hpp"
#include "pi_check.hpp"

namespace {
using namespace check;
void ride(const int* values, opxy::i32* out) {
    check::play<md::pi_rc::State>(md::pi_rc::reset, md::pi_rc::trigger, md::pi_rc::render, values, out);
}
void crash(const int* values, opxy::i32* out) {
    check::play<md::pi_cc::State>(md::pi_cc::reset, md::pi_cc::trigger, md::pi_cc::render, values, out);
}
void hat(const int* values, opxy::i32* out) {
    check::play<md::pi_hh::State>(md::pi_hh::reset, md::pi_hh::trigger, md::pi_hh::render, values, out);
}
// The time after which the level stays 20 dB under its first 10 ms, in seconds.
double ring(const opxy::i32* out) {
    const double start = db(rms(out, 0, 0.01));
    double last = 0;
    for (double t = 0; t < 1.9; t += 0.005)
        if (db(rms(out, t, 0.005)) > start - 20) last = t;
    return last;
}
// The share of energy above 5 kHz, as a one-pole high-pass sees it.
double bright(const opxy::i32* out) {
    double low = 0, high = 0, all = 0;
    for (unsigned i = 0; i < at(0.3); ++i) {
        const double x = out[i] / 2147483648.0;
        low += (x - low) * 0.51;
        high += (x - low) * (x - low);
        all += x * x;
    }
    return high / (all + 1e-30);
}
}

int main() {
    static opxy::i32 out[kFrames];
    //                    PTCH DEC HARD RING AG AU BR GRAB
    const int design[8] = {64, 50, 64, 0, 64, 0, 64, 127};
    ride(design, out);
    expect("the ride sounds", rms(out, 0, 0.1) > 0.002);
    expect("the ride stays within the declared peak", bounded(out));
    crash(design, out);
    expect("the crash sounds", rms(out, 0, 0.1) > 0.002);
    //                       PTCH DEC CLSN RING AG AU BR CLOS
    const int closed[8] = {64, 50, 0, 0, 64, 0, 64, 127};
    hat(closed, out);
    expect("the hat sounds", rms(out, 0, 0.1) > 0.004);

    // DEC: the ride's -20 dB time, 0.17 s at the design's DEC, 1.15 s at 127.
    ride(design, out);
    const double ride_ring = ring(out);
    const int long_ride[8] = {64, 127, 64, 0, 64, 0, 64, 127};
    ride(long_ride, out);
    const double long_ring = ring(out);
    std::printf("the ride's -20 dB time at DEC 50 and 127: %.2f, %.2f s\n", ride_ring, long_ring);
    expect("the ride rings about 0.17 s", ride_ring > 0.12 && ride_ring < 0.25);
    expect("DEC 127 rings over a second", long_ring > 0.9 && long_ring < 1.5);

    // GRAB 0 grabs the ride within 50 ms.
    const int grabbed[8] = {64, 127, 64, 0, 64, 0, 64, 0};
    ride(grabbed, out);
    std::printf("the grabbed ride's -20 dB time: %.3f s\n", ring(out));
    expect("GRAB grabs it", ring(out) < 0.09);

    // HARD: a soft stick on the crash is far quieter and rings far longer.
    const int soft[8] = {64, 50, 16, 0, 64, 0, 64, 127};
    crash(soft, out);
    const double soft_level = rms(out, 0, 0.2), soft_ring = ring(out);
    crash(design, out);
    std::printf("the crash at HARD 16 against 64: %.1f dB, rings %.2f against %.2f s\n",
                db(soft_level) - db(rms(out, 0, 0.2)), soft_ring, ring(out));
    expect("a soft crash is much quieter", db(soft_level) - db(rms(out, 0, 0.2)) < -12);
    expect("a soft crash rings longer", soft_ring > ring(out) * 3);

    // BR brightens the hat.
    const int dark[8] = {64, 50, 0, 0, 64, 0, 0, 127}, light[8] = {64, 50, 0, 0, 64, 0, 127, 127};
    hat(dark, out);
    const double dark_share = bright(out);
    hat(light, out);
    std::printf("the hat above 5 kHz, BR 0 and 127: %.2f, %.2f\n", dark_share, bright(out));
    expect("BR brightens the hat", bright(out) > dark_share * 1.3);   // the dense layer, which BR leaves, narrows the gap

    // CLOS: below 127 the hat rings until the pedal closes it: 0.2 s at CLOS 64.
    const int open[8] = {64, 127, 0, 0, 64, 0, 64, 64};
    hat(open, out);
    const double before = rms(out, 0.15, 0.02), after = rms(out, 0.3, 0.02);
    std::printf("the open hat at 150 and 300 ms: %.1f, %.1f dB\n", db(before), db(after));
    expect("the pedal closes the hat after 0.2 s", db(after) < db(before) - 30);

    // Each hit is struck afresh: two hits differ.
    static opxy::i32 again[kFrames];
    md::pi_cymbal::State voice;
    md::Common shared = {};
    opxy::i32 p[8];
    for (int i = 0; i < 8; ++i) p[i] = q(design[i]);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = kMiddleC;
    md::pi_rc::reset(voice, shared, 1);
    md::pi_rc::trigger(voice, shared);
    for (unsigned b = 0; b < 40; ++b) md::pi_rc::render(voice, shared, out + b * 64, p, pitch);
    md::pi_rc::trigger(voice, shared);
    for (unsigned b = 0; b < 40; ++b) md::pi_rc::render(voice, shared, again + b * 64, p, pitch);
    unsigned same = 0;
    for (unsigned i = 0; i < 2560; ++i) same += out[i] == again[i];
    expect("no two hits are alike", same < 100);

    // A closed hat chokes an open one ringing on another voice.
    {
        md::pi_cymbal::State open_voice, closed_voice;
        md::Common common = {};
        const int open_values[8] = {64, 110, 0, 0, 64, 0, 64, 127};
        opxy::i32 po[8], pc[8];
        for (int i = 0; i < 8; ++i) { po[i] = q(open_values[i]); pc[i] = q(closed[i]); }
        md::pi_hh::reset(open_voice, common, 1);
        md::pi_hh::trigger(open_voice, common);
        for (unsigned b = 0; b < 20; ++b) md::pi_hh::render(open_voice, common, out, po, pitch);
        const double ringing = rms(out, 0, 64 / 44100.0);
        md::pi_hh::reset(closed_voice, common, 2);
        md::pi_hh::trigger(closed_voice, common);
        md::pi_hh::render(closed_voice, common, again, pc, pitch);
        for (unsigned b = 0; b < 60; ++b) md::pi_hh::render(open_voice, common, out, po, pitch);
        std::printf("the open hat, choked: %.1f dB after 90 ms\n", db(rms(out, 0, 64 / 44100.0)) - db(ringing));
        expect("a closed hat chokes an open one", db(rms(out, 0, 64 / 44100.0)) < db(ringing) - 30);
    }

    const int (*sets[2])[8] = {&design, &closed};
    for (int machine = 0; machine < 3; ++machine)
        for (int c = 0; c < 8; ++c)
            for (int end = 0; end < 2; ++end) {
                int values[8];
                for (int i = 0; i < 8; ++i) values[i] = (*sets[machine == 2])[i];
                values[c] = end ? 127 : 0;
                if (machine == 0) ride(values, out);
                else if (machine == 1) crash(values, out);
                else hat(values, out);
                expect("bounded at a control's end", bounded(out));
            }
    const int hot[8] = {127, 127, 127, 127, 127, 127, 0, 127};
    ride(hot, out);
    expect("hot ride stays bounded", bounded(out));
    crash(hot, out);
    expect("hot crash stays bounded", bounded(out));
    hat(hot, out);
    expect("hot hat stays bounded", bounded(out));

    if (!failures) std::puts("PASS pi_cymbal");
    return failures ? 1 : 0;
}
