#pragma once
// PI-RC, PI-CC and PI-HH: the sound, modules any engine can hold
// (docs/VOICES.md). All three are one struck plate of metal, each with its own
// partials and its own measured response to the controls (reference/fit_pi_cymbal.py).
// Their pages:
//   PTCH  DEC   HARD  RING      PTCH  DEC   CLSN  RING
//   AG    AU    BR    GRAB      AG    AU    BR    CLOS     (PI-HH)
// As measured: sixteen inharmonic partials from 1.3 to 6 kHz (the ride) or 1.7
// to 6.2 kHz (the crash and the hat, which share theirs), each struck afresh at
// every hit, so no two hits are alike. Over them, a dense layer as noise, from
// about 2 to 18 kHz, 20 to 45 dB under the strongest partial, that rings with
// them (on the ride, far shorter until DEC is long) and that AU raises and opens
// upward. The stick starts it all with about a millisecond of bare noise (the
// ride and the hat always, the crash only at HARD over 112), and a 19.2 kHz tone
// falling 60 dB in 60 ms sounds on the hat and the hardest crash, with PTCH near
// 64 (it is gone at PTCH 0 and 127; how narrow that is was not measured closely).
// On the crash a soft stick swells in: about 35 ms at HARD 64, 70 at 32, none at 127. Every control moves three things, read
// off the Machinedrum at each step and added up: the level, the ring (-60 dB in
// 0.53 s, 0.43 s and 0.15 s at the design's settings) and the brightness, a
// tilt across the partials. The alloys (AG silver, AU gold, BR bronze) mostly
// tilt them: gold and bronze brighten, bronze also quietens. HARD rings louder
// and shorter (on the crash, a soft stick is far quieter and rings long). GRAB
// below 80 grabs the cymbal after 40 to 135 ms. On the hat, CLOS is the pedal:
// at 127 it is closed and rings for DEC's time; below, the hat rings open for
// 5 ms (CLOS 0) to 2.2 s (112) and 4.6 s (126), doubling every 12.7 steps as
// the emulator's does, then closes with a chick. PTCH moves the partials 0.1 to
// 0.15 octaves either way. A closed hat (CLOS 127, DEC under 64) chokes the open
// ones, as on a drum machine. All three sit 12 dB over the emulator's main mix (the
// other P-I machines 2 dB): the Machinedrum leaves its cymbals far under its drums,
// and a kit of them should stay balanced.
#include <opxy/audio.hpp>
#include "md_dsp.hpp"
#include "md_modal.hpp"

