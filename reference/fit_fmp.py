"""Fits the Monomachine's FM+ machines (FM+STAT, FM+PAR, FM+DYN) by sweeping each
control from a neutral setting on the Monomachine and on our modules.

    ../open-xy/.venv/bin/python reference/fit_fmp.py [STAT|PAR|DYN] [--ours]

The Monomachine's amp envelope is held open (CC 56..58: no attack, hold to the
gate, no decay) so only the machine's own envelopes shape the notes.
"""
import sys

import numpy as np

from probe import render_many, render_machine

AMP = "gate=1.0 cc56=0 cc57=127 cc58=127"
SR = 44100
BASE = {
    "FM+STAT": (8, [80, 64, 0, 0, 80, 0, 64, 64]),
    "FM+PAR": (9, [80, 0, 80, 0, 80, 0, 64, 64]),
    "FM+DYN": (10, [64, 0, 0, 0, 64, 0, 0, 64]),
}
LEVELS = [0, 16, 32, 48, 64, 80, 96, 112, 127]


def onset(x, th=1e-4):
    nz = np.nonzero(np.abs(x) > th)[0]
    return nz[0] if len(nz) else 0


def rms(x):
    return float(np.sqrt(np.mean(x ** 2))) if len(x) else 0.0


def db(v):
    return 20 * np.log10(max(v, 1e-9))


def centroid(x):
    X = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    f = np.fft.rfftfreq(len(x), 1 / SR)
    return float((X * f).sum() / max(X.sum(), 1e-12))


def peaks(x, n=8, floor=-50, pad=8):
    """The strongest partials of a segment: (Hz, dB under the strongest)."""
    N = len(x)
    X = np.abs(np.fft.rfft(x * np.blackman(N), N * pad))
    f = np.fft.rfftfreq(N * pad, 1 / SR)
    L = 20 * np.log10(X + 1e-12)
    L -= L.max()
    pk = [i for i in range(1, len(L) - 1) if L[i] > L[i - 1] and L[i] >= L[i + 1] and L[i] > floor]
    pk = sorted(sorted(pk, key=lambda i: -L[i])[:n])
    out = []
    for i in pk:
        a, b, c = L[i - 1], L[i], L[i + 1]
        d = 0.5 * (a - c) / (a - 2 * b + c)
        out.append((round(f[i] + d * (f[1] - f[0]), 1), round(b - 0.25 * (a - c) * d, 1)))
    return out


def describe(x):
    s = onset(x)
    x = x[s:]
    at = lambda ms, w=20: x[int(ms * 44.1):int((ms + w) * 44.1)]
    lv = [round(db(rms(at(t))), 1) for t in (0, 20, 50, 100, 200, 400, 800)]
    br = [round(centroid(at(t, 40))) for t in (0, 20, 50, 100, 200, 400, 800)]
    return lv, br


def jobs_for(name):
    ident, base = BASE[name]
    out = [(f"fmp-{name[3:].lower()}-base", ident, 1.3, base, AMP)]
    for slot in range(8):
        for v in LEVELS:
            p = list(base)
            p[slot] = v
            out.append((f"fmp-{name[3:].lower()}-{slot}-{v:03d}", ident, 1.3, p, AMP))
    return out


def main():
    import catalog
    which = [a for a in sys.argv[1:] if not a.startswith("--")] or ["STAT", "PAR", "DYN"]
    ours = "--ours" in sys.argv
    for short in which:
        name = "FM+" + short
        jobs = jobs_for(name)
        ref = render_many(jobs, "mm")
        mine = render_machine(name, [(j[0], j[1], j[2], j[3]) for j in jobs]) if ours else {}
        labels = catalog.MACHINES[name][2]
        print(f"== {name}  levels at 0 20 50 100 200 400 800 ms (dB) | centroid (Hz)")
        for j in jobs:
            tag = j[0].split("-", 2)[2]
            if tag != "base":
                tag = labels[int(tag.split("-")[0])] + " " + tag.split("-")[1]
            lv, br = describe(ref[j[0]])
            print(f"{tag:10s} {' '.join(f'{v:6.1f}' for v in lv)} | {' '.join(f'{v:5d}' for v in br)}")
            if ours:
                lv, br = describe(mine[j[0]])
                print(f"{'  ours':10s} {' '.join(f'{v:6.1f}' for v in lv)} | {' '.join(f'{v:5d}' for v in br)}")


if __name__ == "__main__":
    sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parent.parent / "tools"))
    main()
