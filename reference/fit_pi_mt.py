#!/usr/bin/env python3
"""Fits our PI-MT (voices/pi_mt.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_pi_mt.py [--fit-only]

The model, read off the renders (reference/README.md has how they are made):

    head modes at 1, 1.1, 1.99 times f = 69 Hz * 4^PTCH, and TUNE's at 1.01 .. 2.09 f (POS: its level)
    the shell: harmonics of 1 / (1.063 ms + 33.1 us * SIZE), not moved by PTCH
    DEC: -60 dB in 29.3 ms per step (head), 1.43 times that (shell); DAMP shortens the shell

Each line prints the Machinedrum's measure, then ours.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from pi_measure import V, centroid, fit_main, loudness, peak_db, spectrum, sweep_jobs, t60, table  # noqa: E402

NAME = "PI-MT"


def jobs():
    j = sweep_jobs(NAME, 3.0)
    base = [64, 50, 64, 64, 64, 0, 64, 0]
    for p in (0, 127):
        for s in (0, 32, 96, 127):
            q = list(base)
            q[0], q[6] = p, s
            j.append((f"x-mt-0_{p}-6_{s}", 66, 2.0, q))
    return j


def level_at(x, hz, t0=0.0, t1=0.2):
    f, s = spectrum(x, t0, t1)
    return 20 * np.log10(s[(f > hz * 0.985) & (f < hz * 1.015)].max() + 1e-9)


def strongest(x, low, high, t0=0.1, t1=0.5):
    f, s = spectrum(x, t0, t1)
    m = (f > low) & (f < high)
    return float(f[m][np.argmax(s[m])])


def analyse(md, ours):
    level = lambda x: "{:5.1f} {:5.1f} dB c {:4.0f}".format(peak_db(x), loudness(x), centroid(x, 0, 0.05))
    table(md, ours, "peak, rms to 100 ms, brightness to 50 ms", ["pi-mt-default"] + [f"pi-mt-hamr-{v}" for v in (0, 127)], level)
    head = lambda x: "{:6.1f} Hz".format(strongest(x, 50, 120 * 2 ** (2 * 0) * 2.4))
    table(md, ours, "PTCH: the head (strongest under 290 Hz, SIZE 64)", [f"pi-mt-ptch-{v}" for v in V],
          lambda x: "{:6.1f} Hz".format(strongest(x, 55, 300 if True else 0)))
    tube = lambda x: "{:6.1f} Hz".format(strongest(x, 170, 1000))
    table(md, ours, "SIZE: the shell's strongest", [f"pi-mt-size-{v}" for v in V], tube)
    table(md, ours, "PTCH 0 and 127 x SIZE: the shell's strongest", [f"x-mt-0_{p}-6_{s}" for p in (0, 127) for s in (0, 32, 96, 127)], tube)
    ring = lambda x: "{:5.2f} {:5.2f} {:5.2f} s".format(t60(x, 138, 0.015, 0.06), t60(x, 317, 0.015, 0.06), t60(x, 950, 0.015, 0.06))
    table(md, ours, "DEC: T60 of 138, 317, 950 Hz", [f"pi-mt-dec-{v}" for v in V], ring)
    table(md, ours, "DAMP: T60 of 138, 317, 950 Hz", [f"pi-mt-damp-{v}" for v in V], ring)
    rel = lambda hz: lambda x: "{:5.1f} dB".format(level_at(x, hz) - level_at(x, 317))
    table(md, ours, "POS: 218 Hz under the shell", [f"pi-mt-pos-{v}" for v in V], rel(218))
    table(md, ours, "TUNE: its mode", [f"pi-mt-tune-{v}" for v in V], lambda x: "{:6.1f} Hz".format(strongest(x, 145, 300, 0, 0.3)))
    table(md, ours, "HAMR: 1259 Hz under the shell", [f"pi-mt-hamr-{v}" for v in V], rel(1259))


if __name__ == "__main__":
    fit_main(NAME, jobs(), analyse)
