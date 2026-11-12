// The drum kits' screens: the sounding part of each instrument drawn large in
// section, as on an engraved patent plate, the whole screen (docs/SCREEN.md,
// "The kits' sections"). The view
// (voices/mdkit_view.hpp) draws the sections themselves on the four paths;
// these are the few fixed parts it places, moves, turns and hides, shared by
// every machine that draws them.
//
// The artwork may hold 64 KB and the instruments on the left take most of it,
// so these stay few and small, in whole or half pixels. The first ones take
// the common nodes' places (md_view.hpp's rings, handles, cursor and readout,
// which a kit drawn this way never uses); the rest follow the instruments.
//
// A part: {name, style, pivot: [x, y], closed, pts} or {name, style, pivot, r}
// (a circle about the pivot), in stage coordinates. Styles are
// tools/art/build.mjs's shades.

const parts = [];
const add = (name, style, pivot, pts, closed = true) => parts.push({ name, style, pivot, pts, closed });
const ring = (name, style, pivot, r) => parts.push({ name, style, pivot, r });
const rect = (x, y, w, h) => [[x, y], [x + w, y], [x + w, y + h], [x, y + h]];
const arc = (r, a0, a1, n = 6) => Array.from({ length: n + 1 }, (_, i) => {
  const a = a0 + (a1 - a0) * i / n;
  return [Math.round(r * Math.cos(a) * 2) / 2, Math.round(r * Math.sin(a) * 2) / 2];
});

// MD KIT (and MD E12) in the stock look (K3, OP-XY-Dev media/stock-look-audit/mnmd): the
// grey says which encoder moves a part, the drum kit's slots dark to light (the
// body hollow, the rings dark, the striker mid, the hardware light); 646464 and
// 323232 belong to no encoder, and a drum's shell is one flat 323232 solid with
// its seam knocked out in black. Lines are 1 px: the paths draw at scale 2, so
// their entries are at half width. [fill, stroke, width], in voices/mdkit_view.hpp's
// fig::Tone order.
export const MDKIT_PALETTE = [
  ['none', '#CDCDCD', 0.5], ['none', '#4B4B4B', 0.5], ['none', '#969696', 0.5], ['none', '#646464', 0.5],
  ['none', '#CDCDCD', 1], ['none', '#4B4B4B', 1], ['none', '#969696', 1], ['none', '#646464', 1],
  ['#CDCDCD', '#CDCDCD', 0.1], ['#4B4B4B', '#4B4B4B', 0.1], ['#969696', '#969696', 0.1], ['#646464', '#646464', 0.1],
  ['#323232', '#323232', 0.1], ['#000000', '#000000', 0.1], ['#000000', '#CDCDCD', 1], ['#000000', '#323232', 1],
];
export const MDKIT_SHADE = { p_light: 0, p_dark: 1, p_mid: 2, p_neutral: 3, light: 4, dark: 5, mid: 6, neutral: 7,
                             f_light: 8, f_dark: 9, f_mid: 10, f_neutral: 11, f_shell: 12, black: 13, hollow: 14, lug: 15 };

// Rings of sound leaving an edge: four each side, facing out, in encoder 2's grey.
for (let k = 0; k < 4; k++) {
  const r = 7 + 6 * k;
  add(`ring_r${k}`, 'dark', [0, 0], arc(r, -0.75, 0.75), false);
  add(`ring_l${k}`, 'dark', [0, 0], arc(r, Math.PI - 0.75, Math.PI + 0.75), false);
}
// A cymbal's rod, its clutch or wing nut, and the hand that grabs an edge or
// damps a bell's mouth (thumb above, fingers below, pinching to the left).
add('rod', 'f_neutral', [0, 0], rect(-1, -84, 2, 168));
add('clutch', 'f_light', [0, 0], rect(-6, -3.5, 12, 7));
add('hand', 'f_light', [0, 0], [[-2, -9], [6, -12], [16, -11], [24, -6], [26, 2], [22, 10], [12, 13], [3, 11], [-2, 6],
                                [4, 4.5], [12, 5], [16, 1], [14, -4], [6, -5], [-1, -3.5]]);
