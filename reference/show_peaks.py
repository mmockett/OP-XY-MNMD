#!/usr/bin/env python3
"""Peak level in each of the first 30 milliseconds, Machinedrum over ours, both
scaled to the level at 25-30 ms, where the body has settled.
    ../open-xy/.venv/bin/python reference/show_peaks.py trx_bd bd-dec-127 ..."""
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
read = lambda p: np.frombuffer(p.read_bytes()[44:], dtype="<f4").astype(np.float64)
machine = sys.argv[1]
for n in sys.argv[2:]:
    for side, folder in (("md", "probe-md"), ("xy", f"probe-{machine}")):
        a = read(ROOT / "local" / "ref" / folder / f"{n}.wav")
        a = a[int(np.argmax(np.abs(a) > 1e-4)):]
        peaks = np.array([np.abs(a[int(ms * 44.1):int((ms + 1) * 44.1)]).max() for ms in range(30)])
        ref = peaks[24:30].max()
        print(f"{side} {n:12s}", " ".join(f"{p / ref:4.2f}" for p in peaks))
