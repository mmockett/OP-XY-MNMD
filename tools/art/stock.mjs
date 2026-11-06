// The engines drawn in the stock look (OP-XY-Dev media/stock-look-2026-11-06 and
// media/stock-look-audit/mnmd): their views draw the whole screen, as MD KIT's
// do, from the controls; the design's parts and labels give way. These are the
// fixed parts each view places, in its order (the voices/*_view.hpp enums), and
// the palette they share.
//
// The grey is the knob: a part's grey says which encoder moves it, dark to
// light, 191919 or hollow (black with a CDCDCD edge) for encoder 1, then 4B4B4B,
// 969696 and CDCDCD; 7D7D7D, 646464 and 323232 belong to no encoder. Lines are
// 1 to 1.2 px. The paths draw at scale 2, where an entry's stroke width doubles,
// so their entries are at half width.
//
// A part: {name, style, pivot: [x, y], closed, pts} or {name, style, pivot, r}
// (a circle about the pivot), in stage coordinates. The first 27 take the common
// nodes' places (md_view.hpp: nodes 4 .. 30), which md::view::begin hides every
// frame; any more follow.

// [fill, stroke, width], in md_view.hpp's stock::Tone order.
export const STOCK_PALETTE = [
  ['none', '#CDCDCD', 0.6], ['none', '#969696', 0.5], ['none', '#4B4B4B', 0.5],
  ['none', '#7D7D7D', 0.5], ['none', '#646464', 0.5], ['none', '#323232', 0.5],
  ['none', '#CDCDCD', 1], ['none', '#646464', 1],
  ['#4B4B4B', '#4B4B4B', 0.1], ['#969696', '#969696', 0.1], ['#CDCDCD', '#CDCDCD', 0.1],
  ['#646464', '#646464', 0.1], ['#323232', '#323232', 0.1],
  ['#000000', '#CDCDCD', 1], ['#000000', '#000000', 0.1], ['#000000', '#646464', 1],
];
export const STOCK_SHADE = { line4: 0, line3: 1, line2: 2, line_n: 3, line_d: 4, line_a: 5, edge4: 6, edge_n: 7,
                             solid2: 8, solid3: 9, solid4: 10, solid_n: 11, solid_a: 12, hollow: 13, black: 14, hollow_n: 15 };

const r1 = v => Math.round(v * 100) / 100;
const rect = (x, y, w, h) => [[x, y], [x + w, y], [x + w, y + h], [x, y + h]].map(([a, b]) => [r1(a), r1(b)]);
// The doc's handle: a rounded square, `s` across, corners of radius `r`.
const handle = (s = 6, r = 1.5) => {
  const out = [], h = s / 2;
  const corners = [[h - r, -h + r, -Math.PI / 2], [h - r, h - r, 0], [-h + r, h - r, Math.PI / 2], [-h + r, -h + r, Math.PI]];
  for (const [cx, cy, a0] of corners)
    for (let k = 0; k <= 2; k++) { const a = a0 + (Math.PI / 2) * k / 2; out.push([r1(cx + r * Math.cos(a)), r1(cy + r * Math.sin(a))]); }
  return out;
};
// A straight line `len` px long and 1 px wide, about its middle, along +x: a
// filled sliver, so it has area and its own fill.
const bar = (len, w = 1) => rect(-len / 2, -w / 2, len, w);

function parts(list) {
  const out = [];
  // Each part's points are about its own origin, where the view places it (the
  // pivot is only a note of where that usually is); a lead is drawn in place, at 0, 0.
  const add = (name, style, at, pts, closed = true) => out.push({ name, style, pivot: [0, 0], at, pts, closed });
  const ring = (name, style, at, r) => out.push({ name, style, pivot: [0, 0], at, r });
  list(add, ring);
  return out;
}

// SuperWave's rows (voices/swave_view.hpp, W1): black masks over the turns the
// rows' serpentines make at the screen's edges and where the two halves meet, then
// the code on handles at the rows' ends.
const mask = add => {
  add('mask_mid', 'black', [240, 94], rect(-0.8, -72, 1.6, 144));
  add('mask_l', 'black', [0.7, 94], rect(-0.7, -72, 1.4, 144));
  add('mask_r', 'black', [479.3, 94], rect(-0.7, -72, 1.4, 144));
};
const saw = parts(add => {
  mask(add);
  for (let k = 0; k < 7; k++) add(`left${k}`, k === 1 || k === 2 || k === 4 ? 'hollow' : 'solid3', [9, 94], handle());
  for (let k = 0; k < 4; k++) add(`width${k}`, 'solid2', [471, 94], handle());
});
const pulse = parts(add => {
  mask(add);
  for (let k = 0; k < 4; k++) add(`left${k}`, k < 2 ? 'hollow' : 'solid3', [9, 94], handle());
  for (let k = 0; k < 2; k++) add(`width${k}`, 'solid2', [471, 94], handle());
  add('reset', 'solid3', [2.25, 93.5], rect(-1.25, -63.5, 2.5, 127));
});

