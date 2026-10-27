#!/usr/bin/env python3
"""Fits SWAVE-SAW, SWAVE-PULSE and SWAVE-ENS (voices/swave.hpp) to the Monomachine.

    ../open-xy/.venv/bin/python reference/fit_swave.py [saw|pulse|ens|pitch ...]

Every note is held (the AMP page's HOLD at 127) and measured while it sounds,
one control moving from a clean setting, the Monomachine and ours side by side
as "MNM / ours". Levels are compared after each engine's gain (GAIN below): our
output is the Monomachine's times that. The Monomachine keeps a free-running
state between jobs (PWAD's sweep of the pulse width), so every pulse job resets
it with PWRS and PWAD's own jobs run last.

What the Monomachine was found to do (reference/README.md has the summary):
- SAW: a main saw, a pair at +-UNIW (0 .. 53.7 cents, linear) at UNIL's level
  and an inner pair at +-UNIW/2 at UNIX x UNIL, all starting in phase. SUBX is
  a saw an octave down (2.65 x the main's fundamental at 127), SUB1 and SUB2
  sines one and two octaves down (4.55 x and 1.89 x), all linear but SUBX. The
  saws are thin at the fundamental and dull above 2.5 kHz.
- PULSE: the duty runs 1% .. 99% over PW; PWAD sweeps it on, wrapping, at
  0.0422 cycles a second per step; PWRS restarts the sweep at the note. The pair
  is detuned half as far as SAW's.
- ENS: PCH2..4 pick 33 sorted intervals, -12 .. +12 semitones with just 6:5,
  5:4, 4:3 and 3:2 both ways; 61..63 is off. WAVE crossfades saw into pulse; PW
  shapes the pulse, square at 32 and 96 and an octave-down square at 64.
  CHRL fades into a chorus of drifting copies.
"""
import sys

import numpy as np

from probe import render_many, render_machine

SR = 44100
HOLD = "cc56=0 cc57=127 cc58=127 cc59=10"   # AMP: no attack, held while the note is, a short release
ID = {"SWAVE-SAW": 4, "SWAVE-PULSE": 5, "SWAVE-ENS": 14}
GAIN = {"SWAVE-SAW": 2.2, "SWAVE-PULSE": 1.6, "SWAVE-ENS": 2.4}
OFF = 62   # an ENS voice switched off


def hz(note):
    return 440 * 2 ** ((note - 69) / 12)


def both(name, jobs):
    """jobs: [(tag, seconds, [8 values], extra mdref options)] -> {tag: (mnm, ours)}"""
    pre = name.lower().replace("swave-", "sw")
    mm = render_many([(f"{pre}-{t}", ID[name], s, v, f"gate={s - 0.1:.1f} {HOLD} {o}".strip()) for t, s, v, o in jobs], "mm")
    xy = render_machine(name, [(f"{pre}-{t}", 0, s, v, o) for t, s, v, o in jobs])
    return {t: (mm[f"{pre}-{t}"] * GAIN[name], xy[f"{pre}-{t}"]) for t, s, v, o in jobs}


def rms(x, a=0.3, b=1.2):
    s = x[int(a * SR):int(b * SR)]
    return float(np.sqrt((s ** 2).mean()))


def fundamental(x, guess, a=0.3, b=None):
    b = b or len(x) / SR - 0.15
    seg = x[int(a * SR):int(b * SR)]
    n = len(seg); p = 2 ** int(np.ceil(np.log2(n)) + 2)
    s = np.abs(np.fft.rfft(seg * np.hanning(n), p)); fr = np.fft.rfftfreq(p, 1 / SR)
    m = (fr > guess * 0.97) & (fr < guess * 1.03)
    i = int(np.argmax(s * m)); y0, y1, y2 = np.log(s[i - 1:i + 2] + 1e-15)
    return float(fr[i] + 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) * (fr[1] - fr[0]))


def harmonics(x, f, a=0.3, b=1.2, k=12):
    """Least-squares amplitudes of the first k harmonics of f over a .. b seconds."""
    t = np.arange(int(a * SR), int(b * SR)); seg = x[t]
    k = min(k, int(20000 / f))
    cols = []
    for h in range(1, k + 1):
        cols += [np.cos(2 * np.pi * h * f * t / SR), np.sin(2 * np.pi * h * f * t / SR)]
    c, *_ = np.linalg.lstsq(np.array(cols).T, seg, rcond=None)
    return np.abs(c[0::2] - 1j * c[1::2])


