#!/usr/bin/env python3
"""Fits our TRX-BD (voices/trx_bd.hpp) to the Machinedrum's, parameter by parameter.

    ../open-xy/.venv/bin/python reference/fit_bd.py

The model, read off the renders (reference/README.md has how they are made):

    f(t)  = base(PTCH) + drop(t; PTCH) + ramp(RAMP) * exp(-t / tau(RDEC))      Hz
    drop  = depth(PTCH) * max(0, 1 - t / length(PTCH))   the built-in thump, linear in Hz
    level = exp decay to -60 dB in T60(DEC), after the first cycle

Each part is fitted from a sweep with the other parameters at zero, and the
results are printed as tables at the 9 points 0, 16, ..., 127 for engine.cpp.

The "drop" read here crosses the click, which is fixed and is no part of the
sine (it sounds alone until 6.3 ms): it reads the same on both sides, but read
alone it overstates the thump. trx_bd.hpp's thump (about 35 Hz, gone by 11 to 27
ms) and first half cycle came from fitting the waveform itself from 6.3 to 50 ms,
least squares over depth and length at each PTCH; the sine's turning points then
fall within 0.3 ms of the Machinedrum's at PTCH 32 to 127.
"""
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from probe import render_many, render_machine  # noqa: E402
from curves import pitch_track  # noqa: E402

RATE = 44100
V = [0, 16, 32, 48, 64, 80, 96, 112, 127]
BD = 16


def jobs():
    j = []
    for v in V:
        j.append((f"bd-ptch-{v}", BD, 0.6, [v, 64, 0, 0, 0, 0, 0, 0]))
        j.append((f"bd-ramp-{v}", BD, 0.6, [40, 64, v, 64, 0, 0, 0, 0]))
        j.append((f"bd-rdec-{v}", BD, 0.6, [40, 64, 127, v, 0, 0, 0, 0]))
        j.append((f"bd-dec-{v}", BD, 2.5, [40, v, 0, 0, 0, 0, 0, 0]))
    for v in (0, 127):
        j.append((f"bd-ramp127-ptch-{v}", BD, 0.6, [v, 64, 127, 64, 0, 0, 0, 0]))
    for i, n in [(4, "strt"), (5, "nois"), (6, "harm"), (7, "clip")]:
        for v in (0, 32, 64, 96, 127):
            p = [40, 64, 0, 0, 0, 0, 0, 0]
            p[i] = v
            j.append((f"bd-{n}-{v}", BD, 0.6, p))
    return j


def harmonics(a, f0):
    """Harmonics 2..6 in dB under the fundamental, 100-200 ms in."""
    seg = a[int(0.1 * RATE):int(0.2 * RATE)]
    seg = seg * np.hanning(len(seg))
    n = 1 << 17
    s = np.abs(np.fft.rfft(seg, n))
    fr = np.fft.rfftfreq(n, 1 / RATE)
    levels = [20 * np.log10(s[(fr > f0 * k * 0.9) & (fr < f0 * k * 1.1)].max() + 1e-12) for k in range(1, 7)]
    return [round(l - levels[0]) for l in levels[1:]]


def extras(x):
    """The bottom row: peak and click (STRT), high-passed burst energy (NOIS), harmonics (HARM, CLIP)."""
    print("== STRT, NOIS, HARM, CLIP: peak dB, high-passed energy at 0/10/30 ms, harmonics 2..6")
    for n in ("strt", "nois", "harm", "clip"):
        for v in (0, 32, 64, 96, 127):
            a = x[f"bd-{n}-{v}"]
            a = a[start(a):]
            hp = np.diff(a)
            energy = [20 * np.log10(np.sqrt(np.mean(hp[int(s * RATE):int((s + 0.01) * RATE)] ** 2)) + 1e-12)
                      for s in (0, 0.01, 0.03)]
            print(f"  {n.upper()} {v:3d}: peak {20 * np.log10(np.abs(a).max()):6.1f}  hp {' '.join(f'{e:6.1f}' for e in energy)}"
                  f"  harmonics {harmonics(a, 47)}")


def start(x):
    """The first sample that leaves silence: the hit starts there."""
    return int(np.argmax(np.abs(x) > 1e-4))


def track(x):
    t, f = pitch_track(x[start(x):] if False else x)
    return t, f


def hz_track(x):
    """Pitch at each half cycle from all zero crossings, from the hit's true start."""
    x = x[start(x):]
    s = np.signbit(x)
    cross = np.nonzero(s[:-1] != s[1:])[0]
    pos = cross + x[cross] / (x[cross] - x[cross + 1])
    half = np.diff(pos)
    mid = (pos[1:] + pos[:-1]) / 2 / RATE
    return mid, RATE / (2 * half)


