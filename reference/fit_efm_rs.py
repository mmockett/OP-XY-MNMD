#!/usr/bin/env python3
"""Fits our EFM-RS (voices/efm_rs.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_efm_rs.py [--fit-only]

EFM-RS is an FM rim (PTCH DEC MOD) and a snare part (SNAR SPTC SDEC SMOD),
through HPF. The sweeps start from the rim alone (SNAR 0) or the snare part
alone (DEC 0). Each measure prints the Machinedrum, then ours.
"""
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from efm_measure import render_md, render_ours, sweeps, describe  # noqa: E402

RS = 36
NAME = "EFM-RS"
RIM = [64, 64, 0, 0, 0, 64, 64, 0]
SNARE = [64, 0, 0, 0, 127, 64, 96, 0]
LABELS = ["ptch", "dec", "mod", "hpf", "snar", "sptc", "sdec", "smod"]


def jobs():
    return sweeps(RS, "efmrs", [
        ("ptch", 0, RIM), ("dec", 1, RIM), ("mod", 2, [64, 96, 0, 0, 0, 64, 64, 0]),
        ("hpf", 3, [64, 96, 64, 0, 64, 64, 96, 64]),
        ("snar", 4, SNARE), ("sptc", 5, SNARE), ("sdec", 6, SNARE), ("smod", 7, SNARE),
    ], 1.0)


def main():
    sets = [("Machinedrum", render_md(jobs()))]
    if "--fit-only" not in sys.argv:
        sets.append(("ours", render_ours(NAME, jobs())))
    for name, x in sets:
        print(f"######## EFM-RS ({name})")
        describe(x, "efmrs", LABELS)


if __name__ == "__main__":
    main()
