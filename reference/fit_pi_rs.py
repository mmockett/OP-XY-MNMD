#!/usr/bin/env python3
"""Fits our PI-RS (voices/pi_rs.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_pi_rs.py [--fit-only]

The model, read off the renders (reference/README.md has how they are made):

    nine rim modes at fixed frequencies, 144 .. 668 Hz, whose balance PTCH tilts
    PI-SD's head under them, tuned by PTCH; DEC: -60 dB in 0.2 .. 1.66 s
    the stick's crack: 5 ms of noise near 1.7 kHz (PTCH 64), louder with HARD
    RING: the head's mode at 2.75 times; RVOL, RDEC: the snare wires

Each line prints the Machinedrum's measure, then ours.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from pi_measure import V, band, burst, centroid, decays, fit_main, loudness, partials, peak_db, sweep_jobs, t60, table  # noqa: E402

NAME = "PI-RS"


def jobs():
    j = sweep_jobs(NAME, 3.0)
    base = [64, 50, 64, 0, 64, 40, 0, 0]
    for rv in (0, 127):
        for rd in (0, 64, 127):
            q = list(base)
            q[4], q[5] = rv, rd
            j.append((f"x-rs-4_{rv}-5_{rd}", 69, 1.5, q))
    return j


def high(x, a, b):
    y = band(x, 1500, 20000)
    return 20 * np.log10(np.sqrt(np.mean(y[int(a * 44100):int(b * 44100)] ** 2)) + 1e-9)


def analyse(md, ours):
    level = lambda x: "{:5.1f} {:5.1f} dB  -20 {:4.0f} ms".format(peak_db(x), loudness(x), 1000 * decays(x)[0])
    table(md, ours, "peak, rms to 100 ms", ["pi-rs-default"] + [f"pi-rs-hard-{v}" for v in V], level)
    rim = lambda x: " ".join(f"{f:.0f}/{d:.0f}" for f, d in sorted(partials(x, 0.05, 0.3, 6, 60, 0.03)))
    table(md, ours, "PTCH: the strongest modes, 50 to 300 ms", [f"pi-rs-ptch-{v}" for v in V], rim)
    ring = lambda x: "T60 {:4.2f} s  -40 at {:4.0f} ms".format(t60(x, 299, 0.02, 0.05), 1000 * decays(x)[1])
    table(md, ours, "DEC: the 299 Hz mode's ring", [f"pi-rs-dec-{v}" for v in V], ring)

    def crack(x):
        levels, hz = burst(x)
        return "{:5.0f} Hz {:5.1f} {:5.1f} dB c {:5.0f}".format(hz, np.mean(levels[:5]), levels[6], centroid(x, 0, 0.006))
    table(md, ours, "the crack: pitch, level to 5 ms and at 12 ms, brightness",
          [f"pi-rs-hard-{v}" for v in (0, 64, 127)] + [f"pi-rs-ptch-{v}" for v in (0, 64, 127)], crack)
    def ringing(x):
        near = [d for f, d in partials(x, 0.0, 0.15, 12, 60) if abs(f - 488) < 6]
        return "488 Hz {:5.1f} dB".format(near[0] if near else -99)
    table(md, ours, "RING at 0.15 s", [f"pi-rs-ring-{v}" for v in V], ringing)
    wires = lambda x: " ".join(f"{high(x, a, a + 0.02):5.1f}" for a in (0, 0.02, 0.05, 0.1, 0.2, 0.4))
    table(md, ours, "RVOL x RDEC: above 1.5 kHz at 0, 20, 50, 100, 200, 400 ms",
          [f"x-rs-4_{rv}-5_{rd}" for rv in (0, 127) for rd in (0, 64, 127)], wires)


if __name__ == "__main__":
    fit_main(NAME, jobs(), analyse)
