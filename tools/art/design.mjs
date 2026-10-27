// Loads the screen design (docs/design/Main.dc.html) and runs its own drawing
// code, so the OP-XY screens use the design's exact geometry. Everything up to
// the design's UI component is plain JS: data tables, the view toolkit, the
// instruments (INST), the family panels and the Monomachine views.
//
//   import { design, render } from './design.mjs';
//   render('md', 'TRX-BD', values01, h, ph)  ->  [{ key, style, fill, stroke, sw, d }], labels
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const html = readFileSync(path.join(root, 'docs', 'design', 'Main.dc.html'), 'utf8');
const start = html.indexOf('<script type="text/x-dc"');
let code = html.slice(html.indexOf('>', start) + 1, html.indexOf('class Component extends DCLogic'));
// Keep each layer's style#key, which the design's builder folds away.
code = code.replace("L.push({d:'',fill:s[0],", "L.push({key:st,d:'',fill:s[0],");
code = code.replace("out:function(key,reuse){const ly=L.filter(function(l){return l.d.trim();});return {layers:ly,labels:placeLabels(ly,labels,key||'x',!!reuse)};}",
                    "out:function(key,reuse){const ly=L.filter(function(l){return l.d.trim();});return {layers:ly,labels:placeLabels(ly,labels,key||'x',false),raw:labels};}");
if (!code.includes('key:st')) throw new Error('design builder changed: update tools/art/design.mjs');

export const design = new Function(`${code}\nreturn { buildView, info, MACH, MNM, MSPEC, MNMSPEC, ORDER, VST, PX, PW };`)();

// The design's view of a machine with each control at `values[i]` (0..1), at
// hit strength h (0 at rest) and animation phase ph (0 at rest).
export function render(engine, name, values, h = 0, ph = 0) {
  const out = design.buildView(engine, name, k => values[k] ?? 0, h, ph, { step: -1, adur: 520 });
  return { layers: out.layers.map(l => ({ ...l, style: l.key.split('#')[0] })), labels: out.labels, raw: out.raw };
}

// A machine's controls and defaults as the design lists them (0..127).
export function defaults(engine, name) {
  const i = design.info(engine, name);
  return { labels: i.l.map(x => x.toUpperCase()), values: i.d };
}
