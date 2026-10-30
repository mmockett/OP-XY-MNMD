// Runs a Monomachine synth's display on the host and checks every frame the
// device would draw, after tools/check_kit_frames.cpp on the kits' branch: each
// control through all 128 values with the others at each baseline, at rest and
// while a note is held and released, then random settings. Beyond the SDK's
// opxy::draw::valid it checks what the device's renderer does with an outline
// (OP-XY-Dev docs/screens.md): no outline without area, no turn straight back,
// neighbours at least 1/8 px apart, a closing edge of at least 1/4 px. It also
// finds a path whose key did not change when its points should have, counts the
// nodes a frame changes, checks that a held note keeps the screen moving and that
// a still one stays still, and walks each control for what it does on screen.
//
// Use tools/check_synth_frames.py, which passes the definition's defaults:
//   c++ -std=c++11 -I<sdk>/sdk/include -Imachines/<e> -DENGINE=<e> -DCONTROLS=... -DPAGED tools/check_synth_frames.cpp
#include "display.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
using namespace opxy::draw;
typedef ENGINE::Frame EngineFrame;
typedef long long wide;
using opxy::i32;
struct Control { const char* name; int slot; int fallback; };
const Control controls[] = {CONTROLS};
const int control_count = sizeof controls / sizeof controls[0];
#ifdef PAGED
const opxy::u32 first_header = ENGINE::header_node;
#else
const opxy::u32 first_header = EngineFrame::node_count;
#endif
const int frame_q10 = 17;   // 60 fps

ENGINE::DisplayState state, fresh_state;
EngineFrame frame, previous, fresh;
Input input;

int faults[8];
char first_fault[8][160];
const char* const fault_names[8] = {"invalid", "merge", "closing", "no area", "straight back", "off stage", "stale key", "unused"};
char where[96];
void fault(int kind, opxy::u32 path) {
    if (!faults[kind]++) std::snprintf(first_fault[kind], sizeof first_fault[kind], "path %u, %s", path, where);
}

bool shown(const Node& n) { return n.x < hidden_x * unit; }

void check_path(opxy::u32 p) {
    Point xy[EngineFrame::point_count];
    opxy::u32 m = 0;
    for (opxy::u32 i = 0; i < EngineFrame::point_count; ++i) {
        const Point& q = frame.paths[p].points[i];
        if (!m || q.x != xy[m - 1].x || q.y != xy[m - 1].y) xy[m++] = q;   // a repeated last point pads
    }
    if (m < 3) { fault(3, p); return; }
    const i32 apart = unit / 8;
    wide twice = 0;
    for (opxy::u32 i = 0; i < m; ++i) {
        const Point& a = xy[i];
        const Point& b = xy[(i + 1) % m];
        const Point& c = xy[(i + 2) % m];
        const i32 dx = a.x - b.x, dy = a.y - b.y;
        if (i + 1 < m && dx > -apart && dx < apart && dy > -apart && dy < apart) fault(1, p);
        twice += static_cast<wide>(a.x) * b.y - static_cast<wide>(b.x) * a.y;
        // A turn straight back, or so nearly (within 1/32 px of the way in) that it draws the same.
        const wide ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
        const double lu = std::sqrt(static_cast<double>(ux * ux + uy * uy)), lv = std::sqrt(static_cast<double>(vx * vx + vy * vy));
        const double cross = std::fabs(static_cast<double>(ux * vy - uy * vx));
        if (ux * vx + uy * vy < 0 && cross <= (lu > lv ? lu : lv) * unit / 32) fault(4, p);
    }
    const i32 cx = xy[0].x - xy[m - 1].x, cy = xy[0].y - xy[m - 1].y;
    if ((cx < 0 ? -cx : cx) < unit / 4 && (cy < 0 ? -cy : cy) < unit / 4) fault(2, p);
    const double area = (twice < 0 ? -twice : twice) / 2.0 / (unit * unit);
    if (area < 0.05) fault(3, p);
    const Node& n = frame.nodes[p];
    for (opxy::u32 i = 0; i < m; ++i) {
        const wide rx = (static_cast<wide>(xy[i].x) * n.cos - static_cast<wide>(xy[i].y) * n.sin) >> 10;
        const wide ry = (static_cast<wide>(xy[i].x) * n.sin + static_cast<wide>(xy[i].y) * n.cos) >> 10;
        const wide x = n.x + ((rx * n.scale) >> 10), y = n.y + ((ry * n.scale) >> 10);
        if (x < 0 || x > 480LL * unit || y < 38LL * unit || y > 222LL * unit) { fault(5, p); break; }
    }
}

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

