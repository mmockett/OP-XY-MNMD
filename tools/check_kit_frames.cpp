// Runs a drum kit's display on the host and checks every frame the device would
// draw: each machine with each control swept through all 128 values (the others
// at their middle: a nine-step sweep missed TRX-B2's beater collapsing at RAMP
// 80..82), all at 0 and all at 127, struck by a note, and arriving from
// every other machine. Beyond the SDK's rules it checks what the device's
// renderer does with an outline (OP-XY-Dev docs/screens.md): no outline without
// area and no turn straight back, neighbours at least 1/8 px apart, a closing
// edge of at least 1/4 px, and every point on the stage; it checks every frame
// with the SDK's own opxy::draw::valid (a path reaching past 128 px of its origin
// passes the rest and fails the device's); it counts the nodes each frame changes;
// and it walks each control through its range, failing one whose drawing stands
// still for more than a fifth of the walk (a dead span or a two-state switch).
//
//   c++ -std=c++11 -I<sdk>/sdk/include -Imachines/<kit> -DENGINE=<kit> tools/check_kit_frames.cpp -o local/check
#include "display.cpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
using namespace opxy::draw;
typedef ENGINE::Frame EngineFrame;
int failures = 0;
unsigned worst = 0, worst_switch = 0;
char worst_when[96], worst_switch_when[96];
bool switching = false;
const unsigned churn_limit = 30;
const double sharpest_floor = 2.5;   // degrees from straight back
double sharpest[64];
char sharpest_when[64][140];
int too_sharp = 0;
unsigned current = 0;

// A point of a shown path where the device draws it, in Q10 screen pixels.
void place_point(const Node& n, const Point& p, long long& x, long long& y) {
    const long long rx = (static_cast<long long>(p.x) * n.cos - static_cast<long long>(p.y) * n.sin) >> 10;
    const long long ry = (static_cast<long long>(p.x) * n.sin + static_cast<long long>(p.y) * n.cos) >> 10;
    x = n.x + ((rx * n.scale) >> 10);
    y = n.y + ((ry * n.scale) >> 10);
}

bool fail(const char* when, const char* what, unsigned path) {
    std::printf("%s: path %u %s\n", when, path, what);
    ++failures;
    return true;
}

