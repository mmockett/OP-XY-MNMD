// Runs an engine's display on the host and says which part of the frame breaks
// the SDK's rules (opxy::draw::valid only says that one does), across each
// member, the controls at their middles and ends, and a hit's animation.
//
//   c++ -std=c++11 -I../open-xy/sdk/include -Imachines/<engine> -DENGINE=<engine> tools/check_frame.cpp -o local/check
#include "display.cpp"
#include <cmath>
#include <cstdio>

namespace {
using namespace opxy::draw;
typedef ENGINE::Frame EngineFrame;
int failures = 0;

void report(const EngineFrame& f, const char* when) {
    for (opxy::u32 p = 0; p < EngineFrame::path_count; ++p) {
        if (!f.paths[p].key) { std::printf("%s: path %u has no key\n", when, p); ++failures; }
        for (opxy::u32 i = 0; i < EngineFrame::point_count; ++i) {
            const Point& q = f.paths[p].points[i];
            if (q.x < -path_extent * unit || q.x > path_extent * unit || q.y < -path_extent * unit || q.y > path_extent * unit) {
                std::printf("%s: path %u point %u at (%d, %d) Q10, beyond +-64 px\n", when, p, i, q.x, q.y); ++failures; break;
            }
        }
        // What the device does with a shown outline, beyond the SDK's rules: it
        // merges neighbouring points within an eighth of a pixel, and draws a
        // closing edge under a quarter of a pixel as a long sliver.
        if (f.nodes[p].x >= hidden_x * unit) continue;
        Point xy[EngineFrame::point_count];
        opxy::u32 m = 0;
        for (opxy::u32 i = 0; i < EngineFrame::point_count; ++i) {
            const Point& q = f.paths[p].points[i];
            if (!m || q.x != xy[m - 1].x || q.y != xy[m - 1].y) xy[m++] = q;   // a repeated last point pads
        }
        for (opxy::u32 i = 0; i + 1 < m; ++i)
            if (md::view::near(xy[i], xy[i + 1])) { std::printf("%s: path %u points %u and %u merge\n", when, p, i, i + 1); ++failures; break; }
        // A stroke's mitre runs far out at a turn nearly straight back; Wane's 2.57 degrees is the working floor.
        for (opxy::u32 i = 0; i < m && m > 2; ++i) {
            const Point& a = xy[i];
            const Point& b = xy[(i + 1) % m];
            const Point& c = xy[(i + 2) % m];
            const double ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
            if (ux * vx + uy * vy >= 0) continue;
            const double angle = std::atan2(std::fabs(ux * vy - uy * vx), -(ux * vx + uy * vy)) * 180 / M_PI;
            if (angle < 2.5) { std::printf("%s: path %u turns %.2f degrees from straight back\n", when, p, angle); ++failures; break; }
        }
        const opxy::i32 dx = xy[0].x - xy[m - 1].x, dy = xy[0].y - xy[m - 1].y;
        const opxy::i32 closing = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
        if (m > 2 && closing < 2 * md::view::apart) { std::printf("%s: path %u closes over %d Q10\n", when, p, closing); ++failures; }
    }
    for (opxy::u32 i = 0; i < EngineFrame::node_count; ++i) {
        const Node& n = f.nodes[i];
        if (!placement(n.x, n.y, n.scale)) { std::printf("%s: node %u placed at (%d, %d) scale %d\n", when, i, n.x / unit, n.y / unit, n.scale); ++failures; }
        if (!rotation_valid(n.cos, n.sin)) { std::printf("%s: node %u turned by (%d, %d)\n", when, i, n.cos, n.sin); ++failures; }
        if (n.shade >= EngineFrame::palette_count) { std::printf("%s: node %u shade %u\n", when, i, n.shade); ++failures; }
        if (n.paint > fill_and_stroke) { std::printf("%s: node %u paint %u\n", when, i, n.paint); ++failures; }
    }
    for (opxy::u32 i = 0; i < opxy::page_controls; ++i)
        if (!terminated(f.header[i], header_capacity)) { std::printf("%s: header %u not terminated\n", when, i); ++failures; }
}
}

int main() {
    static ENGINE::DisplayState state;
    static EngineFrame frame;
    const opxy::i32 settings[3] = {0x40000000, 0, 0x7fffffff};
    for (opxy::u32 member = 0; member < 32; ++member) {
        for (int s = 0; s < 3; ++s) {
            Input input = {};
            for (int i = 0; i < 8; ++i) input.slots[i] = settings[s];
            input.slots[7] = static_cast<opxy::i32>(member * 0x04000000u);   // MACH sweeps the members of a family
            input.model = member & 15;                                         // and a drum kit's machines
            for (int i = 0; i < 4; ++i) { input.parameters[i] = input.slots[i]; input.shown[i] = static_cast<opxy::u8>(i); }
            input.track = 1;
            input.held_notes = 0xffffffffu;
            ENGINE::display_reset(state);
            char when[64];
            for (int t = 0; t < 80; ++t) {
                input.elapsed = 30;
                if (t == 2) input.note_serial = 1;   // a hit
                ENGINE::display(state, input, frame);
                std::snprintf(when, sizeof when, "member %u setting %d frame %d", member, s, t);
                const int before = failures;
                report(frame, when);
                if (failures > before) return 1;
            }
        }
    }
    std::puts("frames valid");
    return 0;
}
