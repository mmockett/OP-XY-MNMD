#!/usr/bin/env python3
"""Fits the DigiPRO machines (voices/dpro.hpp, voices/dpro_bbox.hpp) to the
Monomachine's: renders each control's sweep on the emulator from the owner's
ROM and through our modules, and prints the measures side by side.

    ../open-xy/.venv/bin/python reference/fit_dpro.py [wave|ddrw|dens|bbox]

Needs the captured waves (tools/capture_dpro.py); without them ours is silent.
Both sides play middle C (the Monomachine's note 60 sounds 261.6 Hz on these
machines, two cycles of an S-set's table). What each control was found to do is
in reference/README.md; this prints the evidence.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from probe import render_many, render_machine  # noqa: E402

RATE = 44100
HOLD = "cc57=127 cc58=127"           # the amp page held open
MIDDLE = 261.626


def held(x, start=0.15, end=0.95):
    return x[int(start * RATE):int(end * RATE)]


def harmonics(x, f, count):
    t = np.arange(len(x)) / RATE
    w = np.hanning(len(x))
    return np.array([2 * np.sum(w * x * np.exp(-2j * np.pi * f * k * t)) / w.sum() for k in range(1, count + 1)])


def peaks(x, lo, hi, n=2):
    """The n strongest spectral peaks between lo and hi Hz: [(Hz, amplitude)]."""
    w = np.hanning(len(x))
    spectrum = np.abs(np.fft.rfft(x * w, 16 * len(x))) / w.sum() * 2
    hz = np.fft.rfftfreq(16 * len(x), 1 / RATE)
    idx = np.where((hz > lo) & (hz < hi))[0]
    local = [i for i in idx[1:-1] if spectrum[i] > spectrum[i - 1] and spectrum[i] >= spectrum[i + 1]]
    return sorted((hz[i], spectrum[i]) for i in sorted(local, key=lambda i: -spectrum[i])[:n])


def shape(a, b, f, count=40):
    """How alike two held notes' cycles are, whatever their phase: the correlation
    of their first harmonics' magnitudes, and b's level over a's."""
    ha, hb = np.abs(harmonics(held(a), f, count)), np.abs(harmonics(held(b), f, count))
    return np.dot(ha, hb) / np.sqrt(np.dot(ha, ha) * np.dot(hb, hb)), np.sqrt(np.mean(held(b) ** 2) / np.mean(held(a) ** 2))


def period(x, shortest=400, longest=3000):
    """The lag where a held note best repeats itself, between shortest and longest."""
    x = held(x) - held(x).mean()
    ac = np.fft.irfft(np.abs(np.fft.rfft(x, 2 * len(x))) ** 2)[:longest]
    return shortest + int(np.argmax(ac[shortest:longest]))


def both(name, machine, cases, seconds=1.0, options=""):
    """cases: {label: eight slots} -> {label: (Monomachine, ours)}."""
    jobs = [(f"{name}-{k}", machine, seconds, v, f"{HOLD} gate={seconds - 0.05:.2f} note=60 {options}".strip())
            for k, v in cases.items()]
    mm = render_many(jobs, "mm")
    ours = render_machine({"wave": "DPRO-WAVE", "ddrw": "DPRO-DDRW", "dens": "DPRO-DENS", "bbox": "DPRO-BBOX"}[name],
                          [(f"{name}-{k}", 0, seconds, v) for k, v in cases.items()])
    return {k: (mm[f"{name}-{k}"], ours[f"{name}-{k}"]) for k in cases}


def fit_wave():
    print("DPRO-WAVE: each S-set at WP 0..127 (shape: harmonic correlation, level: ours / theirs)")
    cases = {f"s{s:02d}wp{wp}": [4 * s + 1, wp, 0, 0, 0, 0, 0, 64] for s in (0, 10, 20, 31) for wp in (0, 24, 64, 100, 127)}
    cases.update({"tune0": [41, 0, 0, 0, 0, 0, 0, 0], "tune127": [41, 0, 0, 0, 0, 0, 0, 127]})
    cases.update({f"sfrq{v}": [41, 0, 0, 0, 64, v, 0, 64] for v in (32, 64, 96)})
    r = both("wave", 6, cases)
    for k, (a, b) in r.items():
        if k.startswith("s") and not k.startswith("sfrq"):
            sim, level = shape(a, b, MIDDLE / 2, 60)
            print(f"  {k:10s} shape {sim:.3f}  level {level:.2f}")
    for k in ("tune0", "tune127"):
        a, b = r[k]
        fa, fb = peaks(held(a), 200, 320, 1)[0][0], peaks(held(b), 200, 320, 1)[0][0]
        print(f"  {k:10s} pitch {12 * np.log2(fa / MIDDLE):+.2f} / {12 * np.log2(fb / MIDDLE):+.2f} semitones")
    for k in ("sfrq32", "sfrq64", "sfrq96"):
        a, b = r[k]
        pa, pb = period(a), period(b)
        print(f"  {k:10s} restarts every {pa} / {pb} samples ({RATE / pa:.1f} / {RATE / pb:.1f} Hz)")


