// Prints the frames an engine's display draws, as JSON, for tools/frame_svg.py
// to draw: the real screen, from the C++, on a machine that may build and run
// host code (CI). Each line on stdin is a setting, "model v1 .. v8 frames": a
// kit's machine (0 for a synth), the eight controls 0..127, and how many frames
// (60 a second) after a note struck at the first; 0 for the screen at rest.
// Each line out is that frame: every node's transform, shade and paint, and
// every path's points (Q10).
//
//   c++ -std=c++11 -I<sdk>/sdk/include -Imachines/<e> -DENGINE=<e> tools/frame_json.cpp -o local/frame_json
#include "display.cpp"
#include <cstdio>
#include <cstring>

int main() {
    using namespace opxy::draw;
    static ENGINE::DisplayState state;
    static ENGINE::Frame frame;
    unsigned model, frames;
    int v[8];
    while (std::scanf("%u %d %d %d %d %d %d %d %d %u", &model, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &frames) == 10) {
        Input input = Input();
        input.amp.attack = input.amp.decay = input.amp.sustain = input.amp.release = envelope_unavailable;
        for (int i = 0; i < 8; ++i) input.slots[i] = v[i] >= 127 ? 0x7fffffff : v[i] << 24;
        for (int i = 0; i < 4; ++i) { input.parameters[i] = input.slots[i]; input.shown[i] = static_cast<opxy::u8>(i); }
        input.track = 1;
        input.model = model;
        input.elapsed = 1024 / 60;
        ENGINE::display_reset(state);
        frame = ENGINE::Frame();
        ENGINE::display(state, input, frame);
        ENGINE::display(state, input, frame);
        if (frames) {
            input.note_serial++;
            input.held_notes = 1;
            for (unsigned t = 0; t < frames; ++t) ENGINE::display(state, input, frame);
        }
        std::printf("{\"model\": %u, \"values\": [%d, %d, %d, %d, %d, %d, %d, %d], \"frames\": %u, \"nodes\": [",
                    model, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], frames);
        for (opxy::u32 n = 0; n < ENGINE::Frame::node_count; ++n) {
            const Node& d = frame.nodes[n];
            std::printf("%s[%d, %d, %d, %d, %d, %u, %u]", n ? ", " : "", d.x, d.y, d.scale, d.cos, d.sin, d.shade, d.paint);
        }
        std::printf("], \"paths\": [");
        for (opxy::u32 p = 0; p < ENGINE::Frame::path_count; ++p) {
            std::printf("%s[", p ? ", " : "");
            for (opxy::u32 i = 0; i < ENGINE::Frame::point_count; ++i)
                std::printf("%s%d, %d", i ? ", " : "", frame.paths[p].points[i].x, frame.paths[p].points[i].y);
            std::printf("]");
        }
        std::printf("]}\n");
    }
    return 0;
}
