#!/usr/bin/env python3
"""Fits our EFM-SD (voices/efm_sd.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_efm_sd.py [--fit-only]

EFM-SD is an FM tone (md_efm.hpp, as EFM-BD's without the sweep) and a noise
with its own decay, both through a high-pass. Each sweep starts from a setting
that isolates the control: the tone alone (NOIS 0, MOD 0), the noise alone
(DEC 0), and so on. Each measure prints the Machinedrum, then ours.
"""
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from efm_measure import (V, render_md, render_ours, sweeps, report, hz_at, rms_db, windows_db, peaks,  # noqa: E402
                         fmt_peaks, band_db, centroid, index_track, tau_of)

SD = 33
NAME = "EFM-SD"
TONE = [64, 96, 0, 64, 0, 64, 64, 0]
NOISE = [64, 0, 127, 64, 0, 64, 64, 0]


def jobs():
    return sweeps(SD, "efmsd", [
        ("ptch", 0, TONE), ("dec", 1, TONE),
        ("nois", 2, [64, 64, 0, 64, 0, 64, 64, 0]), ("ndec", 3, NOISE),
        ("mod", 4, [64, 96, 0, 64, 0, 64, 127, 0]), ("mfrq", 5, [127, 96, 0, 64, 8, 64, 127, 0]),
        ("mdec", 6, [64, 112, 0, 64, 64, 64, 64, 0]),
        ("hpf", 7, [64, 96, 0, 64, 0, 64, 64, 0]), ("nhpf", 7, NOISE),
    ], 1.5)


def extra(x):
    times = [0.0, 0.01, 0.02, 0.05, 0.1, 0.2, 0.4, 0.8]
    print("== tone: Hz and RMS dB (50-100 ms) over PTCH")
    print("  ", " ".join(f"{hz_at(x[f'efmsd-ptch-{v}'], 0.05, 0.2):6.1f}:{rms_db(x[f'efmsd-ptch-{v}'], 0.05, 0.1):5.1f}" for v in V))
    for label in ("dec", "ndec"):
        print(f"== {label.upper()}: dB at", " ".join(f"{t * 1000:.0f}" for t in times), "ms")
        for v in V:
            print(f"  {v:3d}:", " ".join(f"{d:6.1f}" for d in windows_db(x[f"efmsd-{label}-{v}"], times)))
    print("== NOIS: RMS dB of 0-20 ms and 50-60 ms, and the noise's share above 2 kHz (0-20 ms)")
    for v in V:
        a = x[f"efmsd-nois-{v}"]
        print(f"  {v:3d}: {rms_db(a, 0, 0.02):6.1f} {rms_db(a, 0.05, 0.06):6.1f} {band_db(a, 0, 0.02, 2000, 20000):6.1f}")
    print("== NOISE spectrum (NDEC 64): dB in octave bands from 125 Hz, 0-30 ms")
    a = x["efmsd-ndec-64"]
    print("  ", " ".join(f"{band_db(a, 0, 0.03, 125 * 2 ** k, 250 * 2 ** k):6.1f}" for k in range(8)))
    fc = hz_at(x["efmsd-ptch-64"], 0.05, 0.2)
    print("== MOD: FM index at 20 ms (MFRQ 64)")
    print("  ", " ".join(f"{index_track(x[f'efmsd-mod-{v}'], fc, 4 * fc, [0.02])[0]:6.2f}" for v in V))
    print("== MFRQ (PTCH 127, MOD 8): peaks")
    for v in V:
        print(f"  {v:3d}:", fmt_peaks(peaks(x[f"efmsd-mfrq-{v}"], 0.02, 0.3, 5, -40)))
    print("== MDEC: the index's time constant, ms")
    tt = [0.0, 0.01, 0.02, 0.04, 0.07, 0.1, 0.15, 0.2, 0.3, 0.45]
    print("  ", " ".join(f"{tau_of(tt, index_track(x[f'efmsd-mdec-{v}'], fc, 4 * fc, tt), 0.1) * 1000:7.1f}" for v in V))
    for label in ("hpf", "nhpf"):
        print(f"== {label.upper()}: RMS dB (0-50 ms), centroid, dB in octave bands from 125 Hz")
        for v in V:
            a = x[f"efmsd-{label}-{v}"]
            print(f"  {v:3d}: {rms_db(a, 0, 0.05):6.1f} {centroid(a, 0, 0.05):6.0f} |",
                  " ".join(f"{band_db(a, 0, 0.05, 125 * 2 ** k, 250 * 2 ** k):6.1f}" for k in range(8)))


def main():
    labels = []
    for name, x in [("Machinedrum", render_md(jobs()))] + ([] if "--fit-only" in sys.argv else [("ours", render_ours(NAME, jobs()))]):
        print(f"######## EFM-SD ({name})")
        report(x, "efmsd", labels, extra)


if __name__ == "__main__":
    main()
