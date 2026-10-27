#!/usr/bin/env python3
"""Compares this repository's machines with the originals, hit by hit.

    ../open-xy/.venv/bin/python reference/compare.py [--only TRX-BD]

Renders each machine's defaults and a sweep of every control (0, 32, 64, 96,
127, the others at their defaults) twice: through the emulator from your own
ROM (local/mdref) and through our engine that holds the machine. Then it
measures both and writes:

    local/ref/report.md      per machine and parameter, how each measure moves
    local/ref/report.json    the same numbers, for fitting mappings
    local/ref/pairs/*.wav    the Machinedrum hit, a gap, then ours, to listen

The ROMs are MD_ROM and MM_ROM, by default ~/Downloads/elektron_sps1-1uw_os1.63.bin
and ~/Downloads/elektron_sfx6-60_os1.32b.bin, and are only read, never copied.
"""
import argparse
import json
import os
import pathlib
import subprocess
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import catalog  # noqa: E402
from presets import PRESETS  # noqa: E402
sys.path.insert(0, str(ROOT / "reference"))
from probe import render_machine  # noqa: E402

LOCAL = ROOT / "local"
OUT = LOCAL / "ref"
RATE = 44100
SWEEP = [0, 32, 64, 96, 127]

# Machines to compare, and seconds to render each; any machine in the catalog
# with presets can be added. The ROM's instrument must match the machine's.
TARGETS = {"TRX-BD": 1.5, "TRX-SD": 1.0, "TRX-CP": 1.0, "TRX-CH": 1.0, "TRX-OH": 1.5}


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def jobs_for(machine):
    labels = catalog.MACHINES[machine][2]
    base = list(PRESETS[machine]["defaults"]) if machine in PRESETS else [64] * 8
    jobs = [("default", base)]
    for i, label in enumerate(labels):
        if not label:
            continue
        for v in SWEEP:
            p = list(base)
            p[i] = v
            jobs.append((f"{label.lower()}-{v:03d}", p))
    return jobs


def read_wav(path):
    data = path.read_bytes()
    return np.frombuffer(data[44:], dtype="<f4").astype(np.float64)


# ---- Measures ------------------------------------------------------------------
HOP = 256


def envelope(x):
    frames = len(x) // HOP
    e = np.sqrt(np.mean(x[: frames * HOP].reshape(frames, HOP) ** 2, axis=1) + 1e-20)
    return 20 * np.log10(e)


def fall_time(env, db):
    """Seconds after the peak until the level stays db below it."""
    peak = int(np.argmax(env))
    above = np.nonzero(env[peak:] > env[peak] - db)[0]
    return float((peak + (above[-1] if len(above) else 0)) * HOP / RATE)


def pitch(x, start_ms, length_ms, low=25.0, high=2500.0):
    """The strongest spectral peak between low and high, with its share of the
    window's energy (near 1 for a pure tone): zero-padded, Hann-windowed and
    interpolated, so a 50 Hz kick reads correctly from a short window."""
    a = x[int(start_ms * RATE / 1000): int((start_ms + length_ms) * RATE / 1000)]
    if len(a) < 64 or np.sqrt(np.mean(a ** 2)) < 1e-5:
        return None, 0.0
    a = (a - a.mean()) * np.hanning(len(a))
    n = 1 << 16
    s = np.abs(np.fft.rfft(a, n)) ** 2
    f = np.fft.rfftfreq(n, 1 / RATE)
    band = (f >= low) & (f <= high)
    i = int(np.nonzero(band)[0][0] + np.argmax(s[band]))
    y0, y1, y2 = np.log(s[i - 1] + 1e-30), np.log(s[i] + 1e-30), np.log(s[i + 1] + 1e-30)
    d = y0 - 2 * y1 + y2
    delta = 0.5 * (y0 - y2) / d if d else 0.0
    delta = max(-0.5, min(0.5, delta))
    width = max(1, int(n / len(a)))   # the main lobe's half width, in bins
    share = float(np.sum(s[max(0, i - 2 * width): i + 2 * width + 1]) / (np.sum(s) + 1e-30))
    return float((i + delta) * RATE / n), share


def spectrum(x, start_ms, length_ms):
    a = x[int(start_ms * RATE / 1000): int((start_ms + length_ms) * RATE / 1000)]
    if len(a) < 64:
        return None, None
    s = np.abs(np.fft.rfft(a * np.hanning(len(a)))) ** 2 + 1e-20
    f = np.fft.rfftfreq(len(a), 1 / RATE)
    centroid = float(np.sum(f * s) / np.sum(s))
    flatness = float(np.exp(np.mean(np.log(s))) / np.mean(s))
    return centroid, flatness


