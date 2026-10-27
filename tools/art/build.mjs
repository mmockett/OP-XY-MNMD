// Writes an engine's screen artwork from the design: every machine it holds is
// rigged (rig.mjs), its parts merged where they move together, its labels
// drawn in the stroke font, and the result written as
//   machines/<engine>/panel.svg.fragment   the nodes, paths, text slots and palette
//   machines/<engine>/art.h                each part's pivot, shade, kind and effects
//
//   node tools/art/build.mjs <engine> mm:SID-6581
//   node tools/art/build.mjs --pads <kit> md:TRX-BD ...   a drum kit, a machine at a time
import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { rig, flatten } from './rig.mjs';
import { strokes } from './font.mjs';
import { readFileSync } from 'node:fs';

// The verified slots (tools/catalog.py); the design's spellings where they differ.
const CATALOG = JSON.parse(readFileSync(path.join(path.dirname(fileURLToPath(import.meta.url)), '..', 'catalog.json'), 'utf8'));
const DESIGN_NAME = { 'PI-MT': 'PI-XT' };
const DESIGN_LABEL = { NOISE: 'NOIS', SPLEN: 'SPLN', START: 'STRT', RRTL: 'RATL' };

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');

// Palette, in md_view.hpp's Shade order: [fill, stroke, width].
export const PALETTE = [
  ['none', '#f7f5f5', 1.6], ['none', '#f7f5f5', 1], ['none', '#a8a9ad', 1.2], ['none', '#606166', 1],
  ['none', '#3a3b3f', 1], ['none', '#7a7b80', 1], ['none', '#8c8d92', 0.9],
  ['#1c1d20', '#1c1d20', 0.1], ['#3a3b40', '#3a3b40', 0.1], ['#66676c', '#66676c', 0.1], ['#a8a9ad', '#a8a9ad', 0.1], ['#f7f5f5', '#f7f5f5', 0.1],
  ['#000000', '#f7f5f5', 1.4], ['#1c1d20', '#f7f5f5', 1.2], ['#3a3b40', '#c9cacd', 1.1], ['#818287', '#f7f5f5', 1.1],
];
const SHADE = { line: 0, thin: 1, mid: 2, dim: 3, faint: 4, dash: 5, label: 6, f1: 7, f2: 8, f3: 9, f4: 10, f5: 11, node: 11,
                o0: 12, o1: 13, o2: 14, o3: 15 };
const FILLED = s => SHADE[s] >= 7;
const KIND = { rest: 0, struck: 1, hit: 2 };
const COMMON_NODES = 4 + 3 + 3 + 21;   // md_view.hpp: 4 paths, 3 rings, 2 handles, a cursor and the readout's 21 segments

const num = v => { const s = (Math.round(v * 10) / 10).toFixed(1).replace(/\.0$/, ''); return s === '-0' ? '0' : s; };

// Dashes for the 'dash' style: the device draws no dash patterns, so cut the line.
function dashed(polys) {
  const out = [];
  for (const p of polys) {
    const pts = p.closed ? [...p.pts, p.pts[0]] : p.pts;
    let on = true, left = 3, cur = [pts[0]];
    for (let k = 1; k < pts.length; k++) {
      let [x0, y0] = pts[k - 1];
      const [x1, y1] = pts[k];
      let seg = Math.hypot(x1 - x0, y1 - y0);
      while (seg > left) {
        const t = left / seg;
        x0 += (x1 - x0) * t; y0 += (y1 - y0) * t; seg -= left;
        if (on) { cur.push([x0, y0]); out.push({ closed: false, pts: cur }); }
        on = !on; left = 3; cur = [[x0, y0]];
      }
      left -= seg;
      if (on) cur.push([x1, y1]);
    }
    if (on && cur.length > 1) out.push({ closed: false, pts: cur });
  }
  return out;
}

