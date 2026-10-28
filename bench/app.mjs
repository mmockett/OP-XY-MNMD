// The bench page: a track per engine, as OP-XY tracks, each played
// chromatically (the SDK previews a drum kit as its first key's machine).
// Audio and sequencing run in worklet.mjs; this page draws the selected
// track's OP-XY screen with the engine's own compiled display code.
import { PreviewDisplay, SvgDisplay, headerSvg } from './sdk/display.mjs';
import { controlPositions, editedShown, shownControls } from './sdk/controls.mjs';

const $ = id => document.getElementById(id);
const STEPS = 16;
const Q31 = 0x7fffffff;
const word = v => Math.round(v / 127 * Q31);
const status = text => { $('status').textContent = text; };

const BASE = 48;   // C3: the keyboard's lowest note
// A pattern's notes at each step.
const PATTERNS = { 'Four': s => s % 4 ? [] : [BASE + 12], 'Empty': () => [] };
const fresh = name => Array.from({ length: STEPS }, (_, s) => PATTERNS[name](s));

// ---- Machines --------------------------------------------------------------
const ids = await (await fetch('machines/index.json')).json();
const machines = await Promise.all(ids.map(async id => {
  const [manifest, bytes, art] = await Promise.all([
    fetch(`machines/${id}/manifest.json`).then(r => r.json()),
    fetch(`machines/${id}/engine.wasm`).then(r => r.arrayBuffer()),
    fetch(`machines/${id}/panel.svg`).then(r => r.text()),
  ]);
  const module = await WebAssembly.compile(bytes);
  return { id, manifest, module, art, definition: manifest.definition };
}));
const catalog = await (await fetch('catalog.json')).json();
// The bench's tracks: the drum kits, then the Monomachine engines.
const rank = m => (m.id in catalog.kits ? 0 : 1);
machines.sort((a, b) => rank(a) - rank(b) || a.id.localeCompare(b.id));

const defaults = m => m.definition.parameters.map(p => Math.round(p.default * 127));
const STORE = 'md-bench-2';
const saved = (() => { try { return JSON.parse(localStorage.getItem(STORE) || 'null'); } catch { return null; } })();
const tracks = machines.map((m, i) => ({
  machine: m,
  values: saved?.tracks?.[i]?.machine === m.id ? saved.tracks[i].values : defaults(m),
  shown: [0, 1, 2, 3],
  level: saved?.tracks?.[i]?.level ?? 0.8, mute: false,
  serial: 0,
  display: new PreviewDisplay(m.module),
  scene: new SvgDisplay(m.art, m.manifest.graphics),
}));
let pattern = saved?.pattern?.length === tracks.length ? saved.pattern : tracks.map((t, i) => fresh(i ? 'Empty' : 'Four'));
let selected = 0, playing = false, step = -1;
let context, node;

const persist = () => {
  try {
    localStorage.setItem(STORE, JSON.stringify({
      tracks: tracks.map(t => ({ machine: t.machine.id, values: t.values, level: t.level })), pattern,
      bpm: Number($('bpm').value), swing: Number($('swing').value) }));
  } catch { /* storage may be unavailable; the kit still plays */ }
};
const send = message => node?.port.postMessage(message);
const words = t => [...t.values.map(word), ...new Array(8 - t.values.length).fill(0)];

