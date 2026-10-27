// Turns the design's drawing of a machine into an OP-XY rig: a list of parts,
// each one shape in one shade, that the device places, scales and hides.
//
// For every layer the design draws, the rig records:
//   - its flattened shape at rest (the defaults), relative to its centre
//   - how each control moves and scales it, as straight lines between the
//     control's ends (fitted from renders at 0 and 127)
//   - whether it only exists over part of a control's range
//   - for a hit: a struck pose of parts that change shape when struck, and
//     parts that only appear on a hit (rings, sparks), with their motion
// Layers whose shape itself changes with a control (a pitch contour, a wave)
// cannot be a fixed shape: they are listed as `free`, for the machine's view
// code to draw with the mutable paths.
import { render, defaults } from './design.mjs';

// ---- Path flattening: M L H V C S Q T A Z, absolute and relative ---------------
// Resolution is fixed (by angle and by segment), never by size, so the same
// drawing at another size flattens to the same number of points and can be fitted.
export function flatten(d, tolerance = 0) {
  const tokens = d.match(/[A-Za-z]|-?\d*\.?\d+(?:e[-+]?\d+)?/g) || [];
  const polys = [];
  let i = 0, cmd = '', x = 0, y = 0, sx = 0, sy = 0, cur = null, lcx = 0, lcy = 0;
  const num = () => parseFloat(tokens[i++]);
  const move = (nx, ny) => { cur = { pts: [[nx, ny]], closed: false }; polys.push(cur); sx = nx; sy = ny; x = nx; y = ny; };
  const line = (nx, ny) => { if (!cur) move(x, y); cur.pts.push([nx, ny]); x = nx; y = ny; };
  const cubic = (x1, y1, x2, y2, nx, ny) => {
    const n = 8;
    const x0 = x, y0 = y;
    for (let k = 1; k <= n; k++) {
      const t = k / n, u = 1 - t;
      line(u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * nx,
           u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * ny);
    }
    lcx = x2; lcy = y2;
  };
  const arc = (rx, ry, rot, large, sweep, nx, ny) => {
    // SVG endpoint parameterisation to centre form.
    const x0 = x, y0 = y;
    if (rx === 0 || ry === 0) { line(nx, ny); return; }
    const phi = rot * Math.PI / 180, c = Math.cos(phi), s = Math.sin(phi);
    const dx = (x0 - nx) / 2, dy = (y0 - ny) / 2;
    const x1 = c * dx + s * dy, y1 = -s * dx + c * dy;
    rx = Math.abs(rx); ry = Math.abs(ry);
    const l = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
    // Radii within 3% of the chord's are the chord's: the design rounds an
    // ellipse's halves to a tenth, and a half that comes out a sliver over a
    // half turn would flatten to more points than at another size, so a
    // control that resizes the ellipse could not be fitted.
    if (l > 0.94) { rx *= Math.sqrt(l); ry *= Math.sqrt(l); }
    const sign = large === sweep ? -1 : 1;
    const num2 = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
    const coef = sign * Math.sqrt(Math.max(0, num2 / (rx * rx * y1 * y1 + ry * ry * x1 * x1)));
    const cx1 = coef * (rx * y1 / ry), cy1 = coef * (-ry * x1 / rx);
    const cx = c * cx1 - s * cy1 + (x0 + nx) / 2, cy = s * cx1 + c * cy1 + (y0 + ny) / 2;
    const ang = (ux, uy, vx, vy) => Math.atan2(ux * vy - uy * vx, ux * vx + uy * vy);
    const t1 = ang(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry);
    let dt = ang((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);
    if (!sweep && dt > 0) dt -= 2 * Math.PI;
    if (sweep && dt < 0) dt += 2 * Math.PI;
    const n = Math.max(2, Math.ceil(Math.abs(dt) / (Math.PI / 16) - 0.05));   // 32 to a full turn; a half turn is 16 however it rounds
    for (let k = 1; k <= n; k++) {
      const t = t1 + dt * k / n;
      line(cx + c * rx * Math.cos(t) - s * ry * Math.sin(t), cy + s * rx * Math.cos(t) + c * ry * Math.sin(t));
    }
  };
  while (i < tokens.length) {
    if (/[A-Za-z]/.test(tokens[i])) cmd = tokens[i++];
    const rel = cmd === cmd.toLowerCase();
    const ox = rel ? x : 0, oy = rel ? y : 0;
    switch (cmd.toUpperCase()) {
      case 'M': move(ox + num(), oy + num()); cmd = rel ? 'l' : 'L'; break;
      case 'L': line(ox + num(), oy + num()); break;
      case 'H': line(ox + num(), y); break;
      case 'V': line(x, oy + num()); break;
      case 'C': { const a = ox + num(), b = oy + num(), c2 = ox + num(), d2 = oy + num(); cubic(a, b, c2, d2, ox + num(), oy + num()); break; }
      case 'S': { const a = 2 * x - lcx, b = 2 * y - lcy, c2 = ox + num(), d2 = oy + num(); cubic(a, b, c2, d2, ox + num(), oy + num()); break; }
      case 'Q': { const qx = ox + num(), qy = oy + num(), nx = ox + num(), ny = oy + num();
        cubic(x + 2 / 3 * (qx - x), y + 2 / 3 * (qy - y), nx + 2 / 3 * (qx - nx), ny + 2 / 3 * (qy - ny), nx, ny); break; }
      case 'A': { const rx = num(), ry = num(), rot = num(), large = num(), sweep = num(); arc(rx, ry, rot, large, sweep, ox + num(), oy + num()); break; }
      case 'Z': if (cur) cur.closed = true; x = sx; y = sy; cur = null; break;
      default: i++;
    }
  }
  return polys.map(p => ({ closed: p.closed, pts: tolerance > 0 ? simplify(p.pts, tolerance) : p.pts }));
}

// Douglas-Peucker: drop points within `tol` pixels of the line between their neighbours.
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
  return { x0, y0, x1, y1, cx: (x0 + x1) / 2, cy: (y0 + y1) / 2, size: Math.max(x1 - x0, y1 - y0) };
}

