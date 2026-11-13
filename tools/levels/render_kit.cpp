// Renders each default key of a drum kit on the SDK's drum kind, one hit at a
// time, as a 32-bit float mono WAV at 44.1 kHz (1.0 = Q31 full scale): the
// key's stored machine and controls, neutral pad settings, 2 s.
//
//   c++ -std=c++11 -O2 -I <sdk>/sdk/include -DENGINE='"<kit>/engine.cpp"' -DNS=<id> render_kit.cpp -o render-<id>
//   render-<id> <folder>      writes <folder>/key-53.wav .. key-76.wav
#include ENGINE
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
const unsigned rate = 44100, blocks = 1380;   // 2 s

void write_float_wav(const std::string& path, const std::vector<float>& x) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); std::exit(1); }
    const unsigned bytes = unsigned(x.size()) * 4;
    auto u32 = [&](unsigned v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](unsigned v) { const unsigned short s = (unsigned short)v; std::fwrite(&s, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(3); u16(1); u32(rate); u32(rate * 4); u16(4); u16(32);
    std::fwrite("data", 1, 4, f); u32(bytes);
    std::fwrite(x.data(), 4, x.size(), f);
    std::fclose(f);
}

NS::Voice voice;
NS::Shared shared;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: render-kit <folder>\n"); return 2; }
    const std::string folder = argv[1];
    for (opxy::u32 k = 0; k < 24; ++k) {
        opxy::u8 model, raw[8];
        NS::drum_defaults(53 + k, &model, raw);
        opxy::i32 values[8];
        for (int c = 0; c < 8; ++c) values[c] = raw[c] == 127 ? 0x7fffffff : static_cast<opxy::i32>(raw[c]) << 24;
        voice = NS::Voice();
        shared = NS::Shared();
        NS::initialize(voice, shared, 1);
        NS::start(voice, shared, 1);
        opxy::i32 out[opxy::block_frames];
        opxy::u32 pitch[opxy::block_frames];
        for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = ::md::kit::middle_c;
        std::vector<float> x;
        x.reserve(blocks * opxy::block_frames);
        double peak = 0;
        for (unsigned b = 0; b < blocks; ++b) {
            NS::drum_select(voice, shared, 1, model);
            NS::render(voice, shared, 1, out, values, pitch);
            for (unsigned i = 0; i < opxy::block_frames; ++i) {
                const double v = out[i] / 2147483648.0;
                x.push_back(float(v));
                if (std::fabs(v) > peak) peak = std::fabs(v);
            }
        }
        char name[64];
        std::snprintf(name, sizeof name, "/key-%u.wav", 53 + k);
        write_float_wav(folder + name, x);
        std::printf("{\"key\": %u, \"model\": \"%s\", \"peak\": %.4f}\n", 53 + k, NS::model_name(model), peak);
    }
    return 0;
}
