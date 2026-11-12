// MD E12's Shift rows in its own header, as the SDK's padMix and padControls
// give them (written by tools/make_engine.py). The first row is
// MACHINE (the machine under the title "machine"), CHOKE, PAN and
// LEVEL; the second FILTER, SRR, FX I and FX II. PAN reads as
// stock's -100..100 with 0 at the centre, LEVEL in whole dB as "-6dB",
// neither with a plus sign; a send at 0 follows the track's (TRK). Without
// Shift the header shows the key's controls as before. The captions' widths
// in the device font: tools/test_voices.py.
#include "../engine.cpp"
#include "../display.cpp"
#include <cstdio>
#include <cstring>

namespace {
int failures = 0;
mde12::DisplayState state;
mde12::Frame frame;

struct Pads { opxy::i32 filter, srr, fx1, fx2; };
void show(opxy::u32 model, opxy::u32 line, bool shift, opxy::i32 pan, opxy::i32 level, opxy::u32 choke, Pads pads = Pads()) {
    opxy::draw::Input input = opxy::draw::Input();
    input.model = model;
    for (opxy::u32 i = 0; i < 4; ++i) input.shown[i] = static_cast<opxy::u8>(line * 4 + i);
    input.shift = shift ? 1 : 0;
    // The drum kind's choices for a kit without groups: the model and the choke group.
    input.choices[0] = static_cast<opxy::u8>(model);
    input.choices[1] = static_cast<opxy::u8>(choke);
    input.pan = static_cast<opxy::i8>(pan);
    input.level = static_cast<opxy::i8>(level);
    input.line = static_cast<opxy::u8>(line);
    input.pad[0] = static_cast<opxy::i8>(pads.filter);
    input.pad[1] = static_cast<opxy::i8>(pads.srr);
    input.pad[2] = static_cast<opxy::i8>(pads.fx1);
    input.pad[3] = static_cast<opxy::i8>(pads.fx2);
    mde12::display_reset(state);
    mde12::display(state, input, frame);
}

void expect(const char* what, const char* shown, const char* wanted) {
    if (std::strcmp(shown, wanted) != 0) {
        std::printf("FAIL %s reads \"%s\", not \"%s\"\n", what, shown, wanted);
        ++failures;
    }
}
}

