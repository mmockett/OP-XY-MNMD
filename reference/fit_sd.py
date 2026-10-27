#!/usr/bin/env python3
"""Fits MD Snare to the Machinedrum's TRX-SD, measure by measure.

    ../open-xy/.venv/bin/python reference/fit_sd.py

What the renders show (each sweep holds the rest at a clean setting):
  body   one sine near 342 Hz; PTCH moves it only 339 .. 345 Hz; DEC sets its decay
  BUMP   a small pitch bump on the body, gone in 20 to 50 ms; BENV sets how long
  TONE   adds the wires: noise in a 60 Hz band at 6.98 kHz, swelling in over
         about 10 ms and decaying with DEC; level about TONE^1.5
  SNAP   adds a 1.2 .. 7 kHz crack, held about 10 ms, then -0.5 dB/ms
  TUNE   no measurable effect
  CLIP   odd harmonics from 96, as TRX-BD's
Each measure prints as Machinedrum / ours.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from probe import render_many, render_machine  # noqa: E402
from fit_bd import start, envelope_db, hz_track  # noqa: E402

RATE = 44100
V = [0, 32, 64, 96, 127]
SD = 17
CLEAN = [64, 64, 0, 0, 0, 0, 64, 0]


def jobs():
    j = []
    for v in V:
        for i, name in ((0, "ptch"), (1, "dec"), (5, "tone"), (4, "snap"), (7, "clip")):
            p = list(CLEAN)
            p[i] = v
            j.append((f"sd-{name}-{v}", SD, 1.0, p))
        j.append((f"sd2-dec-{v}", SD, 1.0, [64, v, 0, 0, 0, 127, 64, 0]))
        j.append((f"sd2-tune-{v}", SD, 1.0, [64, 64, 0, 0, 0, 127, v, 0]))
        j.append((f"sd2-bump-{v}", SD, 1.0, [64, 64, v, 64, 0, 0, 64, 0]))
        j.append((f"sd2-benv-{v}", SD, 1.0, [64, 64, 127, v, 0, 0, 64, 0]))
    return j


def body_pitch(a):
    t, f = hz_track(a)
    m = (t > 0.1) & (t < 0.3)
    return float(np.median(f[m]))


def t60(a):
    e = envelope_db(a)
    t = np.arange(len(e)) / RATE
    m = (t > 0.03) & (e < e.max() - 6) & (e > e.max() - 50)
    slope = np.polyfit(t[m], e[m], 1)[0]
    return 60 / -slope


def highs(a, cutoff=1500):
    spec = np.fft.rfft(a)
    f = np.fft.rfftfreq(len(a), 1 / RATE)
    spec[f < cutoff] = 0
    return np.fft.irfft(spec, len(a))


def level(h, ms, width=220):
    seg = h[int(ms * 44.1):int(ms * 44.1) + width]
    return 20 * np.log10(np.sqrt(np.mean(seg ** 2)) + 1e-12)


def band(a, t0=0.005, t1=0.04):
    seg = a[int(t0 * RATE):int(t1 * RATE)]
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 15)) ** 2
    f = np.fft.rfftfreq(1 << 15, 1 / RATE)
    m = f > 1000
    sm = np.convolve(s[m], np.ones(15) / 15, mode="same")
    above = f[m][sm > sm.max() / 4]
    return f"{f[m][np.argmax(sm)]:5.0f} Hz, -6 dB {above.min():5.0f}-{above.max():5.0f}"


def harmonics(a, f0=342):
    seg = a[int(0.05 * RATE):int(0.15 * RATE)]
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 16))
    fr = np.fft.rfftfreq(1 << 16, 1 / RATE)
    lv = [20 * np.log10(s[(fr > f0 * k * 0.95) & (fr < f0 * k * 1.05)].max() + 1e-12) for k in (1, 3, 5)]
    return f"3rd {lv[1] - lv[0]:4.0f} 5th {lv[2] - lv[0]:4.0f}"


def main():
    md = render_many(jobs())
    xy = render_machine("TRX-SD", jobs())
    get = lambda x, n: x[n][start(x[n]):]
    both = lambda name, fn: f"{fn(get(md, name))} / {fn(get(xy, name))}"
    print("== body pitch Hz, PTCH sweep")
    for v in V:
        print(f"  PTCH {v:3d}: " + both(f"sd-ptch-{v}", lambda a: f"{body_pitch(a):6.1f}"))
    print("== body -60 dB ms, DEC sweep (TONE 0)")
    for v in V:
        print(f"  DEC {v:3d}: " + both(f"sd-dec-{v}", lambda a: f"{t60(a) * 1000:6.0f}"))
    print("== wires (TONE 127, DEC sweep): level above 1.5 kHz at 0/5/10/20/40/80/160/320 ms, dB under the body's peak")
    for v in V:
        def env(a):
            h, peak = highs(a), 20 * np.log10(np.abs(a).max())
            return " ".join(f"{level(h, t) - peak:5.0f}" for t in (0, 5, 10, 20, 40, 80, 160, 320))
        print(f"  DEC {v:3d}: MD {env(get(md, f'sd2-dec-{v}'))}\n           xy {env(get(xy, f'sd2-dec-{v}'))}")
    print("== wires band, TONE 127, TUNE sweep")
    for v in V:
        print(f"  TUNE {v:3d}: " + both(f"sd2-tune-{v}", band))
    print("== wires level above 1.5 kHz at 10 ms, dB under the body's peak, TONE sweep")
    for v in V[1:]:
        print(f"  TONE {v:3d}: " + both(f"sd-tone-{v}", lambda a: f"{level(highs(a), 10) - 20 * np.log10(np.abs(a).max()):6.1f}"))
    print("== SNAP: band, and level above 1 kHz at 0/10/20/40/80 ms under the body's peak")
    for v in V[1:]:
        def snap(a):
            h, peak = highs(a, 1000), 20 * np.log10(np.abs(a).max())
            return band(a) + "  " + " ".join(f"{level(h, t) - peak:5.0f}" for t in (0, 10, 20, 40, 80))
        print(f"  SNAP {v:3d}: MD {snap(get(md, f'sd-snap-{v}'))}\n            xy {snap(get(xy, f'sd-snap-{v}'))}")
    T = [0.005, 0.01, 0.02, 0.03, 0.05, 0.08]
    print("== bump: pitch at " + " ".join(f"{t * 1000:.0f}" for t in T) + " ms")
    for kind in ("bump", "benv"):
        for v in V:
            def track(a):
                t, f = hz_track(a)
                return " ".join(f"{np.interp(s, t, f):5.0f}" for s in T)
            print(f"  {kind.upper()} {v:3d}: MD {track(get(md, f'sd2-{kind}-{v}'))} | xy {track(get(xy, f'sd2-{kind}-{v}'))}")
    print("== CLIP harmonics")
    for v in V:
        print(f"  CLIP {v:3d}: " + both(f"sd-clip-{v}", harmonics))


if __name__ == "__main__":
    main()