// How `b` sits relative to `a`, if it is `a` moved, uniformly scaled and turned
// about its centre (a least-squares similarity): { dx, dy, s, turn } in pixels
// and radians, with the worst point error, or null if the shapes differ.
function fit(a, b) {
  if (a.length !== b.length) return null;
  const pa = [], pb = [];
  for (let p = 0; p < a.length; p++) {
    if (a[p].pts.length !== b[p].pts.length) return null;
    for (let k = 0; k < a[p].pts.length; k++) { pa.push(a[p].pts[k]); pb.push(b[p].pts[k]); }
  }
  const ba = bounds(a);
  let mbx = 0, mby = 0;
  for (const q of pb) { mbx += q[0]; mby += q[1]; }
  let max = 0, may = 0;
  for (const q of pa) { max += q[0]; may += q[1]; }
  const n = pa.length; max /= n; may /= n; mbx /= n; mby /= n;
  let sxx = 0, sxy = 0, norm = 0;
  for (let k = 0; k < n; k++) {
    const ax = pa[k][0] - max, ay = pa[k][1] - may, bx = pb[k][0] - mbx, by = pb[k][1] - mby;
    sxx += ax * bx + ay * by; sxy += ax * by - ay * bx; norm += ax * ax + ay * ay;
  }
  const turn = norm > 1e-9 ? Math.atan2(sxy, sxx) : 0;
  const s = norm > 1e-9 ? Math.hypot(sxx, sxy) / norm : 1;
  const c = Math.cos(turn) * s, si = Math.sin(turn) * s;
  // Express about the part's own centre (its pivot on the device).
  const ox = max - ba.cx, oy = may - ba.cy;
  const dx = mbx - max + ox - (c * ox - si * oy), dy = mby - may + oy - (si * ox + c * oy);
  let worst = 0;
  for (let k = 0; k < n; k++) {
    const ax = pa[k][0] - ba.cx, ay = pa[k][1] - ba.cy;
    const ex = ba.cx + c * ax - si * ay + dx - pb[k][0], ey = ba.cy + si * ax + c * ay + dy - pb[k][1];
    worst = Math.max(worst, Math.hypot(ex, ey));
  }
  return { dx, dy, s, turn, worst, size: ba.size };
}

