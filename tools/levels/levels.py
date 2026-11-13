#!/usr/bin/env python3
"""The levels report: each machine's loudness at a base and a head, rendered
through the SDK's host voice path, so a level change can be read in dB.

Synths: the init sound and every named preset (definition.json), one held note
at a time on C2, C3 and C4 (2 s held, 0.3 s release, 8 voices, no limiting:
tools/levels/render_synth.cpp). Kits: each of the 24 default keys, one hit
(render_kit.cpp). Each render is measured for its K-weighted momentary
loudness (ITU-R BS.1770, 400 ms windows every 100 ms, mono as one channel),
its maximum taken as LUFS-M, and its sample peak (1.0 = full scale; the SDK
holds a voice to 0.5).

  python tools/levels/levels.py --sdk open-xy --base base --head head --out levels \
      --synth mach --synth hexagon/wane:wane --kit machines/mdkit

A machine is a folder relative to both checkouts, optionally ":namespace"
(default: the folder's name). WAVs of C3 (synths) and of every key (kits)
go to <out>/<machine>/<base|head>/, with report.md and report.json.
Runs in CI: it compiles host binaries.
"""
import argparse
import json
import math
import os
import pathlib
import statistics
import subprocess
import sys

import numpy as np
from scipy.signal import lfilter

HERE = pathlib.Path(__file__).resolve().parent
RATE = 44100
NOTES = (36, 48, 60)
KEEP = 48   # the note whose WAVs are kept


def biquads(fs):
    """BS.1770's K-weighting at fs: a high shelf (+4 dB) and a high-pass."""
    g, q, fc = 4.0, 1 / math.sqrt(2), 1500.0
    a = 10 ** (g / 40)
    w = 2 * math.pi * fc / fs
    al = math.sin(w) / (2 * q)
    c = math.cos(w)
    shelf = ([a * ((a + 1) + (a - 1) * c + 2 * math.sqrt(a) * al), -2 * a * ((a - 1) + (a + 1) * c),
              a * ((a + 1) + (a - 1) * c - 2 * math.sqrt(a) * al)],
             [(a + 1) - (a - 1) * c + 2 * math.sqrt(a) * al, 2 * ((a - 1) - (a + 1) * c),
              (a + 1) - (a - 1) * c - 2 * math.sqrt(a) * al])
    q, fc = 0.5, 38.0
    w = 2 * math.pi * fc / fs
    al = math.sin(w) / (2 * q)
    c = math.cos(w)
    high = ([(1 + c) / 2, -(1 + c), (1 + c) / 2], [1 + al, -2 * c, 1 - al])
    return shelf, high


SHELF, HIGH = biquads(RATE)


def read_wav(path):
    data = pathlib.Path(path).read_bytes()
    at = data.index(b"data") + 8
    return np.frombuffer(data[at:], dtype="<f4").astype(np.float64)


def measure(x):
    peak = float(np.max(np.abs(x))) if len(x) else 0.0
    y = lfilter(SHELF[0], SHELF[1], x)
    y = lfilter(HIGH[0], HIGH[1], y)
    win, hop = int(0.4 * RATE), int(0.1 * RATE)
    if len(y) < win:
        y = np.pad(y, (0, win - len(y)))
    sq = np.concatenate(([0.0], np.cumsum(y * y)))
    means = [(sq[s + win] - sq[s]) / win for s in range(0, len(y) - win + 1, hop)]
    top = max(means)
    lufs = -0.691 + 10 * math.log10(top) if top > 0 else -math.inf
    return round(lufs, 2), round(peak, 4)


