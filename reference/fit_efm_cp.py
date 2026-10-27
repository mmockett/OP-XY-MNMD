#!/usr/bin/env python3
"""Fits our EFM-CP (voices/efm_cp.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_efm_cp.py [--fit-only]

EFM-CP is an FM tone (md_efm.hpp, its modulator fed back as EFM-SD's) under a
train of claps, then a tail, through a high-pass. The sweeps start from the
tone alone (MOD 0, HPF 0); the envelopes print in 1 ms windows. Each measure
prints the Machinedrum, then ours.
"""
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from efm_measure import (V, render_md, render_ours, sweeps, hz_at, rms_db, peaks, fmt_peaks, trim,  # noqa: E402
                         centroid, noisiness)

CP = 35
NAME = "EFM-CP"
BASE = [64, 64, 64, 64, 0, 64, 64, 0]


def jobs():
    return sweeps(CP, "efmcp", [
        ("ptch", 0, BASE), ("dec", 1, BASE), ("clps", 2, BASE), ("cdec", 3, BASE),
        ("mod", 4, [64, 64, 64, 64, 0, 64, 127, 0]), ("mfrq", 5, [64, 64, 64, 64, 32, 64, 127, 0]),
        ("mdec", 6, [64, 64, 64, 64, 64, 64, 64, 0]), ("hpf", 7, [64, 64, 64, 64, 64, 64, 64, 0]),
    ], 1.5)


def ms_env(x, count=80):
    """RMS dB in 1 ms windows from the hit's start."""
    a = trim(x)
    return [10 * np.log10(np.mean(a[i * 44:(i + 1) * 44] ** 2) + 1e-20) for i in range(count)]


def claps(x):
    """The claps' onsets in ms: 1 ms windows that rise 6 dB over the one before."""
    e = ms_env(x, 150)
    return [i for i in range(1, len(e)) if e[i] > e[i - 1] + 6]


def analyse(x):
    print("== PTCH: Hz (tail, 100-300 ms) and RMS dB 0-50 ms, 100-150 ms")
    print("  ", " ".join(f"{hz_at(x[f'efmcp-ptch-{v}'], 0.1, 0.3):6.1f}" for v in V))
    print("  ", " ".join(f"{rms_db(x[f'efmcp-ptch-{v}'], 0, 0.05):6.1f}" for v in V))
    print("  ", " ".join(f"{rms_db(x[f'efmcp-ptch-{v}'], 0.1, 0.15):6.1f}" for v in V))
    for label in ("dec", "clps", "cdec"):
        print(f"== {label.upper()}: claps at (ms); RMS dB 1 ms windows to 60 ms; then 100 200 400 800 ms (10 ms)")
        for v in V:
            a = x[f"efmcp-{label}-{v}"]
            print(f"  {v:3d}: {claps(a)}")
            print("      ", " ".join(f"{d:4.0f}" for d in ms_env(a, 60)), "|",
                  " ".join(f"{rms_db(a, t, t + 0.01):5.0f}" for t in (0.1, 0.2, 0.4, 0.8)))
    for label in ("mod", "mfrq", "mdec"):
        print(f"== {label.upper()}: peaks 100-300 ms, centroid 0-50 ms, noisiness 0-50 ms")
        for v in V:
            a = x[f"efmcp-{label}-{v}"]
            print(f"  {v:3d}: {fmt_peaks(peaks(a, 0.1, 0.3, 6, -30))} | {centroid(a, 0, 0.05):6.0f} {noisiness(a, 0, 0.05):.2f}")
    print("== HPF: RMS dB 0-50 ms, centroid")
    for v in V:
        a = x[f"efmcp-hpf-{v}"]
        print(f"  {v:3d}: {rms_db(a, 0, 0.05):6.1f} {centroid(a, 0, 0.05):6.0f}")


def main():
    sets = [("Machinedrum", render_md(jobs()))]
    if "--fit-only" not in sys.argv:
        sets.append(("ours", render_ours(NAME, jobs())))
    for name, x in sets:
        print(f"######## EFM-CP ({name})")
        analyse(x)


if __name__ == "__main__":
    main()
