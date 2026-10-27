#!/usr/bin/env python3
"""Fits our TRX-RS (voices/trx_rs.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_rs.py [--md-only]

The renders show a rim shot in two parts: a tone that PTCH moves linearly in
Hz and DEC lets ring, and the rim, fixed partials near 0.48, 0.94, 1.38, 1.48,
1.89 and 2.83 kHz that PTCH and DEC leave alone and that are gone in tens of
milliseconds. DIST clips the tone into odd harmonics. Each control is swept
from PTCH 64, DEC 64, DIST 0.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from trx import live, render_machine, onset, t60, peaks, level_at, rms_db, env_ms  # noqa: E402

V = [0, 16, 32, 48, 64, 80, 96, 112, 127]
FIVE = [0, 32, 64, 96, 127]
RS = 20
RIM = [483, 938, 1375, 1484, 1886, 2834, 3410]
WINDOWS = [(0, 0.003), (0.003, 0.008), (0.008, 0.015), (0.015, 0.03), (0.03, 0.06)]


def jobs():
    j = []
    for v in V:
        j.append((f"rs-ptch-{v}", RS, 0.8, [v, 64, 0, 0, 0, 0, 0, 0]))
        j.append((f"rs-dec-{v}", RS, 2.5, [64, v, 0, 0, 0, 0, 0, 0]))
    for v in FIVE:
        j.append((f"rs-dist-{v}", RS, 0.8, [64, 64, v, 0, 0, 0, 0, 0]))
        j.append((f"rs-dist0-{v}", RS, 0.8, [0, 64, v, 0, 0, 0, 0, 0]))
    return j


def tone(a):
    """The tone's frequency, 60 to 200 ms in."""
    seg = a[int(0.06 * 44100):int(0.2 * 44100)]
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 17))
    f = np.fft.rfftfreq(1 << 17, 1 / 44100)
    m = (f > 100) & (f < 450)
    return float(f[m][np.argmax(s[m])])


def tone_t60(a, f0):
    """The tone's -60 dB time from its level in 20 ms windows from 30 ms on."""
    ts = np.arange(0.03, 0.6, 0.02)
    lv = np.array([level_at(a, f0, t, t + 0.02, 0.05) for t in ts])
    m = lv > lv[0] - 40
    slope = np.polyfit(ts[m], lv[m], 1)[0]
    return 60 / -slope if slope < 0 else float("inf")


def above(a, cutoff):
    spec = np.fft.rfft(a)
    spec[np.fft.rfftfreq(len(a), 1 / 44100) < cutoff] = 0
    return np.fft.irfft(spec, len(a))


def rim(x, names):
    """The rim, from every hit: its level above 400 Hz per ms (PTCH 0, where the
    tone is under it), and its spectrum over the first 12 ms, power-averaged."""
    e = np.mean([env_ms(above(x[n][:4410], 400)) for n in names if n.endswith("-0") and "ptch" in n or "dist0" in n], axis=0)
    power = 0
    for n in names:
        a = x[n][:530]
        power = power + np.abs(np.fft.rfft(a * np.hanning(len(a)), 1 << 16)) ** 2
    f = np.fft.rfftfreq(1 << 16, 1 / 44100)
    db = 10 * np.log10(power / len(names) + 1e-30)
    at = lambda hz: db[(f > hz * 0.96) & (f < hz * 1.04)].max()
    top = at(477)
    lines = [477 * k for k in range(1, 10)] + [2248, 2459]
    return e, top, [(hz, round(at(hz) - top)) for hz in lines]


def analyse(x, label):
    get = lambda n: onset(x[n])
    print(f"######## {label}")
    names = [j[0] for j in jobs()]
    e, top, lines = rim({n: get(n) for n in names}, names)
    print("== the rim: dB per ms above 400 Hz, 0..24 ms (PTCH 0)")
    print("  " + " ".join(f"{v:5.1f}" for v in e[:25]))
    print(f"== the rim's spectrum, 0-12 ms, power-averaged over every hit: 477 Hz at {top - 10 * np.log10(530 * 0.375 * 530) :5.1f} dB, then dB under it")
    print("  " + " ".join(f"{hz}:{d}" for hz, d in lines))
    print("== tone Hz, PTCH sweep (DEC 64)")
    hz = {v: tone(get(f"rs-ptch-{v}")) for v in V}
    print("  " + " ".join(f"{hz[v]:6.1f}" for v in V))
    a, b = np.polyfit(np.array(V) / 127, [hz[v] for v in V], 1)
    print(f"  fit: {b:.1f} Hz + {a:.1f} Hz * PTCH/127")
    print("== level (dB) of the tone and each rim partial, by window " + " ".join(f"{w[0] * 1000:.0f}-{w[1] * 1000:.0f}" for w in WINDOWS))
    for v in (0, 64, 127):
        a = get(f"rs-ptch-{v}")
        print(f"  PTCH {v:3d}: tone " + " ".join(f"{level_at(a, hz[v], *w, 0.05):6.1f}" for w in WINDOWS))
        for p in RIM:
            print(f"         {p:5d} " + " ".join(f"{level_at(a, p, *w, 0.02):6.1f}" for w in WINDOWS))
    print("  peaks 0-3 ms, PTCH 64:", peaks(get("rs-ptch-64"), 0, 0.003, 12))
    print("  peaks 0-10 ms, PTCH 64:", peaks(get("rs-ptch-64"), 0, 0.01, 12))
    print("== DEC: the tone's -60 dB ms, the whole hit's, peak dB, level at 0 1 2 5 10 20 50 ms")
    for v in V:
        a = get(f"rs-dec-{v}")
        e = env_ms(a)
        print(f"  DEC {v:3d}: tone {tone_t60(a, hz[64]) * 1000:7.0f}  all {t60(a) * 1000:7.0f}  peak {20 * np.log10(np.abs(a).max()):6.1f}  "
              + " ".join(f"{e[t] - e.max():5.1f}" for t in (0, 1, 2, 5, 10, 20, 50)))
    for kind, at in (("dist", 64), ("dist0", 0)):
        print(f"== DIST at PTCH {at}: peak dB, rms 0-100 ms, the tone's harmonics 2..7 (40-140 ms)")
        f0 = hz[at]
        for v in FIVE:
            a = get(f"rs-{kind}-{v}")
            lv = [level_at(a, f0 * k, 0.04, 0.14, 0.02) for k in range(1, 8)]
            print(f"  DIST {v:3d}: peak {20 * np.log10(np.abs(a).max()):6.1f}  rms {rms_db(a, 0, 0.1):6.1f}  "
                  + " ".join(f"{l - lv[0]:4.0f}" for l in lv[1:]) + f"  (fundamental {lv[0]:5.1f})")


def main():
    analyse(live(jobs()), "Machinedrum TRX-RS")
    if "--md-only" not in sys.argv:
        analyse(render_machine("TRX-RS", jobs()), "TRX-RS (ours)")


if __name__ == "__main__":
    main()
