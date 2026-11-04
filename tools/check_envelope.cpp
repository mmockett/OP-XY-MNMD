// Runs an engine's display on the host against the track's amp envelope
// (SDK 1.10, docs/SCREEN.md "Following the amp envelope"):
//
// - Unknown envelope: a fixed run of notes (held, released, a chord, a tap, a
//   note whose hold the SDK can't report, reduced motion) at several settings,
//   every frame hashed. tools/check_envelope.py compares the hash with the one
//   recorded from the screens before they read the envelope: unknown must draw
//   exactly what it always did.
// - A short envelope (a pluck: no attack, a short decay to silence, stock's
//   shortest release) against a long one (slow attack, a long decay to a held
//   level, a long release): when each goes still, held and after the release,
//   and how many nodes each frame changes while the note is held and while it
//   releases. Stock's words (amp_curve.h): a larger release word is SHORTER.
// - Stock's directions, for an engine that follows the envelope: a larger
//   release word ends the motion sooner, and a larger attack word swells the
//   level slower.
//
// Use tools/check_envelope.py, which compiles it once per engine:
//   c++ -std=c++11 -I<sdk>/sdk/include -Imachines/<e> -DENGINE=<e> [-DKIT] tools/check_envelope.cpp
#include "display.cpp"
#include <cstdio>
#include <cstring>

namespace {
using namespace opxy::draw;
using opxy::i32;
typedef ENGINE::Frame EngineFrame;
const int frame_q10 = 17;   // 60 fps

ENGINE::DisplayState state;
EngineFrame frame, previous;
Input input;
unsigned long long hash = 1469598103934665603ull;

void mix(const void* data, unsigned size) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (unsigned i = 0; i < size; ++i) hash = (hash ^ p[i]) * 1099511628211ull;
}

bool shown(const Node& n) { return n.x < hidden_x * unit; }
// What the renderer redraws: a matrix, a grey or a path's key, on a node shown now or before.
int changed_nodes(const EngineFrame& a, const EngineFrame& b) {
    int changed = 0;
    for (opxy::u32 i = 0; i < EngineFrame::node_count; ++i) {
        const Node& n = a.nodes[i];
        const Node& m = b.nodes[i];
        if (!shown(n) && !shown(m)) continue;
        bool c = n.x != m.x || n.y != m.y || n.scale != m.scale || n.cos != m.cos || n.sin != m.sin || n.shade != m.shade;
        if (i < EngineFrame::path_count) c = c || a.paths[i].key != b.paths[i].key;
        changed += c;
    }
    return changed;
}

Envelope unknown() { Envelope e = {envelope_unavailable, envelope_unavailable, envelope_unavailable, envelope_unavailable}; return e; }
i32 q(double x) { return x >= 1 ? 0x7fffffff : static_cast<i32>(x * 2147483648.0); }
Envelope envelope(double a, double d, double s, double r) { Envelope e = {q(a), q(d), q(s), q(r)}; return e; }

void begin_run(const i32* slots, opxy::u32 model, const Envelope& amp) {
    std::memset(&input, 0, sizeof input);
    ENGINE::display_reset(state);
    frame = EngineFrame();
    input.track = 1;
    input.elapsed = frame_q10;
    input.model = model;
    input.amp = amp;
    for (int i = 0; i < 8; ++i) input.slots[i] = slots[i];
    for (int i = 0; i < 4; ++i) { input.shown[i] = static_cast<opxy::u8>(i); input.parameters[i] = slots[i]; }
}
int step() {
    previous = frame;
    ENGINE::display(state, input, frame);
    mix(&frame, sizeof frame);
    return changed_nodes(frame, previous);
}
void note_on(opxy::u32 notes = 1) { input.note_serial += notes; input.held_notes = notes; }
void note_off() { input.note_off_serial += input.held_notes; input.held_notes = 0; }

