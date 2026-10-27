"""Measures shared by the TRX fitters (fit_tom.py, fit_rs.py, fit_cb.py, ...).

    from trx import live, hz_track, env_ms, t60, peaks

`live` renders Machinedrum jobs as probe.render_many does, then renders again
any hit that came back dead: about one hit in 25, the emulated machine sounds
only a step that drains away, as if its oscillator never ran (same settings,
rendered again, sound normally). A hit is taken as dead when it crosses zero
fewer than three times between 2 and 50 ms after its onset; a hit still dead
after two more tries is kept.

`onset` finds a hit's start from its own level, not from the first sample off
silence (probe's `start`): mdref leaves a second between jobs, so a long hit's
tail can still be sounding, 60 dB down, when the next one starts.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from probe import render_many, render_machine  # noqa: E402,F401


RATE = 44100


def rms_db(a, t0, t1):
    seg = a[int(t0 * RATE):int(t1 * RATE)]
    return 20 * np.log10(np.sqrt(np.mean(seg ** 2)) + 1e-12) if len(seg) else -240.0


def start(x):
    """The hit's first sample: where it first reaches 5% of its peak, moved back
    (at most 2 ms) to where the last 32 samples before it stay under 0.3% of the peak."""
    x = np.abs(np.asarray(x))
    top = x.max() + 1e-12
    i = int(np.argmax(x > top * 0.05))
    first = max(0, i - 88)
    while i > first and x[max(0, i - 32):i].max() > top * 0.003:
        i -= 1
    return i


def onset(x):
    return x[start(x):]


def hz_track(x):
    """Pitch at each half cycle from all zero crossings, from the hit's onset: (s, Hz)."""
    x = onset(x)
    s = np.signbit(x)
    cross = np.nonzero(s[:-1] != s[1:])[0]
    pos = cross + x[cross] / (x[cross] - x[cross + 1])
    half = np.diff(pos)
    mid = (pos[1:] + pos[:-1]) / 2 / RATE
    return mid, RATE / (2 * half)


def dead(a):
    a = onset(a)[int(0.002 * RATE):int(0.05 * RATE)]
    s = np.signbit(a)
    return int(np.count_nonzero(s[:-1] != s[1:])) < 3


def live(jobs, instrument="md", tries=2):
    """render_many, with dead hits rendered again under a new name: {name: samples}."""
    out = render_many(jobs, instrument)
    todo = [j for j in jobs if dead(out[j[0]])]
    for attempt in range(1, tries + 1):
        if not todo:
            break
        again = [(f"{j[0]}~{attempt}",) + tuple(j[1:]) for j in todo]
        fresh = render_many(again, instrument)
        still = []
        for j, k in zip(todo, again):
            out[j[0]] = fresh[k[0]]
            if dead(fresh[k[0]]):
                still.append(j)
        todo = still
    return out


def env_ms(a):
    """Level in dB per millisecond."""
    n = len(a) // 44
    return 20 * np.log10(np.sqrt(np.mean(a[: n * 44].reshape(n, 44) ** 2, axis=1)) + 1e-12)


def t60(a, lo=3, hi=40, skip=0.005):
    """The -60 dB time (s) of a line fitted to the level between lo and hi dB under the peak."""
    a = onset(a)
    e = env_ms(a)
    i = int(np.argmax(e))
    t = np.arange(len(e)) / 1000
    m = (t > t[i] + skip) & (e < e[i] - lo) & (e > e[i] - hi)
    if m.sum() < 4:
        return float("nan")
    slope = np.polyfit(t[m], e[m], 1)[0]
    return 60 / -slope if slope < 0 else float("inf")


def peaks(a, t0, t1, n=8, low=20):
    """The n strongest spectral peaks between t0 and t1 s: [(Hz, dB under the strongest)]."""
    seg = a[int(t0 * RATE):int(t1 * RATE)]
    if len(seg) < 256 or np.abs(seg).max() < 1e-7:
        return []
    size = 1 << 16
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), size))
    f = np.fft.rfftfreq(size, 1 / RATE)
    db = 20 * np.log10(s + 1e-12)
    top = db[f > low].max()
    loc = [i for i in range(2, len(s) - 2) if f[i] > low and s[i] > s[i - 1] and s[i] >= s[i + 1]
           and s[i] > s[i - 2] and s[i] >= s[i + 2]]
    loc = sorted(loc, key=lambda i: -s[i])[:n]
    return sorted((round(float(f[i])), round(float(db[i] - top))) for i in loc)


def level_at(a, f0, t0, t1, width=0.03):
    """The level (dB, absolute) of the strongest line within f0 * (1 +- width) between t0 and t1 s."""
    seg = a[int(t0 * RATE):int(t1 * RATE)]
    size = 1 << 16
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), size)) * 2 / (np.sum(np.hanning(len(seg))) + 1e-12)
    f = np.fft.rfftfreq(size, 1 / RATE)
    m = (f > f0 * (1 - width)) & (f < f0 * (1 + width))
    return 20 * np.log10(s[m].max() + 1e-12)


def centroid(a, t0, t1):
    seg = a[int(t0 * RATE):int(t1 * RATE)]
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / RATE)
    return float(np.sum(f * s) / (np.sum(s) + 1e-20))

