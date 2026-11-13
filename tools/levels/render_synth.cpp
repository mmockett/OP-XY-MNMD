// One host harness for every OP-XY synth machine: plays a scene (a preset's
// controls and a list of notes) through a machine's engine.cpp and writes a
// 32-bit float mono WAV at the SDK's rate (44.1 kHz), 1.0 = Q31 full scale.
//
// Build it once per machine, naming the engine and its namespace:
//   $(brew --prefix llvm)/bin/clang++ -std=c++11 -O2 -I <sdk>/sdk/include \
//     -DENGINE='"<machine>/engine.cpp"' -DNS=<id> render_synth.cpp -o render-<id>
//   render-<id> < scene.txt
//
// The scene, one item a line:
//   out <path>                 the WAV to write
//   seconds <s>                its length (whole 64-sample blocks)
//   attack <s>  release <s>    the amp envelope (see below)
//   params <eight values 0..1> the controls, as definition.json stores them
//   note <on s> <off s> <MIDI> [velocity 0..1]
//
// One voice per note: a free voice (destroy, initialize, start, as the SDK's
// browser preview does), else the oldest note's. A note on a key that is
// still releasing gets a voice of its own, so the old one rings out. Eight
// voices, owners 1..8. A taken voice renders one last block fading to
// silence, as stock does, and the new note starts on the next block.
//
// The amp envelope stands in for the OP-XY's, which runs after render: a
// linear attack, full sustain while the key is held, then an exponential
// release reaching -60 dB at <release> seconds; the voice is destroyed at
// -80 dB. Voices are summed at unity, then a 5 Hz DC blocker. No limiting.
#include ENGINE
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
const unsigned rate = 44100, block = 64, voices_n = 8;

struct Note { double on, off, midi, velocity; };

void write_float_wav(const std::string& path, const std::vector<float>& x, unsigned channels) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); std::exit(1); }
    const unsigned bytes = unsigned(x.size()) * 4;
    auto u32 = [&](unsigned v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](unsigned v) { const unsigned short s = (unsigned short)v; std::fwrite(&s, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(3); u16(channels); u32(rate); u32(rate * 4 * channels); u16(4 * channels); u16(32);
    std::fwrite("data", 1, 4, f); u32(bytes);
    std::fwrite(x.data(), 4, x.size(), f);
    std::fclose(f);
}

opxy::u32 increment(double midi) {
    const double hz = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
    return opxy::u32(hz / rate * 4294967296.0 + 0.5);
}

NS::Voice voices[voices_n];
NS::Shared shared;
}