// A part that keeps its shape this closely is rigid: 1.2 px, or 12% of its size,
// so a drum that also changes aspect still reads as the same drum, resized.
const rigidEnough = (f, lenient = true) => f && f.worst <= (lenient ? Math.max(1.2, 0.12 * f.size) : 1.2);
const same = (a, b) => a.length === b.length && a.every((p, i) => p.pts.length === b[i].pts.length &&
  p.pts.every((q, k) => Math.abs(q[0] - b[i].pts[k][0]) < 0.05 && Math.abs(q[1] - b[i].pts[k][1]) < 0.05));

// What the screens leave out of the design, by machine: labels that repeat a
// header name or say what the drawing already shows (a word also drops a
// changing label it starts, "wp" drops "wp 40"; the header names stepped
// values such as the DigiPRO waves, and the ticks show the chord), and layers (style#key) that
// would mislead on the device, such as BBOX's lit key, which cannot follow the
// played note.
const LEAVE_OUT = {
  'SWAVE-SAW': ['unix', 'unil', 'base', 'subx', 'sub1', 'sub2'],
  'SWAVE-PULSE': ['unil', 'base', 'sub1', 'sub2'],
  'SWAVE-ENS': ['chord'],
  'DPRO-WAVE': ['wave', 'wp'],
  'DPRO-BBOX': ['start', '24 sounds across two octaves', 'f4#lkbb'],
  'DPRO-DDRW': ['wav1', 'wav2', 'mix'],
  'DPRO-DENS': ['chord'],
  'FM+STAT': ['1env', '2vol', 'output'],
  'FM+PAR': ['1env', '2env', '3env', 'output'],
  'FM+DYN': ['output', '1fen', '1ven'],
  'VO-6': ['voc1', 'voc2 ↑'],
};
const leftOut = (name, text) => (LEAVE_OUT[name] ?? []).some(w => text === w || text.startsWith(w + ' '));

// Each filled style's nearest stroke style (build.mjs's PALETTE).
const HOLLOW = { f1: 'faint', f2: 'faint', f3: 'dim', f4: 'mid', f5: 'thin', node: 'thin',
                 o0: 'line', o1: 'thin', o2: 'mid', o3: 'thin' };