namespace md {
namespace pi_cymbal {
const unsigned partial_count = 16;
struct State {
    modal::Mode partials[partial_count];
    u32 held;        // samples until the grab or the pedal; then it is grabbed or closed
    u32 waiting;     // a grab or a pedal is coming
    u32 choke;       // an open hat: the closed hats started when it was struck
    u32 open;        // this hit is an open hat, which a closed one chokes
    u32 seed;
    i32 hiss;        // the dense layer's envelope, Q31
    modal::Mode tone; // the high tone at the strike
    i32 strike;      // the stick's click: its level this block, Q31
    i32 swell;       // how far the plate has come up, Q31: a soft stick on the crash is slow
    i32 pole1, pole2, under;   // the layer's filter: two low-pass poles, and one under them
    u32 fresh;
};

// One plate: its partials (Q16 octaves from middle C) and their levels (Q31), and
// for each of its eight controls, at 0, 16, ..., 127, what it adds to the level and
// the ring (Q16 octaves of amplitude and of time) and to the brightness (Q16
// octaves of tilt); the pitch PTCH sets; the hold before the grab or the pedal
// (samples, 0 for none) on control `choke`; the ring at the design's settings
// (Q16 log2 samples); whether the hold is an open hat's; and its own gain (Q31) and
// tilt (Q16 octaves) at the design's settings.
struct Plate {
    i32 partials[partial_count];
    i32 levels[partial_count];
    i32 level[8][9], ring[8][9], tilt[8][9];
    i32 pitch[9];
    unsigned choke;
    i32 holds[9];
    i32 base;
    bool hat;
    i32 gain, bright;
    // The dense layer, white noise split at two low-pass poles (a corner, Q31, that AU
    // moves, at 0, 16, ..., 127) into a body under them and a top over. It follows the
    // plate's level, all but BR's, and its ring: how much shorter it rings, the levels
    // of its body and its top (Q31), and at 0, 16, ..., 127 what AU adds to its level and
    // DEC and AG to its ring (Q16 octaves). Then by control 2 (HARD or CLSN), likewise:
    // the click (Q31), the high tone (Q31), and the swell, how much of the
    // way up the plate comes per block.
    i32 hiss_shorter, hiss_mid, hiss_high;
    i32 corner[9], au_lift[9], ring_dec[9], ring_ag[9];
    i32 click[9], sheen[9], swell[9];
};

namespace {
using md::multiply;

// 2^octaves for octaves <= 0 (Q16), Q31: 2^-(w + f) is 2^(1 - f) (the exp2 table,
// Q30) over 2^w.
inline i32 power(i32 octaves) {
    if (octaves >= 0) return md::one;
    const u32 down = static_cast<u32>(-octaves);
    const u32 whole = down >> 16;
    if (whole >= 31) return 0;
    const u32 up = 65536u - (down & 0xffffu);
    return static_cast<i32>(static_cast<u32>(exp2_table[up >> 8]) >> whole);
}

// The partials tilt around 3.5 kHz (Q16 octaves from middle C): an octave of tilt
// turns each partial up 4 octaves of amplitude (24 dB) for each octave above it,
// which moves the brightness about as far as the Machinedrum's.
const i32 tilt_centre = 239000;
// Headroom for the level tables' peaks: everything is reckoned 3 octaves (18 dB) down.
const i32 headroom = 3 * 65536;
// A grabbed cymbal or a closed hat dies 60 dB in 0.12 s; the pedal's chick
// strikes the hat again, on top of what still rings, about 8 dB under the hit.
const i32 grab_ring = 810654, chick = 0x28000000;
// The dense layer: a one-pole at 4 kHz under its body. The high tone: 19.2 kHz whatever
// the key, falling 60 dB in 60 ms, sounding only with PTCH near 64 (Q31 at 0, 16, ..., 127).
const u32 tone_increment = 1870500000u;
const i32 tone_time = 730000;
const i32 tone_ptch[9] = {0, 0, 0, 0, md::one, 0, 0, 0, 0};
const i32 under_pole = 0x379b12a1;
// Each hit's balance: every partial's level times 0.5 .. 1, at random.
const i32 vary_low = 0x40000000, vary_span = 0x40000000;

inline i32 sum(const i32 (&table)[8][9], const i32* p) {
    i32 total = 0;
    for (unsigned c = 0; c < 8; ++c) total += md::table9(table[c], p[c]);
    return total;
}
}

// Whether a sound is still to come however quiet it is now: the grab, or the pedal's close.
inline bool waiting(const State& voice) { return voice.waiting != 0; }
inline void trigger(State& voice, Common& shared) {
    voice.fresh = 1;
    voice.seed += 0x9e3779b9u * ++shared.strikes;   // each hit its own
}
inline void reset(State& voice, Common& shared, u32 owner) {
    for (unsigned k = 0; k < partial_count; ++k) {
        voice.partials[k].phase = voice.partials[k].increment = 0;
        voice.partials[k].level = 0;
    }
    voice.hiss = voice.strike = 0;
    voice.tone.phase = voice.tone.increment = 0;
    voice.tone.level = 0;
    voice.swell = md::one;
    voice.pole1 = voice.pole2 = voice.under = 0;
    voice.held = voice.waiting = 0;
    voice.choke = shared.closed_hats;
    voice.open = 0;
    voice.seed = 0x85ebca6bu * owner;
    trigger(voice, shared);
    voice.fresh = 0;
}

inline void render(const Plate& plate, State& voice, Common& shared, opxy::Output output, const i32* p,
                   opxy::Pitch pitch) {
    // Brightening raises the upper partials; the Machinedrum's level barely rises with it.
    const i32 plain = sum(plate.level, p);
    const i32 level = plain - headroom - md::positive(sum(plate.tilt, p)) * 2;
    const i32 tilts = sum(plate.tilt, p);
    const i32 tilt = (plate.bright + tilts) * 4;
    const i32 time = plate.base + sum(plate.ring, p);
    const i32 shift = md::table9(plate.pitch, p[0]);   // PTCH is every plate's first control
    if (voice.fresh) {
        voice.fresh = 0;
        for (unsigned k = 0; k < partial_count; ++k) {
            md::noise(voice.seed);
            voice.partials[k].phase = voice.seed;
            md::noise(voice.seed);
            voice.partials[k].level = vary_low + multiply(static_cast<i32>(voice.seed >> 1), vary_span);
        }
        voice.hiss = md::one;
        voice.tone.phase = 0;
        voice.tone.increment = tone_increment;
        voice.tone.level = md::one;
        voice.strike = md::table9(plate.click, p[2]);
        voice.swell = md::table9(plate.swell, p[2]) == md::one ? md::one : 0;
        voice.pole1 = voice.pole2 = voice.under = 0;
        // The hat at CLOS 127 never closes: its table's last step is 126's time.
        voice.held = plate.hat && p[plate.choke] >= (127 << 24) ? 0 : static_cast<u32>(md::table9(plate.holds, p[plate.choke]));
        voice.waiting = voice.held ? 1 : 0;
        // As on a drum machine, a closed hat (the pedal down, a short DEC) chokes the
        // open ones on any voice of the engine.
        voice.open = 0;
        if (plate.hat) {
            if (!voice.held && p[1] < 0x40000000) ++shared.closed_hats;
            else voice.open = 1;
            voice.choke = shared.closed_hats;
        }
    }
    if (voice.open && voice.choke != shared.closed_hats) {
        voice.open = voice.waiting = 0;
        voice.held = 1;   // choked: it dies as a grabbed one does
    }
    // The plate rings for its time until grabbed, or until the hat's pedal
    // closes it with a chick, then dies fast. A hat at CLOS 127 never closes.
    i32 step = md::decay(time);
    const i32 hiss_time = time - plate.hiss_shorter + md::table9(plate.ring_dec, p[1]) + md::table9(plate.ring_ag, p[4]);
    i32 hiss_step = md::decay(hiss_time > time ? time : hiss_time);   // never longer than the partials
    if (voice.waiting) {
        if (voice.held > opxy::block_frames) {
            voice.held -= opxy::block_frames;
        } else {
            voice.waiting = 0;
            voice.held = 1;   // grabbed, or closed, from here on
            // The hat's pedal closing strikes it again, softly: the chick.
            if (plate.hat)
                for (unsigned k = 0; k < partial_count; ++k)
                    voice.partials[k].level = md::add(voice.partials[k].level, chick);
            if (plate.hat) voice.hiss = md::add(voice.hiss, chick);
        }
    } else if (voice.held) {
        step = hiss_step = md::decay(grab_ring);
    }
    const i32 swell = voice.swell;
    voice.swell = md::add(swell, md::table9(plate.swell, p[2]));

    i32 mixed[opxy::block_frames];
    for (unsigned i = 0; i != opxy::block_frames; ++i) mixed[i] = 0;
    for (unsigned k = 0; k < partial_count; ++k) {
        modal::Mode& partial = voice.partials[k];
        if (partial.level < 0x00010000) continue;
        partial.increment = md::transpose(pitch[0], plate.partials[k] + shift);
        const i32 slope = multiply((plate.partials[k] - tilt_centre) * 256, tilt * 128);   // Q16 octaves
        const i32 amount = multiply(multiply(plate.levels[k], power(level + slope)), plate.gain);
        modal::ring(partial, step, multiply(amount, swell), multiply(amount, voice.swell), mixed);
    }
    if (voice.tone.level > 0x00010000) {
        const i32 tone = multiply(md::table9(plate.sheen, p[2]), md::table9(tone_ptch, p[0]));
        if (tone) modal::ring(voice.tone, md::decay(tone_time), tone, mixed);
        else voice.tone.level = 0;
    }
    if (voice.hiss > 0x00010000) {
        const i32 lift = power(plain - md::table9(plate.level[6], p[6]) + md::table9(plate.au_lift, p[5]) - headroom);
        const i32 mid = multiply(plate.hiss_mid, lift), top = multiply(plate.hiss_high, lift);
        const i32 hiss_next = multiply(voice.hiss, md::per_block(hiss_step));
        // The layer swells with the plate; each gain in a straight line across the block.
        i32 mid_now = multiply(multiply(voice.hiss, mid), swell);
        i32 top_now = multiply(multiply(voice.hiss, top), swell);
        const i32 mid_slope = (multiply(multiply(hiss_next, mid), voice.swell) - mid_now) / 64;
        const i32 top_slope = (multiply(multiply(hiss_next, top), voice.swell) - top_now) / 64;
        voice.hiss = hiss_next;
        const i32 corner = md::table9(plate.corner, p[5]);
        for (unsigned i = 0; i != opxy::block_frames; ++i) {
            const i32 white = md::noise_eighth(voice.seed);
            md::smooth(voice.pole1, white, corner);
            md::smooth(voice.pole2, voice.pole1, corner);
            md::smooth(voice.under, voice.pole2, under_pole);
            mixed[i] += (multiply(voice.pole2 - voice.under, mid_now) + multiply(white - voice.pole1, top_now)) * 16;
            mid_now += mid_slope;
            top_now += top_slope;
        }
    }
    if (voice.strike) {   // the stick's click: a block and a bit of bare noise
        i32 click = voice.strike;
        const i32 slope = -(click >> 6);
        for (unsigned i = 0; i != opxy::block_frames; ++i) {
            mixed[i] += multiply(md::noise_eighth(voice.seed), click) * 8;
            click += slope;
        }
        voice.strike = 0;
    }
    for (unsigned i = 0; i != opxy::block_frames; ++i) output[i] = md::finish(md::gain(mixed[i], 1));
}

namespace {
// PI-RC, the ride.
const Plate rc = {
    {149930, 173033, 177620, 193662, 202531, 222112, 224688, 239646, 247069, 263265, 267974, 272324, 277172, 284881,
        289856, 297424},
    {907218652, 774846444, 954425785, 1613078285, 1372653351, 1719972999, 1323631258, 1947525587, 2147483647,
        1293304241, 1215962049, 916297104, 418978188, 859689986, 441346883, 512254622},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {-10885, -5443, 0, 0, 0, 0, 0, 10885, 27213}, {-21771, -21771, -10885, -5443, 0,
        16328, 27213, 38099, 43541}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {-21771, -16328, -10885, -5443, 0, 10885, 16328,
            27213, 32656}, {0, -5443, -5443, -5443, -5443, 10885, 10885, 16328, 27213}, {59869, 48984, 38099, 21771,
                0, -16328, -32656, -54426, -76197}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{72090, 52429, 39322, 19661, 0, -19661, -32768, -52429, -75366}, {0, 0, 0, 0, 0, 11796, 62259, 125174, 182845},
        {49152, 49152, 19661, 11141, 0, -15729, -36700, -83231, -118620}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {-12452,
            -10486, -9175, -7864, 0, 3277, 14418, 22938, 28180}, {0, -4588, -7864, -7209, -3932, -9830, -7864,
                -17695, -24248}, {8520, 9830, 6554, -3277, 0, -20972, -17039, -20316, -30802}, {0, 0, 0, 0, 0, 0, 0,
                    0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {-13107, -13107, -6554, 0, 0, 0, 0, 6554, 19661}, {0,
        0, 0, 0, 0, 0, 0, 0, 0}, {-22282, -26214, -19005, -9175, 0, 0, 3277, 15729, 31457}, {0, 11141, 13763, 11141,
            5898, 11141, 19005, 13763, 23593}, {-20972, -17039, -3277, 5898, 0, 24904, 24904, 22282, 46531}, {0, 0,
                0, 0, 0, 0, 0, 0, 0}},
    {-9830, -7373, -4915, -2458, 0, 2458, 4915, 7373, 9830},
    7, {1764, 1764, 2205, 3308, 5954, 0, 0, 0, 0}, 951095, false, 272873357, 5243,
    /*hiss*/ 125000, 0x20000000, 0x00d00000, {0x41381691, 0x45893023, 0x498e4d34, 0x4d4ca751, 0x50c91c0e, 0x54083359, 0x570e2561, 0x5c7e0c16, 0x61351da3},
    /*au_lift*/ {0, 10000, 27000, 60000, 75000, 90000, 100000, 100000, 100000},
    /*ring_dec*/ {0, 0, 0, 0, 25000, 75000, 125000, 125000, 125000},
    /*ring_ag*/ {-20000, -25000, -15000, 0, 0, 20000, 50000, 70000, 100000},
    /*click*/ {0x05000000, 0x05000000, 0x05000000, 0x05000000, 0x05000000, 0x05000000, 0x05000000, 0x05000000, 0x05000000}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {md::one, md::one, md::one, md::one, md::one, md::one, md::one, md::one, md::one}};
// PI-CC, the crash.
const Plate cc = {
    {175763, 183961, 208733, 220803, 225509, 232903, 234708, 245894, 256985, 258640, 263167, 275794, 278319, 284149,
        286779, 289356},
    {1133419364, 1104440315, 1082592245, 1007113507, 1495285364, 968830214, 1122021125, 1511623361, 946078652,
        1540082495, 2147483647, 1345396915, 914047226, 1279924235, 1070166833, 1225261103},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {-174709, -130079, -61502, -7075, 47351, 76741, 96335, 96335, 101777}, {-653118,
        -228591, -133889, -47895, 0, 40276, 64223, 62046, 90348}, {0, 16328, 29390, 38099, 38099, 50072, 50072,
            52249, 48984}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {29390, 21771, 20682, 10885, 0,
                -6531, -26125, -39187, -78374}, {-222060, -133889, -58781, -14151, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {-9175, -655, -12452, -655, 3932, 33423, 82575, 136315, 197919}, {183501, 182190,
        165806, 96338, 0, -73400, -96338, -98959, -104858}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0,
            0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0,
        0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {-25559, -25559, -11141, -6554, 0, 5898,
            17039, 26870, 36700}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {-6554, -4915, -3277, -1638, 0, 1638, 3277, 4915, 6554},
    7, {2426, 2205, 2205, 2866, 5733, 0, 0, 0, 0}, 931326, false, 195414396, 22938,
    /*hiss*/ 0, 0x28000000, 0x01c00000, {0x498e4d34, 0x498e4d34, 0x498e4d34, 0x498e4d34, 0x498e4d34, 0x498e4d34, 0x498e4d34, 0x498e4d34, 0x498e4d34},
    /*au_lift*/ {0, 0, 0, 0, 0, 0, 0, 0, 0},
    /*ring_dec*/ {0, 0, 0, 0, 0, 0, 0, 0, 0},
    /*ring_ag*/ {0, 0, 0, 0, 0, 0, 0, 0, 0},
    /*click*/ {0, 0, 0, 0, 0, 0, 0, 0x00400000, 0x02000000}, /*tone*/ {0, 0, 0, 0, 0, 0, 0, 0, 0x0c000000},
    /*swell*/ {33554432, 38347922, 44739243, 59652324, 89478485, 153391689, 268435456, 715827883, md::one}};
// PI-HH, the hat.
const Plate hh = {
    {175763, 184152, 208653, 220591, 225475, 232893, 245903, 252246, 257017, 258632, 263197, 275735, 278319, 284149,
        289379, 299218},
    {1298031166, 1011493760, 1312956081, 899669218, 1392735971, 1104247478, 1550587339, 929675544, 993001939,
        1514576149, 2147483647, 1281112014, 912509637, 1191477512, 1195430196, 1034345123},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {-46154, -39622, -17852, -14586, -435, -2612, -1524, -4790, -12409}, {0, -9797,
        -17416, -22859, -19594, -26125, -30479, -41364, -45718}, {0, 3266, 0, 6531, 7620, 9797, 14151, 25036,
            25036}, {-6531, -5443, -4354, -5443, 0, 5443, 4354, 11974, 14151}, {0, 10885, 13062, 20682, 25036,
                28302, 33744, 40276, 44630}, {23948, 8708, 4354, 1089, 0, -8708, -21771, -30479, -42453}, {0, 0, 0,
                    0, 0, 0, 0, 0, 0}},
    {{13763, 6554, 7209, 5898, 0, -5243, 0, -3277, -3277}, {-76677, -78643, -56361, -7864, 54395, 114033, 182845,
        250348, 302776}, {0, 0, 0, 0, 0, -6554, -10486, -10486, -11796}, {0, 0, 0, 1966, 5243, 1966, 9830, 7864,
            14418}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, -6554, -13107},
                {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{3932, 9830, 0, -1966, 0, -1966, -6554, -8520, -8520}, {62259, 58982, 11796, 1966, 0, 0, 0, 0, 0}, {0, 3277,
        3277, 3277, 3277, 3277, 6554, 6554, 6554}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 10486, 10486,
            17695}, {0, 7864, 7864, 11796, 19005, 27525, 34734, 39322, 51773}, {-23593, -26214, -20972, -6554, 0,
                7864, 17039, 28836, 47186}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {-6554, -4915, -3277, -1638, 0, 1638, 3277, 4915, 6554},
    7, {221, 526, 1176, 2859, 7021, 17098, 40112, 94725, 203918}, 828546, true, 903568607, 20972,
    /*hiss*/ -12000, 0x50000000, 0x03000000, {0x498e4d34, 0x4d4ca751, 0x50c91c0e, 0x54083359, 0x570e2561, 0x59dee00b, 0x5c7e0c16, 0x5eef11d6, 0x61351da3},
    /*au_lift*/ {0, 5000, 27000, 40000, 49000, 71000, 67000, 83000, 91000},
    /*ring_dec*/ {0, 0, 0, 0, 0, 0, 0, 0, 0},
    /*ring_ag*/ {0, 0, 0, 0, 0, 0, 0, 0, 0},
    /*click*/ {0x08000000, 0x08000000, 0x08000000, 0x08000000, 0x08000000, 0x08000000, 0x08000000, 0x08000000, 0x08000000},
    /*tone*/ {0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000},
    /*swell*/ {0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000}};
}
}

namespace pi_rc {
typedef pi_cymbal::State State;
inline void reset(State& s, Common& c, u32 owner) { pi_cymbal::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { pi_cymbal::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p,
    opxy::Pitch pitch) { pi_cymbal::render(pi_cymbal::rc, s, c, o, p, pitch); }
}

namespace pi_cc {
typedef pi_cymbal::State State;
inline void reset(State& s, Common& c, u32 owner) { pi_cymbal::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { pi_cymbal::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p,
    opxy::Pitch pitch) { pi_cymbal::render(pi_cymbal::cc, s, c, o, p, pitch); }
}

namespace pi_hh {
typedef pi_cymbal::State State;
inline void reset(State& s, Common& c, u32 owner) { pi_cymbal::reset(s, c, owner); }
inline void trigger(State& s, Common& c) { pi_cymbal::trigger(s, c); }
inline void render(State& s, Common& c, opxy::Output o, const i32* p,
    opxy::Pitch pitch) { pi_cymbal::render(pi_cymbal::hh, s, c, o, p, pitch); }
}
}
