#!/usr/bin/env python3
"""Fits our TRX-XT and TRX-XC (voices/trx_tom.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_tom.py [--md-only]

Both machines share a page (PTCH DEC RAMP RDEC / DAMP DIST DTYP) and, as the
renders show, a model: one sine at a base set by PTCH, plus RAMP's extra Hz
falling exponentially at RDEC's rate, an exponential decay (DEC), DAMP
softening the strike, and DIST folding the sine into odd harmonics, DTYP
choosing how. Each control is swept from a clean setting (RAMP 0, DEC 100),
the controls of the sweep at their defaults, and the measures printed for the
Machinedrum and for ours.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from trx import live, render_machine, onset, hz_track, env_ms, t60, peaks, level_at, rms_db  # noqa: E402

V = [0, 16, 32, 48, 64, 80, 96, 112, 127]
FIVE = [0, 32, 64, 96, 127]
IDS = {"xt": 18, "xc": 27}
NAMES = {"xt": "TRX-XT", "xc": "TRX-XC"}


def jobs(m):
    i = IDS[m]
    j = []
    for v in V:
        j.append((f"{m}-ptch-{v}", i, 0.8, [v, 100, 0, 64, 0, 0, 0, 0]))
        j.append((f"{m}-dec-{v}", i, 2.5, [64, v, 0, 64, 0, 0, 0, 0]))
        j.append((f"{m}-ramp-{v}", i, 0.8, [64, 100, v, 64, 0, 0, 0, 0]))
        j.append((f"{m}-rdec-{v}", i, 1.2, [64, 100, 127, v, 0, 0, 0, 0]))
    for v in (0, 127):
        j.append((f"{m}-ramp127-ptch-{v}", i, 0.8, [v, 100, 127, 64, 0, 0, 0, 0]))
    for v in FIVE:
        j.append((f"{m}-damp-{v}", i, 0.8, [64, 100, 0, 64, v, 0, 0, 0]))
        j.append((f"{m}-dist-{v}", i, 0.8, [64, 100, 0, 64, 0, v, 0, 0]))
        j.append((f"{m}-dtyp-{v}", i, 0.8, [64, 100, 0, 64, 0, 127, v, 0]))
        j.append((f"{m}-dtyp64-{v}", i, 0.8, [64, 100, 0, 64, 0, 64, v, 0]))
    return j


def base_pitch(a):
    t, f = hz_track(a)
    m = (t > 0.15) & (t < 0.4)
    return float(np.median(f[m])) if m.any() else float("nan")


def extra(a, ref):
    """RAMP's extra Hz over the hit without it: A * exp(-t / tau), fitted from 5 ms on."""
    t0, f0 = hz_track(ref)
    t, f = hz_track(a)
    e = f - np.interp(t, t0, f0)
    m = (t > 0.005) & (t < 0.6) & (e > 1.5)
    if m.sum() < 6:
        return 0.0, 0.0
    slope, icept = np.polyfit(t[m], np.log(e[m]), 1)
    return float(np.exp(icept)), float(-1 / slope) if slope < 0 else float("inf")


def harmonics(a, f0):
    """Harmonics 2..7 in dB under the fundamental, 20 to 120 ms in."""
    lv = [level_at(a, f0 * k, 0.02, 0.12) for k in range(1, 8)]
    return " ".join(f"{l - lv[0]:4.0f}" for l in lv[1:])


def shape(a):
    """Level in dB under the peak at 0, 1, 2, 3, 5, 10, 20, 50 ms."""
    e = env_ms(a)
    return " ".join(f"{e[t] - e.max():5.1f}" for t in (0, 1, 2, 3, 5, 10, 20, 50))


def strike(a):
    """The click before the body: its length in samples (to the first upward zero
    crossing), its depth and the body's level 10 to 60 ms in, both in dB."""
    s = np.signbit(a[:400])
    up = np.nonzero(s[:-1] & ~s[1:])[0]
    length = int(up[0]) + 1 if len(up) else -1
    body = rms_db(a, 0.01, 0.06) + 3.01   # a sine's peak over its rms
    return length, 20 * np.log10(-a[:max(length, 1)].min() + 1e-12) - body, body