// One object moves as one. Where a control moves touching parts of the
// instrument (a drum's shell, hoop and head; a pedal's board, shaft and
// beater), a single similarity fitted to them together moves each of them, so
// they keep their joins: fitted alone, a shell that changes its aspect and the
// head on its end scale by different amounts about different pivots and part.
// The floor's shadow and the panel divide stay out: they touch everything.
const UNJOINED = /#(fl|div)$/;
const alike = (a, b) => a.length === b.length && a.every((p, i) => p.pts.length === b[i].pts.length);
function join(parts, layersAt, controls) {
  const inst = parts.filter(p => p.bounds.x1 < 240 && !UNJOINED.test(p.key));
  const touch = (a, b) => a.x0 - 1.5 < b.x1 && b.x0 - 1.5 < a.x1 && a.y0 - 1.5 < b.y1 && b.y0 - 1.5 < a.y1;
  for (let c = 0; c < controls; c++) {
    for (const end of [0, 1]) {
      const at = layersAt(c, end), moved = new Map();
      for (const p of inst) {
        if (!p.ignored.includes(c) && !p.effects.some(e => e.control === c && e.end === end && !e.absent)) continue;
        const other = at.find(l => l.key === p.key);
        const shape = other && flatten(other.d);
        if (shape && (p.mark === undefined || shape[p.mark])) moved.set(p, p.mark === undefined ? shape : [shape[p.mark]]);
      }
      // Touching parts, transitively.
      const left = [...moved.keys()];
      while (left.length) {
        const group = [left.pop()];
        for (let k = 0; k < group.length; k++)
          for (let j = left.length - 1; j >= 0; j--) if (touch(group[k].bounds, left[j].bounds)) group.push(...left.splice(j, 1));
        // A part drawn differently past a point (KICK's beater, round then
        // square) cannot be fitted point to point: it goes where the parts it
        // touches go.
        const paired = group.filter(p => alike(p.shape, moved.get(p)));
        if (group.length < 2 || !paired.length) continue;
        const f = fit(paired.flatMap(p => p.shape), paired.flatMap(p => moved.get(p)));
        if (!f || f.worst > Math.max(2, 0.15 * f.size)) continue;
        const b = bounds(paired.flatMap(p => p.shape)), k = Math.cos(f.turn) * f.s, si = Math.sin(f.turn) * f.s;
        for (const p of group) {
          const mx = p.bounds.cx - b.cx, my = p.bounds.cy - b.cy;
          p.effects = p.effects.filter(e => !(e.control === c && e.end === end));
          p.effects.push({ control: c, end, dx: k * mx - si * my + f.dx - mx, dy: si * mx + k * my + f.dy - my, s: f.s, turn: f.turn });
          if (p.ignored.includes(c) && p.effects.filter(e => e.control === c).length === 2) p.ignored = p.ignored.filter(x => x !== c);
        }
      }
    }
  }
}

// A layer of several marks that a control moves apart (a hat's two gap ticks,
// one on each cymbal) is no one shape, but each mark is: it becomes a part per
// mark, so each follows what it marks.
function split(parts, layersAt, controls) {
  for (let i = parts.length - 1; i >= 0; i--) {
    const p = parts[i];
    if (p.bounds.x1 >= 240 || !p.ignored.length || p.shape.length < 2) continue;
    const marks = p.shape.map((poly, mark) => ({ mark, key: p.key, style: p.style, shape: [poly], bounds: bounds([poly]), effects: [], ignored: [] }));
    let rigid = true;
    for (let c = 0; c < controls && rigid; c++) {
      for (const end of [0, 1]) {
        const other = layersAt(c, end).find(l => l.key === p.key);
        if (!other) { marks.forEach(m => m.effects.push({ control: c, end, absent: true })); continue; }
        const shape = flatten(other.d);
        if (shape.length !== marks.length) { rigid = false; break; }
        marks.forEach((m, k) => {
          const f = fit(m.shape, [shape[k]]);
          if (!rigidEnough(f)) { rigid = false; return; }
          if (Math.abs(f.dx) > 0.05 || Math.abs(f.dy) > 0.05 || Math.abs(f.s - 1) > 0.002 || Math.abs(f.turn) > 0.002)
            m.effects.push({ control: c, end, dx: f.dx, dy: f.dy, s: f.s, turn: f.turn });
        });
      }
    }
    if (rigid) parts.splice(i, 1, ...marks);
  }
}

