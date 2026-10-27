#!/usr/bin/env python3
"""Fits our EFM-XT (voices/efm_xt.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_efm_xt.py

EFM-XT is laid out as EFM-BD with CLIC in MFB's place, and is the same
machine an octave and a bit higher (reference/fit_efm_bd.py has the model);
these are EFM-BD's sweeps, then CLIC's.
"""
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from efm_measure import V, render_md, render_ours, envelope_db, peak_db, centroid  # noqa: E402
import fit_efm_bd  # noqa: E402

XT = 34
NAME = "EFM-XT"


def jobs():
    j = fit_efm_bd.jobs(XT, "efmxt", "clic")
    for v in V:
        j.append((f"efmxt-click-{v}", XT, 0.5, [64, 64, 16, 64, 0, 64, 64, v]))
    return j


def analyse(x, label):
    fit_efm_bd.analyse(x, label, "efmxt", "clic")
    print("== CLIC (MOD 0): peak dB, dB at 1 2 4 8 ms under the peak, brightness of the first 5 ms")
    for v in V:
        a = x[f"efmxt-click-{v}"]
        env = envelope_db(a, 0.5)
        pk = env.max()
        print(f"  CLIC {v:3d}: peak {peak_db(a):6.1f} " + " ".join(f"{env[int(t * 44.1)] - pk:6.1f}" for t in (1, 2, 4, 8))
              + f"  centroid {centroid(a, 0, 0.005):6.0f}")


def main():
    analyse(render_md(jobs()), "Machinedrum EFM-XT")
    if "--fit-only" not in sys.argv:
        analyse(render_ours(NAME, jobs()), "EFM-XT (ours)")


if __name__ == "__main__":
    main()
