#!/usr/bin/env python3
"""Fits our TRX-CL (voices/trx_cl.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_cl.py [--md-only]

The claves' page is PTCH DEC DUAL ENH / TUNE CLIC. The renders show a sine
that PTCH moves linearly in Hz and DEC lets ring, a second tone that TUNE
places on the same scale, DUAL and ENH shaping the strike. Each control is
swept from PTCH 64, DEC 64, DUAL 0, ENH 40, TUNE 64, CLIC 0.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from trx import live, render_machine, onset, t60, level_at, env_ms, peaks, rms_db  # noqa: E402

V = [0, 16, 32, 48, 64, 80, 96, 112, 127]
FIVE = [0, 32, 64, 96, 127]
CL = 26
LABELS = ["PTCH", "DEC", "DUAL", "ENH", "TUNE", "CLIC"]
CLEAN = [64, 64, 0, 40, 64, 0]


def jobs():
    j = []
    for i, label in enumerate(LABELS):
        for v in (V if label in ("PTCH", "DEC", "TUNE") else FIVE):
            p = list(CLEAN)
            p[i] = v
            if label == "TUNE":
                p[0] = 0    # the main tone low, out of the second's way
            j.append((f"cl-{label.lower()}-{v}", CL, 2.5 if label == "DEC" else 0.6, p + [0, 0]))
    return j


def strongest(a, t0, t1, lo, hi):
    seg = a[int(t0 * 44100):int(t1 * 44100)]
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 17))
    f = np.fft.rfftfreq(1 << 17, 1 / 44100)
    m = (f > lo) & (f < hi)
    return float(f[m][np.argmax(s[m])])


def analyse(x, label):
    get = lambda n: onset(x[n])
    print(f"######## {label}")
    print("== the tone's Hz (20-80 ms), PTCH sweep")
    hz = {v: strongest(get(f"cl-ptch-{v}"), 0.02, 0.08, 300, 5000) for v in V}
    print("  " + " ".join(f"{hz[v]:6.1f}" for v in V))
    a, b = np.polyfit(np.array(V) / 127, [hz[v] for v in V], 1)
    print(f"  fit: {b:.1f} Hz + {a:.1f} Hz * PTCH/127;  peak dB " + " ".join(f"{20 * np.log10(np.abs(get(f'cl-ptch-{v}')).max()):5.1f}" for v in V))
    print("== DEC: -60 dB ms, peak dB, level at 0 1 2 3 5 10 20 50 ms under the peak")
    for v in V:
        s = get(f"cl-dec-{v}")
        e = env_ms(s)
        print(f"  DEC {v:3d}: t60 {t60(s) * 1000:7.0f}  peak {20 * np.log10(np.abs(s).max()):6.1f}  "
              + " ".join(f"{e[t] - e.max():6.1f}" for t in (0, 1, 2, 3, 5, 10, 20, 50)))
    print("== TUNE (PTCH 0, tone at 942 Hz): the second tone's Hz and level under the first, 20-80 ms; peak")
    for v in V:
        s = get(f"cl-tune-{v}")
        f2 = strongest(s, 0.02, 0.08, 1000, 5000) if v else strongest(s, 0.02, 0.08, 300, 5000)
        l1, l2 = level_at(s, hz[0], 0.02, 0.08, 0.01), level_at(s, f2, 0.02, 0.08, 0.01)
        print(f"  TUNE {v:3d}: {f2:7.1f} Hz  {l2 - l1:6.1f} dB  peak {20 * np.log10(np.abs(s).max()):6.1f}  {peaks(s, 0.02, 0.08, 6, 300)}")
    for name in ("DUAL", "ENH", "CLIC"):
        print(f"== {name}: peak dB, rms 0-5 ms and 5-40 ms, level per ms 0..20 under the peak, harmonics 3, 5, 7 (20-80 ms)")
        for v in FIVE:
            s = get(f"cl-{name.lower()}-{v}")
            e = env_ms(s)
            f0 = hz[64]
            lv = [level_at(s, f0 * k, 0.02, 0.08, 0.01) for k in (1, 3, 5, 7)]
            print(f"  {name} {v:3d}: peak {20 * np.log10(np.abs(s).max()):6.1f}  rms {rms_db(s, 0, 0.005):6.1f} {rms_db(s, 0.005, 0.04):6.1f}  "
                  + " ".join(f"{e[t] - e.max():5.1f}" for t in range(0, 21, 2)) + "  " + " ".join(f"{l - lv[0]:4.0f}" for l in lv[1:]))


def main():
    analyse(live(jobs()), "Machinedrum TRX-CL")
    if "--md-only" not in sys.argv:
        analyse(render_machine("TRX-CL", jobs()), "TRX-CL (ours)")


if __name__ == "__main__":
    main()