// SID-6581 (S1): the axis, PW's hollow handle at the pulse's fall, PWAD's dark
// handle at the sweep's reach (its bar is a path), PWRS's reset bar.
const sid = parts(add => {
  add('axis', 'solid_a', [240, 94], bar(480));
  add('pw', 'hollow', [240, 94], handle());
  add('pwad', 'solid2', [240, 94], handle());
  add('reset', 'solid3', [2.25, 94], rect(-1.25, -60, 2.5, 120));
});

// FM+ (F1): the operator diagram without letters or arrowheads. A bundle's lines
// run from a modulator's centre to the carrier's, their ends hidden under the
// operators drawn after them, so they need not reshape as an operator resizes.
const len = (a, b) => Math.hypot(b[0] - a[0], b[1] - a[1]);
const stat = parts((add, ring) => {
  const m1 = [84, 58], m2 = [84, 138], c = [198, 98];
  for (let k = 0; k < 5; k++) add(`b1_${k}`, 'solid3', m1, bar(len(m1, c)));
  for (let k = 0; k < 5; k++) add(`b2_${k}`, 'solid2', m2, bar(len(m2, c)));
  ring('m1', 'hollow', m1, 15.5);
  add('pointer', 'solid2', m1, rect(-1.1, -8, 2.2, 8));
  ring('m2', 'hollow', m2, 12.5);
  ring('carrier', 'solid_a', c, 22);
  add('axis', 'solid_a', [350, 98], bar(260));
  add('loop_end', 'solid4', m1, handle(5, 1));
});
const par = parts((add, ring) => {
  const ys = [40, 96, 152], styles = ['solid2', 'solid4', 'solid2'];
  for (let j = 0; j < 3; j++) for (let k = 0; k < 5; k++) add(`b${j + 1}_${k}`, styles[j], [119, ys[j]], bar(98));
  ring('m1', 'hollow', [70, 40], 10);
  ring('m2', 'solid3', [70, 96], 10);
  ring('m3', 'hollow', [70, 152], 10);
  for (let j = 0; j < 3; j++) ring(`c${j + 1}`, 'solid_a', [168, ys[j]], 12);
  // The carriers' leads to the output, 1 px wide.
  const lead = (y, pts) => add(`lead${y}`, 'solid_a', [0, 0], pts);
  lead(40, [[180, 39.5], [204.3, 39.5], [222.5, 95.8], [221.5, 96.2], [203.7, 40.5], [180, 40.5]]);
  lead(96, rect(180, 95.5, 42, 1));
  lead(152, [[180, 151.5], [203.7, 151.5], [221.5, 95.8], [222.5, 96.2], [204.3, 152.5], [180, 152.5]]);
  add('axis', 'solid_a', [351, 96], bar(258));
});
const dyn = parts((add, ring) => {
  const m1 = [78, 58], m2 = [78, 138], c = [204, 98];
  for (let k = 0; k < 5; k++) add(`b1_${k}`, 'solid3', m1, bar(len(m1, c)));
  for (let k = 0; k < 5; k++) add(`b2_${k}`, 'solid2', m2, bar(len(m2, c)));
  ring('m1', 'hollow', m1, 14);
  ring('m2', 'hollow', m2, 11.5);
  ring('carrier', 'solid_a', c, 22);
  add('axis', 'solid_a', [353, 98], bar(254));
  add('loop_end', 'solid3', m2, handle(5, 1));
});

// GND-SIN (G1, one row, stage from y 30): the axis under the orb, the orb hollow
// in a neutral grey, its radius, and the tip's handle in TUNE's grey. GND-NOIS: the axis.
const sin = parts((add, ring) => {
  add('axis', 'solid_a', [240, 110], bar(480));
  ring('orb', 'hollow_n', [92, 110], 46);
  add('radius', 'solid_n', [92, 110], rect(0, -0.5, 46, 1));
  add('tip', 'solid4', [92, 110], handle());
});
const nois = parts(add => {
  add('axis', 'solid_a', [240, 94], bar(480));
});

export const STOCK = { mmsaw: saw, mmpulse: pulse, mmsid: sid, mmstat: stat, mmpar: par, mmdyn: dyn, mmsin: sin, mmnois: nois };
for (const [engine, list] of Object.entries(STOCK))
  if (list.length > 27) throw new Error(`stock.mjs: ${engine} has ${list.length} parts, more than the 27 common places`);
