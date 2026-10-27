#!/usr/bin/env python3
"""Fits our EFM-BD (voices/efm_bd.hpp) to the Machinedrum's, control by control.

    ../open-xy/.venv/bin/python reference/fit_efm_bd.py

The model, read off the renders (reference/README.md):

    f(t)   = base(PTCH) * (1 + (RAMP/16 - 1) * exp(-t / tau(RDEC)))     the carrier, Hz
    fm(t)  = f(t) * MFRQ/16                                            the modulator
    dev(t) = f(t) * MOD/16 * exp(-t / tau(MDEC))                       FM deviation, Hz
    level  = exp decay to -60 dB in T60(DEC), over a quiet tail from -20 dB

MFB feeds the modulator back into its own phase, one sample late. Each
measure prints the Machinedrum, then ours.
"""
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from efm_measure import (V, render_md, render_ours, hz_at, envelope_db, excess_tau, index_track, tau_of,  # noqa: E402
                         peaks, peak_db, line, body_rate)

BD = 32
NAME = "EFM-BD"


def jobs(machine=BD, prefix="efmbd", last="mfb"):
    """The sweeps; EFM-XT, laid out like EFM-BD, reuses them (reference/fit_efm_xt.py).
    `last` names the eighth slot, swept with MOD 64 as MFB is."""
    j = []
    for v in V:
        j.append((f"{prefix}-ptch-{v}", machine, 1.0, [v, 96, 16, 64, 0, 64, 64, 0]))
        j.append((f"{prefix}-dec-{v}", machine, 2.5, [64, v, 16, 64, 0, 64, 64, 0]))
        j.append((f"{prefix}-ramp-{v}", machine, 1.0, [64, 96, v, 96, 0, 64, 64, 0]))
        j.append((f"{prefix}-rdec-{v}", machine, 1.5, [64, 112, 64, v, 0, 64, 64, 0]))
        j.append((f"{prefix}-mod-{v}", machine, 1.0, [64, 96, 16, 64, v, 64, 127, 0]))
        j.append((f"{prefix}-mfrq-{v}", machine, 1.0, [127, 96, 16, 64, 8, v, 127, 0]))
        j.append((f"{prefix}-mdec-{v}", machine, 1.5, [64, 112, 16, 64, 64, 64, v, 0]))
        j.append((f"{prefix}-{last}-{v}", machine, 1.0, [64, 96, 16, 64, 64, 64, 127, v]))
    return j


def analyse(x, label, prefix="efmbd", last="mfb"):
    print(f"######## {label}")
    x = {k.replace(prefix, "efmbd").replace(f"-{last}-", "-mfb-"): v for k, v in x.items()}
    fc = hz_at(x["efmbd-ptch-64"], 0.1, 0.3)
    print("== PTCH: Hz, 100-300 ms (RAMP 16, MOD 0)")
    print("  ", " ".join(f"{hz_at(x[f'efmbd-ptch-{v}'], 0.1, 0.3):6.1f}" for v in V))
    print("== DEC: dB at 10 20 50 100 200 400 800 1600 ms, and the body's dB/s over its first 10 dB")
    for v in V:
        a = x[f"efmbd-dec-{v}"]
        env = envelope_db(a)
        pk = env.max()
        at = [env[min(len(env) - 1, int(t * 44100))] - pk for t in (0.01, 0.02, 0.05, 0.1, 0.2, 0.4, 0.8, 1.6)]
        cross = np.nonzero(env < pk - 10)[0]
        end = cross[0] / 44100 if len(cross) else 2.0
        print(f"  DEC {v:3d}: " + " ".join(f"{d:6.1f}" for d in at) + f"   body {body_rate(a, 0.004, max(end, 0.008)):6.0f} dB/s")
    print("== RAMP: the start's multiple of the settled pitch (RDEC 96)")
    out = []
    base = fc
    for v in V:
        if v < 16:
            out.append(float("nan"))
            continue
        e, _ = excess_tau(x[f"efmbd-ramp-{v}"], base, 0.003, 0.4)
        out.append((base + e) / base)
    print("  ", " ".join(f"{m:6.2f}" for m in out))
    print("== RDEC: the sweep's time constant, ms (RAMP 64)")
    out = []
    for v in V:
        a = x[f"efmbd-rdec-{v}"]
        _, tau = excess_tau(a, base, 0.002, 1.4, 2.0)
        out.append(tau * 1000)
    print("  ", " ".join(f"{t:6.1f}" for t in out))
    print("== MOD: FM index at the start (MFRQ 64, so the ratio is 4)")
    print("  ", " ".join(f"{index_track(x[f'efmbd-mod-{v}'], fc, 4 * fc, [0.02])[0]:6.2f}" for v in V))
    print("== MFRQ: the strongest peaks (PTCH 127, MOD 8)")
    for v in V:
        print(f"  MFRQ {v:3d}:", " ".join(f"{f:6.0f}({d:3.0f})" for f, d in peaks(x[f"efmbd-mfrq-{v}"], 0.05, 0.4, 5, -40)))
    print("== MDEC: the index's time constant, ms (MOD 64, MFRQ 64)")
    times = [0.0, 0.01, 0.02, 0.04, 0.07, 0.1, 0.15, 0.2, 0.3, 0.45]
    out = []
    for v in V:
        tr = index_track(x[f"efmbd-mdec-{v}"], fc, 4 * fc, times)
        out.append(tau_of(times, tr, 0.1) * 1000)
    print("  ", " ".join(f"{t:7.1f}" for t in out))
    print(f"== {last.upper()}: the carrier's line, Hz, and the peak dB (MOD 64, MFRQ 64)")
    print("  ", " ".join(f"{line(x[f'efmbd-mfb-{v}'], fc * 0.7, 0.05, 0.35, fc * 0.7)[0]:6.1f}" for v in V))
    print("  ", " ".join(f"{peak_db(x[f'efmbd-mfb-{v}']):6.1f}" for v in V))
    print("== level: peak dB at PTCH 0, 64, 127 (MOD 0) and MOD 127")
    print("  ", " ".join(f"{peak_db(x[n]):6.1f}" for n in ("efmbd-ptch-0", "efmbd-ptch-64", "efmbd-ptch-127", "efmbd-mod-127")))


def main():
    analyse(render_md(jobs()), "Machinedrum EFM-BD")
    if "--fit-only" not in sys.argv:
        analyse(render_ours(NAME, jobs()), "EFM-BD (ours)")


if __name__ == "__main__":
    main()
