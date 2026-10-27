#!/usr/bin/env python3
"""Fits our EFM-HH (voices/efm_hh.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_efm_hh.py [--fit-only]

EFM-HH is a high FM pair (PTCH, MOD, MFRQ, MDEC, FB) under a tremolo (TREM,
TFRQ). The sweeps start from the plain carrier (MOD 0, FB 0, TREM 0). Each
measure prints the Machinedrum, then ours.
"""
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from efm_measure import render_md, render_ours, sweeps, describe  # noqa: E402

HH = 38
NAME = "EFM-HH"
PLAIN = [64, 64, 0, 64, 0, 64, 64, 0]
LABELS = ["ptch", "dec", "trem", "tfrq", "mod", "mfrq", "mdec", "fb", "mfb"]


def jobs():
    return sweeps(HH, "efmhh", [
        ("ptch", 0, PLAIN), ("dec", 1, PLAIN), ("trem", 2, [64, 96, 0, 64, 0, 64, 64, 0]),
        ("tfrq", 3, [64, 96, 127, 64, 0, 64, 64, 0]),
        ("mod", 4, [64, 64, 0, 64, 0, 64, 127, 0]), ("mfrq", 5, [64, 64, 0, 64, 32, 64, 127, 0]),
        ("mdec", 6, [64, 64, 0, 64, 64, 64, 64, 0]), ("fb", 7, PLAIN), ("mfb", 7, [64, 64, 0, 64, 64, 64, 127, 0]),
    ], 1.0)


def main():
    sets = [("Machinedrum", render_md(jobs()))]
    if "--fit-only" not in sys.argv:
        sets.append(("ours", render_ours(NAME, jobs())))
    for name, x in sets:
        print(f"######## EFM-HH ({name})")
        describe(x, "efmhh", LABELS, 0.005, 0.05)


if __name__ == "__main__":
    main()
