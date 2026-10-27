#!/usr/bin/env python3
"""Fits the Monomachine's GND-SIN and GND-NOIS (voices/gnd_sin.hpp, gnd_nois.hpp).

    ../open-xy/.venv/bin/python reference/fit_mgnd.py [sin|nois ...]

Notes are held (the AMP page's HOLD at 127) and measured while they sound, as
"MNM / ours" after each engine's gain (GAIN): our output is the Monomachine's
times that.

What the Monomachine was found to do:
- GND-SIN: a pure sine at the played key (harmonics 60 dB down), peaking at
  0.183 of the output; TUNE +-2 semitones about 64, continuous.
- GND-NOIS: white noise rolled off above about 7 kHz. RED fades it into red
  noise, the octave bands falling 3 to 4 dB an octave above 150 Hz at 127, and
  is loudest near 64. ST raises the level up to +2.9 dB while STON is off, and
  does nothing with STON on; TUNE and the played key leave the noise alone.
`nois` also prints the white and red levels that best fit each RED step (the
tables in gnd_nois.hpp), solved from the octave bands.
"""
import sys

import numpy as np

from probe import render_many, render_machine
from fit_swave import SR, HOLD, fundamental, harmonics, hz, rms, db

ID = {"GND-SIN": 1, "GND-NOIS": 2}
GAIN = {"GND-SIN": 2.0, "GND-NOIS": 1.2}
EDGES = [20, 100, 200, 400, 800, 1600, 3200, 6400, 12800, 22050]


def both(name, jobs):
    pre = name.lower().replace("gnd-", "mg")
    mm = render_many([(f"{pre}-{t}", ID[name], s, v, f"gate={s - 0.1:.1f} {HOLD} {o}".strip()) for t, s, v, o in jobs], "mm")
    xy = render_machine(name, [(f"{pre}-{t}", 0, s, v, o) for t, s, v, o in jobs])
    return {t: (mm[f"{pre}-{t}"] * GAIN[name], xy[f"{pre}-{t}"]) for t, s, v, o in jobs}


def bands(x, a=0.3, b=1.4):
    """Energy in each octave band (EDGES), as a fraction of the whole."""
    seg = x[int(a * SR):int(b * SR)]; n = len(seg)
    s = np.abs(np.fft.rfft(seg * np.hanning(n))) ** 2; fr = np.fft.rfftfreq(n, 1 / SR)
    e = np.array([s[(fr >= lo) & (fr < hi)].sum() for lo, hi in zip(EDGES[:-1], EDGES[1:])])
    return e


def sin():
    print("GND-SIN")
    jobs = [(f"n{n}", 1.5, [0] * 7 + [64], f"note={n}") for n in (24, 36, 48, 60, 72, 84, 96)]
    jobs += [(f"t{t}", 1.5, [0] * 7 + [t], "") for t in (0, 32, 96, 127)]
    r = both("GND-SIN", jobs)
    print(" pitch Hz, peak and the 2nd and 3rd harmonics (dB)")
    for n in (24, 36, 48, 60, 72, 84, 96):
        m, o = r[f"n{n}"]
        hm, ho = harmonics(m, fundamental(m, hz(n)), k=3), harmonics(o, fundamental(o, hz(n)), k=3)
        print("  note %-3d %8.2f / %8.2f   peak %.3f / %.3f   h2 %5.1f / %5.1f   h3 %5.1f / %5.1f" % (
            n, fundamental(m, hz(n)), fundamental(o, hz(n)), abs(m[13000:]).max(), abs(o[13000:]).max(),
            db(hm[1]) - db(hm[0]), db(ho[1]) - db(ho[0]), db(hm[2]) - db(hm[0]), db(ho[2]) - db(ho[0])))
    for t in (0, 32, 96, 127):
        want = 261.63 * 2 ** ((t - 64) / 32 / 12)
        print("  TUNE %-3d %8.2f / %8.2f (%.2f)" % (t, fundamental(r[f"t{t}"][0], want), fundamental(r[f"t{t}"][1], want), want))


def nois():
    print("GND-NOIS")
    reds = (0, 16, 32, 48, 64, 80, 96, 112, 127)
    jobs = [(f"red{v}", 1.6, [0, v, 0, 0, 0, 0, 0, 64], "") for v in reds]
    jobs += [(f"st{v}", 1.6, [v, 0, 0, 0, 0, 0, 0, 64], "") for v in (32, 64, 96, 127)]
    jobs += [(f"st{v}-on", 1.6, [v, 0, 127, 0, 0, 0, 0, 64], "") for v in (64, 127)]
    jobs += [(f"n{n}", 1.6, [0, 0, 0, 0, 0, 0, 0, 64], f"note={n}") for n in (36, 84)]
    jobs += [("max", 1.6, [127, 64, 0, 0, 0, 0, 0, 64], "")]
    r = both("GND-NOIS", jobs)
    print(" rms")
    for t in [f"red{v}" for v in reds] + [f"st{v}" for v in (32, 64, 96, 127)] + ["st64-on", "st127-on", "n36", "n84", "max"]:
        print(f"  {t:<12}%.4f / %.4f" % (rms(r[t][0], 0.3, 1.4), rms(r[t][1], 0.3, 1.4)))
    print(" peak: max %.3f / %.3f" % (abs(r["max"][0]).max(), abs(r["max"][1]).max()))
    print(" octave bands (dB of the whole) from", EDGES[:-1])
    for v in reds:
        for side, x in zip(("MNM ", "ours"), r[f"red{v}"]):
            e = bands(x); print(f"  red{v:<4}{side} " + " ".join("%6.1f" % (10 * np.log10(a / e.sum())) for a in e))
    # The mix that fits each RED step: our white path alone (RED 0) and red path alone (RED 127, whose
    # white level is zero), scaled to the module's own table entries at those ends.
    white, red = bands(r["red0"][1]), bands(r["red127"][1])
    print(" fitted levels, white and red, against RED 0's and RED 127's (NNLS on the bands 100 Hz .. 12.8 kHz)")
    for v in reds:
        target = bands(r[f"red{v}"][0])
        a = np.array([white[1:8], red[1:8]]).T; w = 1 / target[1:8]
        coef, *_ = np.linalg.lstsq(a * w[:, None], target[1:8] * w, rcond=None)
        coef = np.sqrt(np.clip(coef, 0, None))
        print(f"  red{v:<6}white %.3f  red %.3f" % tuple(coef))


if __name__ == "__main__":
    for w in sys.argv[1:] or ["sin", "nois"]:
        {"sin": sin, "nois": nois}[w]()
