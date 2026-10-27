#!/usr/bin/env python3
"""Fits our GND-SN, GND-NS and GND-IM (voices/gnd_*.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_gnd.py [--fit-only]

GND-SN is a sine whose pitch starts RAMP above PTCH and falls back at RDEC's
rate; GND-NS white noise; GND-IM an impulse, up then down. Each measure prints
the Machinedrum, then ours.
"""
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from efm_measure import V, render_md, render_ours, sweeps, describe, hz_at, trim, rms_db  # noqa: E402

SN, NS, IM = 1, 2, 3


def sn_jobs():
    return sweeps(SN, "gndsn", [
        ("ptch", 0, [64, 96, 0, 0, 0, 0, 0, 0]), ("dec", 1, [64, 64, 0, 0, 0, 0, 0, 0]),
        ("ramp", 2, [64, 96, 0, 96, 0, 0, 0, 0]), ("rdec", 3, [64, 96, 32, 0, 0, 0, 0, 0]),
    ], 1.5)


def ns_jobs():
    return sweeps(NS, "gndns", [("dec", 0, [64, 0, 0, 0, 0, 0, 0, 0])], 1.5)


def im_jobs():
    base = [64, 64, 64, 64, 0, 0, 0, 0]
    return sweeps(IM, "gndim", [("up", 0, base), ("uval", 1, base), ("down", 2, base), ("dval", 3, base)], 0.5)


def sn_extra(x):
    print("== SN pitch: Hz at 2-12 ms, 20-40 ms, 100-300 ms")
    for label in ("ptch", "ramp", "rdec"):
        for v in V:
            a = x[f"gndsn-{label}-{v}"]
            print(f"  {label} {v:3d}:", " ".join(f"{hz_at(a, t0, t1):8.1f}" for t0, t1 in ((0.002, 0.012), (0.02, 0.04), (0.1, 0.3))))


def im_extra(x):
    print("== IM: the first 120 samples (x1000), every 3rd")
    for label in ("up", "uval", "down", "dval"):
        for v in (0, 64, 127):
            a = x[f"gndim-{label}-{v}"]
            s = int(np.argmax(np.abs(a) > 1e-4))
            print(f"  {label} {v:3d}:", " ".join(f"{int(1000 * y):4d}" for y in a[s:s + 360:3]))


def main():
    for name, render in [("Machinedrum", render_md)] + ([] if "--fit-only" in sys.argv else [("ours", None)]):
        print(f"######## GND ({name})")
        for jobs, machine, prefix, labels, extra in ((sn_jobs, "GND-SN", "gndsn", ["ptch", "dec", "ramp", "rdec"], sn_extra),
                                                       (ns_jobs, "GND-NS", "gndns", ["dec"], None),
                                                       (im_jobs, "GND-IM", "gndim", ["up", "uval", "down", "dval"], im_extra)):
            x = render(jobs()) if render else render_ours(machine, jobs())
            print(f"#### {machine}")
            describe(x, prefix, labels, 0.01, 0.1)
            if extra:
                extra(x)


if __name__ == "__main__":
    main()
