#!/usr/bin/env python3
"""Fits our PI-SD (voices/pi_sd.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_pi_sd.py [--fit-only]

The model, read off the renders (reference/README.md has how they are made):

    head modes at 1, 1.21, 1.77 (and 0.59, 3) times f, f = 86.1 Hz * 2^(1.98 PTCH),
    TENS -4 .. +9 %, HARD +3 %; DEC sets their ring, -60 dB in 20 .. 276 ms
    RING: a mode at 2.75 f, -29 .. -4 dB under the fundamental, ringing on
    a 5 ms noise burst from the strike; the wires' noise, RVOL squared, RDEC long

Each line prints the Machinedrum's measure, then ours.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from pi_measure import V, band, fit_main, fundamental, loudness, partials, peak_db, sweep_jobs, t60, table  # noqa: E402

NAME = "PI-SD"


def jobs():
    j = sweep_jobs(NAME, 3.0)
    base = [64, 50, 64, 0, 40, 64, 40, 0]
    for rv in (0, 64, 127):
        for rd in (64, 100, 127):
            q = list(base)
            q[5], q[6] = rv, rd
            j.append((f"x-sd-5_{rv}-6_{rd}", 65, 1.5, q))
    return j


def high(x, a, b):
    y = band(x, 1500, 20000)
    return 20 * np.log10(np.sqrt(np.mean(y[int(a * 44100):int(b * 44100)] ** 2)) + 1e-9)


def head(x):
    """The fundamental: the strongest peak, found again in its own band, away from its neighbours."""
    f0 = fundamental(x, 0.0, 0.1, 60, 600)
    return fundamental(band(x, 0.88 * f0, 1.1 * f0), 0.0, 0.15, 0.88 * f0, 1.1 * f0)


def analyse(md, ours):
    pitch = lambda x: "{:6.1f} Hz {:6.1f} dB".format(head(x), loudness(x))
    for c in ("ptch", "tens", "hard"):
        table(md, ours, f"{c.upper()}: the fundamental; rms to 100 ms", [f"pi-sd-{c}-{v}" for v in V], pitch)
    ring = lambda x: "T60 {:5.3f} s, -40 dB at {:4.0f} ms".format(
        t60(x, fundamental(x, 0.0, 0.1, 60, 600), 0.04, 0.03), 1000 * __import__("pi_measure").decays(x)[1])
    table(md, ours, "DEC: the fundamental's ring", [f"pi-sd-dec-{v}" for v in V], ring)

    def ringing(x):
        f0 = fundamental(x, 0.0, 0.1, 60, 600)
        ps = dict((round(f / f0, 2), d) for f, d in partials(x, 0.0, 0.1, 8, 60))
        near = [d for r, d in ps.items() if abs(r - 2.75) < 0.08]
        return "2.75 f at {:5.1f} dB; -40 at {:4.0f} ms".format(near[0] if near else -99,
                                                                1000 * __import__("pi_measure").decays(x)[1])
    table(md, ours, "RING: the ringing mode under the fundamental", [f"pi-sd-ring-{v}" for v in V], ringing)
    wires = lambda x: " ".join(f"{high(x, a, a + 0.02):5.1f}" for a in (0, 0.02, 0.05, 0.1, 0.2, 0.4))
    table(md, ours, "RVOL x RDEC: above 1.5 kHz at 0, 20, 50, 100, 200, 400 ms",
          [f"x-sd-5_{rv}-6_{rd}" for rv in (0, 64, 127) for rd in (64, 100, 127)], wires)
    table(md, ours, "RDEC (RVOL 64): above 1.5 kHz", [f"pi-sd-rdec-{v}" for v in V], wires)
    table(md, ours, "RVOL (RDEC 40): above 1.5 kHz", [f"pi-sd-rvol-{v}" for v in V], wires)
    peak = lambda x: f"{peak_db(x):6.1f} dB"
    table(md, ours, "peak", ["pi-sd-default", "pi-sd-ptch-0", "pi-sd-ptch-127", "pi-sd-ring-127"], peak)


if __name__ == "__main__":
    fit_main(NAME, jobs(), analyse)