// A felt damper.
add('felt', 'f_light', [0, 0], [[-13, -3], [13, -3], [15, 0], [13, 3], [-13, 3], [-15, 0]]);
// The echo chamber's loudspeaker and ribbon microphone on its stand.
add('speaker', 'f_mid', [0, 0], [[-16, -15], [-4, -15], [-4, -9], [4, -16], [4, 16], [-4, 9], [-4, 15], [-16, 15]]);
add('mic', 'f_neutral', [0, 0], rect(-6, -12, 12, 18));
add('stand', 'f_neutral', [0, 0], [[-0.8, 6], [0.8, 6], [0.8, 68], [10, 68], [10, 70], [-10, 70], [-10, 68], [-0.8, 68]]);
// Wavefronts of a clap, facing right from the pivot.
for (let k = 0; k < 4; k++) add(`front${k}`, 'light', [0, 0], arc(24, -0.85, 0.85), false);
// A shaker's beads.
for (let k = 0; k < 8; k++) ring(`bead${k}`, 'hollow', [0, 0], 2.6);

// The common nodes' places they take (md_view.hpp: nodes 4 .. 30), all of them.
export const COMMON_SLOTS = 27;
if (parts.length !== COMMON_SLOTS) throw new Error(`fig.mjs: ${parts.length} parts for ${COMMON_SLOTS} places`);
// The hardware that tells one machine and one family from another follows
// the instruments' place (art_first on): TRX's tube lugs, EFM's exciter coil
// and magnet, P-I's rope tensioning and felt washers, a rack tom's mount, a
// floor tom's legs, a kick's pillow.
const lug = [[-2.5, -13], [2.5, -13], [3, -11], [3, 11], [2.5, 13], [-2.5, 13], [-3, 11], [-3, -11]];
add('lug_l', 'lug', [0, 0], lug);
add('lug_r', 'lug', [0, 0], lug);
const coil = [[0, -18]];
for (let k = 0; k < 9; k++) coil.push([k & 1 ? -4.5 : 4.5, -14 + 3.5 * k]);
coil.push([0, 18]);
add('coil', 'neutral', [0, 0], coil, false);
add('magnet', 'f_neutral', [0, 0], rect(-9, -3.5, 18, 7));
const rope = [];
for (let k = 0; k <= 8; k++) rope.push([k & 1 ? 3 : -3, -20 + 5 * k]);
add('rope_l', 'neutral', [0, 0], rope, false);
add('rope_r', 'neutral', [0, 0], rope, false);
add('mount', 'f_neutral', [0, 0], [[0, 0], [16, 0], [16, -3], [3, -3], [3, -20], [0, -20]]);
add('leg_l', 'f_neutral', [0, 0], [[0, 0], [3, 0], [-8, 26], [-11, 26]]);
add('leg_r', 'f_neutral', [0, 0], [[0, 0], [-3, 0], [8, 26], [11, 26]]);
add('pillow', 'f_neutral', [0, 0], [[-34, 0], [-30, -7], [-14, -11], [14, -11], [30, -7], [34, 0]]);
add('felt2', 'f_light', [0, 0], [[-11, -2.5], [11, -2.5], [13, 0], [11, 2.5], [-11, 2.5], [-13, 0]]);
// A spray's strokes, each its own small closed part (a spray on one reshaping
// path broke up on the device), pointing right from the pivot.
for (let k = 0; k < 8; k++) add(`spark${k}`, 'dark', [0, 0], [[0, -0.6], [11, -0.3], [11, 0.3], [0, 0.6]]);
// A clap's rings behind its fronts, and its fronts harder: halfway to a chevron, then one.
for (let k = 0; k < 4; k++) add(`echo${k}`, 'light', [0, 0], arc(24, -0.85, 0.85), false);
const front = hard => Array.from({ length: 7 }, (_, i) => {
  const r = 24, ap = 0.85, a = -ap + 2 * ap * i / 6;
  const x = r * Math.cos(a) * (1 - hard) + (r * Math.cos(ap) + r * (1 - Math.cos(ap)) * (1 - Math.abs(a) / ap) * 2.6) * hard;
  return [Math.round(x * 2) / 2, Math.round(r * Math.sin(a) * 2) / 2];
});
for (let k = 0; k < 4; k++) add(`frontb${k}`, 'light', [0, 0], front(0.5), false);
for (let k = 0; k < 4; k++) add(`frontc${k}`, 'light', [0, 0], front(1), false);
// What a bell hangs from: short, so it stops 28 px under the header.
add('hanger', 'f_neutral', [0, 0], rect(-1, -26, 2, 26));
// A drum's distortion clamps: brackets that close on the head's ends from above
// and below, open away from it.
for (const [name, dir] of [['clamp_tl', -1], ['clamp_tr', -1], ['clamp_bl', 1], ['clamp_br', 1]])
  add(name, 'light', [0, 0], [[-4, 4 * dir], [-4, 0], [4, 0], [4, 4 * dir]], false);
