#!/usr/bin/env python3
"""Fits our PI-MA (voices/pi_ma.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_pi_ma.py [--fit-only]

The model, read off the renders (reference/README.md has how they are made):

    a shaker: the shake's energy falls over DEC (-20 dB in 70 ms .. 1.2 s); seeds hit
    at random, more with GRNS (smoother, louder), each ringing GLEN long (smoother,
    fewer grains heard); the shell's band: brighter with SIZE (3.2 .. 6 kHz), darker
    with HARD (5.2 .. 2.3 kHz)

Each line prints the Machinedrum's measure, then ours: rms to 200 ms, -20 dB time,
brightness, grains heard a second and the envelope's roughness.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from pi_measure import V, centroid, decays, fit_main, grains, loudness, sweep_jobs, table  # noqa: E402

NAME = "PI-MA"


def texture(x):
    g, r = grains(x)
    return "{:5.1f} dB {:5.0f} ms c {:5.0f}  {:4.0f}/s {:4.1f}".format(loudness(x, 0.2), 1000 * decays(x)[0],
                                                                   centroid(x, 0, 0.2), g, r)


def analyse(md, ours):
    for c in ("grns", "dec", "glen", "size", "hard"):
        table(md, ours, c.upper(), [f"pi-ma-{c}-{v}" for v in V], texture)


if __name__ == "__main__":
    fit_main(NAME, sweep_jobs(NAME, 3.0), analyse)