def compile_tool(sdk, tool, engine, ns, exe):
    cmd = [os.environ.get("CXX", "c++"), "-std=c++11", "-O2", "-w", "-I", str(sdk / "sdk" / "include"),
           f'-DENGINE="{engine}"', f"-DNS={ns}", str(HERE / tool), "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        print(f"compile failed: {engine}\n{r.stderr[-3000:]}", file=sys.stderr)
        return False
    return True


def presets(folder):
    """The init sound (each control's default) and every named preset, from
    definition.json, each as its eight controls 0..1 in slot order."""
    d = json.loads((folder / "definition.json").read_text())
    ids = [p["id"] for p in d["parameters"]]
    sounds = {"init": {p["id"]: p.get("default", 0.0) for p in d["parameters"]}}
    for name, values in (d.get("presets") or {}).items():
        if name.lower() != "init":
            sounds[name.lower().replace(" ", "-")] = values
    return [(name, ([float(v.get(i, 0.0)) for i in ids] + [0.0] * 8)[:8]) for name, v in sounds.items()]


def synth(args, side, root, folder, ns, work):
    exe = work / f"bin-{ns}-{side}"
    if not compile_tool(args.sdk, "render_synth.cpp", root / folder / "engine.cpp", ns, exe):
        return None
    wavs = args.out / ns / side
    wavs.mkdir(parents=True, exist_ok=True)
    result = {}
    for name, params in presets(root / folder):
        notes = {}
        for midi in NOTES:
            path = (wavs if midi == KEEP else work) / f"{name}-{midi}.wav"
            scene = (f"out {path}\nseconds 2.5\nattack 0.005\nrelease 0.3\n"
                     f"params {' '.join(f'{v:.6f}' for v in params)}\nnote 0 2.0 {midi} 1\n")
            subprocess.run([str(exe)], input=scene, text=True, capture_output=True, check=True)
            notes[midi] = measure(read_wav(path))
        result[name] = notes
    return result


def kit(args, side, root, folder, ns, work):
    exe = work / f"bin-{ns}-{side}"
    if not compile_tool(args.sdk, "render_kit.cpp", root / folder / "engine.cpp", ns, exe):
        return None
    wavs = args.out / ns / side
    wavs.mkdir(parents=True, exist_ok=True)
    lines = subprocess.run([str(exe), str(wavs)], capture_output=True, text=True, check=True).stdout.splitlines()
    result = {}
    for line in lines:
        d = json.loads(line)
        result[f"{d['key']} {d['model']}"] = {0: measure(read_wav(wavs / f"key-{d['key']}.wav"))}
    return result


def loud(notes):
    """A preset's loudness: the power mean of its notes' LUFS-M."""
    vals = [v[0] for v in notes.values() if v[0] > -math.inf]
    if not vals:
        return -math.inf
    return 10 * math.log10(sum(10 ** (v / 10) for v in vals) / len(vals))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk", type=pathlib.Path, required=True)
    ap.add_argument("--base", type=pathlib.Path, required=True)
    ap.add_argument("--head", type=pathlib.Path, required=True)
    ap.add_argument("--out", type=pathlib.Path, required=True)
    ap.add_argument("--synth", action="append", default=[])
    ap.add_argument("--kit", action="append", default=[])
    args = ap.parse_args()
    args.sdk, args.base, args.head = args.sdk.resolve(), args.base.resolve(), args.head.resolve()
    args.out = args.out.resolve()
    work = args.out / ".work"
    work.mkdir(parents=True, exist_ok=True)
    report, md = {}, ["# Levels: base against head", "",
                      "LUFS-M: K-weighted momentary maximum (400 ms), mono as one channel. Synths: one held note on "
                      "C2, C3 and C4, power-averaged; peak is the loudest of the three. Kits: one hit of each default key. "
                      "Peak: 1.0 is full scale, the SDK's voice ceiling is 0.5.", ""]
    jobs = [(s, synth) for s in args.synth] + [(k, kit) for k in args.kit]
    for spec, fn in jobs:
        folder, _, ns = spec.partition(":")
        ns = ns or pathlib.Path(folder).name
        base = fn(args, "base", args.base, folder, ns, work)
        head = fn(args, "head", args.head, folder, ns, work)
        report[ns] = {"base": base, "head": head}
        md += [f"## {ns}", "", "| sound | base LUFS-M | head LUFS-M | change dB | base peak | head peak |",
               "| --- | ---: | ---: | ---: | ---: | ---: |"]
        changes, heads = [], []
        for name in (head or base or {}):
            b = (base or {}).get(name)
            h = (head or {}).get(name)
            bl = loud(b) if b else None
            hl = loud(h) if h else None
            bp = max(v[1] for v in b.values()) if b else None
            hp = max(v[1] for v in h.values()) if h else None
            ch = hl - bl if bl is not None and hl is not None and bl > -math.inf and hl > -math.inf else None
            if ch is not None:
                changes.append(ch)
            if hl is not None and hl > -math.inf:
                heads.append(hl)
            f = lambda v, n=1: "n/a" if v is None else f"{v:.{n}f}"
            md.append(f"| {name} | {f(bl)} | {f(hl)} | {'n/a' if ch is None else f'{ch:+.2f}'} | {f(bp, 3)} | {f(hp, 3)} |")
        if changes:
            md += ["", f"Change: median {statistics.median(changes):+.2f} dB, from {min(changes):+.2f} to {max(changes):+.2f}. "
                   f"Head: median {statistics.median(heads):.1f} LUFS-M."]
        md.append("")
        print("\n".join(md[-(len(head or base or {}) + 6):]), flush=True)
    (args.out / "report.json").write_text(json.dumps(report, indent=1, default=str))
    (args.out / "report.md").write_text("\n".join(md) + "\n")
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a") as f:
            f.write("\n".join(md) + "\n")
    for p in work.iterdir():
        p.unlink()
    work.rmdir()


if __name__ == "__main__":
    main()