def envelope_db(x):
    x = x[start(x):]
    n = 1 << int(np.ceil(np.log2(len(x))))
    spec = np.fft.fft(x, n)
    h = np.zeros(n)
    h[0] = 1
    h[1:n // 2] = 2
    h[n // 2] = 1
    env = np.abs(np.fft.ifft(spec * h))[: len(x)]
    k = int(0.004 * RATE)   # 4 ms smoothing
    env = np.convolve(env, np.ones(k) / k, mode="same")
    return 20 * np.log10(env + 1e-12)


def fit_line(t, y):
    a, b = np.polyfit(t, y, 1)
    return a, b


def analyse(x, label):
    print(f"######## {label}")
    extras(x)

    print("== base pitch (late, 150-300 ms), PTCH sweep, RAMP 0")
    base = {}
    for v in V:
        t, f = hz_track(x[f"bd-ptch-{v}"])
        m = (t > 0.15) & (t < 0.3)
        base[v] = float(np.median(f[m]))
    logs = np.log2([base[v] for v in V])
    slope, icept = np.polyfit(np.array(V) / 127, logs, 1)
    print("  Hz:", " ".join(f"{base[v]:.1f}" for v in V))
    print(f"  fit: {2 ** icept:.2f} Hz * 2^({slope:.3f} * PTCH/127)")

    print("== built-in drop: depth * max(0, 1 - t/length), PTCH sweep")
    drop = {}
    for v in V:
        t, f = hz_track(x[f"bd-ptch-{v}"])
        e = f - base[v]
        best = None
        for length in np.arange(0.006, 0.08, 0.0005):
            m = (t > 0.002) & (t < 0.1)
            shape = np.maximum(0, 1 - t[m] / length)
            depth = float(np.dot(shape, e[m]) / (np.dot(shape, shape) + 1e-12))
            err = float(np.mean((e[m] - depth * shape) ** 2))
            if best is None or err < best[0]:
                best = (err, depth, length)
        drop[v] = best[1:]
        print(f"  PTCH {v:3d}: depth {best[1]:6.1f} Hz, length {best[2] * 1000:5.1f} ms, rms err {np.sqrt(best[0]):5.1f} Hz")

    print("== RAMP: extra Hz over RAMP 0, A * exp(-t/tau), RDEC 64")
    t0, f0 = hz_track(x["bd-ramp-0"])
    ramp = {}
    for v in V[1:]:
        t, f = hz_track(x[f"bd-ramp-{v}"])
        e = f - np.interp(t, t0, f0)
        m = (t > 0.012) & (t < 0.4) & (e > 1)
        if m.sum() < 5:
            ramp[v] = (0.0, 0.0)
            continue
        a, b = fit_line(t[m], np.log(e[m]))
        ramp[v] = (float(np.exp(b)), float(-1 / a))
        print(f"  RAMP {v:3d}: A {np.exp(b):7.1f} Hz, tau {-1000 / a:6.1f} ms")
    tau64 = ramp[127][1]

    print("== RDEC: tau of the extra Hz, RAMP 127")
    rdec = {}
    for v in V:
        t, f = hz_track(x[f"bd-rdec-{v}"])
        e = f - np.interp(t, t0, f0)
        m = (t > 0.012) & (t < 0.55) & (e > 2)
        a, b = fit_line(t[m], np.log(e[m]))
        rdec[v] = (float(np.exp(b)), float(-1 / a))
        print(f"  RDEC {v:3d}: A {np.exp(b):7.1f} Hz, tau {-1000 / a:8.1f} ms")

    print("== RAMP at PTCH 0 and 127 (is the extra in Hz, independent of PTCH?)")
    for v in (0, 127):
        tp, fp = hz_track(x[f"bd-ptch-{v}"])
        t, f = hz_track(x[f"bd-ramp127-ptch-{v}"])
        e = f - np.interp(t, tp, fp)
        m = (t > 0.012) & (t < 0.4) & (e > 1)
        a, b = fit_line(t[m], np.log(e[m]))
        print(f"  PTCH {v:3d}: A {np.exp(b):7.1f} Hz, tau {-1000 / a:6.1f} ms")

    print("== DEC: level after the first 20 ms, fitted as dB per second")
    dec = {}
    for v in V:
        env = envelope_db(x[f"bd-dec-{v}"])
        peak = env[: int(0.03 * RATE)].max()
        body = env[int(0.02 * RATE)]
        t = np.arange(len(env)) / RATE
        m = (t > 0.02) & (env < body - 3) & (env > body - 45)
        if m.sum() < 10:
            m = (t > 0.02) & (t < 2.4)
        a, b = fit_line(t[m], env[m])
        t60 = 60 / -a if a < 0 else float("inf")
        dec[v] = t60
        print(f"  DEC {v:3d}: body {body - peak:5.1f} dB below the first cycle; -60 dB in {t60 * 1000:8.0f} ms")



def main():
    analyse(render_many(jobs()), "Machinedrum TRX-BD")
    if "--fit-only" not in sys.argv:
        analyse(render_machine("TRX-BD", jobs()), "TRX-BD (ours)")


if __name__ == "__main__":
    main()
