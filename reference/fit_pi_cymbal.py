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
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools"))
import catalog  # noqa: E402
from pi_measure import V, centroid, decays, fit_main, loudness, sweep_jobs, table  # noqa: E402


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
    for short in [a for a in sys.argv[1:] if not a.startswith("-")] or ["RC", "CC", "HH"]:
        name = f"PI-{short}"
        fit_main(name, sweep_jobs(name, 3.0), analyse_for(name))