int main() {
    // The first row's Shift: the machine, the choke group,
    // then the key's pan and level.
    const struct { opxy::u32 model; const char *family, *name; } machines[] = {
        {0, "machine", "BD"}, {1, "machine", "SD"}, {2, "machine", "HT"}, {3, "machine", "LT"}, {4, "machine", "CP"}, {5, "machine", "RS"},
        {6, "machine", "CB"}, {7, "machine", "CH"}, {8, "machine", "OH"}, {9, "machine", "RC"}, {10, "machine", "CC"}, {11, "machine", "BR"},
        {12, "machine", "TA"}, {13, "machine", "TR"}, {14, "machine", "SH"}, {15, "machine", "BC"}};
    for (const auto& m : machines) {
        show(m.model, 0, true, 0, 0, 1);
        expect("MACHINE", frame.header[0], m.name);
        expect("MACHINE's title", frame.texts[0].chars, m.family);
        expect("CHOKE", frame.header[1], "1");
    }
    const struct { opxy::i32 value; const char* text; } pans[] = {
        {-100, "-100"}, {-40, "-40"}, {-1, "-1"}, {0, "0"}, {7, "7"}, {40, "40"}, {100, "100"}};
    const struct { opxy::i32 value; const char* text; } levels[] = {
        {-30, "-30dB"}, {-12, "-12dB"}, {-6, "-6dB"}, {-1, "-1dB"}, {0, "0dB"}};
    const char* const chokes[4] = {"none", "1", "2", "3"};
    for (const auto& pan : pans)
        for (const auto& level : levels)
            for (opxy::u32 choke = 0; choke < 4; ++choke) {
                show(0, 0, true, pan.value, level.value, choke);
                expect("CHOKE", frame.header[1], chokes[choke]);
                expect("PAN", frame.header[2], pan.text);
                expect("LEVEL", frame.header[3], level.text);
            }
    // Its titles, which the two-page header writes in lower case: the bright
    // row's are text slots 0..3.
    show(0, 0, true, 0, 0, 0);
    const char* const first[4] = {"machine", "choke", "pan", "level"};
    for (opxy::u32 c = 0; c < 4; ++c) expect("a first-row Shift title", frame.texts[c].chars, first[c]);
    // The second row's Shift: FILTER, SRR, FX I and FX II; every machine has
    // the row, one with four or fewer controls too.
    for (opxy::u32 m = 0; m < mde12::kit::models; ++m) {
        show(m, 1, true, -40, -6, 2);
        expect("FILTER at 0", frame.header[0], "OPEN");
        expect("SRR at 0", frame.header[1], "OFF");
        expect("FX I at 0", frame.header[2], "TRK");
        expect("FX II at 0", frame.header[3], "TRK");
    }
    const struct { Pads pads; const char *f, *s, *a, *b; } rows[] = {
        // A send is 0 (TRK) or its percent + 1 (SDK 1.15).
        {{-16, 15, 101, 1}, "L150", "689", "100", "0"}, {{-6, 4, 34, 0}, "L2.1k", "8.8k", "33", "TRK"},
        {{-1, 1, 0, 84}, "L8.0k", "22k", "TRK", "83"}, {{1, 2, 38, 2}, "H50", "15k", "37", "1"}, {{15, 9, 51, 0}, "H4.0k", "2.8k", "50", "TRK"}};
    for (const auto& row : rows) {
        show(0, 1, true, 0, 0, 1, row.pads);
        expect("FILTER", frame.header[0], row.f);
        expect("SRR", frame.header[1], row.s);
        expect("FX I", frame.header[2], row.a);
        expect("FX II", frame.header[3], row.b);
        char text[16];
        // The stock header's captions (pad_control_text) are the same.
        mde12::pad_control_text(0, row.pads.filter, text); expect("FILTER's stock caption", text, row.f);
        mde12::pad_control_text(1, row.pads.srr, text); expect("SRR's stock caption", text, row.s);
    }
    show(15, 1, true, 0, 0, 2);
    const char* const second[4] = {"filter", "srr", "fx i", "fx ii"};
    for (opxy::u32 c = 0; c < 4; ++c) expect("a second-row Shift title", frame.texts[c].chars, second[c]);
    // Both Shift rows at once, stacked as the pages: the edited row bright
    // (title slots 0..3, values 4..7), the other grey (titles 8..11, values
    // 12..15). On the first row the second's FILTER, SRR, FX I, FX II sit grey
    // under it; on the second, the first's MACHINE, CHOKE, PAN, LEVEL.
    show(0, 0, true, -40, -6, 2, Pads{-16, 15, 101, 0});
    for (opxy::u32 c = 0; c < 4; ++c) expect("the grey second-row Shift title", frame.texts[8 + c].chars, second[c]);
    const char* const grey_second[4] = {"L150", "689", "100", "TRK"};
    for (opxy::u32 c = 0; c < 4; ++c) expect("a grey second-row Shift value", frame.texts[12 + c].chars, grey_second[c]);
    show(0, 1, true, -40, -6, 2, Pads{-16, 15, 101, 0});
    for (opxy::u32 c = 0; c < 4; ++c) expect("the grey first-row Shift title", frame.texts[8 + c].chars, first[c]);
    const char* const grey_first[4] = {"BD", "2", "-40", "-6dB"};
    for (opxy::u32 c = 0; c < 4; ++c) expect("a grey first-row Shift value", frame.texts[12 + c].chars, grey_first[c]);
    for (opxy::u32 c = 0; c < 4; ++c) expect("a bright second-row Shift title", frame.texts[c].chars, second[c]);
    for (opxy::u32 c = 0; c < 4; ++c) expect("a bright second-row Shift value", frame.texts[4 + c].chars, grey_second[c]);
    // Without Shift the header shows the key's own controls, as before, on
    // either row, whatever its pad settings.
    for (opxy::u32 line = 0; line < 2; ++line) {
        show(0, line, false, -100, -30, 3, Pads{-16, 15, 101, 101});
        for (opxy::u32 c = 0; c < 4; ++c)
            if (!(frame.header[c][0] >= '0' && frame.header[c][0] <= '9')) {
                std::printf("FAIL E12-BD's header without Shift reads \"%s\"\n", frame.header[c]); ++failures;
            }
    }
    if (!failures) std::puts("PASS mde12 header");
    return failures ? 1 : 0;
}
