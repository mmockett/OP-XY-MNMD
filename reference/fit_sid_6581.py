#!/usr/bin/env python3
"""Fits SID-6581 to the Monomachine: renders clean settings through the emulator
(the AMP page held open, so the level is steady) and through voices/sid_6581.hpp,
and prints the same measures side by side, Monomachine / ours.

    ../open-xy/.venv/bin/python reference/fit_sid_6581.py

What it found (OS 1.32b), at note 60 (261.6 Hz; this machine plays MIDI pitch):
- WAVE, MOD, MSRC and PWRS are steps, v * n >> 7 as the LCD shows them: TRI SAW
  PULS MIX NOIS; OFF RING SYNC R+S; MFRQ PRCH; OFF ON.
- TRI, SAW and PULS share one amplitude. PW is symmetric about 64: the duty is
  0.5 + |PW - 64| / 128, so PW 64 is a full-width pulse, silent.
- PWAD sweeps PW at a steady rate (40 steps a second at 16, 185 at 64, 332 at
  127), wrapping, so the duty rises and falls like a triangle; PWRS on starts the
  sweep from PW at the note.
- MIX is one fixed shape, whatever PW: nothing for the first half cycle, then a
  rippled hump; 0.31 of the saw's level.
- NOIS is held noise, a new value 16 times a cycle (the SID's noise clock), so it
  follows the key.
- TUNE is +-2 semitones, 1/32 semitone a step. MFRQ sets the modulator at
  2^((MFRQ - 32) / 32) x 261.6 Hz on every key; TUNE leaves it. RING ignores WAVE: the two triangles and
  1.1 x their product, the product's low difference tones weaker (a high-pass
  near 200 Hz). SYNC plays WAVE at the modulator's pitch, restarted half way
  through its cycle at each cycle of the note (R+S: at its start). The saw falls. MSRC = PRCH takes the previous track's oscillator, MFRQ unused.
- The output rolls off above 2 kHz: -1.5 dB at 2.1 kHz, -5 at 4.2, -10 at 8.4.
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from probe import render_machine, render_many  # noqa: E402

SR = 44100
F0 = 261.626
STEADY = "cc57=127 cc58=127"
#           PW PWAD PWRS WAVE MOD MSRC MFRQ TUNE
BASE = [32, 0, 0, 40, 0, 0, 64, 64]
CASES = {
    "saw": {},
    "tri": {"WAVE": 10},
    "pulse-pw0": {"WAVE": 64, "PW": 0},
    "pulse-pw32": {"WAVE": 64, "PW": 32},
    "pulse-pw48": {"WAVE": 64, "PW": 48},
    "pulse-pw56": {"WAVE": 64, "PW": 56},
    "pulse-pw96": {"WAVE": 64, "PW": 96},
    "mix": {"WAVE": 90},
    "noise": {"WAVE": 115},
    "tune0": {"TUNE": 0},
    "tune127": {"TUNE": 127},
    "ring-m40": {"WAVE": 10, "MOD": 40, "MFRQ": 40},
    "ring-m72": {"WAVE": 10, "MOD": 40, "MFRQ": 72},
    "sync-saw-m8": {"MOD": 80, "MFRQ": 8},
    "sync-saw-m40": {"MOD": 80, "MFRQ": 40},
    "sync-saw-m56": {"MOD": 80, "MFRQ": 56},
    "sync-tri-m40": {"WAVE": 10, "MOD": 80, "MFRQ": 40},
    "sync-pulse-m48": {"WAVE": 64, "MOD": 80, "MFRQ": 48},
    "rs-m40": {"WAVE": 10, "MOD": 110, "MFRQ": 40},
    "pwad16": {"WAVE": 64, "PWAD": 16, "PWRS": 127},
    "pwad64": {"WAVE": 64, "PWAD": 64, "PWRS": 127},
    "pwad127": {"WAVE": 64, "PWAD": 127, "PWRS": 127},
}
LABELS = ["PW", "PWAD", "PWRS", "WAVE", "MOD", "MSRC", "MFRQ", "TUNE"]


def values(case):
    v = list(BASE)
    for k, x in CASES[case].items():
        v[LABELS.index(k)] = x
    return v


def window(x, start=0.15, length=0.4):
    return x[int(start * SR):int((start + length) * SR)]


def rms(x):
    return float(np.sqrt(np.mean(window(x) ** 2)))


def spectrum(x):
    a = window(x) * np.hanning(int(0.4 * SR))
    return np.abs(np.fft.rfft(a, 1 << 19)), np.fft.rfftfreq(1 << 19, 1 / SR)


def at(sp, fr, hz):
    i = int(round(hz / fr[1]))
    return sp[max(0, i - 8):i + 9].max()


def harmonics(x, f=F0, n=8):
    sp, fr = spectrum(x)
    h = np.array([at(sp, fr, k * f) for k in range(1, n + 1)])
    return h / max(h[0], 1e-12)


def pitch(x):
    sp, fr = spectrum(x)
    m = (fr > 200) & (fr < 330)
    return fr[m][np.argmax(sp[m])]


def peaks(x, k=5):
    sp, fr = spectrum(x)
    loc = np.where((sp[1:-1] > sp[:-2]) & (sp[1:-1] >= sp[2:]))[0] + 1
    loc = loc[(fr[loc] > 30) & (fr[loc] < 12000)]
    top = loc[np.argsort(sp[loc])[::-1][:k]]
    ref = sp[top[0]]
    return " ".join(f"{fr[i]:.0f}({20 * np.log10(sp[i] / ref):.0f})" for i in sorted(top))


def bands(x):
    sp, fr = spectrum(x)
    sp = sp ** 2
    b = np.array([np.sum(sp[(fr >= lo) & (fr < 2 * lo)]) for lo in (125, 250, 500, 1000, 2000, 4000, 8000)])
    return " ".join(f"{v:.0f}" for v in 10 * np.log10(b / b.sum()))


def duty_track(x):
    """The share of each cycle the pulse is high, every 50 ms over the first 0.9 s."""
    period = SR / F0
    out = []
    for t in np.arange(0.02, 0.9, 0.05):
        a = x[int(t * SR):int(t * SR + 4 * period)]
        lo, hi = np.percentile(a, 5), np.percentile(a, 95)
        out.append("--" if hi - lo < 0.01 * max(abs(x).max(), 1e-9) else f"{round(np.mean(a > (lo + hi) / 2) * 99):2d}")
    return " ".join(out)


def main():
    names = list(CASES)
    mm = render_many([(f"sid-fit-{n}", 3, 1.0, values(n), STEADY) for n in names], "mm")
    ours = render_machine("SID-6581", [(f"sid-fit-{n}", 3, 1.0, values(n)) for n in names])
    ref = {side: rms(r["sid-fit-saw"]) for side, r in (("mm", mm), ("xy", ours))}
    for n in names:
        a, b = mm[f"sid-fit-{n}"], ours[f"sid-fit-{n}"]
        la, lb = (20 * np.log10(rms(x) / ref[s] + 1e-12) for x, s in ((a, "mm"), (b, "xy")))
        print(f"{n:15s} level {la:+5.1f} / {lb:+5.1f} dB")
        if n.startswith(("saw", "tri", "pulse", "mix", "sync")):
            print("   harmonics", " ".join(f"{p:.2f}" for p in harmonics(a)), "/", " ".join(f"{p:.2f}" for p in harmonics(b)))
        if n.startswith("tune"):
            print(f"   pitch {pitch(a):.1f} / {pitch(b):.1f} Hz")
        if n.startswith(("ring", "rs", "sync")):
            print("   peaks", peaks(a), "/", peaks(b))
        if n == "noise":
            print("   bands", bands(a), "/", bands(b))
        if n.startswith("pwad"):
            print("   duty", duty_track(a))
            print("   ours", duty_track(b))


if __name__ == "__main__":
    main()
