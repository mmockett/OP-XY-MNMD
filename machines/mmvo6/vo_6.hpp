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
    const u32 increment = md::transpose(pitch[0], tune);
    // 1 / increment, for where an impulse falls within a sample.
    const i32 l = log2_of(increment);
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
    const i32 octaves[4] = {m.o1, m.o2, m.o2 + f3_above_f2 > f3 ? m.o2 + f3_above_f2 : f3, f4};
    const i32 frequency = md::cutoff(octaves[0]), damp = damping(first_width, octaves[0]);
    i32 a1[3], a2[3];
    i32 dc = 0x40000000;                                  // the all-pole stages' gain at DC, Q30
    for (u32 k = 0; k < 3; ++k) {
        a1[k] = multiply(radius[k], md::sine(md::transpose(at_20hz, octaves[k + 1]) + 0x40000000u));   // 2 r cos, Q30
        a2[k] = -radius2[k];
        dc = multiply(dc, 0x40000000 + radius2[k] - a1[k]) * 2;
    }
    if (voice.peak > limit_peak) voice.limit = md::clamp(voice.limit + log2_of(limit_peak) - log2_of(voice.peak), limit_most, 0);
    else voice.limit = voice.limit + limit_release < 0 ? voice.limit + limit_release : 0;
    voice.peak = 0;
    const i32 norm = normal(m.o1, m.o2) + m.boost + log2_of(static_cast<u32>(dc)) - 30 * md::octave + voice.limit;
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

    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 e = voice.carry - pulse_mean;
        voice.carry = 0;
        const u32 before = voice.phase;
        voice.phase += increment;
        if (voice.phase < before) {
            // Crossed alpha = phase / increment samples ago: alpha here, the rest next sample.
            const i32 alpha = md::clamp(multiply(static_cast<i32>((voice.phase << (30 - whole)) & 0x7fffffffu), inverse) * 2, 0, md::one);
            const i32 here = multiply(pulse, alpha);
            e += here;
            voice.carry = pulse - here;
        }
        i32 n = 0;
        if (breathing || hissing) n = md::noise(voice.seed);
        if (breathing) { e += multiply(n, noise_gain); noise_gain += noise_slope; }
        voice.first.run(e, frequency, damp);
        e = voice.first.low;
        for (u32 k = 0; k < 3; ++k) {
            i32* y = voice.poles[k];
            const i32 next = e + (multiply(a1[k], y[0]) + multiply(a2[k], y[1])) * 2;
            y[1] = y[0];
            y[0] = next;
            e = next;
        }
        if (hissing) {
            // The hiss: the band-pass, twice.
            i32 h = multiply(n, hiss_gain);
            for (u32 k = 0; k < 2; ++k) {
                i32* z = voice.hiss[k];
                const i32 y = (multiply(band.b0, h - z[1]) - multiply(band.a1, z[2]) - multiply(band.a2, z[3])) * 2;
                z[1] = z[0]; z[0] = h; z[3] = z[2]; z[2] = y;
                h = y;
            }
            e += md::gain(h, 2);
            hiss_gain += hiss_slope;
        }
        const u32 size = static_cast<u32>(e < 0 ? -e : e);
        if (size > voice.peak) voice.peak = size;
        out[i] = md::finish(e);
    }
    voice.age += opxy::block_frames;
}
}
}
