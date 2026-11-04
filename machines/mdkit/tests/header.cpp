// MD KIT's Shift rows in its own header, as the SDK's padMix gives them: the
// first row KIT, MACHINE, PAN and LEVEL, the second CHOKE and three blanks.
// PAN reads as stock's -100..100 with 0 at the centre, LEVEL in whole dB as
// "-6dB"; neither carries a plus sign. Without Shift the header shows the
// key's controls as before. The captions' widths in the device font:
// tools/test_voices.py.
#include "../engine.cpp"
#include "../display.cpp"
#include <cstdio>
#include <cstring>

namespace {
int failures = 0;
mdkit::DisplayState state;
mdkit::Frame frame;

void show(opxy::u32 model, opxy::u32 line, bool shift, opxy::i32 pan, opxy::i32 level, opxy::u32 choke) {
    opxy::draw::Input input = opxy::draw::Input();
    input.model = model;
    for (opxy::u32 i = 0; i < 4; ++i) input.shown[i] = static_cast<opxy::u8>(line * 4 + i);
    input.shift = shift ? 1 : 0;
    // The drum kind's choices for a kit with groups: the family (TRX here),
    // the model's place in it and the choke group.
    input.choices[0] = 0;
    input.choices[2] = static_cast<opxy::u8>(choke);
    input.pan = static_cast<opxy::i8>(pan);
    input.level = static_cast<opxy::i8>(level);
    input.line = static_cast<opxy::u8>(line);
    mdkit::display_reset(state);
    mdkit::display(state, input, frame);
}

void expect(const char* what, const char* shown, const char* wanted) {
    if (std::strcmp(shown, wanted) != 0) {
        std::printf("FAIL %s reads \"%s\", not \"%s\"\n", what, shown, wanted);
        ++failures;
    }
}
}

int main() {
    // The first row's Shift: the family, the machine, then the key's pan and level.
    const struct { opxy::i32 value; const char* text; } pans[] = {
        {-100, "-100"}, {-40, "-40"}, {-1, "-1"}, {0, "0"}, {7, "7"}, {40, "40"}, {100, "100"}};
    const struct { opxy::i32 value; const char* text; } levels[] = {
        {-30, "-30dB"}, {-12, "-12dB"}, {-6, "-6dB"}, {-1, "-1dB"}, {0, "0dB"}};
    for (const auto& pan : pans)
        for (const auto& level : levels) {
            show(0, 0, true, pan.value, level.value, 0);
            expect("KIT", frame.header[0], "TRX");
            expect("MACHINE", frame.header[1], mdkit::model_name(0));
            expect("PAN", frame.header[2], pan.text);
            expect("LEVEL", frame.header[3], level.text);
        }
    // Its titles, which the two-page header writes in lower case: the bright
    // row's are text slots 0..3.
    show(0, 0, true, 0, 0, 0);
    const char* const first[4] = {"kit", "machine", "pan", "level"};
    for (opxy::u32 c = 0; c < 4; ++c) expect("a first-row Shift title", frame.texts[c].chars, first[c]);
    // The second row's Shift: the choke group, then three blanks; every
    // machine has the row, one with four or fewer controls too (GND-NS has one).
    const char* const chokes[4] = {"none", "1", "2", "3"};
    for (opxy::u32 m = 0; m < mdkit::kit::models; ++m)
        for (opxy::u32 choke = 0; choke < 4; ++choke) {
            show(m, 1, true, -40, -6, choke);
            expect("CHOKE", frame.header[0], chokes[choke]);
            for (opxy::u32 c = 1; c < 4; ++c) expect("a free second-row Shift cell", frame.header[c], "");
        }
    show(31, 1, true, 0, 0, 2);
    const char* const second[4] = {"choke", "", "", ""};
    for (opxy::u32 c = 0; c < 4; ++c) expect("a second-row Shift title", frame.texts[c].chars, second[c]);
    // Without Shift the header shows the key's own controls, as before, on
    // either row, whatever its pan and level.
    for (opxy::u32 line = 0; line < 2; ++line) {
        show(0, line, false, -100, -30, 3);
        for (opxy::u32 c = 0; c < 4; ++c)
            if (!(frame.header[c][0] >= '0' && frame.header[c][0] <= '9')) {
                std::printf("FAIL TRX-BD's header without Shift reads \"%s\"\n", frame.header[c]); ++failures;
            }
    }
    return failures ? 1 : 0;
}
