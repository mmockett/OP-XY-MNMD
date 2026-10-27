#!/usr/bin/env python3
"""Fits our PI-ML (voices/pi_ml.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_pi_ml.py [--fit-only]

The model, read off the renders (reference/README.md has how they are made):

    seven modes at 0.58, 0.69, 0.74, 1, 1.11, 1.97, 2.07 times f = 372 Hz * 4^PTCH,
    their balance varying a few dB from hit to hit
    DEC: -60 dB in 50 ms .. 1.85 s; HARD: soft strikes far quieter, mostly the upper pair
    TENS: all modes start up to twice as high, relaxing in 20 .. 120 ms

Each line prints the Machinedrum's measure, then ours.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from pi_measure import V, centroid, fit_main, loudness, mode_track, peak_db, spectrum, sweep_jobs, t60, table  # noqa: E402

NAME = "PI-ML"
RATIOS = [0.581, 0.689, 0.742, 1, 1.112, 1.970, 2.070]


def jobs():
    j = sweep_jobs(NAME, 3.0)
    for k in range(4):
        j.append((f"rep-ml-{k}", 67, 2.0, [64, 50, 64, 40, 0, 0, 0, 0]))
    return j


def strongest(x, low, high, t0=0.1, t1=0.3):
    f, s = spectrum(x, t0, t1)
    m = (f > low) & (f < high)
    return float(f[m][np.argmax(s[m])])


def stretch(x, late):
    t, f = mode_track(x, RATIOS, late * 0.9, late * 3, win=0.006, hop=0.002, span=0.08, steps=150)
    e = f / late
    return "x{:.2f} at 5 ms, x{:.2f} at 30 ms".format(np.interp(0.005, t, e), np.interp(0.03, t, e))


def analyse(md, ours):
    level = lambda x: "{:5.1f} {:5.1f} dB c {:4.0f}".format(peak_db(x), loudness(x), centroid(x, 0.05, 0.3))
    table(md, ours, "repeated hits: peak, rms to 100 ms, brightness", [f"rep-ml-{k}" for k in range(4)], level)
    table(md, ours, "HARD: peak, rms to 100 ms, brightness", [f"pi-ml-hard-{v}" for v in V], level)
    table(md, ours, "PTCH (TENS 40): the pitch whose modes sound most, 100 to 300 ms",
          [f"pi-ml-ptch-{v}" for v in (0, 32, 64, 96, 127)], lambda x: "{:6.0f}".format(main(x)))
    table(md, ours, "TENS: the stretch", [f"pi-ml-tens-{v}" for v in V], lambda x: stretch(x, 743))
    ring = lambda x: "{:5.2f} {:5.2f} s".format(t60(x, 743, 0.015, 0.03), t60(x, 551, 0.015, 0.03))
    table(md, ours, "DEC: T60 at 743 and 551 Hz", [f"pi-ml-dec-{v}" for v in V], ring)


def main(x):
    """The pitch whose modes sound most from 100 to 300 ms: the summed spectrum at its ratios."""
    f, s = spectrum(x, 0.1, 0.3)
    grid = np.geomspace(300, 1600, 2000)
    score = [sum(np.interp(g * r, f, s) for r in RATIOS) for g in grid]
    return float(grid[int(np.argmax(score))])


if __name__ == "__main__":
    fit_main(NAME, jobs(), analyse)