void check() {
    if (!valid(frame)) fault(0, 0);
    for (opxy::u32 p = 0; p < EngineFrame::path_count; ++p)
        if (shown(frame.nodes[p]) && (frame.paths[p].key != previous.paths[p].key || !shown(previous.nodes[p]))) check_path(p);
}

void set(const i32* slots) {
    for (int i = 0; i < 8; ++i) input.slots[i] = slots[i];
    for (int i = 0; i < 4; ++i) {
        input.shown[i] = static_cast<opxy::u8>(i);
        input.parameters[i] = slots[i];
    }
}
void begin_run(const i32* slots) {
    std::memset(&input, 0, sizeof input);
    ENGINE::display_reset(state);
    frame = EngineFrame();
    input.track = 1;
    input.elapsed = frame_q10;
    set(slots);
    ENGINE::display(state, input, frame);
}
int step() {
    previous = frame;
    ENGINE::display(state, input, frame);
    check();
    return changed_nodes(frame, previous);
}
void note_on(int notes = 1) { input.note_serial += notes; input.held_notes = notes; }
void note_off() { input.note_off_serial += input.held_notes; input.held_notes = 0; }

// A control's walk: the drawing at this value as if nothing came before, to
// catch a key that stayed the same when the points should have changed.
void check_stale() {
    Input fi = input;
    ENGINE::display_reset(fresh_state);
    fresh = EngineFrame();
    fi.note_serial = input.note_serial;
    ENGINE::display(fresh_state, fi, fresh);
    for (opxy::u32 p = 0; p < EngineFrame::path_count; ++p)
        if (shown(frame.nodes[p]) && shown(fresh.nodes[p]) &&
            std::memcmp(frame.paths[p].points, fresh.paths[p].points, sizeof frame.paths[p].points)) fault(6, p);
}

i32 q(int v) { return v >= 127 ? 0x7fffffff : v << 24; }

// The farthest any drawn point lies from the other frame's drawing, in pixels (Hausdorff over paths and nodes).
double travel(const EngineFrame& a, const EngineFrame& b) {
    double most = 0;
    for (opxy::u32 i = 0; i < first_header; ++i) {
        const Node& n = a.nodes[i];
        const Node& m = b.nodes[i];
        if (shown(n) != shown(m)) {
            if (i < EngineFrame::path_count) {
                // A path that came or went: its size.
                const Node& v = shown(n) ? n : m;
                const typename EngineFrame::path_type& pa = shown(n) ? a.paths[i] : b.paths[i];
                i32 x0 = 1 << 30, x1 = -(1 << 30), y0 = 1 << 30, y1 = -(1 << 30);
                for (opxy::u32 k = 0; k < EngineFrame::point_count; ++k) {
                    if (pa.points[k].x < x0) x0 = pa.points[k].x;
                    if (pa.points[k].x > x1) x1 = pa.points[k].x;
                    if (pa.points[k].y < y0) y0 = pa.points[k].y;
                    if (pa.points[k].y > y1) y1 = pa.points[k].y;
                }
                const double e = std::sqrt(static_cast<double>(x1 - x0) * (x1 - x0) + static_cast<double>(y1 - y0) * (y1 - y0)) * v.scale / unit / unit;
                if (e > most) most = e;
            } else if (most < 4) most = 4;
            continue;
        }
        if (!shown(n)) continue;
        const double dx = (n.x - m.x) / static_cast<double>(unit), dy = (n.y - m.y) / static_cast<double>(unit);
        const double d = std::sqrt(dx * dx + dy * dy);
        if (d > most) most = d;
        if (n.scale != m.scale || n.cos != m.cos || n.sin != m.sin) {
            // Nodes other than paths carry no size here: a turn or scale counts as a step of 2 px.
            if (most < 2) most = 2;
        }
        if (i < EngineFrame::path_count && std::memcmp(a.paths[i].points, b.paths[i].points, sizeof a.paths[i].points)) {
            for (int way = 0; way < 2; ++way) {
                const Point* s = way ? b.paths[i].points : a.paths[i].points;
                const Point* t = way ? a.paths[i].points : b.paths[i].points;
                for (opxy::u32 j = 0; j < EngineFrame::point_count; ++j) {
                    wide best = 1LL << 60;
                    for (opxy::u32 k = 0; k < EngineFrame::point_count; ++k) {
                        const wide ex = s[j].x - t[k].x, ey = s[j].y - t[k].y, e = ex * ex + ey * ey;
                        if (e < best) best = e;
                    }
                    const double h = std::sqrt(static_cast<double>(best)) / unit * n.scale / unit;
                    if (h > most) most = h;
                }
            }
        }
    }
    return most;
}