// ---- Screen ----------------------------------------------------------------
let last = performance.now();
function drawScreen(now) {
  const elapsed = Math.min(8192, Math.max(0, Math.round((now - last) * 1.024)));
  last = now;
  const t = tracks[selected];
  const p = t.machine.definition.parameters;
  try {
    t.scene.apply(t.display.tick(shownControls(words(t), t.shown), elapsed, t.serial >>> 0, selected,
                                 matchMedia('(prefers-reduced-motion: reduce)').matches));
  } catch (error) { status(`Display stopped: ${error.message}`); return; }
  const titles = t.shown.map(i => p[i].label.toLowerCase());
  const values = t.shown.map((i, k) => t.scene.header[k] || String(t.values[i]));
  // The stock header under the artwork, as the device draws it: a paged engine covers it with its own.
  $('screen').innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 480 220" role="img">
    <rect width="480" height="220" fill="#000"/>${headerSvg(titles, values)}${t.scene.svg()}</svg>`;
  requestAnimationFrame(drawScreen);
}

// ---- Encoders: top row on the encoder, bottom row its alternate --------------
const COLOURS = ['#698eff', '#00ed95', '#ffffff', '#ff8a3d'];
function buildEncoders() {
  const t = tracks[selected];
  const p = t.machine.definition.parameters;
  $('encoders').innerHTML = '';
  for (let lane = 0; lane < 4; ++lane) {
    const column = document.createElement('div');
    column.className = 'lane';
    column.style.setProperty('--ink', COLOURS[lane]);
    for (const index of [lane, lane + 4]) {
      if (index >= p.length || p[index].absent) continue;   // an empty slot has no encoder
      const knob = document.createElement('div');
      knob.className = 'knob' + (index >= 4 ? ' alt' : '');
      knob.tabIndex = 0;
      knob.setAttribute('role', 'slider');
      knob.setAttribute('aria-label', p[index].label);
      knob.setAttribute('aria-valuemin', '0');
      knob.setAttribute('aria-valuemax', '127');
      knob.dataset.index = index;
      knob.innerHTML = `<svg viewBox="0 0 48 48" aria-hidden="true"><circle cx="24" cy="24" r="19" class="ring"/>
        <path class="arc" d=""/><line x1="24" y1="24" x2="24" y2="8" class="needle"/></svg>
        <span class="label">${p[index].label}</span><span class="value"></span>`;
      column.append(knob);
      attachKnob(knob, index);
    }
    $('encoders').append(column);
  }
  refreshEncoders();
}
function arc(v) {
  const a0 = -135 * Math.PI / 180, a1 = a0 + (v / 127) * 270 * Math.PI / 180;
  const pt = a => [24 + 19 * Math.sin(a), 24 - 19 * Math.cos(a)];
  const [x0, y0] = pt(a0), [x1, y1] = pt(a1);
  return `M${x0.toFixed(2)} ${y0.toFixed(2)} A19 19 0 ${a1 - a0 > Math.PI ? 1 : 0} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}
function refreshEncoders() {
  const t = tracks[selected];
  for (const knob of $('encoders').querySelectorAll('.knob')) {
    const i = Number(knob.dataset.index), v = t.values[i];
    knob.querySelector('.value').textContent = v;
    knob.querySelector('.arc').setAttribute('d', arc(v));
    knob.querySelector('.needle').setAttribute('transform', `rotate(${-135 + v / 127 * 270} 24 24)`);
    knob.setAttribute('aria-valuenow', v);
    knob.classList.toggle('shown', t.shown.includes(i));
  }
}
// The positions show the control last touched: on a paged engine, its whole page.
function showControl(t, index) {
  const d = t.machine.definition;
  t.shown = editedShown(t.shown, controlPositions(d.parameters), index, d.press === 'page');
}
function setValue(index, value) {
  const t = tracks[selected];
  t.values[index] = Math.max(0, Math.min(127, Math.round(value)));
  showControl(t, index);   // as on the device: the screen shows what was last turned
  send({ type: 'parameters', track: selected, value: words(t) });
  refreshEncoders(); refreshPresets(); persist();
}
function attachKnob(knob, index) {
  let startY = 0, startV = 0;
  knob.addEventListener('pointerdown', e => {
    knob.setPointerCapture(e.pointerId); startY = e.clientY; startV = tracks[selected].values[index];
    showControl(tracks[selected], index); refreshEncoders();
  });
  knob.addEventListener('pointermove', e => {
    if (!knob.hasPointerCapture(e.pointerId)) return;
    setValue(index, startV + (startY - e.clientY) * (e.shiftKey ? 0.1 : 0.6));
  });
  knob.addEventListener('wheel', e => {
    e.preventDefault();
    setValue(index, tracks[selected].values[index] - Math.sign(e.deltaY) * (e.shiftKey ? 1 : 4));
  }, { passive: false });
  knob.addEventListener('dblclick', () => setValue(index, defaults(tracks[selected].machine)[index]));
  knob.addEventListener('keydown', e => {
    const d = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1 }[e.key];
    if (d) { e.preventDefault(); e.stopPropagation(); setValue(index, tracks[selected].values[index] + d * (e.shiftKey ? 8 : 1)); }
  });
}

// ---- Presets -----------------------------------------------------------------
function buildPresets() {
  const t = tracks[selected], def = t.machine.definition;
  $('machine-name').textContent = def.name;
  $('machine-text').textContent = t.machine.manifest.description || '';
  $('presets').innerHTML = '';
  for (const [name, preset] of Object.entries(def.presets)) {
    const b = document.createElement('button');
    b.textContent = name;
    b.addEventListener('click', () => {
      def.parameters.forEach((p, i) => { t.values[i] = Math.round(preset[p.id] * 127); });
      send({ type: 'parameters', track: selected, value: words(t) });
      refreshEncoders(); refreshPresets(); persist(); hit(selected);
    });
    $('presets').append(b);
  }
  refreshPresets();
}
function refreshPresets() {
  const t = tracks[selected], def = t.machine.definition;
  for (const b of $('presets').children) {
    const preset = def.presets[b.textContent];
    b.setAttribute('aria-pressed', String(def.parameters.every((p, i) => Math.round(preset[p.id] * 127) === t.values[i])));
  }
}