def lines(x, lo, hi, a=0.3, floor=0.05, sep=3, n=8):
    """Spectral lines between lo and hi Hz: (Hz, amplitude)."""
    seg = x[int(a * SR):int((len(x) / SR - 0.15) * SR)]; n_ = len(seg)
    s = np.abs(np.fft.rfft(seg * np.hanning(n_))); fr = np.fft.rfftfreq(n_, 1 / SR)
    m = (fr > lo) & (fr < hi); ss = s * m; out = []
    for i in np.argsort(ss)[::-1]:
        if ss[i] < ss.max() * floor or len(out) >= n:
            break
        if all(abs(i - j) > sep for j, _ in out) and s[i] >= s[i - 1] and s[i] >= s[i + 1]:
            out.append((i, s[i]))
    out.sort()
    return [(round(float(fr[i]), 2), float(v / n_ * 4)) for i, v in out]


def db(v):
    return 20 * np.log10(max(v, 1e-9))


def cents(f, ref):
    return 1200 * np.log2(f / ref)


def row(label, mm, xy, fmt="%.4f"):
    print(f"  {label:<22}" + (fmt % mm) + " / " + (fmt % xy))


def profile(label, pair, f):
    """Harmonics 1..8 in dB against the 2nd, MNM then ours."""
    for side, x in zip(("MNM ", "ours"), pair):
        h = harmonics(x, fundamental(x, f))[:8]
        print(f"  {label:<14}{side} " + " ".join("%6.1f" % (db(v) - db(h[1])) for v in h))


def saw():
    print("SWAVE-SAW")
    jobs = [("plain", 1.5, [0, 0, 0, 0, 0, 0, 0, 64], "")]
    for v in (32, 64, 96, 127):
        jobs += [(f"unil{v}", 1.5, [v, 0, 0, 0, 0, 0, 0, 64], ""), (f"unix{v}", 1.5, [127, 0, v, 0, 0, 0, 0, 64], ""),
                 (f"subx{v}", 1.5, [0, 0, 0, 0, v, 0, 0, 64], ""), (f"sub1{v}", 1.5, [0, 0, 0, 0, 0, v, 0, 64], ""),
                 (f"sub2{v}", 1.5, [0, 0, 0, 0, 0, 0, v, 64], "")]
    for v in (16, 64, 127):
        jobs.append((f"uniw{v}", 4.5, [127, v, 0, 0, 0, 0, 0, 64], ""))
    for n in (36, 48, 72, 84):
        jobs.append((f"n{n}", 1.5, [0, 0, 0, 0, 0, 0, 0, 64], f"note={n}"))
    jobs += [("max", 1.5, [127, 64, 127, 0, 127, 127, 127, 64], ""), ("design", 1.5, [40, 30, 0, 0, 0, 30, 0, 64], "")]
    r = both("SWAVE-SAW", jobs)
    print(" rms, at each engine's level")
    for t in ["plain"] + [f"{c}{v}" for c in ("unil", "unix", "subx", "sub1", "sub2") for v in (32, 64, 96, 127)] + ["max", "design"]:
        row(t, rms(r[t][0]), rms(r[t][1]))
    print(" peak: max", "%.3f / %.3f" % (abs(r["max"][0]).max(), abs(r["max"][1]).max()))
    print(" UNIW: the outer pair's detune in cents (UNIL 127)")
    for v in (16, 64, 127):
        for side, x in zip(("MNM ", "ours"), r[f"uniw{v}"]):
            print(f"  uniw{v:<4}{side} ", [round(cents(f, 261.63), 1) for f, _ in lines(x, 240, 285, sep=2)])
    print(" sub levels, the sub's line against the main saw's fundamental")
    for c, at in (("subx", 130.8), ("sub1", 130.8), ("sub2", 65.4)):
        for side in (0, 1):
            x = r[f"{c}127"][side]; base = r["plain"][side]
            sub = max(a for f, a in lines(x, at * 0.97, at * 1.03)); main = max(a for f, a in lines(base, 255, 268))
            print(f"  {c} 127 {'MNM ' if side == 0 else 'ours'} {sub / main:.2f} x")
    print(" harmonics against the 2nd, dB")
    profile("n60", r["plain"], 261.63)
    for n in (36, 48, 72, 84):
        profile(f"n{n}", r[f"n{n}"], hz(n))


