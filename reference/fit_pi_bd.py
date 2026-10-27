#!/usr/bin/env python3
"""Fits our PI-BD (voices/pi_bd.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_pi_bd.py [--fit-only]

The model, read off the renders (reference/README.md has how they are made):

    two membrane modes, f and 1.595 f, f = 28.9 Hz * 2^(1.97 PTCH)
    both start higher by the strike's stretch, ratio 1 + A(TENS) e^(-t / 27 ms)
    the fundamental falls 60 dB in 31.3 ms per DEC step; the second in 0.1 .. 1.2 s
    a short click from the strike, duller with HAMR

Each line prints the Machinedrum's measure, then ours.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from pi_measure import V, burst, centroid, fit_main, loudness, fundamental, mode_track, peak_db, sweep_jobs, t60, table  # noqa: E402

NAME = "PI-BD"


def jobs():
    j = sweep_jobs(NAME, 3.0)
    base = [64, 50, 64, 64, 40, 0, 0, 0]
    for p in (0, 32, 96, 127):
        for t in (0, 64, 127):
            q = list(base)
            q[0], q[4] = p, t
            j.append((f"x-bd-0_{p}-4_{t}", 64, 1.0, q))
    return j


def bend(x):
    """The stretch at the start (ratio) and its time constant, from the fundamental's track."""
    late = fundamental(x, 0.3, 1.0, 15, 400)
    t, f = mode_track(x, [1, 1.595], late * 0.8, late * 3.5, win=0.02, hop=0.002, span=0.15, steps=200)
    e = f / late - 1
    m = (e > 0.02) & (t < 0.12)
    if m.sum() < 4:
        return late, 1.0, 0.0
    a, b = np.polyfit(t[m], np.log(e[m]), 1)
    return late, 1 + float(np.exp(b)), float(-1000 / a)


def analyse(md, ours):
    late = lambda x: f"{fundamental(x, 0.3, 1.0, 15, 400):6.1f} Hz"
    table(md, ours, "PTCH: settled pitch", [f"pi-bd-ptch-{v}" for v in V], late)
    stretch = lambda x: "x{1:.2f} tau {2:5.1f} ms".format(*bend(x))
    table(md, ours, "TENS: the strike's stretch", [f"pi-bd-tens-{v}" for v in V], stretch)
    table(md, ours, "PTCH x TENS: stretch", [f"x-bd-0_{p}-4_{t}" for p in (0, 32, 96, 127) for t in (0, 64, 127)], stretch)
    body = lambda x: f"T60 {t60(x, fundamental(x, 0.3, 1.0, 15, 400), 0.05, 0.1):5.2f} s"
    table(md, ours, "DEC: the fundamental's ring", [f"pi-bd-dec-{v}" for v in V], body)
    second = lambda x: f"T60 {t60(x, 1.595 * fundamental(x, 0.3, 1.0, 15, 400), 0.05, 0.1):5.2f} s"
    table(md, ours, "DEC: the second mode's ring", [f"pi-bd-dec-{v}" for v in V], second)
    def strike(x):
        levels, hz = burst(x)
        return f"{peak_db(x):5.1f} {loudness(x):5.1f} dB  burst {hz:5.0f} Hz {np.mean(levels[:5]):5.1f} {levels[6]:5.1f} dB"
    for c in ("ptch", "tens", "hamr", "hard", "damp"):
        table(md, ours, f"{c.upper()}: peak, rms to 100 ms; the strike's burst, its pitch, level to 5 ms and at 12 ms",
              [f"pi-bd-{c}-{v}" for v in V], strike)


if __name__ == "__main__":
    fit_main(NAME, jobs(), analyse)
