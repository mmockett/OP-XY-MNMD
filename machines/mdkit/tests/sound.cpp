// Checks for MD KIT, written by tools/make_engine.py: each key's stored
// machine and controls sound as that machine, within the declared peak; every
// machine stays bounded with its controls at both ends; the names fit, and
// the header leaves an unnamed slot blank.
// The voices' own checks: tools/test_voices.py.
#include "../engine.cpp"
#include "../display.cpp"
#include <cstdio>
#include <cstring>

namespace {
int failures = 0;
// Without the private samples a sampled kit is silent: its sounds go unchecked.
const bool captured = true;
// The adapter's whole path for one note (drum_voice.cpp.in): the played key's
// defaults, its stored model and controls over them, the track's eight offsets
// around 64 on the eight controls, clamped, then Q31.
void device(opxy::u32 key, const opxy::u8* stored_model, const opxy::u8* stored, const opxy::i32* track,
            opxy::u8& model, opxy::i32* values) {
    opxy::u8 raw[8];
    mdkit::drum_defaults(key, &model, raw);
    if (stored) { model = *stored_model; for (int c = 0; c < 8; ++c) raw[c] = stored[c]; }
    if (model >= mdkit::kit::models) mdkit::drum_defaults(key, &model, raw);
    for (int c = 0; c < 8; ++c) {
        opxy::i32 v = static_cast<opxy::i32>(raw[c]) + static_cast<opxy::i32>(static_cast<opxy::u32>(track[c]) >> 24) - 64;
        v = v < 0 ? 0 : v > 127 ? 127 : v;
        values[c] = v == 127 ? 0x7fffffff : v << 24;
    }
}
const opxy::i32 neutral[8] = {0x40000000, 0x40000000, 0x40000000, 0x40000000,
                              0x40000000, 0x40000000, 0x40000000, 0x40000000};
mdkit::Voice voice;
mdkit::Shared shared;
// Renders a note on the voice (fresh unless reused) and returns a fingerprint of its sound.
opxy::u32 play(opxy::u32 model, const opxy::i32* values, bool& bounded, bool& sounding, bool reuse = false) {
    opxy::i32 out[opxy::block_frames];
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = ::md::kit::middle_c;
    if (!reuse) {
        voice = mdkit::Voice();
        shared = mdkit::Shared();
        mdkit::initialize(voice, shared, 1);
    }
    mdkit::start(voice, shared, 1);
    sounding = false;
    bounded = true;
    opxy::u32 print = 0;
    for (unsigned b = 0; b < 690; ++b) {
        mdkit::drum_select(voice, shared, 1, model);
        mdkit::render(voice, shared, 1, out, values, pitch);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            sounding = sounding || out[i] != 0;
            bounded = bounded && out[i] >= -1073741824 && out[i] <= 1073741824;
            print = print * 31u + static_cast<opxy::u32>(out[i]);
        }
    }
    return print;
}
}

