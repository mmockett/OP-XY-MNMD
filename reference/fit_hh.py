#!/usr/bin/env python3
"""Fits MD Hats to the Machinedrum's TRX-CH (OPEN 0) and TRX-OH (OPEN 127).

    ../open-xy/.venv/bin/python reference/fit_hh.py

What the renders show, each control swept from GAP 0, DEC 64, HPF 0, LPF 127, MTAL 64:
  sound  metallic partials near 3.8, 4.24, 5.3, 5.9, 6.8, 7.6, 8.4 and 9.3 kHz,
         in a band about 3.6 .. 11 kHz
  DEC    how long the level holds (TRX-CH about 5 to 200 ms), then it falls
  GAP    how fast it falls after the hold
  HPF    the band's lower edge, 3.6 .. 5.9 kHz; LPF its upper edge, 5.7 .. 10.5 kHz
  MTAL   the weight of the lower partials
  TRX-OH holds about 3.3 x as long, and falls far longer at high GAP
For each setting: the level (dB under the peak) at 0..1200 ms, then the 0-50 ms
band and centroid, Machinedrum over ours.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from probe import render_many, render_machine  # noqa: E402
from fit_bd import start  # noqa: E402
from fit_cp import env_ms, band  # noqa: E402

V = [0, 32, 64, 96, 127]
LABELS = ["GAP", "DEC", "HPF", "LPF", "MTAL"]
CLEAN = [0, 64, 0, 127, 64]
TIMES = (0, 5, 10, 20, 40, 80, 160, 320, 640, 1200)


def jobs(model):
    md_id = {"ch": 22, "oh": 23}[model]
    j = []
    for i, label in enumerate(LABELS):
        for v in V:
            p = list(CLEAN)
            p[i] = v
            j.append((f"{model}-{label.lower()}-{v}", md_id, 1.5, p + [0, 0, 0]))
    return j


def row(a):
    a = a[start(a):]
    e = env_ms(a)
    peak = e.max()
    return " ".join(f"{e[min(len(e) - 1, t)] - peak:5.0f}" for t in TIMES) + f" | {band(a, 0, 0.05)}"


def main():
    md = {**render_many(jobs("ch")), **render_many(jobs("oh"))}
    xy = {**render_machine("TRX-CH", jobs("ch")), **render_machine("TRX-OH", jobs("oh"))}
    print("level at " + " ".join(f"{t:5d}" for t in TIMES) + " ms")
    for model in ("ch", "oh"):
        for label in LABELS:
            for v in V:
                name = f"{model}-{label.lower()}-{v}"
                print(f"{model.upper()} {label} {v:3d} MD {row(md[name])}\n            xy {row(xy[name])}")
            print()


if __name__ == "__main__":
    main()
