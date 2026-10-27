#!/usr/bin/env python3
"""Fits our TRX-CY (voices/trx_cy.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_cy.py [--md-only]

The cymbal's page is RICH DEC TOP TTUN / SIZE PEAK. Its spectrum is a fixed set
of lines, the odd harmonics of square waves near 432, 513, 551, 760, 774 and
848 Hz (with lines at 1160 and 1547 Hz), weighted by the controls: so the fit
compares band levels, half an octave wide from 250 Hz to 16 kHz, 20 to 120 ms
in and 400 to 600 ms in, and the decay. Each control is swept from the
design's defaults (RICH 64, DEC 50, TOP 64, TTUN 64, SIZE 64, PEAK 64).
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from trx import live, render_machine, onset  # noqa: E402

V = [0, 16, 32, 48, 64, 80, 96, 112, 127]
FIVE = [0, 32, 64, 96, 127]
CY = 24
LABELS = ["RICH", "DEC", "TOP", "TTUN", "SIZE", "PEAK"]
CLEAN = [64, 50, 64, 64, 64, 64]
CENTRES = [250 * 2 ** (k / 2) for k in range(13)]


def jobs():
    j = []
    for i, label in enumerate(LABELS):
        for v in (V if label == "DEC" else FIVE):
            p = list(CLEAN)
            p[i] = v
            j.append((f"cy-{label.lower()}-{v}", CY, 3.0 if label == "DEC" else 1.0, p + [0, 0]))
    return j


def bands(a, t0, t1):
    seg = a[int(t0 * 44100):int(t1 * 44100)]
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / 44100)
    return np.array([10 * np.log10(s[(f >= c / 2 ** 0.25) & (f < c * 2 ** 0.25)].sum() + 1e-20) for c in CENTRES])


def analyse(x, label):
    get = lambda n: onset(x[n])
    print(f"######## {label}")
    ref = bands(get("cy-rich-64"), 0.02, 0.12).max()
    print("== bands, dB under the default's loudest, 20-120 ms | 400-600 ms (every other band)")
    print("  Hz        " + " ".join(f"{c:5.0f}" for c in CENTRES))
    for name in LABELS:
        if name == "DEC":
            continue
        for v in FIVE:
            a = get(f"cy-{name.lower()}-{v}")
            print(f"  {name} {v:3d}: " + " ".join(f"{b - ref:5.0f}" for b in bands(a, 0.02, 0.12))
                  + "  | " + " ".join(f"{b - ref:4.0f}" for b in bands(a, 0.4, 0.6)[::2])
                  + f"  peak {20 * np.log10(np.abs(a).max()):6.1f}")
    times = (0, 50, 100, 200, 300, 500, 700, 1000, 1500, 2000, 2800)
    print("== DEC: peak dB; level (20 ms windows) under the first window's at " + " ".join(f"{t}" for t in times) + " ms")
    for v in V:
        a = get(f"cy-dec-{v}")
        w = [20 * np.log10(np.sqrt(np.mean(a[int(t * 44.1):int(t * 44.1) + 882] ** 2)) + 1e-12) for t in times]
        print(f"  DEC {v:3d}: peak {20 * np.log10(np.abs(a).max()):6.1f}  " + " ".join(f"{l - w[0]:6.1f}" for l in w))


def main():
    analyse(live(jobs()), "Machinedrum TRX-CY")
    if "--md-only" not in sys.argv:
        analyse(render_machine("TRX-CY", jobs()), "TRX-CY (ours)")


if __name__ == "__main__":
    main()
