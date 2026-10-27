// The bench's audio thread: one SDK PreviewEngine per track (the engines' own
// compiled C++), mixed, with the sequencer clocked here so every trig lands on
// its sample. Each step holds the notes it plays, held until the next step.
import { PreviewEngine } from './sdk/engine.mjs';

const STEPS = 16;

class Kit extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const { modules, parameters } = options.processorOptions;
    this.tracks = modules.map((module, i) => ({
      held: [],
      engine: new PreviewEngine(module, sampleRate, parameters[i]),
      buffer: new Float32Array(128),
      level: 0.8, mute: false,
    }));
    this.pattern = this.tracks.map(() => Array.from({ length: STEPS }, () => []));
    this.bpm = 120; this.swing = 0;
    this.playing = false; this.step = 0; this.countdown = 0;
    this.master = 3;
    this.port.onmessage = ({ data }) => this.receive(data);
    this.port.postMessage({ ready: true });
  }

  receive(data) {
    try {
      if (data.type === 'parameters') this.tracks[data.track].engine.setParameters(data.value);
      if (data.type === 'pattern') this.pattern = data.value;
      if (data.type === 'tempo') this.bpm = data.bpm;
      if (data.type === 'swing') this.swing = data.value;
      if (data.type === 'mix') Object.assign(this.tracks[data.track], data.value);
      if (data.type === 'master') this.master = data.value;
      if (data.type === 'note') this.note(data.track, data.note, data.on);
      if (data.type === 'play') { this.playing = true; this.step = 0; this.countdown = 0; }
      if (data.type === 'stop') { this.playing = false; }
      if (data.type === 'panic') for (const t of this.tracks) t.engine.panic();
    } catch (error) { this.port.postMessage({ error: error.message }); }
  }

  note(track, note, on) {
    const engine = this.tracks[track].engine;
    if (on) engine.noteOn(note, 1); else engine.noteOff(note);
  }

  // Samples until the step after this one; odd steps sit later by the swing.
  stepLength(step) {
    const base = sampleRate * 60 / this.bpm / 4;
    const lean = base * this.swing * 0.5;
    return Math.max(1, Math.round(step % 2 ? base - lean : base + lean));
  }

  process(_inputs, outputs) {
    const out = outputs[0][0];
    if (!out) return true;
    out.fill(0);
    let at = 0;
    while (at < out.length) {
      if (this.playing && this.countdown === 0) {
        const fired = [];
        this.tracks.forEach((t, i) => {
          for (const n of t.held) t.engine.noteOff(n);
          t.held = [];
          const notes = this.pattern[i][this.step] ?? [];
          for (const n of notes) { t.engine.noteOn(n, 1); t.held.push(n); }
          if (notes.length) fired.push(i);
        });
        this.port.postMessage({ step: this.step, fired });
        this.countdown = this.stepLength(this.step);
        this.step = (this.step + 1) % STEPS;
      }
      const span = this.playing ? Math.min(out.length - at, this.countdown) : out.length - at;
      for (const t of this.tracks) {
        if (t.buffer.length < out.length) t.buffer = new Float32Array(out.length);
        const view = t.buffer.subarray(0, span);
        t.engine.render(view);
        if (t.mute) continue;
        const g = t.level * this.master;
        for (let i = 0; i < span; ++i) out[at + i] += view[i] * g;
      }
      at += span;
      if (this.playing) this.countdown -= span;
    }
    for (let i = 1; i < outputs[0].length; ++i) outputs[0][i].set(out);
    return true;
  }
}
registerProcessor('md-kit', Kit);