bool check(const EngineFrame& f, const EngineFrame& before, const char* when, bool counted = true) {
    if (!valid(f)) { std::printf("%s: not a valid frame (opxy::draw::valid)\n", when); ++failures; return false; }
    for (opxy::u32 p = 0; p < EngineFrame::path_count; ++p) {
        if (f.nodes[p].x >= hidden_x * unit) continue;
        Point xy[EngineFrame::point_count];
        opxy::u32 m = 0;
        for (opxy::u32 i = 0; i < EngineFrame::point_count; ++i) {
            const Point& q = f.paths[p].points[i];
            if (!m || q.x != xy[m - 1].x || q.y != xy[m - 1].y) xy[m++] = q;
        }
        for (opxy::u32 i = 0; i + 1 < m; ++i)
            if (md::view::near(xy[i], xy[i + 1]) && !fail(when, "has neighbouring points within 1/8 px", p)) return false;
        const opxy::i32 dx = xy[0].x - xy[m - 1].x, dy = xy[0].y - xy[m - 1].y;
        const opxy::i32 closing = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
        if (m > 2 && closing < 2 * md::view::apart && !fail(when, "closes over less than 1/4 px", p)) return false;
        // Area (shoelace, px^2) and turns straight back.
        long long twice = 0;
        for (opxy::u32 i = 0; i < m; ++i) {
            const Point& a = xy[i];
            const Point& b = xy[(i + 1) % m];
            twice += static_cast<long long>(a.x) * b.y - static_cast<long long>(b.x) * a.y;
            const Point& c = xy[(i + 2) % m];
            const long long ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
            if (m > 2 && ux * vy - uy * vx == 0 && ux * vx + uy * vy < 0 && !fail(when, "turns straight back", p)) return false;
            // A stroke's mitre runs far out at a turn nearly straight back.
            if (m > 2 && ux * vx + uy * vy < 0) {
                const double angle = std::atan2(std::fabs(static_cast<double>(ux * vy - uy * vx)), -static_cast<double>(ux * vx + uy * vy)) * 180 / M_PI;
                if (angle < sharpest[current]) { sharpest[current] = angle; std::snprintf(sharpest_when[current], sizeof sharpest_when[0], "path %u, %s", p, when); }
                if (angle < sharpest_floor) ++too_sharp;
            }
        }
        const double area = (twice < 0 ? -twice : twice) / 2.0 / (unit * unit);
        if (m > 2 && area < 0.05 && !fail(when, "has no area", p)) return false;
        for (opxy::u32 i = 0; i < m; ++i) {
            long long x, y;
            place_point(f.nodes[p], xy[i], x, y);
            if (x < 0 || x > 480LL * unit || y < static_cast<long long>(md::view::sy(0)) || y > static_cast<long long>(md::view::sy(160))) {
                char what[96];
                std::snprintf(what, sizeof what, "point %u at (%lld, %lld) px, off the stage", i, x / unit, y / unit);
                if (!fail(when, what, p)) return false;
                break;
            }
        }
    }
    unsigned changed = 0;
    for (opxy::u32 i = 0; i < EngineFrame::node_count; ++i)
        changed += std::memcmp(&f.nodes[i], &before.nodes[i], sizeof f.nodes[i]) != 0;
    for (opxy::u32 i = 0; i < EngineFrame::path_count; ++i)
        changed += std::memcmp(&f.paths[i], &before.paths[i], sizeof f.paths[i]) != 0 &&
                   !std::memcmp(&f.nodes[i], &before.nodes[i], sizeof f.nodes[i]);
    if (counted && switching && changed > worst_switch) {
        worst_switch = changed; std::snprintf(worst_switch_when, sizeof worst_switch_when, "%s", when);
    } else if (counted && !switching && changed > worst) { worst = changed; std::snprintf(worst_when, sizeof worst_when, "%s", when); }
    return true;
}

ENGINE::DisplayState state;
EngineFrame frame, previous;

// Frames of one machine at its settings: a note struck at frame 2 and held for
// 20 frames, then its release, 60 frames in all. A run that starts from another
// machine counts its first frame as the switch.
bool run(opxy::u32 model, const opxy::i32* slots, const char* setting, int from) {
    Input input = Input();
    for (int i = 0; i < 8; ++i) input.slots[i] = slots[i];
    for (int i = 0; i < 4; ++i) { input.parameters[i] = slots[i]; input.shown[i] = static_cast<opxy::u8>(i); }
    input.track = 1;
    input.elapsed = 1024 / 60;
    if (from < 0) { ENGINE::display_reset(state); frame = EngineFrame(); }
    char when[96];
    current = model;
    for (int t = 0; t < 60; ++t) {
        input.model = model;
        if (t == 2) { input.note_serial++; input.held_notes = 1; }
        if (t == 22) { input.held_notes = 0; input.note_off_serial++; }
        previous = frame;
        ENGINE::display(state, input, frame);
        if (from >= 0)
            std::snprintf(when, sizeof when, "%s from %s, %s, frame %d", ENGINE::kit::model[model].name,
                          ENGINE::kit::model[from].name, setting, t);
        else
            std::snprintf(when, sizeof when, "%s (%u), %s, frame %d", ENGINE::kit::model[model].name, model, setting, t);
        // A reset's first frame draws everything; the device shows it once.
        switching = from >= 0;
        if (!check(frame, previous, when, from >= 0 || t > 0)) return false;
        if (from >= 0 && t == 0) break;   // the switch frame only
    }
    return true;
}
}

