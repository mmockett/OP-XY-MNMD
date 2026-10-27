#!/usr/bin/env python3
"""Prints the level envelope (dB under the peak) of Machinedrum hits and ours, side by side.
    ../open-xy/.venv/bin/python reference/show_env.py trx_bd bd-dec-0 bd-dec-64 ..."""
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from fit_bd import envelope_db  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
import numpy as np  # noqa: E402

T = [0, 2, 4, 6, 8, 10, 15, 20, 30, 50, 80, 120, 200, 300, 500, 800, 1200, 2000]
machine, names = sys.argv[1], sys.argv[2:]
read = lambda p: np.frombuffer(p.read_bytes()[44:], dtype="<f4").astype(np.float64)
print("ms           " + " ".join(f"{t:6d}" for t in T))
for n in names:
    for side, folder in (("md", "probe-md"), ("xy", f"probe-{machine}")):
        e = envelope_db(read(ROOT / "local" / "ref" / folder / f"{n}.wav"))
        pk = e.max()
        print(f"{side} {n:10s}" + " ".join(f"{e[min(len(e) - 1, int(t * 44.1))] - pk:6.1f}" for t in T))
