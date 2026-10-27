#!/usr/bin/env python3
"""Fits our EFM-CB (voices/efm_cb.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_efm_cb.py [--fit-only]

EFM-CB is two tones a fifth apart (FB shapes them), frequency-modulated by MOD,
MFRQ and MDEC, with SNAP at the attack. The sweeps start from the plain tones
(SNAP 0, FB 0, MOD 0). Each measure prints the Machinedrum, then ours.
"""
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from efm_measure import render_md, render_ours, sweeps, describe  # noqa: E402

CB = 37
NAME = "EFM-CB"
TONE = [64, 96, 0, 0, 0, 64, 64, 0]
LABELS = ["ptch", "dec", "snap", "fb", "mod", "mfrq", "mdec"]


def jobs():
    return sweeps(CB, "efmcb", [
        ("ptch", 0, TONE), ("dec", 1, TONE), ("snap", 2, TONE), ("fb", 3, TONE),
        ("mod", 4, [64, 96, 0, 0, 0, 64, 127, 0]), ("mfrq", 5, [64, 96, 0, 0, 32, 64, 127, 0]),
        ("mdec", 6, [64, 96, 0, 0, 64, 64, 64, 0]),
    ], 1.0)


def main():
    sets = [("Machinedrum", render_md(jobs()))]
    if "--fit-only" not in sys.argv:
        sets.append(("ours", render_ours(NAME, jobs())))
    for name, x in sets:
        print(f"######## EFM-CB ({name})")
        describe(x, "efmcb", LABELS, 0.02, 0.2)


if __name__ == "__main__":
    main()