def measure(x):
    env = envelope(x)
    peak = float(20 * np.log10(np.max(np.abs(x)) + 1e-12))
    onset = int(np.argmax(np.abs(x) > np.max(np.abs(x)) * 0.05)) * 1000 / RATE
    early, early_strength = pitch(x, onset + 2, 40)
    late, late_strength = pitch(x, onset + 120, 120)
    c_early, flat_early = spectrum(x, onset, 30)
    c_late, flat_late = spectrum(x, onset + 30, 120)
    return {
        "peak_db": round(peak, 1),
        "t20_ms": round(fall_time(env, 20) * 1000),
        "t40_ms": round(fall_time(env, 40) * 1000),
        "pitch_early_hz": None if early is None else round(early, 1),
        "pitch_late_hz": None if late is None else round(late, 1),
        "tonal_late": round(late_strength, 2),
        "centroid_early_hz": None if c_early is None else round(c_early),
        "centroid_late_hz": None if c_late is None else round(c_late),
        "noise_early": None if flat_early is None else round(flat_early, 3),
    }


# ---- Report ---------------------------------------------------------------------
KEYS = [("peak_db", "peak dB"), ("t20_ms", "-20 dB ms"), ("t40_ms", "-40 dB ms"),
        ("pitch_early_hz", "early Hz"), ("pitch_late_hz", "late Hz"),
        ("centroid_early_hz", "bright early"), ("centroid_late_hz", "bright late"), ("noise_early", "noise")]


def fmt(v):
    return "–" if v is None else str(v)


def pair(md, xy, path):
    """The Machinedrum hit, 0.3 s of silence, then ours, each scaled by its default's peak."""
    gap = np.zeros(int(0.3 * RATE))
    a = np.concatenate([md, gap, xy, gap])
    pcm = (np.clip(a, -1, 1) * 32767).astype("<i2").tobytes()
    head = b"RIFF" + (36 + len(pcm)).to_bytes(4, "little") + b"WAVEfmt " + (16).to_bytes(4, "little")
    head += (1).to_bytes(2, "little") + (1).to_bytes(2, "little") + RATE.to_bytes(4, "little")
    head += (RATE * 2).to_bytes(4, "little") + (2).to_bytes(2, "little") + (16).to_bytes(2, "little")
    head += b"data" + len(pcm).to_bytes(4, "little")
    path.write_bytes(head + pcm)


def main():
    parser = argparse.ArgumentParser(description="Compare the machines with the originals")
    parser.add_argument("--only", action="append")
    parser.add_argument("--no-render", action="store_true", help="re-measure the last renders")
    args = parser.parse_args()
    machines = args.only or list(TARGETS)
    for sub in ("md", "xy", "pairs"):
        (OUT / sub).mkdir(parents=True, exist_ok=True)
    from probe import render_many
    results = {}
    for machine in machines:
        instrument, md_id, _ = catalog.MACHINES[machine]
        seconds = TARGETS.get(machine, 1.0)
        jobs = [(f"cmp-{machine}-{tag}", md_id, seconds, p) for tag, p in jobs_for(machine)]
        md = render_many(jobs, instrument)
        xy = render_machine(machine, jobs)
        md_scale = 0.5 / (np.max(np.abs(md[jobs[0][0]])) + 1e-9)
        xy_scale = 0.5 / (np.max(np.abs(xy[jobs[0][0]])) + 1e-9)
        rows = {}
        for (name, _, _, p), (tag, _) in zip(jobs, jobs_for(machine)):
            rows[tag] = {"parameters": p, "md": measure(md[name]), "xy": measure(xy[name])}
            pair(md[name] * md_scale, xy[name] * xy_scale, OUT / "pairs" / f"{machine}-{tag}.wav")
        results[machine] = rows
        print(f"compared {machine}: {len(jobs)} settings", flush=True)

    (OUT / "report.json").write_text(json.dumps(results, indent=1))
    lines = ["# Comparison with the originals", "",
             "Each cell is **original / ours**. Levels are as rendered, so compare peak dB within a column,",
             "not across. Pitches are spectral peaks: early is 2-42 ms after the onset, late 120-240 ms;",
             "noise is spectral flatness (0 tonal, 1 white).", ""]
    for machine, rows in results.items():
        lines += [f"## {machine}", ""]
        head = "| setting | " + " | ".join(k[1] for k in KEYS) + " |"
        lines += [head, "|" + "---|" * (len(KEYS) + 1)]
        for tag, r in rows.items():
            cells = [f"{fmt(r['md'][k])} / {fmt(r['xy'][k])}" for k, _ in KEYS]
            lines.append(f"| {tag} | " + " | ".join(cells) + " |")
        lines.append("")
    (OUT / "report.md").write_text("\n".join(lines))
    print(f"wrote {OUT / 'report.md'}")


if __name__ == "__main__":
    main()
