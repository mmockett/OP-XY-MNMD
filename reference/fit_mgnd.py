#!/usr/bin/env python3
"""Fits the Monomachine's GND-SIN and GND-NOIS (voices/gnd_sin.hpp, gnd_nois.hpp).

    ../open-xy/.venv/bin/python reference/fit_mgnd.py [sin|nois|key ...]

Notes are held (the AMP page's HOLD at 127) and measured while they sound, as
"MNM / ours" after each engine's gain (GAIN): our output is the Monomachine's
times that.

What the Monomachine was found to do:
- GND-SIN: a pure sine at the played key (harmonics 60 dB down), peaking at
  0.183 of the output; TUNE +-2 semitones about 64, continuous.
- GND-NOIS: white noise rolled off above about 7 kHz. RED fades it into red
  noise, loudest near 64. ST raises the level up to +2.9 dB while STON is off,
  and does nothing with STON on.
- GND-NOIS's key: the red part is noise falling 6 dB an octave (to 31 Hz and
  below) through a two-pole Butterworth high-pass at half the played note's
  frequency (-3 dB at 131 Hz for middle C, -12 dB an octave under), which
  TUNE moves +-2 semitones. So at RED 127 the zero-crossing rate rises about
  3.2 x from C2 to C6 and the level falls about 2 x an octave pair. The white
  part keeps its level and low end at every key, but its top darkens as the
  key rises: its centroid falls from 5.1 kHz at C1 to 2.7 kHz at C7 (0.93
  octave), steeply above a corner near 15 kHz at C1 and 10 kHz at C8 (`top`).
- GND-NOIS's unnamed slots (3 to 6, the "-" on its LCD) do nothing measurable:
  at RED 0, 64 and 127, notes 36, 60 and 84, and with STON on, each at 32 and
  127 moves the level and octave bands no more than rendering the same
  setting twice does (`slots`).
`nois` also prints the white and red levels that best fit each RED step (the
tables in gnd_nois.hpp), solved from the octave bands; `key` the key and TUNE;
`top` the white top's corner at each key (gnd_nois.hpp's top_pole_of).
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


def key():
    print("GND-NOIS's key and TUNE")
    notes = (24, 36, 48, 60, 72, 84, 96, 108)
    jobs = [(f"k{red}-n{n}", 1.6, [0, red, 0, 0, 0, 0, 0, 64], f"note={n}") for red in (0, 64, 127) for n in notes]
    jobs += [(f"k127-t{t}", 1.6, [0, 127, 0, 0, 0, 0, 0, t], "") for t in (0, 32, 96, 127)]
    r = both("GND-NOIS", jobs)

    def crossings(x, a=0.3, b=1.4):
        seg = x[int(a * SR):int(b * SR)]
        return np.mean(np.signbit(seg[1:]) != np.signbit(seg[:-1])) * SR
    print(" rms and zero crossings a second, MNM / ours")
    for t, *_ in jobs:
        m, o = r[t]
        print(f"  {t:<10} %.4f / %.4f   %6.0f / %6.0f" % (rms(m, 0.3, 1.4), rms(o, 0.3, 1.4), crossings(m), crossings(o)))
    print(" octave bands (dB of the whole) at RED 127 from", EDGES[:-1])
    for n in notes:
        for side, x in zip(("MNM ", "ours"), r[f"k127-n{n}"]):
            e = bands(x); print(f"  n{n:<4}{side} " + " ".join("%6.1f" % (10 * np.log10(a / e.sum())) for a in e))


def mm_only(jobs):
    """The Monomachine's GND-NOIS alone, held: (name, seconds, slots, options) -> {name: samples}."""
    r = render_many([(f"mgnois-{t}", ID["GND-NOIS"], s, v, f"gate={s - 0.1:.1f} {HOLD} {o}".strip()) for t, s, v, o in jobs], "mm")
    return {t: r[f"mgnois-{t}"] * GAIN["GND-NOIS"] for t, s, v, o in jobs}


def slots():
    print("GND-NOIS's unnamed slots: the level (dB) and the largest octave band's move (dB) against the base;"
          " the base rendered twice shows the spread")
    jobs = []
    for red, ston in ((0, 0), (64, 0), (127, 0), (64, 127)):
        for n in (36, 60, 84) if not ston else (60,):
            base = [0, red, ston, 0, 0, 0, 0, 64]
            jobs += [(f"slot-r{red}-o{ston}-n{n}-base", 1.6, base, f"note={n}"), (f"slot-r{red}-o{ston}-n{n}-again", 1.6, base, f"note={n} ")]
            for slot in (3, 4, 5, 6):
                for v in (32, 127):
                    p = list(base); p[slot] = v
                    jobs.append((f"slot-r{red}-o{ston}-n{n}-s{slot}v{v}", 1.6, p, f"note={n}"))
    r = mm_only(jobs)

    def moved(a, b):
        ea, eb = bands(a), bands(b)
        keep = ea > ea.sum() * 1e-3
        return db(rms(a, 0.3, 1.4)) - db(rms(b, 0.3, 1.4)), np.abs(10 * np.log10(ea[keep] / eb[keep])).max()
    for t, *_ in jobs:
        if t.endswith("-base"):
            continue
        base = t.rsplit("-", 1)[0] + "-base"
        level, band = moved(r[t], r[base])
        print(f"  {t:<26} level {level:+5.2f}  band {band:4.1f}")


def top():
    """The white part's top at each C: a pair of 1-poles whose corner gives the original's centroid."""
    print("GND-NOIS's white top: the original's centroid at RED 0 and the pair of 1-poles' corner that gives it")
    notes = (24, 36, 48, 60, 72, 84, 96, 108)
    r = mm_only([(f"k0-n{n}", 1.6, [0, 0, 0, 0, 0, 0, 0, 64], f"note={n}") for n in notes])
    f = np.linspace(15, SR / 2, 20000)

    def centroid(fc):   # white noise through the pair; our measure reads 0.990 of it (5676 Hz at the old 8 kHz)
        a = 1 - np.exp(-2 * np.pi * fc / SR)
        p = np.abs(a / (1 - (1 - a) * np.exp(-2j * np.pi * f / SR))) ** 4
        return 0.990 * (p * f).sum() / p.sum(), p.mean()
    corners = []
    for n in notes:
        seg = r[f"k0-n{n}"][int(0.3 * SR):int(1.4 * SR)]
        s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2; fr = np.fft.rfftfreq(len(seg), 1 / SR)
        want = (s[fr >= 15] * fr[fr >= 15]).sum() / s[fr >= 15].sum()
        lo, hi = 1000.0, 40000.0
        for _ in range(60):
            mid = (lo * hi) ** 0.5
            lo, hi = (mid, hi) if centroid(mid)[0] < want else (lo, mid)
        corners.append(mid)
        print(f"  C{n // 12 - 1}  centroid {want:5.0f} Hz  corner {mid:5.0f} Hz  gain holding the level {np.sqrt(centroid(8000)[1] / centroid(mid)[1]):.3f}")
    # The table runs C0 .. C8: C0's corner continues C1 and C2's ratio.
    print("  C0 corner (extrapolated) %.0f Hz" % (corners[0] ** 2 / corners[1]))


if __name__ == "__main__":
    for w in sys.argv[1:] or ["sin", "nois", "key"]:
        {"sin": sin, "nois": nois, "key": key, "slots": slots, "top": top}[w]()