// The settings a run plays at: every control at 64, 0 and 127, then fixed random ones.
void setting(int k, i32* s) {
    unsigned seed = 0x9e3779b9u * static_cast<unsigned>(k + 1);
    for (int i = 0; i < 8; ++i) {
        seed = seed * 1664525u + 1013904223u;
        s[i] = k == 0 ? 0x40000000 : k == 1 ? 0 : k == 2 ? 0x7fffffff : static_cast<i32>(seed >> 1);
    }
}
#ifdef KIT
const opxy::u32 models = ENGINE::kit::models;
#else
const opxy::u32 models = 1;
#endif

// The fixed run with the envelope unknown; every frame goes into the hash.
void unknown_run(const i32* s, opxy::u32 model) {
    begin_run(s, model, unknown());
    for (int t = 0; t < 5; ++t) step();
    note_on();
    for (int t = 0; t < 90; ++t) step();
    input.elapsed = 100;                       // a slow frame, then the usual
    step();
    input.elapsed = frame_q10;
    note_off();
    for (int t = 0; t < 60; ++t) step();
    note_on(3);                                // a chord, one note let go early
    for (int t = 0; t < 20; ++t) step();
    input.note_off_serial += 1; input.held_notes = 2;
    for (int t = 0; t < 10; ++t) step();
    note_off();
    for (int t = 0; t < 40; ++t) step();
    input.note_serial += 1;                    // a tap between frames
    input.note_off_serial += 1;
    for (int t = 0; t < 60; ++t) step();
    input.held_notes = notes_unavailable;      // a host that can't say what is held
    input.note_serial += 1;
    for (int t = 0; t < 60; ++t) step();
    input.held_notes = 0;
    input.reduced_motion = 1;
    note_on();
    for (int t = 0; t < 30; ++t) step();
    note_off();
    for (int t = 0; t < 10; ++t) step();
}

struct Life {
    double hold_mean, release_mean;   // changed nodes a frame
    int hold_peak, release_peak, rest;
    double last_held, last_released;  // seconds from the note, and from the release, of the last change
};
// A note held for `hold` seconds, then released, at one envelope.
Life life(const i32* s, opxy::u32 model, const Envelope& amp, double hold, opxy::u32 notes = 1) {
    Life l = {0, 0, 0, 0, 0, -1, -1};
    begin_run(s, model, amp);
    for (int t = 0; t < 5; ++t) step();
    for (int t = 0; t < 120; ++t) l.rest += step();   // at rest: nothing
    note_on(notes);
    const int held = static_cast<int>(hold * 60);
    long sum = 0;
    for (int t = 0; t < held; ++t) {
        const int n = step();
        sum += n;
        if (n > l.hold_peak) l.hold_peak = n;
        if (n) l.last_held = (t + 1) / 60.0;
    }
    l.hold_mean = static_cast<double>(sum) / held;
    note_off();
    sum = 0;
    int moving = 0;
    for (int t = 0; t < 60 * 12; ++t) {
        const int n = step();
        if (n) { l.last_released = (t + 1) / 60.0; moving = t + 1; }
        sum += n;
        if (n > l.release_peak) l.release_peak = n;
    }
    l.release_mean = moving ? static_cast<double>(sum) / moving : 0;
    return l;
}
}