// ---- Tracks, steps and keys ------------------------------------------------------
function buildTracks() {
  $('tracks').innerHTML = '';
  tracks.forEach((t, ti) => {
    const b = document.createElement('button');
    b.className = 'track-tab'; b.setAttribute('aria-pressed', String(ti === selected));
    b.innerHTML = `<span class="num">${ti + 1}</span>${t.machine.definition.name}`;
    b.addEventListener('click', () => select(ti));
    const mute = document.createElement('button');
    mute.className = 'mute'; mute.textContent = 'M'; mute.title = 'Mute';
    mute.setAttribute('aria-pressed', String(t.mute));
    mute.addEventListener('click', () => { t.mute = !t.mute; mute.setAttribute('aria-pressed', String(t.mute)); send({ type: 'mix', track: ti, value: { mute: t.mute } }); });
    const cell = document.createElement('span'); cell.className = 'tab'; cell.append(b, mute);
    $('tracks').append(cell);
  });
  buildGrid();
}
// The selected track's steps: a row per note of two octaves, the Cs and the notes in use.
function buildGrid() {
  const t = tracks[selected], grid = $('grid');
  grid.innerHTML = '';
  for (let k = 23; k >= 0; --k) {
    const note = BASE + k;
    if (!pattern[selected].some(n => n.includes(note)) && k % 12) continue;
    const row = document.createElement('div');
    row.className = 'row' + ([1, 3, 6, 8, 10].includes(k % 12) ? ' black' : '');
    const name = document.createElement('button');
    name.className = 'name';
    name.textContent = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'][k % 12] + (3 + Math.floor(k / 12));
    name.addEventListener('pointerdown', () => play(selected, note));
    const steps = document.createElement('div');
    steps.className = 'steps';
    for (let s = 0; s < STEPS; ++s) {
      const cell = document.createElement('button');
      cell.className = 'step' + (s % 4 === 0 ? ' beat' : '');
      cell.setAttribute('aria-label', `${name.textContent} step ${s + 1}`);
      const on = () => pattern[selected][s].includes(note);
      cell.setAttribute('aria-pressed', String(on()));
      cell.addEventListener('click', () => {
        const notes = pattern[selected][s];
        if (on()) notes.splice(notes.indexOf(note), 1); else notes.push(note);
        cell.setAttribute('aria-pressed', String(on()));
        send({ type: 'pattern', value: pattern }); persist();
      });
      steps.append(cell);
    }
    row.append(name, steps);
    grid.append(row);
  }
  showStep();
}
function showStep() {
  for (const row of $('grid').children)
    [...row.querySelector('.steps').children].forEach((c, s) => c.classList.toggle('now', playing && s === step));
}
// The keyboard: two octaves from C3, played with the mouse or the computer's keys.
const KEYS = 'awsedftgyhujkolp;\'';
function buildKeyboard() {
  const board = $('keyboard');
  board.innerHTML = '';
  for (let k = 0; k < 24; ++k) {
    const key = document.createElement('button');
    key.className = 'key' + ([1, 3, 6, 8, 10].includes(k % 12) ? ' black' : '');
    key.title = KEYS[k] ? `key ${KEYS[k].toUpperCase()}` : '';
    key.addEventListener('pointerdown', e => { e.preventDefault(); play(selected, BASE + k); });
    key.addEventListener('pointerup', () => release(selected, BASE + k));
    key.addEventListener('pointerleave', () => release(selected, BASE + k));
    board.append(key);
  }
}
function select(ti) {
  selected = ti;
  for (const [i, cell] of [...$('tracks').children].entries()) cell.firstChild.setAttribute('aria-pressed', String(i === ti));
  buildEncoders(); buildPresets(); buildGrid(); buildKeyboard();
}
async function play(ti, note) {
  await enableAudio();
  send({ type: 'note', track: ti, note, on: true });
  tracks[ti].serial++;
}
function release(ti, note) { send({ type: 'note', track: ti, note, on: false }); }
function hit(ti) { play(ti, BASE); }