i32 defaults[8];
void baseline_of(int b, i32* s) {
    static const int fixed[3] = {0, 64, 127};
    for (int i = 0; i < 8; ++i) s[i] = b == 0 ? defaults[i] : q(fixed[b - 1]);
}

struct Churn { double mean; int peak; };
// A note held for `seconds`, the mean and peak over the hold.
Churn hold(const i32* slots, int notes, int seconds, int* moving_late = 0) {
    begin_run(slots);
    for (int t = 0; t < 10; ++t) step();
    note_on(notes);
    Churn c = {0, 0};
    long sum = 0;
    const int frames = 60 * seconds;
    int late = 0;
    for (int t = 0; t < frames; ++t) {
        const int n = step();
        sum += n;
        if (n > c.peak) c.peak = n;
        if (t >= frames - 60 && n) ++late;
    }
    if (moving_late) *moving_late = late;
    c.mean = static_cast<double>(sum) / frames;
    return c;
}
}

int main() {
    for (int i = 0; i < 8; ++i) defaults[i] = 0;
    for (int c = 0; c < control_count; ++c) defaults[controls[c].slot] = controls[c].fallback;
    static const char* const baseline_names[4] = {"defaults", "all 0", "all 64", "all 127"};

    // Every value of every control at each baseline: at rest, then a note's life.
    double worst_mean = 0;
    int worst_peak = 0;
    char worst_where[96] = "";
    for (int b = 0; b < 4; ++b) {
        i32 base[8];
        baseline_of(b, base);
        for (int c = 0; c < control_count; ++c) {
            begin_run(base);
            for (int v = 0; v < 128; ++v) {
                std::snprintf(where, sizeof where, "%s, %s=%d at rest", baseline_names[b], controls[c].name, v);
                i32 s[8];
                std::memcpy(s, base, sizeof s);
                s[controls[c].slot] = q(v);
                set(s);
                step();
                step();
                check_stale();
            }
            for (int v = 0; v < 128; ++v) {
                std::snprintf(where, sizeof where, "%s, %s=%d held", baseline_names[b], controls[c].name, v);
                i32 s[8];
                std::memcpy(s, base, sizeof s);
                s[controls[c].slot] = q(v);
                begin_run(s);
                step();
                note_on();
                long sum = 0;
                int count = 0, peak = 0;
                for (int t = 0; t < 60; ++t) { const int n = step(); if (n > peak) peak = n; sum += n; ++count; }
                input.elapsed = 100;             // then later in a long hold, faster
                for (int t = 0; t < 40; ++t) { step(); input.elapsed = t < 5 ? 100 : 700; }
                input.elapsed = frame_q10;
                note_off();
                for (int t = 0; t < 30; ++t) { const int n = step(); if (n > peak) peak = n; }
                const double mean = static_cast<double>(sum) / count;
                if (mean > worst_mean) { worst_mean = mean; std::snprintf(worst_where, sizeof worst_where, "%s", where); }
                if (peak > worst_peak) worst_peak = peak;
            }
        }
    }
    // Random settings, with chords.
    unsigned seed = 0x1234567u;
    for (int r = 0; r < 3000; ++r) {
        i32 s[8];
        for (int i = 0; i < 8; ++i) s[i] = 0;
        for (int c = 0; c < control_count; ++c) {
            seed = seed * 1664525u + 1013904223u;
            s[controls[c].slot] = q(static_cast<int>(seed >> 25));
        }
        std::snprintf(where, sizeof where, "random setting %d", r);
        begin_run(s);
        step();
        note_on(1 + (r & 7));
        long sum = 0;
        int peak = 0;
        for (int t = 0; t < 60; ++t) { const int n = step(); sum += n; if (n > peak) peak = n; }
        note_off();
        for (int t = 0; t < 20; ++t) { const int n = step(); if (n > peak) peak = n; }
        const double mean = sum / 60.0;
        if (mean > worst_mean) { worst_mean = mean; std::snprintf(worst_where, sizeof worst_where, "%s", where); }
        if (peak > worst_peak) worst_peak = peak;
    }

    int total = 0;
    for (int k = 0; k < 8; ++k) {
        if (k != 5) total += faults[k];   // past the screen's edge it only clips
        if (faults[k]) std::printf("  %-14s %8d  first: %s\n", fault_names[k], faults[k], first_fault[k]);
    }
    if (!total) std::puts("  outlines: no faults in the sweep");

    // Churn, a held note's motion, stillness.
    i32 base[8];
    baseline_of(0, base);
    int late = 0;
    const Churn at_default = hold(base, 1, 3, &late);
    std::snprintf(where, sizeof where, "defaults, chord");
    const Churn chord = hold(base, 8, 3);
    std::printf("  churn: defaults mean %.1f peak %d; 8 notes mean %.1f peak %d; worst mean %.1f (%s), worst peak %d\n",
                at_default.mean, at_default.peak, chord.mean, chord.peak, worst_mean, worst_where, worst_peak);
    std::printf("  held note: moving in %d of the last 60 frames of a 3 s hold\n", late);
    // After release: how long until it is still; at rest: nothing.
    note_off();
    int to_still = 0, tail = 0;
    for (int t = 0; t < 600; ++t) if (step()) { to_still = t + 1; ++tail; }
    begin_run(base);
    int idle = 0;
    for (int t = 0; t < 300; ++t) idle += step();
    std::printf("  after release: still after %d frames (%.2f s); at rest: %d changed nodes in 5 s\n", to_still, to_still / 60.0, idle);

    // Reduced motion: the views' own motion stays still under a held note.
    begin_run(base);
    input.reduced_motion = 1;
    for (int t = 0; t < 10; ++t) step();
    note_on();
    int reduced = 0;
    for (int t = 0; t < 180; ++t) reduced += step();
    std::printf("  reduced motion: %d changed nodes over a 3 s hold\n", reduced);
    if (reduced) { std::puts("  FAIL: moves under reduced motion"); ++total; }

    // Each control's walk at rest, at defaults: how many of its 127 steps move
    // or recolour anything, the longest run that does not, and how far it goes.
    std::puts("  control  moving steps  grey-only  longest still  travel 0..127 px");
    static EngineFrame walk[128];
    for (int c = 0; c < control_count; ++c) {
        begin_run(base);
        int steps = 0, grey = 0, still = 0, longest = 0;
        for (int v = 0; v < 128; ++v) {
            i32 s[8];
            std::memcpy(s, base, sizeof s);
            s[controls[c].slot] = q(v);
            set(s);
            ENGINE::display(state, input, frame);
            walk[v] = frame;
            if (!v) continue;
            const double d = travel(walk[v], walk[v - 1]);
            bool shade = false;
            for (opxy::u32 i = 0; i < first_header; ++i)
                shade = shade || (shown(walk[v].nodes[i]) && walk[v].nodes[i].shade != walk[v - 1].nodes[i].shade);
            if (d > 0) ++steps;
            else if (shade) ++grey;
            if (d > 0 || shade) still = 0;
            else if (++still > longest) longest = still;
        }
        double most = 0;
        for (int v = 8; v < 128; v += 8) {
            const double d = travel(walk[v], walk[0]);
            if (d > most) most = d;
        }
        const double ends = travel(walk[127], walk[0]);
        if (ends > most) most = ends;
        std::printf("  %-7s %6d %10d %12d %14.1f\n", controls[c].name, steps, grey, longest, most);
    }
    return total ? 1 : 0;
}