int main() {
    // Every key keeps its own stored machine: keys storing different machines
    // sound different, and a voice taken over by another key changes machine.
    {
        opxy::u32 prints[24];
        opxy::u8 models[24], stored[24][8];
        for (opxy::u32 k = 0; k < 24; ++k) {
            models[k] = static_cast<opxy::u8>((k * 5u) % mdkit::kit::models);
            for (int c = 0; c < 8; ++c) stored[k][c] = 64;
            opxy::u8 model;
            opxy::i32 values[8];
            device(53 + k, &models[k], stored[k], neutral, model, values);
            if (model != models[k]) { std::printf("FAIL key %u renders another key's machine\n", 53 + k); ++failures; }
            bool bounded, sounding;
            prints[k] = play(model, values, bounded, sounding);
        }
        for (opxy::u32 a = 0; a < 24; ++a)
            for (opxy::u32 b = a + 1; b < 24; ++b)
                if (captured && models[a] != models[b] && prints[a] == prints[b]) {
                    std::printf("FAIL keys %u and %u sound the same with different machines\n", 53 + a, 53 + b); ++failures;
                }
        opxy::u8 first_model, second_model;
        opxy::i32 first[8], second[8];
        device(53, &models[0], stored[0], neutral, first_model, first);
        device(54, &models[1], stored[1], neutral, second_model, second);
        bool bounded, sounding;
        play(first_model, first, bounded, sounding);
        play(second_model, second, bounded, sounding, true);
        if (voice.machine != models[1]) { std::printf("FAIL a reused voice keeps the previous key's machine\n"); ++failures; }
        // A step lock on control c moves that control only, and the model never.
        for (int c = 0; c < 8; ++c) {
            opxy::i32 locked[8];
            for (int i = 0; i < 8; ++i) locked[i] = neutral[i];
            locked[c] = 0x40000000 + (10 << 24);
            opxy::u8 model;
            opxy::i32 values[8];
            device(53, &models[0], stored[0], locked, model, values);
            for (int i = 0; i < 8; ++i)
                if ((static_cast<opxy::u32>(values[i]) >> 24) != (i == c ? 74u : 64u) || model != models[0]) {
                    std::printf("FAIL a lock on control %d reaches control %d\n", c, i); ++failures;
                }
        }
    }
    for (opxy::u32 key = 53; key < 77; ++key) {
        opxy::u8 model;
        opxy::i32 values[8];
        device(key, nullptr, nullptr, neutral, model, values);
        if (model != mdkit::kit::key_model[key - 53]) { std::printf("FAIL key %u defaults to another machine\n", key); ++failures; }
        bool bounded, sounding;
        play(model, values, bounded, sounding);
        if (captured && !sounding) { std::printf("FAIL key %u is silent\n", key); ++failures; }
        if (!bounded) { std::printf("FAIL key %u above the declared peak\n", key); ++failures; }
    }
    for (opxy::u32 m = 0; m < mdkit::kit::models; ++m) {
        const opxy::u32 count = mdkit::parameter_count(m);
        if (count < 1 || count > 8) { std::printf("FAIL machine %u has %u controls\n", m, count); ++failures; }
        for (opxy::u32 s = 0; s < 8; ++s) {
            const char* label = mdkit::parameter_name(m, s);
            if (std::strlen(label) > 8 || (s >= count && label[0]) || (s == 0 && !label[0]) ||
                !std::strncmp(label, "SPARE", 5) || !std::strncmp(label, "CTRL", 4)) {
                std::printf("FAIL machine %u slot %u label\n", m, s); ++failures;
            }
            // An unnamed slot is blank on the device and must do nothing: its
            // control at either end sounds as at the middle.
            if (label[0]) continue;
            opxy::i32 middle[8], moved[8];
            for (int c = 0; c < 8; ++c) middle[c] = moved[c] = 64 << 24;
            bool bounded, sounding;
            const opxy::u32 still = play(m, middle, bounded, sounding);
            for (int end = 0; end < 2; ++end) {
                moved[s] = end ? 0x7fffffff : 0;
                if (play(m, moved, bounded, sounding) != still) {
                    std::printf("FAIL %s's unnamed slot %u changes its sound\n", mdkit::model_name(m), s); ++failures;
                }
            }
        }
        for (int end = 0; end < 2; ++end) {
            opxy::i32 values[8];
            for (int c = 0; c < 8; ++c) values[c] = end ? 0x7fffffff : 0;
            bool bounded, sounding;
            play(m, values, bounded, sounding);
            if (!bounded) { std::printf("FAIL %s above the declared peak with its controls at %s\n", mdkit::model_name(m), end ? "127" : "0"); ++failures; }
        }
        // The screen's header: a number for each named slot, nothing for an unnamed one.
        for (opxy::u32 page = 0; page < 2; ++page) {
            static mdkit::DisplayState state;
            static mdkit::Frame frame;
            opxy::draw::Input input = opxy::draw::Input();
            input.model = m;
            for (opxy::u32 i = 0; i < 4; ++i) input.shown[i] = static_cast<opxy::u8>(page * 4 + i);
            mdkit::display_reset(state);
            mdkit::display(state, input, frame);
            for (opxy::u32 i = 0; i < 4; ++i) {
                const bool named = mdkit::parameter_name(m, page * 4 + i)[0] != 0;
                const char* shown = frame.header[i];
                if (named ? !(shown[0] >= '0' && shown[0] <= '9') : std::strcmp(shown, " ") != 0) {
                    std::printf("FAIL %s's header for slot %u reads \"%s\"\n", mdkit::model_name(m), page * 4 + i, shown); ++failures;
                }
            }
        }
    }
    if (!failures) std::puts("PASS mdkit");
    return failures ? 1 : 0;
}