// ---- Transport -------------------------------------------------------------------
for (const name of Object.keys(PATTERNS)) $('patterns').append(new Option(name, name));
$('patterns').value = '';
$('patterns').addEventListener('change', e => {
  pattern[selected] = fresh(e.target.value);
  send({ type: 'pattern', value: pattern }); buildGrid(); persist();
});
if (saved?.bpm) $('bpm').value = saved.bpm;
if (saved?.swing) $('swing').value = saved.swing;
$('bpm').addEventListener('change', () => { send({ type: 'tempo', bpm: Math.max(40, Math.min(300, Number($('bpm').value) || 120)) }); persist(); });
$('swing').addEventListener('input', () => { send({ type: 'swing', value: Number($('swing').value) }); persist(); });
$('master').addEventListener('input', () => send({ type: 'master', value: Number($('master').value) }));

async function enableAudio() {
  if (context) return;
  $('audio').disabled = true;
  try {
    context = new AudioContext({ latencyHint: 'interactive', sampleRate: 44100 });
    await context.audioWorklet.addModule('worklet.mjs');
    node = new AudioWorkletNode(context, 'md-kit', { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2],
      processorOptions: { modules: tracks.map(t => t.machine.module), parameters: tracks.map(words) } });
    node.port.onmessage = ({ data }) => {
      if (data.error) status(`Audio error: ${data.error}`);
      if (data.ready) status('Ready · Space to play, A to \' play the keys');
      if (data.step !== undefined) {
        step = data.step;
        for (const i of data.fired) tracks[i].serial++;
        showStep();
      }
    };
    node.connect(context.destination);
    send({ type: 'pattern', value: pattern });
    send({ type: 'tempo', bpm: Number($('bpm').value) });
    send({ type: 'swing', value: Number($('swing').value) });
    tracks.forEach((t, i) => send({ type: 'mix', track: i, value: { level: t.level, mute: t.mute } }));
    $('audio').textContent = 'Audio on';
    await context.resume();
  } catch (error) {
    status(`Audio failed: ${error.message}`);
    context = null; $('audio').disabled = false;
  }
}
async function togglePlay() {
  await enableAudio();
  playing = !playing;
  send({ type: playing ? 'play' : 'stop' });
  $('play').setAttribute('aria-pressed', String(playing));
  $('play').textContent = playing ? '■' : '▶';
  if (!playing) { step = -1; showStep(); }
}
$('audio').addEventListener('click', enableAudio);
$('play').addEventListener('click', togglePlay);

document.addEventListener('keydown', e => {
  if (e.target.matches('input, select, textarea') || e.metaKey || e.ctrlKey || e.altKey) return;
  if (e.code === 'Space') { e.preventDefault(); togglePlay(); return; }
  const track = '123456789'.indexOf(e.key);
  if (track >= 0 && track < tracks.length) { select(track); return; }
  const k = KEYS.indexOf(e.key.toLowerCase());
  if (k >= 0 && !e.repeat) play(selected, BASE + k);
});
document.addEventListener('keyup', e => {
  const k = KEYS.indexOf(e.key.toLowerCase());
  if (k >= 0) release(selected, BASE + k);
});

$('export').addEventListener('click', async () => {
  persist();
  try { await navigator.clipboard.writeText(localStorage.getItem(STORE)); status('Kit copied'); }
  catch { status('Clipboard unavailable'); }
});
$('import').addEventListener('click', async () => {
  try {
    const kit = JSON.parse(await navigator.clipboard.readText());
    kit.tracks.forEach((k, i) => { if (tracks[i]?.machine.id === k.machine) tracks[i].values = k.values.map(Number); });
    if (Array.isArray(kit.pattern) && kit.pattern.length === tracks.length) pattern = kit.pattern;
    tracks.forEach((t, i) => send({ type: 'parameters', track: i, value: words(t) }));
    send({ type: 'pattern', value: pattern });
    buildTracks(); select(selected); persist(); status('Kit pasted');
  } catch (error) { status(`Could not paste a kit: ${error.message}`); }
});

buildTracks();
// ?track=N opens a track; ?flash=1 hits it on screen only, without audio: for screenshots.
const query = new URLSearchParams(location.search);
select(Math.min(tracks.length - 1, Math.max(0, Number(query.get('track')) || 0)));
if (query.get('flash')) setTimeout(() => { tracks[selected].serial++; }, 200);
// ?selftest=1: start audio and the sequencer, then report what ran (headless checks).
if (query.get('selftest')) {
  let errors = [];
  const report = () => { document.title = 'selftest ' + JSON.stringify({ step, serials: tracks.map(t => t.serial), errors,
    state: context?.state }); };
  enableAudio().then(() => {
    node.port.addEventListener('message', ({ data }) => { if (data.error) errors.push(data.error); });
    togglePlay();
    setTimeout(report, 1500);
  });
}
requestAnimationFrame(drawScreen);
// For automation and checks: the same calls the page makes.
window.kit = { tracks, hit, play, select, togglePlay, enableAudio, setValue, get playing() { return playing; }, get step() { return step; } };
