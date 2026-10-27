#!/usr/bin/env python3
"""Fits our TRX-B2 (voices/trx_b2.hpp) to the Machinedrum's.

    ../open-xy/.venv/bin/python reference/fit_b2.py [--md-only]

TRX-B2 is the Machinedrum's second kick, PTCH DEC RAMP HOLD / TICK NOIS DIRT
DIST. Swept from PTCH 64, DEC 100, RAMP 0, HOLD 0 and the bottom row at 0: the
base pitch (RAMP 0), RAMP's extra Hz and its fall, the level's hold (HOLD) and
fall (DEC), the opening's click (TICK) and noise (NOIS), and what DIRT and
DIST add.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from trx import live, render_machine, onset, hz_track, env_ms, t60, level_at, rms_db  # noqa: E402
from fit_tom import base_pitch, extra, harmonics  # noqa: E402
from fit_bd import envelope_db  # noqa: E402

V = [0, 16, 32, 48, 64, 80, 96, 112, 127]
FIVE = [0, 32, 64, 96, 127]
B2 = 28


def jobs():
    j = []
    for v in V:
        j.append((f"b2-ptch-{v}", B2, 0.8, [v, 100, 0, 0, 0, 0, 0, 0]))
        j.append((f"b2-ramp-{v}", B2, 0.8, [64, 100, v, 0, 0, 0, 0, 0]))
        j.append((f"b2-dec-{v}", B2, 2.5, [64, v, 0, 0, 0, 0, 0, 0]))
        j.append((f"b2-hold-{v}", B2, 2.5, [64, 32, 0, v, 0, 0, 0, 0]))
    for v in (0, 127):
        j.append((f"b2-ramp127-ptch-{v}", B2, 0.8, [v, 100, 127, 0, 0, 0, 0, 0]))
    for i, n in [(4, "tick"), (5, "nois"), (6, "dirt"), (7, "dist")]:
        for v in FIVE:
            p = [64, 100, 0, 0, 0, 0, 0, 0]
            p[i] = v
            j.append((f"b2-{n}-{v}", B2, 0.8, p))
    return j


def above(a, cutoff):
    spec = np.fft.rfft(a)
    spec[np.fft.rfftfreq(len(a), 1 / 44100) < cutoff] = 0
    return np.fft.irfft(spec, len(a))


def analyse(x, label):
    get = lambda n: onset(x[n])
    print(f"######## {label}")
    print("== base pitch Hz (150-400 ms), PTCH sweep, RAMP 0; early (2-10 ms)")
    base = {v: base_pitch(get(f"b2-ptch-{v}")) for v in V}
    print("  " + " ".join(f"{base[v]:6.1f}" for v in V))
    print("  " + " ".join(f"{np.median(hz_track(get(f'b2-ptch-{v}'))[1][1:6]):6.1f}" for v in V))
    slope, icept = np.polyfit(np.array(V) / 127, np.log2([base[v] for v in V]), 1)
    print(f"  fit: {2 ** icept:.2f} Hz * 2^({slope:.3f} PTCH/127)")
    print("== RAMP: extra Hz A, tau ms; pitch at 2, 5, 10, 20 ms")
    ref = get("b2-ramp-0")
    for v in V[1:]:
        a = get(f"b2-ramp-{v}")
        A, tau = extra(a, ref)
        t, f = hz_track(a)
        print(f"  RAMP {v:3d}: A {A:7.1f} Hz, tau {tau * 1000:7.1f} ms | " + " ".join(f"{np.interp(s, t, f):6.1f}" for s in (0.002, 0.005, 0.01, 0.02)))
    for v in (0, 127):
        A, tau = extra(get(f"b2-ramp127-ptch-{v}"), get(f"b2-ptch-{v}"))
        print(f"  RAMP 127 at PTCH {v:3d}: A {A:7.1f} Hz, tau {tau * 1000:6.1f} ms")
    times = (2, 5, 10, 20, 50, 100, 200, 400, 800, 1600, 2400)
    for name in ("dec", "hold"):
        print(f"== {name.upper()}: peak dB; hold (ms to 3 dB under the level at 5 ms); then -60 dB ms; envelope at " + " ".join(map(str, times)) + " ms")
        for v in V:
            a = get(f"b2-{name}-{v}")
            e = envelope_db(a)
            held = int(np.argmax(e[220:] < e[220] - 3)) + 220
            t = np.arange(len(e)) / 44100
            m = (t > held / 44100) & (e < e[held] - 3) & (e > e[held] - 40)
            slope = np.polyfit(t[m], e[m], 1)[0] if m.sum() > 20 else float("nan")
            print(f"  {name.upper()} {v:3d}: peak {20 * np.log10(np.abs(a).max()):6.1f}  hold {held / 44.1:6.0f}  t60 {-60000 / slope if slope < 0 else float('inf'):7.0f}  "
                  + " ".join(f"{e[min(len(e) - 1, int(t * 44.1))] - e[220]:6.1f}" for t in times))
    print("== TICK, NOIS: peak dB, level above 1 kHz at 0-2, 2-5, 5-10, 10-20, 20-40 ms (dB of full scale), centroid 0-10 ms")
    for name in ("tick", "nois"):
        for v in FIVE:
            a = get(f"b2-{name}-{v}")
            h = above(a[:8820], 1000)
            seg = a[:441]
            s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
            f = np.fft.rfftfreq(len(seg), 1 / 44100)
            print(f"  {name.upper()} {v:3d}: peak {20 * np.log10(np.abs(a).max()):6.1f}  "
                  + " ".join(f"{rms_db(h, t0, t1):6.1f}" for t0, t1 in ((0, 0.002), (0.002, 0.005), (0.005, 0.01), (0.01, 0.02), (0.02, 0.04)))
                  + f"  centroid {np.sum(f * s) / np.sum(s):6.0f}  first ms {' '.join(f'{v * 100:+.1f}' for v in a[:44:4])}")
    print("== DIRT, DIST: peak dB, rms 0-100 ms, harmonics 2..7 (20-120 ms), level above 1 kHz 20-120 ms")
    for name in ("dirt", "dist"):
        for v in FIVE:
            a = get(f"b2-{name}-{v}")
            print(f"  {name.upper()} {v:3d}: peak {20 * np.log10(np.abs(a).max()):6.1f}  rms {rms_db(a, 0, 0.1):6.1f}  {harmonics(a, base[64])}"
                  f"  highs {rms_db(above(a[:8820], 1000), 0.02, 0.12):6.1f}")


def main():
    analyse(live(jobs()), "Machinedrum TRX-B2")
    if "--md-only" not in sys.argv:
        analyse(render_machine("TRX-B2", jobs()), "TRX-B2 (ours)")


if __name__ == "__main__":
    main()
