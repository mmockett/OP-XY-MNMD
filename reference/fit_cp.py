#!/usr/bin/env python3
"""Fits MD Clap to the Machinedrum's TRX-CP.

    ../open-xy/.venv/bin/python reference/fit_cp.py

What the renders show, each control swept from a clean setting (ROOM 0, and
ROOM 127 for the room's own controls):
  hands  broadband noise about 0.5 .. 8 kHz; HARD brightens it strongly, TONE a little
  RICH   how long each hand rings: short at 0, long at 127
  RATE   the train's length, about 3 to 62 ms: three gaps, the second an eighth
         longer and the third an eighth shorter
  room   a band about half an octave wide, falling 30 dB an octave above and 6 below;
         RTUN moves it 550 Hz .. 3 kHz, RSIZ sets its fall, none to -60 dB in about
         0.5 s, and its level, 11 dB up from small to big
For each setting: the level (dB under the peak) at 0..640 ms, then the band and
centroid of the hands (0-40 ms) and the room (150-400 ms), Machinedrum over ours.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from probe import render_many, render_machine  # noqa: E402
from fit_bd import start  # noqa: E402

RATE = 44100
V = [0, 32, 64, 96, 127]
CP = 19
LABELS = ["CLPY", "TONE", "HARD", "RICH", "RATE", "ROOM", "RSIZ", "RTUN"]
CLEAN = [64, 64, 64, 64, 64, 0, 64, 64]
TIMES = (0, 5, 10, 20, 40, 80, 160, 320, 640)


def jobs():
    j = []
    for i, label in enumerate(LABELS):
        for v in V:
            p = list(CLEAN)
            p[i] = v
            if label in ("RSIZ", "RTUN"):
                p[5] = 127
            j.append((f"cp-{label.lower()}-{v}", CP, 1.0, p))
    return j


def env_ms(a):
    n = len(a) // 44
    return 20 * np.log10(np.sqrt(np.mean(a[: n * 44].reshape(n, 44) ** 2, axis=1)) + 1e-12)


def band(a, t0, t1):
    seg = a[int(t0 * RATE):int(t1 * RATE)]
    if len(seg) < 256 or np.abs(seg).max() < 1e-6:
        return "silent"
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 15)) ** 2
    f = np.fft.rfftfreq(1 << 15, 1 / RATE)
    sm = np.convolve(s, np.ones(31) / 31, mode="same")
    m = f > 100
    above = f[m][sm[m] > sm[m].max() / 4]
    return f"{above.min():5.0f}-{above.max():5.0f} Hz, centroid {np.sum(f * s) / np.sum(s):5.0f}"


def row(a):
    a = a[start(a):]
    e = env_ms(a)
    peak = e.max()
    levels = " ".join(f"{e[min(len(e) - 1, t)] - peak:5.0f}" for t in TIMES)
    return f"{levels} | hands {band(a, 0, 0.04)} | room {band(a, 0.15, 0.4)}"


def main():
    md = render_many(jobs())
    xy = render_machine("TRX-CP", jobs())
    print("level at " + " ".join(f"{t:5d}" for t in TIMES) + " ms")
    for label in LABELS:
        for v in V:
            name = f"cp-{label.lower()}-{v}"
            print(f"{label} {v:3d} MD {row(md[name])}\n         xy {row(xy[name])}")
        print()


if __name__ == "__main__":
    main()