function simplify(pts, tol) {
  if (pts.length < 3) return pts;
  // A loop whose ends meet: simplify each half from the point farthest from the ends.
  const [fx, fy] = pts[0], [lx, ly] = pts[pts.length - 1];
  if (Math.hypot(fx - lx, fy - ly) < 1e-6 && pts.length > 3) {
    let far = 1, best = -1;
    for (let k = 1; k < pts.length - 1; k++) {
      const d = Math.hypot(pts[k][0] - fx, pts[k][1] - fy);
      if (d > best) { best = d; far = k; }
    }
    return [...simplify(pts.slice(0, far + 1), tol).slice(0, -1), ...simplify(pts.slice(far), tol)];
  }
  const keep = new Uint8Array(pts.length); keep[0] = keep[pts.length - 1] = 1;
  const stack = [[0, pts.length - 1]];
  while (stack.length) {
    const [a, b] = stack.pop();
    let best = -1, far = tol;
    const [ax, ay] = pts[a], [bx, by] = pts[b], len = Math.hypot(bx - ax, by - ay) || 1e-9;
    for (let k = a + 1; k < b; k++) {
      const dist = Math.abs((bx - ax) * (ay - pts[k][1]) - (ax - pts[k][0]) * (by - ay)) / len;
      if (dist > far) { far = dist; best = k; }
    }
    if (best >= 0) { keep[best] = 1; stack.push([a, best], [best, b]); }
  }
  return pts.filter((_, k) => keep[k]);
}

