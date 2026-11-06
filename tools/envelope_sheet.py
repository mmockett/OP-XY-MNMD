#!/usr/bin/env python3
"""Contact sheets of a screen under a short amp envelope against a long one
(docs/SCREEN.md "Following the amp envelope"), at 1:1 in the device's font.

    XY_FONT=<UniversNextPro-Light.ttf> python3 tools/envelope_sheet.py --out <folder> mmsaw mmsin

Each engine gets one PNG: a row for a pluck (no attack, a short decay to
nothing, release word 0x7f000000: stock's shortest, about 6 ms) and one for a
pad (a slow attack, a long decay to 0.7, release word 0: stock's longest, about
55 s to -66 dB; a larger release word is a SHORTER release), the note held for
a second, frames at the times on top. A drum
kit takes its keys' machines after a colon. It builds each engine's preview
with the SDK (tools/sdk_path.py), renders the frames in headless Chrome with
the SDK's own browser display, and writes the sheets and their staging into
--out, never into the checkout. The font is stock content: point XY_FONT at
your copy; it is never copied.
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from sdk_path import SDK  # noqa: E402

CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
TIMES = [0.1, 0.4, 0.9, 1.15, 1.6, 2.4, 3.2]   # seconds from the note; released at 1.0
RELEASE = 1.0
ENVELOPES = {"short": [0, 0.1, 0, 0x7f000000 / 2 ** 31], "long": [0.45, 0.5, 0.7, 0]}   # attack, decay, sustain, release words / 2^31

PAGE = """<!doctype html><meta charset="utf-8">
<style>
@font-face { font-family: 'UniversNextPro-Light'; src: url('FONT'); }
html, body { margin: 0; background: #111; color: #ccc; font: 14px 'UniversNextPro-Light'; }
svg text { font-family: 'UniversNextPro-Light' !important; }
.grid { display: grid; grid-template-columns: 70px repeat(COLUMNS, 480px); gap: 8px; padding: 8px; width: max-content; }
.label { align-self: center; } .time { text-align: center; }
svg.screen { width: 480px; height: 220px; display: block; }
</style>
<div class="grid" id="grid"></div>
<script type="module">
import { PreviewDisplay, SvgDisplay, headerSvg } from './sdk/display.mjs';
import { shownControls } from './sdk/controls.mjs';
const job = JOB;
const grid = document.getElementById('grid');
const cell = (html, cls) => { const d = document.createElement('div'); d.className = cls || ''; d.innerHTML = html; grid.append(d); };
const manifest = await (await fetch('preview/manifest.json')).json();
const module = await WebAssembly.compile(await (await fetch('preview/engine.wasm')).arrayBuffer());
const art = await (await fetch('preview/panel.svg')).text();
const p = manifest.definition.parameters;
const words = p.map(x => Math.round(x.default * 0x7fffffff));
while (words.length < 8) words.push(0);
cell(''); for (const t of job.times) cell(`${t.toFixed(2)} s${t < job.release ? ', held' : ''}`, 'time');
for (const row of job.rows) {
  const display = new PreviewDisplay(module), scene = new SvgDisplay(art, manifest.graphics);
  const amp = Object.fromEntries(['attack', 'decay', 'sustain', 'release'].map((k, i) => [k, Math.min(0x7fffffff, Math.round(row.amp[i] * 2 ** 31))]));
  cell(row.label, 'label');
  let serial = 0, held = 0, off = 0, next = 0;
  const frames = Math.round(job.times[job.times.length - 1] * 60);
  for (let f = -10; f <= frames; f++) {
    if (f === 0) { serial++; held = 1; }
    if (f === Math.round(job.release * 60)) { held = 0; off++; }
    scene.apply(display.tick(shownControls(words, [0, 1, 2, 3]), 17, serial, 1, false, { held_notes: held, note_off_serial: off },
                             row.model, 0, [0, 0, 0, 0], amp));
    if (next < job.times.length && f === Math.round(job.times[next] * 60)) {
      const titles = [0, 1, 2, 3].map(i => p[i].label.toLowerCase()), values = [0, 1, 2, 3].map(i => scene.header[i] || '');
      cell(`<svg class="screen" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 480 220"><rect width="480" height="220" fill="#000"/>` +
           headerSvg(titles, values).replace('font-family="sans-serif"', 'font-family="UniversNextPro-Light"') + scene.svg() + '</svg>');
      next++;
    }
  }
}
await document.fonts.ready;
document.title = 'done';
</script>
"""


def main():
    a = argparse.ArgumentParser()
    a.add_argument("engines", nargs="+", help="engine IDs; a drum kit as id:model,model")
    a.add_argument("--out", required=True)
    a.add_argument("--font", default=None, help="UniversNextPro-Light (default: $XY_FONT)")
    a.add_argument("--cxx", default=None)
    args = a.parse_args()
    import os
    font = pathlib.Path(args.font or os.environ.get("XY_FONT", ""))
    if not font.is_file():
        sys.exit("Set XY_FONT (or --font) to UniversNextPro-Light.ttf: the screens are judged in the device's font")
    out = pathlib.Path(args.out).resolve()
    if ROOT in out.parents or out == ROOT:
        sys.exit("--out must be outside the checkout")
    out.mkdir(parents=True, exist_ok=True)
    cxx = args.cxx or subprocess.run(["brew", "--prefix", "llvm"], capture_output=True, text=True).stdout.strip() + "/bin/clang++"
    python = SDK / ".venv" / "bin" / "python"
    if not python.exists():
        python = pathlib.Path.home() / "Documents" / "GitHub" / "open-xy" / ".venv" / "bin" / "python"
    for spec in args.engines:
        engine, _, models = spec.partition(":")
        stage = out / f"stage-{engine}"
        shutil.rmtree(stage, ignore_errors=True)
        (stage / "sdk").mkdir(parents=True)
        for name in ("display.mjs", "controls.mjs"):
            shutil.copy(SDK / "sdk" / "browser" / name, stage / "sdk" / name)
        subprocess.run([str(python), str(SDK / "sdk.py"), "preview", "--machine", str(ROOT / "machines" / engine),
                        "--out", str(stage / "preview"), "--cxx", cxx], check=True, capture_output=True)
        rows = []
        for model in [int(m) for m in models.split(",")] if models else [0]:
            for name, amp in ENVELOPES.items():
                rows.append({"label": f"{name}{f' {model}' if models else ''}", "amp": amp, "model": model})
        page = PAGE.replace("FONT", font.as_uri()).replace("COLUMNS", str(len(TIMES)))
        page = page.replace("JOB", json.dumps({"times": TIMES, "release": RELEASE, "rows": rows}))
        (stage / "index.html").write_text(page)
        width = 70 + len(TIMES) * 488 + 16
        height = 40 + len(rows) * 228 + 16
        png = out / f"{engine}-envelope.png"
        subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--allow-file-access-from-files",
                        "--force-device-scale-factor=1", f"--window-size={width},{height}", "--virtual-time-budget=8000",
                        f"--screenshot={png}", (stage / "index.html").as_uri()], check=True, capture_output=True)
        print(png)


if __name__ == "__main__":
    sys.exit(main())
