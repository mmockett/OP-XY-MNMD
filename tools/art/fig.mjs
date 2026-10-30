// The drum kits' right side: the sounding part of each instrument drawn large
// in section, as on an engraved patent plate, beside the playing drawing on
// the left (docs/SCREEN.md, "The kits' right side"). The view
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

// Rings of sound leaving an edge: three each side, facing out.
for (let k = 0; k < 3; k++) {
  const r = 7 + 6 * k;
  add(`ring_r${k}`, k < 2 ? 'mid' : 'dim', [0, 0], arc(r, -0.75, 0.75), false);
  add(`ring_l${k}`, k < 2 ? 'mid' : 'dim', [0, 0], arc(r, Math.PI - 0.75, Math.PI + 0.75), false);
}
// A cymbal's rod, its clutch or wing nut, and the hand that grabs an edge or
// damps a bell's mouth (thumb above, fingers below, pinching to the left).
add('rod', 'dim', [0, 0], rect(-1, -84, 2, 168));
add('clutch', 'o2', [0, 0], rect(-6, -3.5, 12, 7));
add('hand', 'o2', [0, 0], [[-2, -9], [6, -12], [16, -11], [24, -6], [26, 2], [22, 10], [12, 13], [3, 11], [-2, 6],
                           [4, 4.5], [12, 5], [16, 1], [14, -4], [6, -5], [-1, -3.5]]);
// A drum's distortion clamps, 160 px long, and a felt damper.
add('clamp_t', 'o2', [0, 0], rect(-80, -1.5, 160, 3));
add('clamp_b', 'o2', [0, 0], rect(-80, -1.5, 160, 3));
add('felt', 'o2', [0, 0], [[-13, -3], [13, -3], [15, 0], [13, 3], [-13, 3], [-15, 0]]);
// The echo chamber's loudspeaker and ribbon microphone on its stand.
add('speaker', 'o2', [0, 0], [[-16, -15], [-4, -15], [-4, -9], [4, -16], [4, 16], [-4, 9], [-4, 15], [-16, 15]]);
add('mic', 'o2', [0, 0], rect(-6, -12, 12, 18));
add('stand', 'mid', [0, 0], [[-0.8, 6], [0.8, 6], [0.8, 68], [10, 68], [10, 70], [-10, 70], [-10, 68], [-0.8, 68]]);
// Wavefronts of a clap, facing right from the pivot.
for (let k = 0; k < 4; k++) add(`front${k}`, k ? 'mid' : 'thin', [0, 0], arc(12, -0.85, 0.85), false);
// A shaker's beads.
for (let k = 0; k < 8; k++) ring(`bead${k}`, 'f4', [0, 0], 2.6);

export const FIG = parts;
// The common nodes' places they take (md_view.hpp: nodes 4 .. 30), all of them.
export const COMMON_SLOTS = 27;
if (parts.length !== COMMON_SLOTS) throw new Error(`fig.mjs: ${parts.length} parts for ${COMMON_SLOTS} places`);