def analyse(x, m, label):
    get = lambda n: onset(x[n])
    print(f"######## {label}")
    print("== base pitch Hz (150-400 ms), PTCH sweep, RAMP 0")
    base = {v: base_pitch(get(f"{m}-ptch-{v}")) for v in V}
    print("  " + " ".join(f"{base[v]:6.1f}" for v in V))
    ok = [v for v in V if np.isfinite(base[v])]
    slope, icept = np.polyfit(np.array(ok) / 127, np.log2([base[v] for v in ok]), 1)
    print(f"  fit: {2 ** icept:.2f} Hz * 2^({slope:.3f} PTCH/127)")
    print("  click length, samples: " + " ".join(f"{strike(get(f'{m}-ptch-{v}'))[0]:6d}" for v in V))
    print("  early (2-6 ms) Hz: " + " ".join(f"{np.median(hz_track(get(f'{m}-ptch-{v}'))[1][:4]):6.1f}" for v in V))
    print("== DEC: -60 dB ms, peak dB, shape at 0 1 2 3 5 10 20 50 ms")
    for v in V:
        a = get(f"{m}-dec-{v}")
        print(f"  DEC {v:3d}: t60 {t60(a) * 1000:7.0f}  peak {20 * np.log10(np.abs(a).max()):6.1f}  {shape(a)}")
    print("== RAMP: extra Hz A, tau ms (RDEC 64); early pitch at 1, 3, 10 ms")
    ref = get(f"{m}-ramp-0")
    for v in V[1:]:
        a = get(f"{m}-ramp-{v}")
        A, tau = extra(a, ref)
        t, f = hz_track(a)
        print(f"  RAMP {v:3d}: A {A:7.1f} Hz, tau {tau * 1000:7.1f} ms | " + " ".join(f"{np.interp(s, t, f):6.1f}" for s in (0.001, 0.003, 0.01)))
    print("== RDEC: A, tau ms (RAMP 127)")
    for v in V:
        A, tau = extra(get(f"{m}-rdec-{v}"), ref)
        print(f"  RDEC {v:3d}: A {A:7.1f} Hz, tau {tau * 1000:8.1f} ms")
    print("== RAMP 127 at PTCH 0 and 127: A, tau (is RAMP in Hz or in octaves?)")
    for v in (0, 127):
        A, tau = extra(get(f"{m}-ramp127-ptch-{v}"), get(f"{m}-ptch-{v}"))
        print(f"  PTCH {v:3d}: base {base[v]:6.1f}  A {A:7.1f} Hz, tau {tau * 1000:6.1f} ms")
    print("== DAMP: peak dB, t60, click length and depth under the body, the body's peak dB")
    for v in FIVE:
        a = get(f"{m}-damp-{v}")
        length, depth, body = strike(a)
        print(f"  DAMP {v:3d}: peak {20 * np.log10(np.abs(a).max()):6.1f}  t60 {t60(a) * 1000:5.0f}  click {length:4d} samples {depth:6.1f} dB  body {body:6.1f}")
    for kind, title in (("dist", "DIST (DTYP 0)"), ("dtyp", "DTYP (DIST 127)"), ("dtyp64", "DTYP (DIST 64)")):
        print(f"== {title}: peak dB, rms 0-100 ms, harmonics 2..7 under the fundamental")
        for v in FIVE:
            a = get(f"{m}-{kind}-{v}")
            print(f"  {v:3d}: peak {20 * np.log10(np.abs(a).max()):6.1f}  rms {rms_db(a, 0, 0.1):6.1f}  {harmonics(a, base[64])}")


def main():
    for m in ("xt", "xc"):
        analyse(live(jobs(m)), m, f"Machinedrum {NAMES[m]}")
        if "--md-only" not in sys.argv:
            analyse(render_machine(NAMES[m], jobs(m)), m, f"{NAMES[m]} (ours)")


if __name__ == "__main__":
    main()
