#!/usr/bin/env python3
"""Fits our TRX-MA (voices/trx_ma.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_ma.py [--md-only]

The maracas' page is ATT SUS REV DAMP / RATL RTYP TONE HARD: a shaker of
bright noise. Each control is swept from the design's defaults (ATT 0, SUS 40,
REV 0, DAMP 0, RATL 40, RTYP 0, TONE 64, HARD 64) and measured: the level over
time (2 ms windows, dB under the peak), when the peak comes, the spectrum's
centroid and its band edges early and late, and how much the level flickers
from millisecond to millisecond (the rattle).
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from trx import live, render_machine, onset, env_ms, centroid  # noqa: E402

FIVE = [0, 32, 64, 96, 127]
MA = 25
LABELS = ["ATT", "SUS", "REV", "DAMP", "RATL", "RTYP", "TONE", "HARD"]
CLEAN = [0, 40, 0, 0, 40, 0, 64, 64]
TIMES = (0, 5, 10, 20, 40, 60, 80, 120, 160, 200, 250, 300, 400)


def jobs():
    j = []
    for i, label in enumerate(LABELS):
        for v in FIVE:
            p = list(CLEAN)
            p[i] = v
            j.append((f"ma-{label.lower()}-{v}", MA, 1.0, p))
    return j


def edges(a, t0, t1):
    """The band where the smoothed spectrum is within 6 dB of its top."""
    seg = a[int(t0 * 44100):int(t1 * 44100)]
    if len(seg) < 256 or np.abs(seg).max() < 1e-7:
        return "  silent"
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 14)) ** 2
    f = np.fft.rfftfreq(1 << 14, 1 / 44100)
    sm = np.convolve(s, np.ones(41) / 41, mode="same")
    above = f[sm > sm.max() / 4]
    return f"{above.min():5.0f}-{above.max():5.0f}"


def analyse(x, label):
    get = lambda n: x[n][int(np.argmax(np.abs(x[n]) > np.abs(x[n]).max() * 0.01)):]
    print(f"######## {label}")
    print("== level (2 ms windows, dB under the peak) at " + " ".join(f"{t:4d}" for t in TIMES)
          + " ms | peak at ms, peak dB | centroid and band 0-40 ms, 60-160 ms | flicker dB")
    for name in LABELS:
        for v in FIVE:
            a = get(f"ma-{name.lower()}-{v}")
            n = len(a) // 88
            e = 20 * np.log10(np.sqrt(np.mean(a[: n * 88].reshape(n, 88) ** 2, axis=1)) + 1e-12)
            top = int(np.argmax(e))
            row = " ".join(f"{e[min(n - 1, t // 2)] - e[top]:4.0f}" for t in TIMES)
            fine = env_ms(a[:int(0.2 * 44100)])
            live_ms = fine[fine > fine.max() - 30]
            flicker = float(np.std(np.diff(live_ms))) if len(live_ms) > 3 else 0.0
            print(f"  {name} {v:3d}: {row} | {top * 2:4d} {20 * np.log10(np.abs(a).max()):6.1f} | "
                  f"{centroid(a, 0, 0.04):6.0f} {edges(a, 0, 0.04)} {centroid(a, 0.06, 0.16):6.0f} {edges(a, 0.06, 0.16)} | {flicker:4.1f}")


def main():
    analyse(live(jobs()), "Machinedrum TRX-MA")
    if "--md-only" not in sys.argv:
        analyse(render_machine("TRX-MA", jobs()), "TRX-MA (ours)")


if __name__ == "__main__":
    main()