int main() {
    std::string out = "out.wav";
    double seconds = 6, attack = 0.005, release = 0.5, params[8] = {0};
    std::vector<Note> notes;
    char word[64];
    char line[4096];
    while (std::fgets(line, sizeof line, stdin)) {
        if (std::sscanf(line, "%63s", word) != 1 || word[0] == '#') continue;
        const char* rest = std::strstr(line, word) + std::strlen(word);
        if (!std::strcmp(word, "out")) { char path[2048]; if (std::sscanf(rest, "%2047s", path) == 1) out = path; }
        else if (!std::strcmp(word, "seconds")) std::sscanf(rest, "%lf", &seconds);
        else if (!std::strcmp(word, "attack")) std::sscanf(rest, "%lf", &attack);
        else if (!std::strcmp(word, "release")) std::sscanf(rest, "%lf", &release);
        else if (!std::strcmp(word, "params"))
            std::sscanf(rest, "%lf %lf %lf %lf %lf %lf %lf %lf", &params[0], &params[1], &params[2], &params[3], &params[4],
                        &params[5], &params[6], &params[7]);
        else if (!std::strcmp(word, "note")) {
            Note n = {0, 0, 0, 1};
            if (std::sscanf(rest, "%lf %lf %lf %lf", &n.on, &n.off, &n.midi, &n.velocity) >= 3) notes.push_back(n);
        }
    }
    opxy::i32 p[8];
    for (int i = 0; i < 8; ++i) p[i] = opxy::i32(std::lround(std::fmin(1.0, std::fmax(0.0, params[i])) * 2147483647.0));

    std::memset(static_cast<void*>(voices), 0, sizeof voices);
    std::memset(static_cast<void*>(&shared), 0, sizeof shared);
    int playing[voices_n];          // the note each voice plays, or -1
    bool fading[voices_n];          // taken: one last block fading to silence
    int pending[voices_n];          // the note waiting for a fading voice, or -1
    double gain[voices_n], born[voices_n];
    bool ever[voices_n];
    for (unsigned v = 0; v < voices_n; ++v) { playing[v] = -1; fading[v] = false; pending[v] = -1; gain[v] = 0; born[v] = 0; ever[v] = false; }
    std::vector<bool> started(notes.size(), false);
    const unsigned blocks = unsigned(seconds * rate / block), total = blocks * block;
    std::vector<float> mix(total, 0.f);
    const double fall = std::exp(-6.907755 / (std::fmax(release, 0.002) * rate));
    const double rise = 1.0 / (std::fmax(attack, 0.0005) * rate);
    unsigned steals = 0, most = 0;
    double peak = 0;
    double x1 = 0, y1 = 0;
    const double r = 1 - 2 * M_PI * 5.0 / rate;

    auto begin = [&](unsigned v, int n) {
        if (ever[v]) NS::destroy(voices[v], shared, v + 1);
        NS::initialize(voices[v], shared, v + 1);
        NS::start(voices[v], shared, v + 1);
        ever[v] = true;
        playing[v] = n; gain[v] = 0; born[v] = notes[n].on; fading[v] = false;
    };

    for (unsigned b = 0; b < blocks; ++b) {
        const double t = double(b) * block / rate;
        // A fading voice has rendered its last block: its new note starts now.
        for (unsigned v = 0; v < voices_n; ++v)
            if (fading[v] && pending[v] >= 0 && playing[v] < 0) { begin(v, pending[v]); pending[v] = -1; }
        for (unsigned n = 0; n < notes.size(); ++n) {
            if (started[n] || t + 1e-9 < notes[n].on) continue;
            started[n] = true;
            unsigned v = voices_n;
            for (unsigned u = 0; u < voices_n && v == voices_n; ++u) if (playing[u] < 0 && pending[u] < 0) v = u;
            if (v < voices_n) { begin(v, int(n)); continue; }
            // Take the oldest note's voice: it fades over one block.
            v = voices_n;
            for (unsigned u = 0; u < voices_n; ++u)
                if (!fading[u] && pending[u] < 0 && (v == voices_n || born[u] < born[v])) v = u;
            if (v == voices_n) { std::fprintf(stderr, "no voice for note %u\n", n); continue; }
            fading[v] = true; pending[v] = int(n); ++steals;
        }
        unsigned sounding = 0;
        double out[block] = {0};
        for (unsigned v = 0; v < voices_n; ++v) {
            if (playing[v] < 0) continue;
            ++sounding;
            const Note& n = notes[playing[v]];
            opxy::u32 pitch[block];
            const opxy::u32 inc = increment(n.midi);
            for (unsigned i = 0; i < block; ++i) pitch[i] = inc;
            opxy::i32 o[block];
            NS::render(voices[v], shared, v + 1, o, p, pitch);
            for (unsigned i = 0; i < block; ++i) {
                const double ts = t + double(i) / rate;
                if (fading[v]) gain[v] *= 1.0 - double(i + 1) / block;   // to silence by the block's end
                else if (ts >= n.off) gain[v] *= fall;
                else gain[v] = std::fmin(1.0, gain[v] + rise);
                out[i] += o[i] / 2147483648.0 * gain[v] * n.velocity;
            }
            if (fading[v] || (t >= n.off && gain[v] < 1e-4)) {
                if (!fading[v]) { NS::destroy(voices[v], shared, v + 1); ever[v] = false; }
                playing[v] = -1;
            }
        }
        if (sounding > most) most = sounding;
        for (unsigned i = 0; i < block; ++i) {
            const double y = out[i] - x1 + r * y1;
            x1 = out[i]; y1 = y;
            mix[b * block + i] = float(y);
            if (std::fabs(y) > peak) peak = std::fabs(y);
        }
    }
    write_float_wav(out, mix, 1);
    std::printf("{\"out\": \"%s\", \"peak\": %.4f, \"most\": %u, \"steals\": %u}\n", out.c_str(), peak, most, steals);
    return 0;
}