// Whether anything drawn differs between two frames: a node shown or hidden, moved,
// scaled, turned or recoloured, or a shown path reshaped.
bool differs(const EngineFrame& a, const EngineFrame& b) {
    for (opxy::u32 i = 0; i < EngineFrame::node_count; ++i) {
        const Node& n = a.nodes[i];
        const Node& m = b.nodes[i];
        const bool shown = n.x < hidden_x * unit, was = m.x < hidden_x * unit;
        if (shown != was) return true;
        if (!shown) continue;
        if (n.x != m.x || n.y != m.y || n.scale != m.scale || n.cos != m.cos || n.sin != m.sin || n.shade != m.shade) return true;
        if (i < EngineFrame::path_count && a.paths[i].key != b.paths[i].key) return true;
    }
    return false;
}

// Walks a machine's control through its range in steps of 8, the others at their
// middle, at rest: a player turning it must see the drawing move.
void walk(opxy::u32 model, int control) {
    static ENGINE::DisplayState state;
    static EngineFrame now, last;
    unsigned steps = 0, still = 0;
    for (int v = 0; v <= 127; v += 8) {
        Input input = Input();
        for (int i = 0; i < 8; ++i) input.slots[i] = 64 << 24;
        input.slots[control] = v << 24;
        for (int i = 0; i < 4; ++i) { input.parameters[i] = input.slots[i]; input.shown[i] = static_cast<opxy::u8>(i); }
        input.model = model;
        input.track = 1;
        input.elapsed = 1024 / 60;
        ENGINE::display_reset(state);
        now = EngineFrame();
        ENGINE::display(state, input, now);
        if (v) { ++steps; if (!differs(now, last)) ++still; }
        last = now;
    }
    if (still * 5 > steps) {
        std::printf("%s (%u), %s: %u of %u steps change nothing in the drawing\n", ENGINE::kit::model[model].name, model,
                    ENGINE::kit::model[model].labels[control], still, steps);
        ++failures;
    }
}

int main() {
    for (int i = 0; i < 64; ++i) sharpest[i] = 180;
    const opxy::i32 middle = 64 << 24;
    for (opxy::u32 m = 0; m < ENGINE::kit::models; ++m) {
        opxy::i32 slots[8];
        for (int end = 0; end < 2; ++end) {
            for (int i = 0; i < 8; ++i) slots[i] = end ? 0x7fffffff : 0;
            run(m, slots, end ? "all at 127" : "all at 0", -1);
        }
        for (int c = 0; c < 8; ++c)
            for (opxy::u32 v = 0; v < 128; ++v) {
                for (int i = 0; i < 8; ++i) slots[i] = middle;
                slots[c] = v >= 127 ? 0x7fffffff : static_cast<opxy::i32>(v << 24);
                char setting[32];
                std::snprintf(setting, sizeof setting, "control %d at %u", c + 1, v);
                run(m, slots, setting, -1);
            }
        for (int i = 0; i < 8; ++i) slots[i] = middle;
        for (opxy::u32 from = 0; from < ENGINE::kit::models; ++from) {
            if (from == m) continue;
            run(from, slots, "middle", -1);
            run(m, slots, "middle", static_cast<int>(from));
        }
        for (int c = 0; c < 8; ++c)
            if (ENGINE::kit::model[m].labels[c][0]) walk(m, c);
    }
    std::printf("%s: %u machines checked; most nodes changed in a frame while playing: %u (%s); "
                "on a switch: %u (%s)\n", failures ? "FAIL" : "PASS", ENGINE::kit::models, worst, worst_when,
                worst_switch, worst_switch_when);
    if (worst > churn_limit) std::printf("WARN: more than %u nodes changed in a frame while playing\n", churn_limit);
    for (opxy::u32 m = 0; m < ENGINE::kit::models; ++m)
        std::printf("sharpest %-3s (%2u): %6.2f  %s\n", ENGINE::kit::model[m].name, m, sharpest[m], sharpest_when[m]);
    std::printf("%d turns under %.1f degrees from straight back\n", too_sharp, sharpest_floor);
    if (too_sharp) ++failures;
    return failures ? 1 : 0;
}