def fit_ddrw():
    print("DPRO-DDRW: WAV1 SINE (D01), WAV2 SQR (D05)")
    cases = {f"mix{v}": [0, v, 8, 0, 0, 0, 0, 64] for v in (0, 32, 64, 96, 127)}
    cases.update({f"wid{v}": [0, 64, 8, 0, 0, v, 0, 64] for v in (0, 64, 127)})
    cases.update({f"br{v}": [2, 0, 2, 0, v, 64, 0, 64] for v in (0, 80, 112, 127)})
    cases.update({f"d{w:02d}": [2 * w, 0, 2 * w, 0, 0, 64, 0, 64] for w in (1, 16, 30, 50, 63)})
    r = both("ddrw", 32, cases)
    for v in (0, 32, 64, 96, 127):
        a, b = r[f"mix{v}"]
        ha, hb = np.abs(harmonics(held(a), MIDDLE, 3)), np.abs(harmonics(held(b), MIDDLE, 3))
        print(f"  MIX {v:3d}  h1 {ha[0]:.3f} / {hb[0]:.3f}  h3 {ha[2]:.3f} / {hb[2]:.3f}")
    for v in (0, 64, 127):
        a, b = r[f"wid{v}"]
        pa = [f"{12 * np.log2(f / MIDDLE) * 100:+.0f}" for f, _ in peaks(held(a, 0.1, 0.95), 240, 285)]
        pb = [f"{12 * np.log2(f / MIDDLE) * 100:+.0f}" for f, _ in peaks(held(b, 0.1, 0.95), 240, 285)]
        print(f"  WID {v:3d}  the two waves at {', '.join(pa)} / {', '.join(pb)} cents")
    for k in [k for k in r if k.startswith("br") or k.startswith("d")]:
        sim, level = shape(*r[k], MIDDLE, 40)
        print(f"  {k:6s}   shape {sim:.3f}  level {level:.2f}")


def fit_dens():
    print("DPRO-DENS: chords on SINE (D01), no chorus")
    cases = {"alone": [62, 62, 62, 0, 0, 0, 0, 64], "fifth": [100, 62, 62, 0, 0, 0, 0, 64],
             "chord": [20, 40, 100, 0, 0, 0, 0, 64], "octave": [0, 62, 62, 0, 0, 0, 0, 64],
             "just": [105, 78, 62, 0, 0, 0, 0, 64],
             "chorus": [62, 62, 62, 0, 0, 127, 0, 64], "wide": [62, 62, 62, 0, 0, 127, 127, 64],
             "org": [62, 62, 62, 40, 0, 0, 0, 64]}
    r = both("dens", 33, cases, 1.5)
    for k, (a, b) in r.items():
        pa = [(round(12 * np.log2(f / MIDDLE), 2), round(m, 3)) for f, m in peaks(held(a), 100, 600, 4) if m > 0.005]
        pb = [(round(12 * np.log2(f / MIDDLE), 2), round(m, 3)) for f, m in peaks(held(b), 100, 600, 4) if m > 0.005]
        ra, rb = np.sqrt(np.mean(held(a) ** 2)), np.sqrt(np.mean(held(b) ** 2))
        print(f"  {k:7s} rms {ra:.3f} / {rb:.3f}  peaks (semitones, level) {pa} / {pb}")


def fit_bbox():
    print("DPRO-BBOX at middle C: sample 12 (key 12)")
    cases = {"plain": [64, 0, 0, 0, 0, 64, 0, 0], "ptch0": [0, 0, 0, 0, 0, 64, 0, 0], "ptch127": [127, 0, 0, 0, 0, 64, 0, 0],
             "strt64": [64, 64, 0, 0, 0, 64, 0, 0], "rtrg": [64, 0, 0, 0, 8, 64, 0, 0], "rtim16": [64, 0, 0, 0, 8, 16, 0, 0]}
    r = both("bbox", 7, cases, 1.0)

    def onsets(x, kernel):
        c = np.correlate(x[:RATE], kernel, "valid") / np.sqrt(np.convolve(x[:RATE] ** 2, np.ones(len(kernel)), "valid")
                                                              * (kernel ** 2).sum() + 1e-12)
        found = []
        for i in range(1, len(c) - 1):
            if c[i] > 0.9 and c[i] >= c[i - 1] and c[i] >= c[i + 1] and (not found or i - found[-1] > 40):
                found.append(i)
        return found

    def trim(x):
        a = np.abs(x)
        o = int(np.argmax(a > a.max() * 0.01))
        return x[o:o + 8000]

    for k, (a, b) in r.items():
        ta, tb = trim(a), trim(b)
        n = min(len(ta), len(tb))
        x = np.fft.irfft(np.fft.rfft(ta[:n], 2 * n) * np.conj(np.fft.rfft(tb[:n], 2 * n)))
        sim = np.abs(x).max() / np.sqrt(np.dot(ta[:n], ta[:n]) * np.dot(tb[:n], tb[:n]))
        la = len(np.where(np.abs(a) > np.abs(a).max() * 1e-2)[0]) / RATE * 1000
        lb = len(np.where(np.abs(b) > np.abs(b).max() * 1e-2)[0]) / RATE * 1000
        line = f"  {k:8s} match {sim:.3f}  length {la:.0f} / {lb:.0f} ms  peak {np.abs(a).max():.3f} / {np.abs(b).max():.3f}"
        if k.startswith("rt"):
            kernel = trim(r["plain"][0])[:400]
            oa, ob = onsets(a, kernel), onsets(b, trim(r["plain"][1])[:400])
            line += f"  restarts every {np.mean(np.diff(oa)) / 44.1 if len(oa) > 1 else 0:.1f} / {np.mean(np.diff(ob)) / 44.1 if len(ob) > 1 else 0:.1f} ms"
        print(line)


if __name__ == "__main__":
    wanted = sys.argv[1:] or ["wave", "ddrw", "dens", "bbox"]
    for w in wanted:
        globals()[f"fit_{w}"]()