def pulse():
    print("SWAVE-PULSE")
    jobs = []
    for v in (0, 8, 16, 32, 48, 64, 96, 127):
        jobs.append((f"pw{v}", 1.5, [0, 0, 0, 0, v, 0, 127, 64], ""))
    for v in (64, 127):
        jobs += [(f"unil{v}", 2.5, [v, 64, 0, 0, 64, 0, 127, 64], ""), (f"sub1{v}", 1.5, [0, 0, v, 0, 64, 0, 127, 64], ""),
                 (f"sub2{v}", 1.5, [0, 0, 0, v, 64, 0, 127, 64], "")]
    for n in (36, 84):
        jobs.append((f"n{n}", 1.5, [0, 0, 0, 0, 64, 0, 127, 64], f"note={n}"))
    jobs += [("max", 1.5, [127, 64, 127, 127, 64, 0, 127, 64], ""), ("design", 1.5, [30, 20, 30, 0, 64, 20, 127, 64], "")]
    jobs += [(f"pwad{v}", 4.0, [0, 0, 0, 0, 64, v, 127, 64], "") for v in (32, 64, 127)]
    r = both("SWAVE-PULSE", jobs)
    print(" rms, and the duty from the fundamental's level (sin pi d against the square's)")
    sq = [harmonics(x, fundamental(x, 261.63))[0] for x in r["pw64"]]
    for v in (0, 8, 16, 32, 48, 64, 96, 127):
        d = [np.arcsin(min(1, harmonics(x, fundamental(x, 261.63))[0] / s)) / np.pi for x, s in zip(r[f"pw{v}"], sq)]
        print(f"  pw{v:<20}%.4f / %.4f   duty %.3f / %.3f" % (rms(r[f"pw{v}"][0]), rms(r[f"pw{v}"][1]), d[0], d[1]))
    for t in ("unil64", "unil127", "sub164", "sub1127", "sub264", "sub2127", "max", "design"):
        row(t, rms(r[t][0]), rms(r[t][1]))
    print(" peak: max", "%.3f / %.3f" % (abs(r["max"][0]).max(), abs(r["max"][1]).max()))
    for side, x in zip(("MNM ", "ours"), r["unil127"]):
        print(f"  pair, cents {side}", [round(cents(f, 261.63), 1) for f, _ in lines(x, 250, 275, sep=2)])
    print(" harmonics against the fundamental (the 2nd is absent on a square), dB")
    for t, n in (("pw64", 60), ("n36", 36), ("n84", 84), ("pw32", 60)):
        for side, x in zip(("MNM ", "ours"), r[t]):
            h = harmonics(x, fundamental(x, hz(n)))[:8]
            print(f"  {t:<6}{side} " + " ".join("%6.1f" % (db(v) - db(h[0])) for v in h))
    print(" PWAD: seconds between squares (the 2nd harmonic's nulls)")
    for v in (32, 64, 127):
        out = []
        for x in r[f"pwad{v}"]:
            f = fundamental(x, 261.63, 0.3, 0.6); nulls = []
            ratio = []
            for t in np.arange(0.1, 3.8, 0.02):
                h = harmonics(x, f, t, t + 0.02, 3); ratio.append(h[1] / max(h[0], 1e-9))
            nulls = [0.1 + 0.02 * i for i in range(1, len(ratio) - 1) if ratio[i] < ratio[i - 1] and ratio[i] <= ratio[i + 1] and ratio[i] < 0.25]
            out.append(np.median(np.diff(nulls)) if len(nulls) > 1 else float("nan"))
        print(f"  pwad{v:<18}%.3f / %.3f" % tuple(out))