// The rig for one machine. `instrument` says which instrument it is ('md' or 'mm').
export function rig(instrument, name) {
  const instrument0 = instrument === 'mm' ? 'mnm' : instrument;   // the design's name for the Monomachine
  const { labels, values } = defaults(instrument0, name);
  const rest = values.map(v => v / 127);
  const base = render(instrument0, name, rest);
  const layers = new Map(base.layers.map(l => [l.key, l]));
  const shapes = new Map(base.layers.map(l => [l.key, flatten(l.d)]));
  const parts = [], free = [];
  for (const [key, layer] of layers) {
    const shape = shapes.get(key);
    const part = { key, style: layer.style, shape, bounds: bounds(shape), effects: [], ignored: [] };
    const isInstrument = instrument === 'md' && part.bounds.x1 < 240;
    // The instrument (a Machinedrum machine's left half) always draws; a control
    // it cannot follow as a fixed shape is ignored. Elsewhere such a layer is
    // the view code's to draw.
    let rigid = true;
    for (let c = 0; c < labels.length; c++) {
      for (const end of [0, 1]) {
        const vals = rest.slice(); vals[c] = end;
        // The same layer restyled ("o1#opC3" lit as "o2#opC3") keeps its place in its rest style.
        const at = render(instrument0, name, vals).layers;
        const id = key.split('#')[1];
        const other = at.find(l => l.key === key) || (id && at.find(l => l.key.split('#')[1] === id));
        if (!other) { part.effects.push({ control: c, end, absent: true }); continue; }
        const f = fit(shape, flatten(other.d));
        if (!rigidEnough(f, isInstrument)) {
          if (!part.ignored.includes(c)) part.ignored.push(c);
          rigid = false;
          continue;
        }
        if (Math.abs(f.dx) > 0.05 || Math.abs(f.dy) > 0.05 || Math.abs(f.s - 1) > 0.002 || Math.abs(f.turn) > 0.002)
          part.effects.push({ control: c, end, dx: f.dx, dy: f.dy, s: f.s, turn: f.turn });
      }
    }
    if (rigid || isInstrument) {
      part.effects = part.effects.filter(e => !part.ignored.includes(e.control));
      parts.push(part);
    } else free.push({ key, style: layer.style, controls: part.ignored.map(c => labels[c]) });
  }
  if (instrument === 'md') {
    const at = new Map(), layersAt = (c, end) => {
      if (!at.has(c * 2 + end)) { const vals = rest.slice(); vals[c] = end; at.set(c * 2 + end, render(instrument0, name, vals).layers); }
      return at.get(c * 2 + end);
    };
    split(parts, layersAt, labels.length);
    join(parts, layersAt, labels.length);
  }
  // An instrument the design draws as another one past a control's middle
  // (E12-BC's conga, a tom below half BC): the other's layers are parts shown
  // only there, so the instrument never vanishes.
  if (instrument === 'md') {
    for (let c = 0; c < labels.length; c++) {
      for (const end of [0, 1]) {
        if (!parts.some(p => p.bounds.x1 < 240 && p.effects.some(e => e.control === c && e.end === end && e.absent))) continue;
        const vals = rest.slice(); vals[c] = end;
        for (const l of render(instrument0, name, vals).layers) {
          if (layers.has(l.key)) continue;
          const shape = flatten(l.d), b = bounds(shape);
          if (b.x1 < 240) parts.push({ key: l.key, style: l.style, shape, bounds: b, effects: [{ control: c, end, only: true }], ignored: [] });
        }
      }
    }
  }
  // The device draws the view's paths beneath every art node, and a node's fill
  // only ever changes colour: a filled part that the design draws a view's layer
  // over would hide it, so it is drawn as its outline's stroke.
  const order = [...layers.keys()];
  for (const p of parts) {
    const at = order.indexOf(p.key);
    const covers = free.some(f => {
      if (order.indexOf(f.key) <= at) return false;
      const b = bounds(shapes.get(f.key));
      return b.x0 < p.bounds.x1 && p.bounds.x0 < b.x1 && b.y0 < p.bounds.y1 && p.bounds.y0 < b.y1;
    });
    if (covers && HOLLOW[p.style]) p.style = HOLLOW[p.style];
  }
  // A hit: the struck pose (early) and late in the animation.
  const early = render(instrument0, name, rest, 1, 0.2), late = render(instrument0, name, rest, 0.4, 0.7);
  const hit = [];
  for (const l of early.layers) {
    const shape = flatten(l.d);
    if (layers.has(l.key) && same(shape, shapes.get(l.key))) continue;
    // A part that keeps its shape when struck just moves: no second drawing.
    const restPart = parts.find(q => q.key === l.key);
    if (restPart) {
      const f = fit(restPart.shape, shape);
      if (rigidEnough(f, instrument === 'md' && restPart.bounds.x1 < 240)) {
        restPart.strike = { dx: f.dx, dy: f.dy, s: f.s, turn: f.turn };
        continue;
      }
    }
    const latePose = late.layers.find(q => q.key === l.key);
    const motion = latePose ? fit(shape, flatten(latePose.d)) : null;
    hit.push({ key: l.key, style: l.style, shape, bounds: bounds(shape), struck: layers.has(l.key),
               motion: rigidEnough(motion) ? motion : null });
  }
  // Labels: fixed, moving with a control (as a part does), or with text that
  // changes, which only the view can write (a text slot).
  const texts = [], dynamic = [];
  for (const t of base.labels) {
    const label = { t: t.t, x: parseFloat(t.x), y: parseFloat(t.y), fs: parseFloat(t.fs) || 9, effects: [] };
    let fixed = true;
    for (let c = 0; c < labels.length && fixed; c++) {
      for (const end of [0, 1]) {
        const vals = rest.slice(); vals[c] = end;
        const other = render(instrument0, name, vals).labels;
        // The same words, nearest where they were and not where another label with
        // them sits still: a view may show the same word twice (VO-6's u).
        const at = (o, p) => Math.hypot(parseFloat(o.x) - parseFloat(p.x), parseFloat(o.y) - parseFloat(p.y));
        const taken = o => base.labels.some(b => b !== t && b.t === t.t && at(o, b) < 0.5);
        const same = other.filter(o => o.t === t.t && !taken(o)).sort((p, q) => at(p, t) - at(q, t))[0];
        if (!same) {
          // Gone at this end: absent, unless a label like it took its place with new words.
          if (other.length === base.labels.length) { fixed = false; break; }
          label.effects.push({ control: c, end, absent: true });
          continue;
        }
        const dx = parseFloat(same.x) - label.x, dy = parseFloat(same.y) - label.y;
        if (Math.abs(dx) > 0.5 || Math.abs(dy) > 0.5) label.effects.push({ control: c, end, dx, dy, s: 1, turn: 0 });
      }
    }
    // A label moved the same way at both ends of a control was nudged aside by the
    // design's label placement, not moved by the control: it stays put (static text).
    for (let c = 0; c < labels.length; c++) {
      const [lo, hi] = [0, 1].map(end => label.effects.find(e => e.control === c && e.end === end));
      if (lo && hi && !lo.absent && !hi.absent && Math.abs(lo.dx - hi.dx) < 0.5 && Math.abs(lo.dy - hi.dy) < 0.5)
        label.effects = label.effects.filter(e => e.control !== c);
    }
    (fixed ? texts : dynamic).push(label);
  }
  for (const d of dynamic) free.push({ key: 'text:' + d.t, style: 'label', controls: [], x: d.x, y: d.y });
  const kept = p => !leftOut(name, p.key.startsWith('text:') ? p.key.slice(5) : p.key);
  // A Monomachine view animates its own drawing while a note plays: the design's
  // struck copy of a layer the view draws, or a layer only a hit draws, would be
  // a second trace over it (docs/SCREEN.md).
  const onlyMoving = h => instrument !== 'mm' || (h.struck && !free.some(f => f.key === h.key));
  return { instrument, name, labels, values, parts: parts.filter(kept), free: free.filter(kept), hit: hit.filter(h => kept(h) && onlyMoving(h)),
           texts: texts.filter(t => !leftOut(name, t.t)) };
}
