// A check of GND-SIN's engine, written by tools/make_engine.py: it sounds at
// middle C, stays within the declared peak at its controls' ends and across
// four octaves. The voice's fitted checks: voices/tests/gnd_sin.cpp.
// Built with the data captured from the owner's ROM.
#include "../engine.cpp"
#include <cstdio>

int main() {
    static mmsin::Voice voice;
    static mmsin::Shared shared;
    opxy::i32 out[opxy::block_frames];
    int failures = 0;
    for (int octave = -2; octave <= 2; ++octave) {
        opxy::u32 pitch[opxy::block_frames];
        for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = octave < 0 ? 25479004u >> -octave : 25479004u << octave;
        for (int extreme = 0; extreme < 3; ++extreme) {
            opxy::i32 p[8];
            for (int i = 0; i < 8; ++i) p[i] = extreme == 0 ? 0x40000000 : extreme == 1 ? 0 : 0x7fffffff;
            mmsin::initialize(voice, shared, 1);
            mmsin::start(voice, shared, 1);
            bool sounding = false, bounded = true;
            for (unsigned b = 0; b < 690; ++b) {
                mmsin::render(voice, shared, 1, out, p, pitch);
                for (unsigned i = 0; i < opxy::block_frames; ++i) {
                    sounding = sounding || out[i] != 0;
                    bounded = bounded && out[i] >= -1073741824 && out[i] <= 1073741824;
                }
            }
            if (!bounded) { std::printf("FAIL octave %d, setting %d, above the declared peak\n", octave, extreme); ++failures; }
            if (true && extreme == 0 && octave == 0 && !sounding) { std::puts("FAIL silent at the middle setting"); ++failures; }
        }
    }
    if (!failures) std::puts("PASS mmsin");
    return failures ? 1 : 0;
}
