#!/usr/bin/env python3
"""Time curves of rendered hits, for fitting: the pitch from upward zero
crossings (good for sine-like drums), and the level in 5 ms steps.

    ../open-xy/.venv/bin/python reference/curves.py trx_bd-default trx_bd-ramp-127 ...

Prints, for each hit, the Machinedrum and ours side by side at a few times.
Import it for `pitch_track` and `level_track`.
"""
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
REF = ROOT / "local" / "ref"
RATE = 44100


def read(path):
    return np.frombuffer(pathlib.Path(path).read_bytes()[44:], dtype="<f4").astype(np.float64)


def onset(x):
    return int(np.argmax(np.abs(x) > np.max(np.abs(x)) * 0.05))


def pitch_track(x):
    """(times s, Hz) at each upward zero crossing after the onset, interpolated."""
    x = x[onset(x):]
    s = np.signbit(x)
    up = np.nonzero(s[:-1] & ~s[1:])[0]
    if len(up) < 3:
        return np.array([]), np.array([])
    # Sub-sample crossing positions.
    pos = up + x[up] / (x[up] - x[up + 1])
    period = np.diff(pos)
    return pos[1:] / RATE, RATE / period


def level_track(x, hop_ms=5):
    hop = int(RATE * hop_ms / 1000)
    x = x[onset(x):]
    n = len(x) // hop
    rms = np.sqrt(np.mean(x[: n * hop].reshape(n, hop) ** 2, axis=1) + 1e-20)
    return np.arange(n) * hop_ms / 1000, 20 * np.log10(rms)


def pitch_at(times, hz, t):
    if len(times) == 0 or t > times[-1]:
        return None
    return float(np.interp(t, times, hz))


TIMES = [0.002, 0.005, 0.01, 0.02, 0.04, 0.08, 0.16, 0.32]

if __name__ == "__main__":
    for name in sys.argv[1:]:
        print(name)
        for side in ("md", "xy"):
            x = read(REF / side / f"{name}.wav")
            t, f = pitch_track(x)
            lt, lv = level_track(x)
            peak = lv.max()
            ps = " ".join(f"{(pitch_at(t, f, s) or 0):6.1f}" for s in TIMES)
            ls = " ".join(f"{(lv[min(len(lv) - 1, int(s / 0.005))] - peak):6.1f}" for s in TIMES)
            print(f"  {side} Hz {ps}\n  {side} dB {ls}")
