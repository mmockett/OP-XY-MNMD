#!/usr/bin/env python3
"""Fits the E12 machines' controls: sweeps each from the raw setting the
capture uses (tools/capture_e12.py) on the Machinedrum and on our voice
(voices/e12.hpp), and prints what it does on both.

    ../open-xy/.venv/bin/python reference/fit_e12.py [ptch bend dec hp hpq strt rtrg own] [--md-only]

Needs the capture (our side is silent without it).
"""
import sys
import pathlib

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "reference"))
sys.path.insert(0, str(ROOT / "tools"))
import catalog  # noqa: E402
from probe import render_many, render_machine  # noqa: E402
from capture_e12 import raw, difference, NAMES  # noqa: E402

RATE = 44100


def onset(x, th=1e-3):
    return int(np.argmax(np.abs(x) > th * np.abs(x).max()))


def env_db(x, w=441):
    n = len(x) // w
    return 20 * np.log10(np.sqrt((x[:n * w].reshape(n, w) ** 2).mean(1)) + 1e-12)


def rate_of(ref, x, n=2205, skip=0, lo=-4.0, hi=4.0, steps=161):
    """Octaves x plays faster than ref, from their starts: x(t) ~ ref(2^o t)."""
    oa, ox = onset(ref), onset(x)
    t = np.arange(n) + skip
    X = x[ox + skip:ox + skip + n]
    best = (-2, 0, 0)
    for _ in range(3):
        for o in np.linspace(lo, hi, steps):
            for sh in np.arange(-2, 2.01, 0.5):
                R = np.interp(oa + t * 2 ** o + sh, np.arange(len(ref)), ref)
                c = np.dot(R, X) / np.sqrt(np.dot(R, R) * np.dot(X, X) + 1e-20)
                if c > best[0]:
                    best = (c, o, sh)
        w = (hi - lo) / steps * 2
        lo, hi, steps = best[1] - w, best[1] + w, 41
    return best[1], best[0]


