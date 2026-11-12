// Prints the frames an engine's display draws, as JSON, for tools/frame_svg.py
// to draw: the real screen, from the C++, on a machine that may build and run
// host code (CI). Each line on stdin is a setting, "model v1 .. v8 frames
// [shift line c0 c1 c2]": a kit's machine (0 for a synth), the eight controls
// 0..127, how many frames (60 a second) after a note struck at the first (0 for
// the screen at rest), and optionally whether Shift is held, the row the
// encoders are on (0 or 1) and a kit's choices (its model group, model and
// choke group, or without groups its model and choke group).
// Each line out is that frame: every node's transform, shade and paint, every
// path's points (Q10), and every text slot's place, scale and characters.
//
//   c++ -std=c++11 -I<sdk>/sdk/include -Imachines/<e> -DENGINE=<e> tools/frame_json.cpp -o local/frame_json
#include "display.cpp"
#include <cstdio>
#include <cstring>

int main() {
    using namespace opxy::draw;
    static ENGINE::DisplayState state;
    static ENGINE::Frame frame;
    unsigned model, frames, shift, line, choices[3];
    int v[8];
    char setting[256];
    while (std::fgets(setting, sizeof setting, stdin)) {
        shift = line = choices[0] = choices[1] = choices[2] = 0;
        if (std::sscanf(setting, "%u %d %d %d %d %d %d %d %d %u %u %u %u %u %u", &model, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5],
                        &v[6], &v[7], &frames, &shift, &line, &choices[0], &choices[1], &choices[2]) < 10) break;
        Input input = Input();
        input.amp.attack = input.amp.decay = input.amp.sustain = input.amp.release = envelope_unavailable;
        for (int i = 0; i < 8; ++i) input.slots[i] = v[i] >= 127 ? 0x7fffffff : v[i] << 24;
        line = line ? 1 : 0;
        for (int i = 0; i < 4; ++i) { input.parameters[i] = input.slots[4 * line + i]; input.shown[i] = static_cast<opxy::u8>(4 * line + i); }
        // A drum kit's Shift, with its mixer at the defaults (pan centred, 0 dB,
        // every pad field neutral).
        input.shift = shift ? 1 : 0;
        input.line = static_cast<opxy::u8>(line);
        for (int i = 0; i < 3; ++i) input.choices[i] = static_cast<opxy::u8>(choices[i]);
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
        std::printf("], \"texts\": [");
        for (opxy::u32 s = 0; s < ENGINE::Frame::text_count; ++s) {
            const Text& x = frame.texts[s];
            std::printf("%s[%d, %d, %d, \"", s ? ", " : "", x.x, x.y, x.scale);
            for (opxy::u32 i = 0; i < text_capacity && x.chars[i]; ++i)
                if (x.chars[i] >= ' ' && x.chars[i] != '"' && x.chars[i] != '\\') std::putchar(x.chars[i]);
            std::printf("\"]");
        }
        std::printf("]}\n");
    }
    return 0;
}
