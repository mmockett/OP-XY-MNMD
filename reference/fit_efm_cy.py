#!/usr/bin/env python3
"""Fits our EFM-CY (voices/efm_cy.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_efm_cy.py [--fit-only]

EFM-CY is an FM cymbal (PTCH, FB, MOD, MFRQ, MDEC) through HPF. The sweeps
start from the plain carrier (FB 0, MOD 0, HPF 0). Each measure prints the
Machinedrum, then ours.
"""
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from efm_measure import render_md, render_ours, sweeps, describe  # noqa: E402

CY = 39
NAME = "EFM-CY"
PLAIN = [64, 64, 0, 0, 0, 64, 64, 0]
LABELS = ["ptch", "dec", "fb", "hpf", "mod", "mfrq", "mdec", "mfb"]


def jobs():
    return sweeps(CY, "efmcy", [
        ("ptch", 0, PLAIN), ("dec", 1, PLAIN), ("fb", 2, PLAIN), ("hpf", 3, [64, 64, 0, 0, 64, 64, 127, 0]),
        ("mod", 4, [64, 64, 0, 0, 0, 64, 127, 0]), ("mfrq", 5, [64, 64, 0, 0, 32, 64, 127, 0]),
        ("mdec", 6, [64, 64, 0, 0, 64, 64, 64, 0]), ("mfb", 2, [64, 64, 0, 0, 64, 64, 127, 0]),
    ], 1.0)


def main():
    sets = [("Machinedrum", render_md(jobs()))]
    if "--fit-only" not in sys.argv:
        sets.append(("ours", render_ours(NAME, jobs())))
    for name, x in sets:
        print(f"######## EFM-CY ({name})")
        describe(x, "efmcy", LABELS, 0.01, 0.1)


if __name__ == "__main__":
    main()