def pitch_track(x, at, n=2048):
    """The fundamental (Hz) of x in windows starting at each of `at` (samples after its start), by autocorrelation."""
    o, out = onset(x), []
    for t in at:
        w = x[o + t:o + t + n] * np.hanning(n)
        c = np.fft.irfft(np.abs(np.fft.rfft(w, 2 * n)) ** 2)[:n]
        lo = 20
        k = lo + int(np.argmax(c[lo:n // 2]))
        # parabolic peak
        y0, y1, y2 = c[k - 1], c[k], c[k + 1]
        d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) if (y0 - 2 * y1 + y2) else 0
        out.append(RATE / (k + d))
    return out


def offset_in(ref, x, n=1024, most=60000):
    """Where in ref x starts (samples), by the best match of x's opening."""
    oa, ox = onset(ref), onset(x, 1e-4)
    X = x[ox:ox + n]
    R = ref[oa:oa + most + n]
    c = np.correlate(R, X, "valid")
    e = np.sqrt(np.convolve(R * R, np.ones(n), "valid"))
    k = int(np.argmax(c / (e + 1e-12)))
    return k, float(c[k] / (e[k] * np.linalg.norm(X) + 1e-12))


def hits(ref, x, n=256, th=0.6):
    """The starts (ms) and levels of each repeat of ref's opening in x."""
    oa, ox = onset(ref), onset(x, 1e-4)
    R = ref[oa:oa + n]
    X = x[ox:]
    c = np.correlate(X, R, "valid")
    e = np.sqrt(np.convolve(X * X, np.ones(n), "valid")) * np.linalg.norm(R)
    r = c / (e + 1e-12)
    out, k = [], 0
    while k < len(r):
        if r[k] > th and np.abs(X[k:k + n]).max() > 1e-4:
            j = k + int(np.argmax(r[k:k + 64]))
            out.append((round(j / RATE * 1000, 1), round(float(np.dot(X[j:j + n], R) / np.dot(R, R)), 3)))
            k = j + n
        else:
            k += 1
    return out


def last_sound(x, th=1e-4):
    o = onset(x)
    return (np.nonzero(np.abs(x[o:]) > th)[0][-1]) / RATE * 1000


def spectrum_ratio(ref, x, n=8192):
    oa, ox = onset(ref), onset(x)
    w = np.hanning(n)
    A = np.abs(np.fft.rfft(ref[oa:oa + n] * w)) ** 2
    B = np.abs(np.fft.rfft(x[ox:ox + n] * w)) ** 2
    f = np.fft.rfftfreq(n, 1 / RATE)
    bands = [(20 * 2 ** (k / 2), 20 * 2 ** ((k + 1) / 2)) for k in range(20)]
    out = []
    for a, b in bands:
        m = (f >= a) & (f < b)
        out.append(10 * np.log10(B[m].sum() / (A[m].sum() + 1e-20) + 1e-20))
    return [round(a) for a, _ in bands], out


def sweep(name, slot, values, seconds=2.0, ours=True, base=None):
    p0 = base or raw(name)
    jobs = []
    for v in values:
        p = list(p0)
        p[slot] = v
        jobs.append((f"e12f-{name.lower()}-{catalog.MACHINES[name][2][slot].lower()}-{v:03d}" +
                     ("" if base is None else "-" + "-".join(map(str, p0))), catalog.MACHINES[name][1], seconds, p))
    md = render_many(jobs)
    xy = render_machine(name, jobs) if ours else {}
    return [(v, md[j[0]], xy.get(j[0])) for v, j in zip(values, jobs)]


def main():
    want = [a for a in sys.argv[1:] if not a.startswith("--")] or ["all", "ptch", "bend", "dec", "hp", "hpq", "strt", "rtrg", "stop", "snap"]
    ours = "--md-only" not in sys.argv
    if "ptch" in want:
        print("PTCH on E12-CB: octaves from PTCH 64 (MD / ours)")
        rows = sweep("E12-CB", 0, [0, 8, 16, 24, 32, 40, 48, 56, 63, 64, 65, 72, 80, 88, 96, 104, 112, 120, 127], ours=ours)
        ref = rows[9][1]
        refx = rows[9][2]
        for v, md, xy in rows:
            a = rate_of(ref, md)
            b = rate_of(refx, xy) if xy is not None else (np.nan, 0)
            print(f"  {v:3d}  {a[0]:+.3f} ({a[1]:.3f})  {b[0]:+.3f}")
    if "dec" in want:
        print("DEC on E12-OH: level against DEC 127, dB at 10, 50, 100, 200, 400, 700 ms (MD / ours)")
        rows = sweep("E12-OH", 1, [0, 8, 16, 32, 48, 64, 80, 96, 112, 120, 126, 127], ours=ours)
        rm, rx = env_db(rows[-1][1][onset(rows[-1][1]):]), env_db(rows[-1][2][onset(rows[-1][2]):]) if ours else None
        for v, md, xy in rows:
            e = env_db(md[onset(md):]) - rm[:len(env_db(md[onset(md):]))]
            s = " ".join(f"{e[k]:6.1f}" for k in (1, 5, 10, 20, 40, 70))
            if xy is not None:
                f = env_db(xy[onset(xy):]) - rx[:len(env_db(xy[onset(xy):]))]
                s += "  |  " + " ".join(f"{f[k]:6.1f}" for k in (1, 5, 10, 20, 40, 70))
            print(f"  {v:3d}  {s}")
    if "strt" in want:
        print("STRT on E12-CC: where the sample starts, samples (and share of 64 s of it)")
        rows = sweep("E12-CC", 4, [0, 1, 8, 16, 32, 48, 64, 80, 96, 112, 127], ours=ours)
        for v, md, xy in rows:
            k, c = offset_in(rows[0][1], md)
            s2 = f"   ours {offset_in(rows[0][2], xy)[0]}" if xy is not None else ""
            print(f"  {v:3d}  {k:6d} ({c:.3f}){s2}")
    if "rtrg" in want:
        print("RTRG on E12-BD at RTIM 64: each hit's start (ms) and level")
        for v, md, xy in sweep("E12-BD", 5, [0, 1, 2, 4, 8, 16, 32, 64, 96, 127], ours=ours):
            print(f"  {v:3d}  {hits(md, md)[:12]}" + (f"\n   us  {hits(xy, xy)[:12]}" if xy is not None else ""))
        print("RTIM on E12-BD at RTRG 8")
        base = raw("E12-BD")
        base[5] = 8
        for v, md, xy in sweep("E12-BD", 6, [0, 8, 16, 32, 48, 64, 80, 96, 112, 127], ours=ours, base=base):
            print(f"  {v:3d}  {hits(md, md)[:10]}" + (f"\n   us  {hits(xy, xy)[:10]}" if xy is not None else ""))
    if "bend" in want:
        print("BEND on E12-CB: Hz at 0, 20, 50, 100, 200, 300 ms")
        at = [int(t * RATE / 1000) for t in (0, 20, 50, 100, 200, 300)]
        for v, md, xy in sweep("E12-CB", 7, [0, 16, 32, 48, 56, 63, 64, 65, 72, 80, 96, 112, 127], ours=ours):
            s2 = ("   us " + " ".join(f"{f:7.1f}" for f in pitch_track(xy, at))) if xy is not None else ""
            print(f"  {v:3d}  " + " ".join(f"{f:7.1f}" for f in pitch_track(md, at)) + s2)
    if "stop" in want:
        print("STOP on E12-OH: the last sound, ms")
        for v, md, xy in sweep("E12-OH", 3, [0, 1, 4, 8, 16, 32, 48, 64, 80, 96, 112, 120, 126, 127], ours=ours):
            print(f"  {v:3d}  {last_sound(md):7.1f}" + (f"  {last_sound(xy):7.1f}" if xy is not None else ""))
    if "snap" in want:
        print("SNAP x SPLN on E12-BD: gain over the raw sample, by 5 ms")
        rows = sweep("E12-BD", 2, [0], ours=ours)
        r0 = rows[0][1]
        for spln in (16, 64, 127):
            base = raw("E12-BD")
            base[3] = spln
            for v, md, xy in sweep("E12-BD", 2, [16, 64, 127], ours=ours, base=base):
                a, b = r0[onset(r0):], md[onset(md):]
                n = min(len(a), len(b), 441 * 20)
                g = [np.dot(a[k:k + 220], b[k:k + 220]) / (np.dot(a[k:k + 220], a[k:k + 220]) + 1e-12) for k in range(0, n, 220)]
                print(f"  spln {spln:3d} snap {v:3d}  " + " ".join(f"{x:4.2f}" for x in g))
    if "all" in want:
        print("Every machine, ours against the Machinedrum: how unlike (0 the same, 1 unrelated) and our level / theirs x 1.2")
        settings = {"raw": {}, "own 64": {3: 64}, "own 127": {3: 127}, "HP 64": {2: 64}, "DEC 64": {1: 64},
                    "PTCH 40": {0: 40}, "STRT 32": {4: 32}, "BEND 90": {7: 90}}
        print("        " + "".join(f"{k:>15s}" for k in settings))
        for name in NAMES:
            jobs = []
            for k, over in settings.items():
                p = raw(name)
                for slot, v in over.items():
                    p[slot] = v
                jobs.append((f"e12a-{name.lower()}-" + "-".join(map(str, p)), catalog.MACHINES[name][1], 2.0, p))
            md, xy = render_many(jobs), render_machine(name, jobs)
            cells = []
            for (j, *_rest) in jobs:
                d, g = difference(md[j], xy[j])
                cells.append(f"{d:6.3f} x{g / 1.2:5.2f}")
            print(f"  {name}  " + "".join(f"{c:>15s}" for c in cells))
    if "hp" in want:
        print("HP on E12-SD: dB against HP 0 by band")
        rows = sweep("E12-SD", 2, [0, 16, 32, 48, 64, 80, 96, 112, 127], ours=ours)
        bands, _ = spectrum_ratio(rows[0][1], rows[0][1])
        print("       " + " ".join(f"{b:6d}" for b in bands))
        for v, md, xy in rows:
            print(f"  {v:3d}  " + " ".join(f"{d:6.1f}" for d in spectrum_ratio(rows[0][1], md)[1]))
            if xy is not None:
                print(f"   us  " + " ".join(f"{d:6.1f}" for d in spectrum_ratio(rows[0][2], xy)[1]))
    if "hpq" in want:
        print("HPQ on E12-SD at HP 64: dB against HP 64 HPQ 0 by band")
        base = raw("E12-SD")
        base[2] = 64
        rows = sweep("E12-SD", 3, [0, 32, 64, 96, 127], ours=ours, base=base)
        bands, _ = spectrum_ratio(rows[0][1], rows[0][1])
        print("       " + " ".join(f"{b:6d}" for b in bands))
        for v, md, xy in rows:
            print(f"  {v:3d}  " + " ".join(f"{d:6.1f}" for d in spectrum_ratio(rows[0][1], md)[1]))
            if xy is not None:
                print(f"   us  " + " ".join(f"{d:6.1f}" for d in spectrum_ratio(rows[0][2], xy)[1]))


if __name__ == "__main__":
    main()
