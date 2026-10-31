#pragma once
// VO-6: the sound, a module any engine can hold (docs/VOICES.md).
#include <opxy/audio.hpp>
#include "md_dsp.hpp"

// MNM VO-6: the Monomachine's formant voice, written from scratch in fixed point
// and fitted to the Monomachine's own (reference/fit_vo_6.py, reference/README.md).
// Top row on the encoders, bottom row as their alternates:
//   VOC1  VOC2  V-SW  VOIC
//   CONS  CLEN  CVOL  TUNE
// As on the Monomachine: a buzz of impulses at the played pitch, or noise as
// VOIC turns from voiced to whispered, through four resonances in cascade, each
// with unity gain at DC, so the vowel's shape comes from where its formants
// sit, and its power is held whatever they are. VOC1 moves the first formant
// from 195 to 708 Hz and VOC2 the second from 499 Hz to 2.68 kHz; the third
// stays at 2.49 kHz (or 1.11 times the second) and the fourth at 3.58 kHz.
// CONS puts one of twenty consonants before the vowel, CLEN and CVOL its
// length and level; with V-SW off the consonant plays alone. TUNE is +-2
// semitones. The first formant runs as a state-variable filter, the others
// all-pole, two multiplies each.
namespace md_mmvo6 {} namespace md = md_mmvo6; namespace md_mmvo6 {
namespace vo_6 {
struct State {
    u32 phase;          // the buzz's phase
    i32 carry;          // the part of the last impulse due next sample
    md::Svf first;      // the first formant
    i32 poles[3][2];    // the others, all-pole: each's last two outputs
    u32 seed;
    i32 hiss[2][4];     // the consonants' band of noise: two band-passes' x1 x2 y1 y2
    u32 age;            // samples since the note started
    u32 trill;          // RR's flutter
    i32 gains[3];       // the buzz's, the noise's and the hiss's levels at the last block's end
    i32 limit;          // the limiter's gain, Q16 octaves (0 or less)
    u32 peak;           // the last block's peak
    u32 fresh;
    // Block values kept while what they are made from holds (cached: bit 0 the
    // pitch's, bit 1 the formants'); each is worked out again when it moves.
    u32 cached;
    u32 tuned_from;     // pitch[0] and TUNE's octaves: the increment and its log2
    i32 tuned_by;
    u32 increment;
    i32 log2_increment;
    i32 formants[2];    // o1 and o2: the formants' coefficients and the power's gain
    i32 frequency, damp, a1[3];
    i32 shape;          // normal(o1, o2) + log2 of the cascade's gain at DC - 30 octaves
};
namespace {
using md::multiply;

enum { VOC1, VOC2, VSW, VOIC, CONS, CLEN, CVOL, TUNE };

// Formants as Q16 octaves above 20 Hz (cutoff()'s scale).
const i32 f1_low = 215312, f1_span = 121897;          // 195 Hz x 2^(1.86 VOC1)
const i32 f2_points[9] = {304150, 325740, 346626, 365720, 385204, 404221, 423540, 443386, 462907};   // 499 Hz .. 2.68 kHz
const i32 f3 = 456131, f4 = 490327;                   // 2.49 and 3.58 kHz
const i32 f3_above_f2 = 9867;                         // and the third at least 1.11 times the second
// The first formant's width, 70 Hz, as B / 40 / 256 (Q31): its damping is
// B / (2 F) = this x 2^(8 - octaves).
const u32 first_width = 14680064;
// The others run all-pole, y = x + a1 y1 + a2 y2, at radius r for widths 55,
// 145 and 280 Hz (r Q31, r^2 Q30); their gains at DC, 1 - a1 - a2, are taken
// out of the buzz and the noise before the cascade, so it keeps unity gain at DC.
const i32 radius[3] = {2139086084, 2125415390, 2105072988};
const i32 radius2[3] = {1065360679, 1051786956, 1031749949};
const u32 at_20hz = 1947831;                          // the phase increment of 20 Hz
const i32 least_damping = 0x00cccccd;                 // Q at most 80
const i32 buzz = 0x06666666;                          // the excitation's RMS, Q31, before the cascade's gains
// A harmonic on a narrow formant can stand 13 dB over the vowel's usual level:
// a block that peaks above 0.36 of full scale turns the excitation down by its
// excess, from the next block, and it comes back at about 40 dB a second.
const u32 limit_peak = 773094113;                     // 0.36, Q31
const i32 limit_release = 655;                        // 0.01 octave a block
const i32 limit_most = -3 * 65536;                    // and at most 18 dB down
const i32 lowest = 1415765;                           // log2 of C1's phase increment, Q16
const i32 root3 = 1859775393;                         // sqrt 3, Q30: uniform noise to buzz's RMS

// The consonants (reference/fit_vo_6.py writes this table from its CONSONANTS):
// silence, up to two segments that voice, aspirate through the formants or hiss
// through a band, then the vowel. Lengths in samples; levels Q31, aspiration at
// half its full-CVOL level and hiss at an eighth (it runs at a quarter, for
// headroom in its filter); ramps as 2^31 / samples; the hiss's band
// as a band-pass biquad's b0, a1, a2 in Q30 (b1 = 0, b2 = -b0), run twice.
struct Band { i32 b0, a1, a2; };
struct Segment { u32 length; i32 voice, aspirate, hiss, rise; Band band; };
struct Consonant {
    u32 delay;
    Segment segment[2];
    u32 stretch;          // the segment CLEN lengthens: 1, 2, or 0 for none
    i32 locus[2];         // the formants during the consonant, Q16 octaves above 20 Hz; 0: the vowel's
    u32 overlap;          // the vowel starts this long before the consonant ends
    i32 rise, glide;      // the vowel's rise and the formants' glide from the locus
    i32 boost;            // the vowel's overshoot, Q16 octaves of gain, settling over 80 ms
    u32 trill;            // RR's flutter, a phase increment per block
};
const Consonant consonants[21] = {
    {0, {{0, 0, 0, 0, 48806447, {0, 0, 0}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 0, {0, 0}, 0, 885195, 48806447, 0, 0u},   // -
    {0, {{2205, 428479319, 0, 0, 48806447, {0, 0, 0}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 0, {256042, 359914}, 2205, 2434789, 1948715, 0, 0u},   // B
    {0, {{265, 0, 0, 45504608, 48806447, {115620646, -1786262003, 842500531}}, {617, 679093957, 0, 0, 48806447, {0, 0, 0}}}, 0, {256042, 420046}, 265, 3243933, 1948715, 0, 0u},   // D
    {0, {{3748, 0, 0, 14585257, 4869577, {170501050, -1610462287, 732739725}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {0, 0}, 441, 1390857, 3243933, 32656, 0u},   // F
    {0, {{529, 0, 0, 125704775, 48806447, {50678989, -2005556089, 972383846}}, {617, 0, 135176087, 0, 48806447, {0, 0, 0}}}, 0, {256042, 430562}, 0, 8103712, 2434789, 0, 0u},   // G
    {0, {{3087, 0, 427464319, 19074619, 1623192, {159415370, -1713872602, 754911084}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {0, 0}, 0, 4869577, 4869577, 43541, 0u},   // H
    {2205, {{1764, 1354970580, 0, 0, 2434789, {0, 0, 0}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {246080, 444423}, 0, 6083523, 973915, 54426, 0u},   // J
    {0, {{1235, 0, 1516700640, 217386854, 48806447, {66525353, -1974491748, 940691118}}, {882, 0, 852903448, 0, 48806447, {0, 0, 0}}}, 0, {0, 0}, 0, 3243933, 3243933, 0, 0u},   // K
    {2205, {{3087, 1705806895, 0, 0, 3243933, {0, 0, 0}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {230920, 417223}, 0, 8103712, 1623192, 108853, 0u},   // L
    {2205, {{3308, 959245710, 0, 0, 3243933, {0, 0, 0}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {190506, 369876}, 0, 8103712, 1623192, 65312, 0u},   // M
    {2205, {{3308, 1913946816, 0, 0, 3243933, {0, 0, 0}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {226717, 420046}, 0, 8103712, 1623192, 108853, 0u},   // N
    {0, {{265, 0, 0, 35340661, 48806447, {50920079, -2035478227, 971901665}}, {882, 0, 1073741824, 0, 48806447, {0, 0, 0}}}, 0, {256042, 359914}, 0, 4059515, 2434789, 0, 0u},   // P
    {1323, {{3748, 191394682, 0, 0, 4869577, {0, 0, 0}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {287855, 387114}, 0, 811596, 1217394, 0, 0u},   // R
    {882, {{3748, 761955951, 0, 0, 4869577, {0, 0, 0}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {287855, 387114}, 0, 1948715, 2434789, 0, 137127301u},   // RR
    {0, {{3748, 0, 0, 5357368, 1623192, {238990780, -1430736235, 595760263}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {0, 0}, 441, 1390857, 3243933, 21771, 0u},   // S
    {0, {{3528, 0, 0, 25071701, 3243933, {147651305, -1685549777, 778439214}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {0, 0}, 441, 1623192, 3243933, 43541, 0u},   // SJ
    {0, {{1102, 0, 0, 168653729, 48806447, {131627966, -1775140799, 810485892}}, {1544, 0, 538145694, 0, 48806447, {0, 0, 0}}}, 0, {0, 0}, 0, 3243933, 3243933, 0, 0u},   // T
    {0, {{794, 0, 0, 16294901, 48806447, {91993459, -1884319171, 889754907}}, {2558, 0, 0, 1662908, 48806447, {298697109, -1305102576, 476347607}}}, 2, {0, 0}, 0, 1948715, 3243933, 21771, 0u},   // TJ
    {0, {{2205, 0, 0, 26293464, 48806447, {166447330, -1629238041, 740847164}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {0, 0}, 662, 1082401, 3243933, 32656, 0u},   // TH
    {0, {{794, 0, 0, 1522017, 48806447, {158143376, -1666453471, 757455072}}, {970, 539423504, 0, 0, 48806447, {0, 0, 0}}}, 0, {256042, 378887}, 0, 3243933, 2434789, 32656, 0u},   // V
    {0, {{3175, 67909396, 0, 12684484, 9761289, {145189590, -1721381252, 783362644}}, {0, 0, 0, 0, 48806447, {0, 0, 0}}}, 1, {0, 0}, 0, 405798, 3243933, 0, 0u},   // Z
};
const i32 clen_points[9] = {0, 220, 220, 529, 1235, 3308, 7056, 12789, 22050};   // CLEN's added length (S, measured)
const u32 boost_length = 3528;                       // 80 ms
const i32 boost_rate = 608709;                       // 2^31 / boost_length
const i32 fade_rate = 9761289;                       // 2^31 / 220: a consonant's last 5 ms
const u32 tail_length = 1764;                        // 40 ms
const i32 tail_rate = 1217424;                       // 2^31 / 1764

// t / length as Q31, from a ramp's rate (2^31 / length), capped at one; ramps
// are shorter than 32768 samples.
inline i32 ramp(u32 t, i32 rate) {
    if (t >= 32768u) return md::one;
    const u32 r = static_cast<u32>(rate);
    const u32 high = t * (r >> 16);
    if (high >= 0x8000u) return md::one;
    const u32 sum = (high << 16) + t * (r & 0xffffu);
    return sum >= 0x80000000u ? md::one : static_cast<i32>(sum);
}

// What the syllable does at sample t: the levels of buzz, noise and hiss (Q31),
// the formants, the hiss's band and the output's gain.
struct Syllable {
    i32 voice, aspirate, hiss;   // aspirate and hiss before CVOL, at half
    i32 vowel;                   // the vowel's own level, Q31
    i32 o1, o2;                  // formants
    Band band;
    i32 boost;                   // Q16 octaves
};

inline Syllable at(const Consonant& c, u32 t, u32 extra, bool sing, i32 vowel1, i32 vowel2) {
    Syllable m = {0, 0, 0, 0, vowel1, vowel2, c.segment[0].band, 0};
    const u32 a = c.segment[0].length + (c.stretch == 1 ? extra : 0);
    const u32 b = c.segment[1].length + (c.stretch == 2 ? extra : 0);
    const u32 end = c.delay + a + b;
    const u32 start = end > c.overlap ? end - c.overlap : 0;
    // Before a vowel the consonant fades in its last 5 ms; alone, it tails off over 40 ms more.
    const u32 last = end + (sing ? 0 : tail_length);
    if (t >= c.delay && t < last) {
        const bool second = t >= c.delay + a;
        const Segment& s = c.segment[second ? 1 : 0];
        i32 level = ramp(t - (second ? c.delay + a : c.delay), s.rise);
        const i32 fade = ramp(last - t, sing ? fade_rate : tail_rate);
        level = multiply(level, fade);
        m.voice = multiply(s.voice, level);
        m.aspirate = multiply(s.aspirate, level);
        m.hiss = multiply(s.hiss, level);
        if (s.band.b0) m.band = s.band;
    }
    const bool before = t < start;
    if (!before && sing) {
        const u32 since = t - start;
        m.vowel = ramp(since, c.rise);
        if (since < boost_length) m.boost = multiply(c.boost, md::one - ramp(since, boost_rate));
    }
    // The formants: at the locus until the vowel, then gliding to it.
    const i32 g = before ? 0 : ramp(t - start, c.glide);
    if (c.locus[0]) m.o1 = c.locus[0] + multiply(vowel1 - c.locus[0], g);
    if (c.locus[1]) m.o2 = c.locus[1] + multiply(vowel2 - c.locus[1], g);
    return m;
}

// The Monomachine keeps a vowel's power whatever its formants, rising 2.9 dB
// across VOC1: the cascade's gain for white noise, undone, in Q16 octaves, on a
// grid of the first formant (9 steps, 195 .. 708 Hz) by the second (499 .. 2675 Hz).
const i32 grid1 = 215312, grid2 = 304150;             // the grids' first octaves
const u32 per_step1 = 70459, per_step2 = 54107;       // 2^30 / each grid's step in Q16 octaves
const i32 power[9][9] = {
    {126922, 134683, 139810, 143116, 145099, 145895, 144620, 130058, 110238},
    {106557, 117330, 124390, 128924, 131641, 132751, 131113, 112925, 89944},
    {82159, 97348, 107159, 113411, 117152, 118711, 116652, 94373, 68347},
    {51674, 73641, 87476, 96168, 101347, 103558, 101046, 74365, 45570},
    {11247, 44347, 64352, 76599, 83828, 86999, 84068, 52899, 21740},
    {-45548, 5893, 36146, 53817, 64046, 68657, 65456, 29994, -3030},
    {-88633, -49502, -228, 26398, 41208, 48037, 44915, 5669, -28665},
    {-41704, -122298, -51774, -8255, 14090, 24462, 22091, -20105, -55140},
    {248, -83074, -134816, -55869, -19463, -3089, -3496, -47448, -82506},
};

// A grid position, Q16 steps 0 .. 8, from octaves.
inline u32 grid(i32 octaves, i32 first, u32 per_step) {
    const u32 d = static_cast<u32>(md::clamp(octaves - first, 0, 0x7ffff)) >> 2;
    const u32 at = (d * per_step) >> 12;
    return at > 8 * 65536 - 1 ? 8 * 65536 - 1 : at;
}
inline i32 lerp(i32 a, i32 b, u32 fraction) { return a + multiply(b - a, static_cast<i32>(fraction << 15)); }
// The normalising gain for formants at o1, o2, Q16 octaves.
inline i32 normal(i32 o1, i32 o2) {
    const u32 x = grid(o1, grid1, per_step1), y = grid(o2, grid2, per_step2);
    const u32 i = x >> 16, j = y >> 16, fx = x & 0xffffu, fy = y & 0xffffu;
    const u32 i2 = i < 8 ? i + 1 : 8, j2 = j < 8 ? j + 1 : 8;
    return lerp(lerp(power[i][j], power[i][j2], fy), lerp(power[i2][j], power[i2][j2], fy), fx);
}

// log2 of an increment, Q16, to 1/256 octave: the buzz's level and timing.
inline i32 log2_of(u32 x) {
    if (!x) return 0;
    i32 whole = 31;
    while (!(x & 0x80000000u)) { x <<= 1; --whole; }
    const i32 mantissa = static_cast<i32>(x >> 1);   // Q30, 1 .. 2
    u32 lo = 0, hi = 256;
    while (hi - lo > 1) {
        const u32 mid = (lo + hi) >> 1;
        if (md::exp2_table[mid] <= mantissa) lo = mid; else hi = mid;
    }
    return whole * md::octave + static_cast<i32>(lo << 8);
}

// A level times 2^octaves; transpose() takes -16 .. 16 octaves, and below 15
// down the level is nothing anyway.
inline i32 level(i32 base, i32 octaves) {
    if (octaves < -15 * md::octave) return 0;
    return static_cast<i32>(md::transpose(static_cast<u32>(base), octaves > 15 * md::octave ? 15 * md::octave : octaves));
}

// A resonance's damping for a width at a frequency (Q16 octaves above 20 Hz).
inline i32 damping(u32 width, i32 octaves) {
    const i32 d = static_cast<i32>(md::transpose(width, 8 * md::octave - octaves));
    return d < least_damping ? least_damping : d;
}

// VOIC: the buzz's and the noise's amplitudes, by the buzz's measured share of the power.
const i32 voiced_points[9] = {0x7fffffff, 0x7f5bbf93, 0x7c19c135, 0x75505e3a, 0x5eed6632, 0x4fef9c04, 0x25516e7c, 0x0e058df7, 0};
const i32 unvoiced_points[9] = {0, 0x0ccccccd, 0x1f5a7ced, 0x33333333, 0x55dd7151, 0x63f8a04b, 0x7a707007, 0x7f3acc72, 0x7fffffff};
}

inline void trigger(State& voice, Common&) { voice.fresh = 1; }
inline void reset(State& voice, Common&, u32 owner) {
    voice.phase = 0;
    voice.carry = 0;
    voice.first.reset();
    for (u32 k = 0; k < 6; ++k) voice.poles[k >> 1][k & 1] = 0;
    for (u32 k = 0; k < 8; ++k) voice.hiss[k >> 2][k & 3] = 0;
    voice.seed = 0x2545f491u * owner;
    voice.age = 0;
    voice.trill = 0;
    voice.gains[0] = voice.gains[1] = voice.gains[2] = 0;
    voice.limit = 0;
    voice.peak = 0;
    voice.fresh = 0;
    voice.cached = 0;
}

namespace {
namespace block {
// A resonant pole can swing past full scale for a moment. The sums are done in
// unsigned words, which wrap by definition; a signed overflow would be undefined.
inline i32 wrapped(i32 x) { return static_cast<i32>(static_cast<u32>(x) * 2u); }   // x * 2
inline i32 fed(i32 in, i32 a, i32 b) {                                              // in + (a + b) * 2
    return static_cast<i32>(static_cast<u32>(in) + (static_cast<u32>(a) + static_cast<u32>(b)) * 2u);
}
// md::noise without its zero check: xorshift never returns to 0, so a seed
// checked once before a run of draws (first_seed) stays nonzero through it.
inline u32 first_seed(u32 seed) { return seed ? seed : 0x9e3779b9u; }
inline i32 noise(u32& seed) {
    u32 x = seed;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    seed = x;
    return static_cast<i32>(x);
}
// md::gain(x, 2) without branches: x * 4 inside +-one / 4, else +-one
// (+-(limit * 4 + 3)).
inline i32 limited(i32 value, i32 low, i32 high) {   // md::clamp, as MIN and MAX
    value = value > high ? high : value;
    return value < low ? low : value;
}
inline i32 gain4(i32 value) {
    const i32 limit = md::one >> 2;
    const i32 within = limited(value, -limit, limit);
    const i32 beyond = limited(value, -limit - 1, limit + 1) - within;   // -1, 0 or 1
    return within * 4 + beyond * 3;
}
// The buzz's impulses and, when Breathing, the noise, written to out. Returns the
// noise's seed after the block.
template <bool Breathing>
__attribute__((noinline)) u32 excite(i32* out, State& voice, u32 increment, u32 whole, i32 inverse, i32 pulse, i32 pulse_mean,
                                     i32 noise_gain, i32 noise_slope) {
    u32 phase = voice.phase, seed = first_seed(voice.seed);
    i32 carry = voice.carry;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 e = carry - pulse_mean;
        carry = 0;
        const u32 before = phase;
        phase += increment;
        if (__builtin_expect(phase < before, 0)) {
            // Crossed alpha = phase / increment samples ago: alpha here, the rest next sample.
            const i32 alpha = md::clamp(wrapped(multiply(static_cast<i32>((phase << (30 - whole)) & 0x7fffffffu), inverse)), 0, md::one);
            const i32 here = multiply(pulse, alpha);
            e += here;
            carry = pulse - here;
        }
        if (Breathing) { e += multiply(noise(seed), noise_gain); noise_gain += noise_slope; }
        out[i] = e;
    }
    voice.phase = phase;
    voice.carry = carry;
    return seed;
}
// The first formant (md::Svf::run, on locals).
__attribute__((noinline)) void first(i32* out, md::Svf& svf, i32 frequency, i32 damping) {
    i32 low = svf.low, band = svf.band, high = svf.high;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        low += multiply(frequency, band) * 2;
        high = out[i] - low - multiply(damping, band) * 2;
        band += multiply(frequency, high) * 2;
        out[i] = low;
    }
    svf.low = low; svf.band = band; svf.high = high;
}
// One all-pole formant.
__attribute__((noinline)) void pole(i32* out, i32* y, i32 a1, i32 a2) {
    // Two samples a turn, so the last two outputs swap roles instead of moving.
    i32 y0 = y[0], y1 = y[1];
    for (unsigned i = 0; i < opxy::block_frames; i += 2) {
        y1 = fed(out[i], multiply(a1, y0), multiply(a2, y1));
        out[i] = y1;
        y0 = fed(out[i + 1], multiply(a1, y1), multiply(a2, y0));
        out[i + 1] = y0;
    }
    y[0] = y0; y[1] = y1;
}
// Two all-pole formants in cascade.
__attribute__((noinline)) void poles(i32* out, i32 (*y)[2], const i32* a1, const i32* a2) {
    i32 p0 = y[0][0], p1 = y[0][1], q0 = y[1][0], q1 = y[1][1];
    const i32 a = a1[0], b = a2[0], c = a1[1], d = a2[1];
    for (unsigned i = 0; i < opxy::block_frames; i += 2) {
        p1 = fed(out[i], multiply(a, p0), multiply(b, p1));
        q1 = fed(p1, multiply(c, q0), multiply(d, q1));
        out[i] = q1;
        p0 = fed(out[i + 1], multiply(a, p1), multiply(b, p0));
        q0 = fed(p0, multiply(c, q1), multiply(d, q0));
        out[i + 1] = q0;
    }
    y[0][0] = p0; y[0][1] = p1; y[1][0] = q0; y[1][1] = q1;
}
// The hiss: the block's noise again from `seed`, through the band-pass twice,
// added to out. Returns the seed after the block.
__attribute__((noinline)) u32 hiss(i32* out, i32 (*z)[4], u32 seed, i32 hiss_gain, i32 hiss_slope, const Band& band) {
    const i32 b0 = band.b0, a1 = band.a1, a2 = band.a2;
    seed = first_seed(seed);
    i32 x1 = z[0][0], x2 = z[0][1], y1 = z[0][2], y2 = z[0][3];
    i32 u1 = z[1][0], u2 = z[1][1], v1 = z[1][2], v2 = z[1][3];
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        const i32 h = multiply(noise(seed), hiss_gain);
        const i32 y = (multiply(b0, h - x2) - multiply(a1, y1) - multiply(a2, y2)) * 2;
        x2 = x1; x1 = h; y2 = y1; y1 = y;
        const i32 v = (multiply(b0, y - u2) - multiply(a1, v1) - multiply(a2, v2)) * 2;
        u2 = u1; u1 = y; v2 = v1; v1 = v;
        out[i] += gain4(v);
        hiss_gain += hiss_slope;
    }
    z[0][0] = x1; z[0][1] = x2; z[0][2] = y1; z[0][3] = y2;
    z[1][0] = u1; z[1][1] = u2; z[1][2] = v1; z[1][3] = v2;
    return seed;
}
// The block's peak, from `peak`, and the output's ceiling.
__attribute__((noinline)) u32 finish(opxy::Output out, const i32* in, u32 peak) {
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        const i32 e = in[i];
        const u32 size = static_cast<u32>(e < 0 ? -e : e);
        if (size > peak) peak = size;
        out[i] = md::finish(e);
    }
    return peak;
}
}
}

inline void render(State& voice, Common&, opxy::Output out, const i32* p, opxy::Pitch pitch) {
    if (voice.fresh) {
        voice.fresh = 0;
        voice.phase = 0xffffffffu;   // an impulse on the first sample
        voice.carry = 0;
        voice.age = 0;
        voice.trill = 0x40000000u;  // the flutter starts at its loudest
        voice.first.reset();
        for (u32 k = 0; k < 6; ++k) voice.poles[k >> 1][k & 1] = 0;
        for (u32 k = 0; k < 8; ++k) voice.hiss[k >> 2][k & 3] = 0;
        voice.gains[0] = voice.gains[1] = voice.gains[2] = 0;
    }
    const i32 tune = multiply(md::positive(p[TUNE]) - 0x40810204, 21675);   // (TUNE - 64) / 32 semitones: 127 / 384 octave per unit
    if (!(voice.cached & 1u) || voice.tuned_from != pitch[0] || voice.tuned_by != tune) {
        voice.tuned_from = pitch[0];
        voice.tuned_by = tune;
        voice.increment = md::transpose(pitch[0], tune);
        voice.log2_increment = log2_of(voice.increment);
        voice.cached |= 1u;
    }
    const u32 increment = voice.increment;
    // 1 / increment, for where an impulse falls within a sample.
    const i32 l = voice.log2_increment;
    const u32 whole = static_cast<u32>(l) >> 16;
    const i32 inverse = md::exp2_table[256 - ((static_cast<u32>(l) >> 8) & 255u)];   // 2^(1 - fraction), Q30

    // The syllable now and at the block's end.
    const Consonant& c = consonants[(static_cast<u32>(md::positive(p[CONS])) >> 16) * 21u >> 15];
    const bool sing = p[VSW] >= 0x40000000 || &c == &consonants[0];
    const u32 extra = static_cast<u32>(md::table9(clen_points, p[CLEN]));
    const i32 vowel1 = f1_low + multiply(md::positive(p[VOC1]), f1_span);
    const i32 vowel2 = md::table9(f2_points, p[VOC2]);
    const Syllable m = at(c, voice.age, extra, sing, vowel1, vowel2);
    const Syllable next = at(c, voice.age + opxy::block_frames, extra, sing, vowel1, vowel2);
    // RR's flutter: between full and a quarter, at its rate.
    i32 flutter = md::one;
    if (c.trill) {
        flutter = 0x50000000 + multiply(0x30000000, md::sine(voice.trill));
        voice.trill += c.trill;
    }

    // The formants, and the gain that keeps their power.
    if (!(voice.cached & 2u) || voice.formants[0] != m.o1 || voice.formants[1] != m.o2) {
        const i32 octaves[4] = {m.o1, m.o2, m.o2 + f3_above_f2 > f3 ? m.o2 + f3_above_f2 : f3, f4};
        voice.frequency = md::cutoff(octaves[0]);
        voice.damp = damping(first_width, octaves[0]);
        i32 dc = 0x40000000;                              // the all-pole stages' gain at DC, Q30
        for (u32 k = 0; k < 3; ++k) {
            voice.a1[k] = multiply(radius[k], md::sine(md::transpose(at_20hz, octaves[k + 1]) + 0x40000000u));   // 2 r cos, Q30
            dc = multiply(dc, 0x40000000 + radius2[k] - voice.a1[k]) * 2;
        }
        voice.shape = normal(m.o1, m.o2) + log2_of(static_cast<u32>(dc)) - 30 * md::octave;
        voice.formants[0] = m.o1;
        voice.formants[1] = m.o2;
        voice.cached |= 2u;
    }
    const i32 frequency = voice.frequency, damp = voice.damp;
    const i32* a1 = voice.a1;
    const i32 a2[3] = {-radius2[0], -radius2[1], -radius2[2]};
    if (voice.peak > limit_peak) voice.limit = md::clamp(voice.limit + log2_of(limit_peak) - log2_of(voice.peak), limit_most, 0);
    else voice.limit = voice.limit + limit_release < 0 ? voice.limit + limit_release : 0;
    voice.peak = 0;
    const i32 norm = voice.shape + m.boost + voice.limit;
    // The impulse: K sqrt(period) high, so every pitch has the same power, less
    // its mean K / sqrt(period); the noise at K.
    // Below C1 (32.7 Hz) the impulses grow no taller, so each stays under the ceiling.
    const i32 height = opxy::q31::half(l > lowest ? l : lowest);
    const i32 impulse = level(buzz, 16 * md::octave - height + norm);
    const i32 mean = level(buzz, height - 16 * md::octave + norm + (l > lowest ? 0 : l - lowest));
    const i32 noise_unit = level(multiply(buzz, root3) * 2, norm);
    const Band band = m.band;

    // The levels, ramped across the block: VOIC splits the voice into buzz and noise.
    const i32 voiced = md::table9(voiced_points, p[VOIC]), unvoiced = md::table9(unvoiced_points, p[VOIC]);
    const i32 cvol = 0x0a3d70a4 + multiply(md::positive(p[CVOL]), 0x75c28f5c);   // 0.08 + 0.92 CVOL: never quite silent
    i32 target[3];
    {
        const i32 v = md::add(next.vowel, multiply(next.voice, flutter));
        target[0] = multiply(v, voiced);
        target[1] = md::add(multiply(v, unvoiced), md::gain(multiply(next.aspirate, cvol), 1));
        target[2] = level(md::gain(multiply(next.hiss, cvol), 1), voice.limit);
    }
    // The buzz steps once a block (it only scales the impulses); the noise and the
    // hiss ramp across it.
    const i32 buzz_gain = opxy::q31::half(voice.gains[0]) + opxy::q31::half(target[0]);
    const i32 pulse = multiply(impulse, buzz_gain), pulse_mean = multiply(mean, buzz_gain);
    i32 noise_gain = multiply(voice.gains[1], noise_unit);
    const i32 noise_slope = multiply(multiply(target[1] - voice.gains[1], 1 << 25), noise_unit);   // / 64
    i32 hiss_gain = voice.gains[2];
    const i32 hiss_slope = multiply(target[2] - hiss_gain, 1 << 25);
    const bool breathing = voice.gains[1] || target[1], hissing = voice.gains[2] || target[2];
    if (!hissing) for (u32 k = 0; k < 8; ++k) voice.hiss[k >> 2][k & 3] = 0;
    for (u32 k = 0; k < 3; ++k) voice.gains[k] = target[k];

    // The samples, in passes over the output (block::), each a short loop with
    // its state in locals: the same arithmetic in the same order.
    i32 e[opxy::block_frames];
    const u32 seed = voice.seed;
    if (breathing) voice.seed = block::excite<true>(e, voice, increment, whole, inverse, pulse, pulse_mean, noise_gain, noise_slope);
    else block::excite<false>(e, voice, increment, whole, inverse, pulse, pulse_mean, noise_gain, noise_slope);
    block::first(e, voice.first, frequency, damp);
    block::poles(e, voice.poles, a1, a2);
    block::pole(e, voice.poles[2], a1[2], a2[2]);
    if (hissing) voice.seed = block::hiss(e, voice.hiss, seed, hiss_gain, hiss_slope, band);
    voice.peak = block::finish(out, e, voice.peak);
    voice.age += opxy::block_frames;
}
}
}