function bounds(polys) {
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (const p of polys) for (const [x, y] of p.pts) { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
  return { cx: (x0 + x1) / 2, cy: (y0 + y1) / 2 };
}

// A path's d relative to its pivot, M/L/Z only.
function pathData(polys, cx, cy) {
  let d = '';
  for (const p of polys) {
    const pts = simplify(p.pts, 0.6);
    if (pts.length < 2) continue;
    d += 'M' + pts.map(([x, y], k) => (k ? 'L' : '') + num(x - cx) + ' ' + num(y - cy)).join(' ').replace(/^/, '') + (p.closed ? ' Z ' : ' ');
  }
  return d.trim();
}

// Quantize an effect so parts that move together can merge.
const sig = e => [e.control, e.end, e.absent ? 'x' : e.only ? 'o' : [num(e.dx), num(e.dy), (e.s ?? 1).toFixed(3), (e.turn ?? 0).toFixed(3)].join(',')].join(':');
const pure = effects => effects.every(e => e.absent || e.only || (Math.abs((e.s ?? 1) - 1) < 0.002 && Math.abs(e.turn ?? 0) < 0.002));

// One subpath per path, at most 64 points: the OP-XY's SVG parser takes no more
// (every machine known to run on the device draws this way). A part's shape is
// split into several nodes that share its pivot and its movement.
const MAX_POINTS = 64;
function subpaths(shape) {
  const out = [];
  for (const p of shape) {
    const pts = simplify(p.pts, 0.6);
    if (pts.length < 2) continue;
    if (p.closed && pts.length <= MAX_POINTS) { out.push({ closed: true, pts }); continue; }
    const line = p.closed ? [...pts, pts[0]] : pts;
    for (let i = 0; i < line.length - 1; i += MAX_POINTS - 1) out.push({ closed: false, pts: line.slice(i, i + MAX_POINTS) });
  }
  return out;
}

// Layers a kit's per-pad screen draws on its paths rather than as nodes: a
// view animates them, or places the design's node markers itself.
const PADS_FREE = ['mid#nl', 'line#pc', 'line#wc', 'node'];
// Instrument layers whose struck pose a kit's per-pad screen leaves out, for
// nodes, or because the pose drops part of the layer (CP's lit hand) or loses
// its controls (the hats' gap ticks, which instead ride the top plate).
const PADS_STILL = ['f4#nd', 'mid#ri', 'o1#rc', 'f5#gt'];
// A kit's per-pad screen moves the instrument itself on a hit, as one object:
// drum heads flex inside their hoops; a cowbell swings from the top of its rod
// and a triangle from its string; a cymbal's edge, bell and nut tilt with its
// bow; the hats' top gap tick rides the top plate.
const PADS_FLEX = ['o0#head', 'o0#hds', 'o0#hdt', 'o0#hdc'];
const PADS_SWING = [{ from: 'mid#rod', parts: ['mid#rod', 'o2#side', 'o3#top', 'o1#front', 'o3#clamp', 'dim#bd'], turn: 0.07 },
                    { from: 'dim#str', parts: ['dim#str', 'line#tri'], turn: 0.09 }];
const PADS_RIDE = [{ on: 'o1#bow', parts: ['thin#edge', 'o3#bell', 'f4#nut'] }];
// A hit that moves no point of the instrument PADS_SUBTLE pixels (claves,
// rimshot, cymbals, hats, brush, tambourine ...) is scaled up to move one
// PADS_REACH, by at most PADS_LIFT; a node scales by 2 at most (the hats' gap
// line stops there). One with a struck shape of its own (maracas, shaker)
// already shows its hit.
const PADS_SUBTLE = 16, PADS_REACH = 20, PADS_LIFT = 4;
function padStrikes(parts) {
  for (const { from, parts: keys, turn } of PADS_SWING) {
    const hung = parts.filter(p => keys.includes(p.key)), rod = hung.find(p => p.key === from);
    if (!rod || !hung.some(p => p !== rod)) continue;
    let top = rod.shape[0].pts[0];
    for (const poly of rod.shape) for (const q of poly.pts) if (q[1] < top[1]) top = q;
    const c = Math.cos(turn), s = Math.sin(turn);
    for (const p of hung) {
      const b = bounds(p.shape), x = b.cx - top[0], y = b.cy - top[1];
      p.strike = { dx: c * x - s * y - x, dy: s * x + c * y - y, s: 1, turn };
    }
  }
  for (const { on, parts: keys } of PADS_RIDE) {
    const host = parts.find(p => p.key === on);
    if (!host?.strike) continue;
    const h = bounds(host.shape), { dx, dy, s, turn } = host.strike, c = Math.cos(turn), sn = Math.sin(turn);
    for (const p of parts.filter(p => keys.includes(p.key))) {
      const b = bounds(p.shape), x = b.cx - h.cx, y = b.cy - h.cy;
      p.strike = { dx: dx + s * (c * x - sn * y) - x, dy: dy + s * (sn * x + c * y) - y, s, turn };
    }
  }
  const plate = parts.find(p => p.key === 'o1#tp'), ticks = parts.filter(p => p.key === 'f5#gt');
  if (plate?.strike && ticks.length)
    ticks.reduce((a, b) => (b.bounds.cy < a.bounds.cy ? b : a)).strike = { dx: 0, dy: plate.strike.dy, s: 1, turn: 0 };
}
function flexHeads(parts) {
  for (const p of parts.filter(p => PADS_FLEX.includes(p.key))) {
    const s = p.strike ?? { dx: 0, dy: 0, s: 1, turn: 0 };
    p.strike = { ...s, s: s.s * 1.06 };
  }
}
function liftStrikes(parts) {
  let reach = 0;
  for (const p of parts.filter(p => p.strike)) {
    const b = bounds(p.shape), { dx, dy, s, turn } = p.strike, c = Math.cos(turn), sn = Math.sin(turn);
    for (const poly of p.shape) for (const [x, y] of poly.pts) {
      const u = x - b.cx, v = y - b.cy;
      reach = Math.max(reach, Math.hypot(dx + s * (c * u - sn * v) - u, dy + s * (sn * u + c * v) - v));
    }
  }
  if (!reach || reach >= PADS_SUBTLE) return;
  const k = Math.min(PADS_LIFT, PADS_REACH / reach);
  for (const p of parts.filter(p => p.strike)) {
    const { dx, dy, s, turn } = p.strike;
    p.strike = { dx: dx * k, dy: dy * k, s: Math.min(2, 1 + (s - 1) * k), turn: turn * k };
  }
}

export function build(engine, members, pads = false) {
  const nodes = [], machines = [], texts = [];
  for (const [instrument, name] of members) {
    const r = rig(instrument, DESIGN_NAME[name] ?? name);
    if (pads) {
      // One machine at a time, full size: no labels (static text shows for every
      // machine) and no sparks, so a kit's machines fit the device's nodes.
      // The panel's struck poses and its noise are drawn live by the view.
      r.texts = []; r.free = r.free.filter(f => !f.key.startsWith('text:'));
      r.hit = r.hit.filter(h => h.struck && bounds(h.shape).cx < 236 && !PADS_FREE.includes(h.key) && !PADS_STILL.includes(h.key));
      r.free.push(...r.parts.filter(p => PADS_FREE.includes(p.key)).map(p => ({ key: p.key, controls: [] })));
      r.parts = r.parts.filter(p => !PADS_FREE.includes(p.key));
      padStrikes(r.parts);
      if (!r.hit.length) liftStrikes(r.parts);
      flexHeads(r.parts);
    }
    // Effects and defaults by the machine's own slots, not the design's order.
    const slots = CATALOG.machines[name].slots;
    const slotOf = r.labels.map(l => slots.indexOf(DESIGN_LABEL[l] ?? l));
    r.labels.forEach((l, k) => { if (slotOf[k] < 0) throw new Error(`${name}: the design's ${l} is not on the machine`); });
    const slotted = effects => effects.map(e => ({ ...e, control: slotOf[e.control] }));
    for (const p of r.parts) p.effects = slotted(p.effects);
    const defaults = new Array(8).fill(0);
    r.values.forEach((v, k) => { defaults[slotOf[k]] = v; });
    r.values = defaults;
    // Rest parts, then labels (rest, no effects), then the hit's parts.
    const pieces = r.parts.map(p => ({ kind: 'rest', style: p.style, shape: p.style === 'dash' ? dashed(p.shape) : p.shape,
                                       effects: p.effects, key: p.key, strike: p.strike, struckBy: r.hit.some(h => h.struck && h.key === p.key) }));
    // Fixed labels are static text; a label that moves is drawn in the stroke font.
    for (const t of r.texts) {
      if (!t.effects.length) { texts.push({ t: t.t, x: t.x, y: t.y, fs: t.fs, fill: '#8c8d92' }); continue; }
      pieces.push({ kind: 'rest', style: 'label', shape: strokes(t.t, t.x, t.y, t.fs).map(pts => ({ closed: false, pts })),
                    effects: slotted(t.effects), key: 'label' });
    }
    // A label like "gap 40" or "wave 3 → 4": its word is art; the view writes the number with the readout.
    for (const f of r.free.filter(f => f.key.startsWith('text:'))) {
      const m = f.key.slice(5).match(/^(\D*?)\s*\d[\d\s→]*$/);
      // Only a word: FM+'s "×3.5" leaves no "×3." behind.
      if (m && /[a-z]/i.test(m[1]) && !/[.\d]$/.test(m[1])) texts.push({ t: m[1], x: f.x, y: f.y, fs: 9, fill: '#8c8d92' });
      // One number between words ("chord · 3 voices"): the words are static text, the number's place left blank.
      const n = !m && f.key.slice(5).match(/^(.*?[a-z].*?)(\d+)(.*[a-z].*)$/i);
      if (n) {
        texts.push({ t: n[1].trimEnd(), x: f.x, y: f.y, fs: 9, fill: '#8c8d92' });
        texts.push({ t: n[3].trimStart(), x: f.x + (n[1].length + n[2].length + 1) * 9 * 0.5, y: f.y, fs: 9, fill: '#8c8d92' });
      }
    }
    for (const h of r.hit) {
      const rest = r.parts.find(p => p.key === h.key);
      pieces.push({ kind: h.struck ? 'struck' : 'hit', style: h.style, shape: h.style === 'dash' ? dashed(h.shape) : h.shape,
                    effects: rest ? rest.effects : [], motion: h.motion, key: h.key });
    }
    // Merge pieces that share a shade, a kind and identical pure-translation effects.
    const groups = new Map();
    for (const piece of pieces) {
      const strikeSig = piece.strike ? [num(piece.strike.dx), num(piece.strike.dy), piece.strike.s.toFixed(3), piece.strike.turn.toFixed(3)].join(',') : '';
      const mergeable = pure(piece.effects) && !piece.motion && !piece.struckBy && (!piece.strike || Math.abs(piece.strike.s - 1) < 0.002 && Math.abs(piece.strike.turn) < 0.002);
      const id = mergeable ? [piece.kind, piece.style, strikeSig, piece.effects.map(sig).sort().join('|')].join('/') : Symbol();
      if (!groups.has(id)) groups.set(id, { ...piece, shape: [...piece.shape] });
      else groups.get(id).shape.push(...piece.shape);
    }
    const mine = [];
    for (const g of groups.values()) {
      const b = bounds(g.shape);
      for (const sub of subpaths(g.shape)) {
        const node = { ...g, shape: [sub], cx: b.cx, cy: b.cy, d: pathData([sub], b.cx, b.cy) };
        // Machines drawn the same way share the node (CH and OH, XT and XC ...).
        const id = [node.kind, node.style, node.d, Math.round(node.cx * 4), Math.round(node.cy * 4), node.struckBy ? 1 : 0,
                    JSON.stringify(node.effects.map(sig).sort()), JSON.stringify(node.motion ?? null), JSON.stringify(node.strike ?? null)].join('/');
        let index = nodes.findIndex(n => n.id === id);
        if (index < 0) { index = nodes.length; nodes.push({ ...node, id }); }
        if (!mine.includes(index)) mine.push(index);
      }
    }
    machines.push({ name, instrument, nodes: mine, defaults: r.values, labels: r.labels, free: r.free });
  }
  return { engine, nodes, machines, texts };
}

function header(engine, art) {
  const lines = [];
  const effects = [];
  const parts = art.nodes.map(n => {
    const firstEffect = effects.length;
    // One record per control: its effect at each end, relative to the default.
    const byControl = new Map();
    for (const e of n.effects) {
      if (!byControl.has(e.control)) byControl.set(e.control, { control: e.control, ends: [null, null] });
      byControl.get(e.control).ends[e.end] = e;
    }
    for (const { control, ends } of byControl.values()) {
      const q = e => e && !e.absent && !e.only ? [Math.round(e.dx * 1024), Math.round(e.dy * 1024), Math.round((e.s ?? 1) * 1024),
                                       Math.round((e.turn ?? 0) * 1800 / Math.PI)] : [0, 0, 1024, 0];
      const lo = q(ends[0]), hi = q(ends[1]);
      const absent = (ends[0]?.absent ? 1 : 0) | (ends[1]?.absent ? 2 : 0) | (ends[0]?.only ? 4 : 0) | (ends[1]?.only ? 8 : 0);
      effects.push(`    {${control}, ${absent}, {${lo.join(', ')}}, {${hi.join(', ')}}},`);
    }
    const pose = t => t ? [Math.round(t.dx * 1024), Math.round(t.dy * 1024), Math.round(t.s * 1024), Math.round(t.turn * 1800 / Math.PI)] : [0, 0, 1024, 0];
    return `    {${Math.round(n.cx * 4)}, ${Math.round(n.cy * 4)}, ${SHADE[n.style]}, ${FILLED(n.style) ? 2 : 1}, ${KIND[n.kind]}, ${n.struckBy ? 1 : 0}, ` +
           `${firstEffect}, ${effects.length - firstEffect}, {${pose(n.motion).join(', ')}}, {${pose(n.strike).join(', ')}}},`;
  });
  lines.push('#pragma once');
  lines.push('// Generated by tools/art/build.mjs from the screen design (docs/design): edit the');
  lines.push('// design or the generator, then regenerate. md_rig.hpp reads these tables.');
  lines.push('#include "md_rig.hpp"', '', `namespace ${engine} {`, 'namespace art {');
  lines.push(`const opxy::u32 node_count = ${art.nodes.length};`);
  lines.push('// x, y (quarter pixels on the stage), shade, paint, kind, replaced when struck, effects,');
  lines.push('// hit motion (early to late), strike (rest to struck) {dx, dy, scale, turn}');
  lines.push(`const md::rig::Part parts[${art.nodes.length}] = {`, ...parts, '};');
  lines.push('// control, absent at (1 low, 2 high; 4 only low, 8 only high), low end {dx, dy, scale, turn}, high end');
  lines.push(`const md::rig::Effect effects[${Math.max(1, effects.length)}] = {`, ...(effects.length ? effects : ['    {0, 0, {0, 0, 1024, 0}, {0, 0, 1024, 0}},']), '};');
  const list = [];
  lines.push('// each machine\'s parts: first entry in members, count, the design defaults (0..127)');
  lines.push(`const md::rig::Machine machines[${art.machines.length}] = {`);
  for (const m of art.machines) {
    const d = m.defaults.slice(0, 8); while (d.length < 8) d.push(0);
    lines.push(`    {${list.length}, ${m.nodes.length}, {${d.join(', ')}}},   // ${m.name}${m.free.length ? '; the view draws ' + m.free.map(f => f.key).join(', ') : ''}`);
    list.push(...m.nodes);
  }
  lines.push('};');
  lines.push(`const opxy::u16 members[${list.length}] = {${list.join(', ')}};`);
  lines.push('}', '}', '');
  return lines.join('\n');
}

function panel(engine, art) {
  const out = [`<g id="${engine}_panel">`];
  out.push('  <path d="M0 20 L480 20 L480 220 L0 220 Z" fill="#000000" stroke="none"/>');
  const park = 'transform="matrix(1 0 0 1 640 320)"';
  // Nodes 0..3: the mutable paths, 64 distinct points each.
  const seed = Array.from({ length: 64 }, (_, i) => `${num(-32 + i)} ${num((i % 2) * 0.5)}`);
  for (let p = 0; p < 4; p++)
    out.push(`  <path id="${engine}_node_${p}" ${park} d="M${seed.join(' L')} Z" fill="none" stroke="#f7f5f5" stroke-width="1.6" stroke-linejoin="round"/>`);
  // Rings, handles and the cursor.
  for (let k = 0; k < 3; k++)
    out.push(`  <circle id="${engine}_node_${4 + k}" ${park} cx="0" cy="0" r="20" fill="none" stroke="#a8a9ad" stroke-width="1.2"/>`);
  for (let k = 0; k < 2; k++)
    out.push(`  <path id="${engine}_node_${7 + k}" ${park} d="M-2.5 -2.5 L2.5 -2.5 L2.5 2.5 L-2.5 2.5 Z" fill="#f7f5f5" stroke="#f7f5f5" stroke-width="0.1"/>`);
  out.push(`  <path id="${engine}_node_9" ${park} d="M0 -40 L0 40" fill="none" stroke="#a8a9ad" stroke-width="1.2"/>`);
  // The readout: three digits of seven segments a..g, horizontal or vertical strokes about their centres.
  for (let digit = 0; digit < 3; digit++)
    for (let seg = 0; seg < 7; seg++) {
      const d = [0, 3, 6].includes(seg) ? 'M-1.6 0 L1.6 0' : 'M0 -1.2 L0 1.2';
      out.push(`  <path id="${engine}_node_${10 + digit * 7 + seg}" ${park} d="${d}" fill="none" stroke="#8c8d92" stroke-width="0.9"/>`);
    }
  art.nodes.forEach((n, i) => {
    const [fill, stroke, width] = PALETTE[SHADE[n.style]];
    // Art nodes are authored about their pivots and placed by the first frame, so they need no parking transform.
    out.push(`  <path id="${engine}_node_${COMMON_NODES + i}" d="${n.d}" fill="${fill}" stroke="${stroke}" stroke-width="${width}"${fill === 'none' ? ' stroke-linejoin="round"' : ''}/>`);
  });
  // Static labels, in the device's font: screen coordinates, baseline in the transform.
  const esc = t => t.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  for (const t of art.texts ?? [])
    out.push(`  <g><text transform="matrix(1 0 0 1 ${num(t.x)} ${num(30 + t.y + t.fs * 0.83)})" fill="${t.fill}" font-family="UniversNextPro-Light" font-size="${num(t.fs)}px">${esc(t.t)}</text></g>`);
  PALETTE.forEach(([fill, stroke, width], i) =>
    out.push(`  <circle id="${engine}_palette_${i}" ${park} cx="0" cy="0" r="1" fill="${fill === 'none' ? '#000000' : fill}" stroke="${stroke}" stroke-width="${width}"/>`));
  out.push('</g>');
  return out.join('\n') + '\n';
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2), pads = args[0] === '--pads';
  const [engine, ...specs] = pads ? args.slice(1) : args;
  const art = build(engine, specs.map(s => s.split(':')), pads);
  const dir = path.join(root, 'machines', engine);
  const svg = panel(engine, art);
  writeFileSync(path.join(dir, 'panel.svg.fragment'), svg);
  writeFileSync(path.join(dir, 'art.h'), header(engine, art));
  const nodes = COMMON_NODES + art.nodes.length;
  console.log(`${engine}: ${art.machines.length} machines, ${nodes} nodes, ${svg.length} bytes of artwork` +
              (nodes > 256 ? '  OVER 256 NODES' : '') + (svg.length > 65536 ? '  OVER 64 KB' : ''));
  for (const m of art.machines) if (m.free.length) console.log(`  ${m.name}: the view draws ${m.free.map(f => f.key + ' (' + f.controls.join(' ') + ')').join(', ')}`);
}