// The hoops that hold the head, light, each on a post down to the head.
add('hoop_l', 'f_light', [0, 0], rect(-4, -3, 8, 6));
add('hoop_r', 'f_light', [0, 0], rect(-4, -3, 8, 6));
add('post_l', 'f_light', [0, 0], rect(-0.5, 0, 1, 8.5));
add('post_r', 'f_light', [0, 0], rect(-0.5, 0, 1, 8.5));
// A beater's or mallet's ball, solid in the striker's grey with its eye knocked
// out; HARM's second ball; the pivot it swings from.
ring('ball', 'f_mid', [0, 0], 8);
ring('eye', 'black', [0, 0], 2.4);
ring('ball2', 'f_mid', [0, 0], 3);
add('pivot', 'f_neutral', [0, 0], [[-3, -4], [3, -4], [4, -3], [4, 3], [3, 4], [-3, 4], [-4, 3], [-4, -3]]);
// NOIS: grains among the rings, in encoder 2's grey.
for (let k = 0; k < 8; k++) ring(`grain${k}`, 'f_dark', [0, 0], 1.3);
// A drum's shell, one flat solid with its seam knocked out: each wall an outer
// and an inner strip of segments 16 px long (as many as its depth needs, placed
// down it, overlapping), a cap bridging the seam at its top, and the floor's two
// strips, each of two 125 px pieces from its ends.
for (const side of ['l', 'r'])
  for (const strip of ['o', 'i'])
    for (let k = 0; k < 6; k++) add(`wall_${side}${strip}${k}`, 'f_shell', [0, 0], rect(-1.65, 0, 3.3, 16));
add('cap_l', 'f_shell', [0, 0], rect(-0.75, 0, 1.5, 6));
add('cap_r', 'f_shell', [0, 0], rect(-0.75, 0, 1.5, 6));
add('floor_o0', 'f_shell', [0, 0], rect(0, -1.65, 125, 3.3));
add('floor_o1', 'f_shell', [0, 0], rect(-125, -1.65, 125, 3.3));
add('floor_i0', 'f_shell', [0, 0], rect(0, -1.65, 125, 3.3));
add('floor_i1', 'f_shell', [0, 0], rect(-125, -1.65, 125, 3.3));
const kit = parts.splice(0);

// ---- MD E12: a strip of tape on a splicing block, seen from above ----
// The block with its channel, its 45 and 90 degree slots and a razor blade
// resting by the cut; splicing-tape patches over the joins; and the diagonal
// splice where a second sample's tape takes over. The tape, its recorded
// stripes and the cut-off scrap are paths. voices/mdkit_view.hpp's fig_e12::Part
// names them in this order. In MD KIT's stock look and palette: the block and
// its fittings belong to no encoder (a 646464 outline, its rails, slots, blade
// and notches 646464 solids), the patches are RTRG's (encoder 2's 4B4B4B
// solid), and the second sample's splice the fourth control's (CDCDCD).
add('e12_block', 'neutral', [0, 0], [[-150, -40], [150, -40], [152, -38], [152, 38], [150, 40], [-150, 40], [-152, 38], [-152, -38]]);
add('e12_ch_top', 'f_neutral', [0, 0], rect(-144, -0.6, 288, 1.2));
add('e12_ch_bot', 'f_neutral', [0, 0], rect(-144, -0.6, 288, 1.2));
add('e12_slot45', 'f_neutral', [0, 0], [[-40, 40], [-38, 40], [42, -40], [40, -40]]);
add('e12_slot90', 'f_neutral', [0, 0], rect(-1, -40, 2, 80));
add('e12_blade', 'f_neutral', [0, 0], [[-8, -36], [8, -36], [8, -6], [3, -6], [3, -2], [-3, -2], [-3, -6], [-8, -6]]);
for (let k = 0; k < 4; k++) add(`e12_patch${k}`, 'f_dark', [0, 0], rect(-3, -22, 6, 44));
add('e12_diag', 'light', [0, 0], [[-20, 20], [-19, 20], [21, -20], [20, -20]]);
// The block's mark for each machine: four notches filed in its right end, as a binary gauge.
for (let k = 0; k < 4; k++) add(`e12_notch${k}`, 'f_neutral', [0, 0], rect(-1.5, -4, 3, 8));
const e12 = parts.splice(0);

// Each kit drawn this way, and its parts.
export const FIGS = { mdkit: kit, mde12: e12 };
export const FIG = kit;