def ens():
    print("SWAVE-ENS")
    jobs = [("one", 1.5, [OFF, OFF, OFF, 0, 64, 0, 0, 64], "")]
    for v in (0, 10, 22, 34, 41, 47, 59, 62, 66, 78, 82, 90, 93, 101, 105, 125):
        jobs.append((f"pch{v}", 1.5, [v, OFF, OFF, 0, 64, 0, 0, 64], ""))
    for v in (32, 64, 96, 127):
        jobs.append((f"wave{v}", 1.5, [OFF, OFF, OFF, v, 64, 0, 0, 64], ""))
    for v in (0, 16, 32, 48, 64):
        jobs.append((f"pw{v}", 1.5, [OFF, OFF, OFF, 127, v, 0, 0, 64], ""))
    for v in (32, 64, 127):
        jobs.append((f"chrl{v}", 3.0, [OFF, OFF, OFF, 0, 64, v, 64, 64], ""))
    jobs += [("four", 1.5, [64, 72, 84, 0, 64, 0, 0, 64], ""), ("design", 1.5, [84, 100, OFF, 0, 64, 40, 40, 64], "")]
    r = both("SWAVE-ENS", jobs)
    print(" PCH2: the second voice's interval, semitones")
    for v in (0, 10, 22, 34, 41, 47, 59, 62, 66, 78, 82, 90, 93, 101, 105, 125):
        found = []
        for x, base in zip(r[f"pch{v}"], r["one"]):
            mine = dict(lines(base, 100, 1200, floor=0.02, n=12))
            new = [(f, a) for f, a in lines(x, 100, 1200, floor=0.02, n=12) if all(abs(f - g) > 2 for g in mine)]
            found.append(round(12 * np.log2(new[0][0] / 261.63), 2) if new else None)
        print(f"  pch{v:<20}{found[0]} / {found[1]}")
    print(" rms")
    for t in ["one"] + [f"wave{v}" for v in (32, 64, 96, 127)] + [f"pw{v}" for v in (0, 16, 32, 48, 64)] + [f"chrl{v}" for v in (32, 64, 127)] + ["four", "design"]:
        row(t, rms(r[t][0]), rms(r[t][1]))
    print(" WAVE 127: the pulse's harmonics of the octave below, dB against the octave-down fundamental")
    for v in (16, 32, 48, 64):
        for side, x in zip(("MNM ", "ours"), r[f"pw{v}"]):
            h = harmonics(x, fundamental(x, 130.8, 0.3, 1.3) if v == 64 else fundamental(x, 261.63) / 2, k=8)
            print(f"  pw{v:<4}{side} " + " ".join("%6.1f" % (db(a) - db(max(h))) for a in h))
    print(" harmonics of the saw against the 2nd, dB")
    profile("one", r["one"], 261.63)


def pitch():
    print("pitch: the played note and TUNE, Hz")
    for name, values in (("SWAVE-SAW", [0, 0, 0, 0, 0, 0, 0]), ("SWAVE-PULSE", [0, 0, 0, 0, 64, 0, 127]),
                         ("SWAVE-ENS", [OFF, OFF, OFF, 0, 64, 0, 0])):
        jobs = [(f"pn{n}", 1.2, values + [64], f"note={n}") for n in (36, 48, 60, 72, 84, 96)]
        jobs += [(f"pt{t}", 1.2, values + [t], "") for t in (0, 32, 96, 127)]
        r = both(name, jobs)
        print(" ", name)
        for n in (36, 48, 60, 72, 84, 96):
            print("   note %d  %.2f / %.2f (%.2f)" % (n, fundamental(r[f"pn{n}"][0], hz(n)), fundamental(r[f"pn{n}"][1], hz(n)), hz(n)))
        for t in (0, 32, 96, 127):
            want = 261.63 * 2 ** ((t - 64) / 32 / 12)
            print("   TUNE %d  %.2f / %.2f (%.2f)" % (t, fundamental(r[f"pt{t}"][0], want), fundamental(r[f"pt{t}"][1], want), want))


if __name__ == "__main__":
    wanted = sys.argv[1:] or ["saw", "pulse", "ens", "pitch"]
    for w in wanted:
        {"saw": saw, "pulse": pulse, "ens": ens, "pitch": pitch}[w]()
