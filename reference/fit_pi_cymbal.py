#!/usr/bin/env python3
"""Fits our PI-RC, PI-CC and PI-HH (voices/pi_cymbal.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_pi_cymbal.py [RC CC HH] [--fit-only]

The model, read off the renders (reference/README.md has how they are made):

    sixteen partials per plate (the averaged spectrum of every hit), struck afresh
    at each hit; every control adds to the level, the ring and a tilt across the
    partials, as measured at each step; GRAB (PI-RC, PI-CC) grabs the cymbal after a
    hold, CLOS (PI-HH) closes the hat after one

Each line prints the Machinedrum's measure, then ours: rms to 200 ms, the -20 and
-40 dB times, and the brightness to 300 ms.

    ../open-xy/.venv/bin/python reference/fit_pi_cymbal.py --hiss [RC CC HH]

prints the tables behind the plates' dense high layer: what each control adds, at
0, 16, ..., 127, to the level above 6.5 kHz (20 to 60 ms, Q16 octaves of amplitude)
and to its ring (Q16 octaves of time, from its fall from 20-40 ms to 80-160 ms).
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools"))
import numpy as np  # noqa: E402
import catalog  # noqa: E402
from make_engine import defaults  # noqa: E402
from pi_measure import V, centroid, decays, fit_main, loudness, sweep_jobs, table  # noqa: E402
from probe import render_many  # noqa: E402
from trx import start  # noqa: E402

RATE = 44100


def high_level(x, t0, t1, low=6500.0):
    """Mean power above `low`, in a window from the hit's start."""
    x = x[start(x):]
    s = x[int(t0 * RATE):int(t1 * RATE)]
    s = s * np.hanning(len(s))
    p = np.abs(np.fft.rfft(s)) ** 2
    f = np.fft.rfftfreq(len(s), 1 / RATE)
    return 10 * np.log10(p[f >= low].sum() / len(s) + 1e-14)


def hiss_measure(hits):
    """(level dB at 20-60 ms, -60 dB time in ms) over several hits of one setting."""
    level = np.mean([high_level(x, 0.02, 0.06) for x in hits])
    fall = np.mean([high_level(x, 0.02, 0.04) - high_level(x, 0.08, 0.16) for x in hits])
    return level, 60 / max(fall, 1.0) * 90


def hiss_tables(name, hits=2):
    _, ident, slots = catalog.MACHINES[name]
    design = list(defaults(name))
    jobs = []
    for c, label in enumerate(slots):
        for v in V:
            p = list(design)
            p[c] = v
            jobs += [(f"pi-fix-hiss-{name}-{'-'.join(map(str, p))}-{h}", ident, 0.3, p) for h in range(hits)]
    done = render_many(jobs)
    base = None
    levels, rings = [], []
    for c, label in enumerate(slots):
        row_l, row_r = [], []
        for v in V:
            p = list(design)
            p[c] = v
            row = [done[f"pi-fix-hiss-{name}-{'-'.join(map(str, p))}-{h}"] for h in range(hits)]
            row_l.append(hiss_measure(row) if abs(row[0]).max() > 1e-5 else None)
        levels.append(row_l)
    # The design's own setting, from every control's run at its design value.
    centre = [levels[c][min(range(len(V)), key=lambda i: abs(V[i] - design[c]))] for c in range(len(slots))]
    base = (np.mean([m[0] for m in centre if m]), np.mean([m[1] for m in centre if m]))
    print(f"{name}: at the design, {base[0]:.1f} dB, -60 dB in {base[1]:.0f} ms")
    for table_name, fn in (("level", lambda m: (m[0] - base[0]) / 6.0206), ("ring", lambda m: np.log2(m[1] / base[1]))):
        print(f"  {table_name}:")
        for c, label in enumerate(slots):
            vals = [fn(m) if m else (-6.0 if table_name == "level" else 0.0) for m in levels[c]]
            q = [int(round(v * 65536)) for v in vals]
            print(f"    {label or '-':5s}", "{" + ", ".join(map(str, q)) + "},")


def measure(x):
    if abs(x).max() < 1e-5:
        return "silent"
    d20, d40 = decays(x)
    return "{:5.1f} dB {:4.0f} {:4.0f} ms c {:4.1f}k".format(loudness(x, 0.2), 1000 * d20, 1000 * d40, centroid(x, 0, 0.3) / 1000)


def analyse_for(name):
    short = name.split("-")[1].lower()

    def analyse(md, ours):
        for label in catalog.MACHINES[name][2]:
            if label:
                table(md, ours, f"{name} {label}", [f"pi-{short}-{label.lower()}-{v}" for v in V], measure)
    return analyse


if __name__ == "__main__":
    if "--hiss" in sys.argv:
        for short in [a for a in sys.argv[1:] if not a.startswith("-")] or ["RC", "CC", "HH"]:
            hiss_tables(f"PI-{short}")
        sys.exit()
    for short in [a for a in sys.argv[1:] if not a.startswith("-")] or ["RC", "CC", "HH"]:
        name = f"PI-{short}"
        fit_main(name, sweep_jobs(name, 3.0), analyse_for(name))
