#!/usr/bin/env python3
"""Fits our TRX-CB (voices/trx_cb.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_cb.py [--md-only]

The cowbell's page is PTCH DEC ENH DAMP / TONE BUMP. Each control is swept
from PTCH 64, DEC 100, ENH 40, DAMP 0, TONE 64, BUMP 0 and measured: the level
of each line in the spectrum (the bell's square near 1117 Hz and its odd
harmonics, a second voice near 1051 Hz, and the lines at 1752, 3856 and 4556
Hz), the pitch in the first milliseconds, and the decay.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from trx import live, render_machine, onset, t60, level_at, env_ms, peaks  # noqa: E402

V = [0, 16, 32, 48, 64, 80, 96, 112, 127]
CB = 21
LABELS = ["PTCH", "DEC", "ENH", "DAMP", "TONE", "BUMP"]
CLEAN = [64, 100, 40, 0, 64, 0]
LINES = [1051, 1117, 1752, 2234, 3153, 3352, 3856, 4556, 5587]


def jobs():
    j = []
    for i, label in enumerate(LABELS):
        for v in V:
            p = list(CLEAN)
            p[i] = v
            j.append((f"cb-{label.lower()}-{v}", CB, 1.5 if label == "DEC" else 0.6, p + [0, 0]))
    return j


def lines(a, t0=0.02, t1=0.06):
    lv = [level_at(a, f, t0, t1, 0.02) for f in LINES]
    return " ".join(f"{l - lv[1]:5.0f}" for l in lv) + f"  (1117 at {lv[1]:5.1f} dB)"


def early(a):
    """The strongest line's frequency in 4 ms windows from 0 to 16 ms."""
    out = []
    for t in (0, 0.002, 0.004, 0.008, 0.012):
        seg = a[int(t * 44100):int(t * 44100) + 176]
        s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 15))
        f = np.fft.rfftfreq(1 << 15, 1 / 44100)
        m = (f > 300) & (f < 2000)
        out.append(f[m][np.argmax(s[m])])
    return " ".join(f"{v:6.0f}" for v in out)


def analyse(x, label):
    get = lambda n: onset(x[n])
    print(f"######## {label}")
    print("== line levels (dB under 1117 Hz), 20-60 ms: " + " ".join(f"{f:5d}" for f in LINES))
    for name in LABELS:
        if name == "DEC":
            continue
        for v in V:
            a = get(f"cb-{name.lower()}-{v}")
            print(f"  {name} {v:3d}: {lines(a)}  | early Hz {early(a)}  peak {20 * np.log10(np.abs(a).max()):6.1f}")
    print("== DEC: -60 dB ms, peak dB, level at 0 1 2 5 10 20 50 100 ms; lines at 100-200 ms")
    for v in V:
        a = get(f"cb-dec-{v}")
        e = env_ms(a)
        late = lines(a, 0.1, 0.2) if len(a) > 0.2 * 44100 and e[150] > e.max() - 70 else ""
        print(f"  DEC {v:3d}: t60 {t60(a) * 1000:6.0f}  peak {20 * np.log10(np.abs(a).max()):6.1f}  "
              + " ".join(f"{e[t] - e.max():5.1f}" for t in (0, 1, 2, 5, 10, 20, 50, 100)) + f"  {late}")


def main():
    analyse(live(jobs()), "Machinedrum TRX-CB")
    if "--md-only" not in sys.argv:
        analyse(render_machine("TRX-CB", jobs()), "TRX-CB (ours)")


if __name__ == "__main__":
    main()