int main() {
    // Unknown: the hash of every frame of the fixed run, at every setting and model.
    for (opxy::u32 m = 0; m < models; ++m)
        for (int k = 0; k < 6; ++k) {
            i32 s[8];
            setting(k, s);
            unknown_run(s, m);
        }
    std::printf("unknown %016llx\n", hash);

    // A pluck: no attack, a short decay to nothing (word 0.1, about 0.3 s),
    // stock's shortest release (word 0x7f000000, about 6 ms). A pad: a slow
    // attack (word 0.45, about 0.9 s), a long decay to 0.7 and a long release
    // (word 0.4, about 3.8 s to -66 dB).
    Envelope pluck = envelope(0, 0.1, 0, 0), pad = envelope(0.45, 0.5, 0.7, 0.4);
    pluck.release = 0x7f000000;
    int failures = 0;
    double worst_mean = 0;
    int worst_peak = 0, rest = 0;
    double short_end = 0, long_end = 0, short_held = 0, long_held = 0;
    for (opxy::u32 m = 0; m < models; ++m)
        for (int k = 0; k < 6; ++k) {
            i32 s[8];
            setting(k, s);
            for (int e = 0; e < 2; ++e) {
                const Life l = life(s, m, e ? pad : pluck, 2.0, k == 3 ? 4 : 1);
                rest += l.rest;
                if (l.hold_mean > worst_mean) worst_mean = l.hold_mean;
                if (l.release_mean > worst_mean) worst_mean = l.release_mean;
                if (l.hold_peak > worst_peak) worst_peak = l.hold_peak;
                if (l.release_peak > worst_peak) worst_peak = l.release_peak;
                if (e) { if (l.last_released > long_end) long_end = l.last_released; if (l.last_held > long_held) long_held = l.last_held; }
                else { if (l.last_released > short_end) short_end = l.last_released; if (l.last_held > short_held) short_held = l.last_held; }
                if (!k && !m)
                    std::printf("  %-5s held: mean %5.1f peak %3d, last change %.2f s in; released: mean %5.1f peak %3d, still after %.2f s\n",
                                e ? "long" : "short", l.hold_mean, l.hold_peak, l.last_held, l.release_mean, l.release_peak,
                                l.last_released < 0 ? 0 : l.last_released);
            }
        }
    std::printf("  every setting: short held until %.2f s, still %.2f s after the release; long held until %.2f s, still %.2f s after\n",
                short_held, short_end < 0 ? 0 : short_end, long_held, long_end < 0 ? 0 : long_end);
    std::printf("  churn: worst mean %.1f, worst peak %d; at rest %d\n", worst_mean, worst_peak, rest);
    if (rest) { std::puts("  FAIL: changes at rest"); ++failures; }
    if (worst_mean > 25) { std::puts("  FAIL: more than 25 nodes a frame on average"); ++failures; }
    // A pluck has died away within 0.4 s of the note, held or not.
#ifdef FOLLOWS
    if (short_held > 0.4 || short_end > 0.4) { std::puts("  FAIL: a pluck's motion outlasts its sound"); ++failures; }
#else
    std::puts("  doesn't read the envelope: its motion is the same under any");
#endif
#ifdef FOLLOWS
    // Stock's directions, at the middle setting: the motion after the release
    // (release words 0.25, 0.5 and 0x7f000000: about 7.9 s, 3 s and 6 ms to
    // -66 dB), and the envelope's level 0.1 s into a held note (attack words 0,
    // 0.375 and 0.5: one block, about 0.33 s and 1.45 s).
    {
        i32 s[8];
        setting(0, s);
        const i32 releases[3] = {q(0.25), q(0.5), 0x7f000000}, attacks[3] = {0, q(0.375), q(0.5)};
        double lasted[3];
        i32 swell[3];
        for (int k = 0; k < 3; ++k) {
            Envelope e = pad;
            e.release = releases[k];
            lasted[k] = life(s, 0, e, 2.0).last_released;
            Envelope r = pad;
            r.attack = attacks[k];
            input.amp = r;
            const md::view::Moment m = {102, 0, 0, md::view::amp_of(input)};   // 0.1 s, Q10
            swell[k] = m.level();
        }
        std::printf("  directions: release 0.25, 0.5, 0x7f still after %.2f, %.2f and %.2f s; attack 0, 0.375, 0.5 at %.2f, %.2f and %.2f 0.1 s in\n",
                    lasted[0] < 0 ? 0 : lasted[0], lasted[1] < 0 ? 0 : lasted[1], lasted[2] < 0 ? 0 : lasted[2],
                    swell[0] / 2147483648.0, swell[1] / 2147483648.0, swell[2] / 2147483648.0);
        // An engine still after the release under any is still under all: nothing to order.
        if (lasted[0] > 0 && !(lasted[0] > lasted[1] && lasted[1] > lasted[2])) {
            std::puts("  FAIL: a larger release word doesn't end the motion sooner"); ++failures;
        }
        if (!(swell[0] > swell[1] && swell[1] > swell[2])) { std::puts("  FAIL: a larger attack word doesn't swell slower"); ++failures; }
    }
#endif
    std::printf("RESULT %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
