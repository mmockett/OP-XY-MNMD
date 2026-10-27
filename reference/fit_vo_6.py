#!/usr/bin/env python3
"""Fits VO-6, the Monomachine's formant voice, to the original through the
emulator (reference/README.md has what was found).

    ../open-xy/.venv/bin/python reference/fit_vo_6.py [batch ...]

Batches: lcd (CONS's steps, read off the LCD), vowels (the formants, fitted
as a cascade to whispered vowels), voices (VOIC), consonants (their shape,
CLEN and CVOL), tuning, compare (the original beside voices/vo_6.hpp), and
table (prints the consonant table voices/vo_6.hpp holds). Renders are cached
in local/ref/probe-md/ (reference/probe.py); with EMULATOR_LOCK set to a
file, each emulator run holds an exclusive lock on it, for sharing the Mac.
"""
import contextlib
import fcntl
import os
import pathlib
import subprocess
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from probe import ROMS, ROOT, render_many, render_machine  # noqa: E402
import vo  # noqa: E402

ID = 11
BASE = [64, 64, 0, 0, 0, 64, 64, 64]   # VOC1 VOC2 V-SW VOIC CONS CLEN CVOL TUNE
HOLD = "cc57=127"                       # the amp page's HOLD: the note sustains while gated
CONS = ["-", "B", "D", "F", "G", "H", "J", "K", "L", "M", "N", "P", "R", "RR", "S", "SJ", "T", "TJ", "TH", "V", "Z"]


def p(**kw):
    out = list(BASE)
    for k, v in kw.items():
        out[["voc1", "voc2", "vsw", "voic", "cons", "clen", "cvol", "tune"].index(k)] = v
    return out


def cons_value(i):
    """The middle of consonant i's step on the original's 0..127 (v * 21 >> 7)."""
    return (i * 128 + 64) // 21


@contextlib.contextmanager
def emulator():
    lock = os.environ.get("EMULATOR_LOCK")
    if not lock:
        yield
        return
    with open(lock, "a") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)


def original(jobs):
    with emulator():
        return render_many(jobs, "mm")


# --- Measures --------------------------------------------------------------

def welch(x, n=4096):
    frames = [x[i:i + n] * np.hanning(n) for i in range(0, len(x) - n, n // 2)]
    s = np.mean([np.abs(np.fft.rfft(f)) ** 2 for f in frames], axis=0)
    return np.fft.rfftfreq(n, 1 / vo.SR), 10 * np.log10(np.maximum(s, 1e-20))


def cascade(f, formants, widths):
    """dB response of two-pole resonances in cascade, each with unity gain at DC."""
    z = np.exp(1j * 2 * np.pi * f / vo.SR)
    h = np.ones_like(z)
    for hz, bw in zip(formants, widths):
        r = np.exp(-np.pi * bw / vo.SR)
        a1, a2 = -2 * r * np.cos(2 * np.pi * hz / vo.SR), r * r
        h = h * (1 + a1 + a2) / (1 + a1 / z + a2 / z ** 2)
    return 20 * np.log10(np.abs(h))


def simplex(cost, x0, step, iterations=1500):
    """Nelder-Mead, enough for a handful of parameters."""
    pts = [np.array(x0, float)] + [np.array(x0, float) + np.eye(len(x0))[i] * step[i] for i in range(len(x0))]
    vals = [cost(q) for q in pts]
    for _ in range(iterations):
        order = np.argsort(vals)
        pts, vals = [pts[i] for i in order], [vals[i] for i in order]
        c = np.mean(pts[:-1], axis=0)
        xr = c + (c - pts[-1]); fr = cost(xr)
        if fr < vals[0]:
            xe = c + 2 * (c - pts[-1]); fe = cost(xe)
            pts[-1], vals[-1] = (xe, fe) if fe < fr else (xr, fr)
        elif fr < vals[-2]:
            pts[-1], vals[-1] = xr, fr
        else:
            xc = c + 0.5 * (pts[-1] - c); fc = cost(xc)
            if fc < vals[-1]:
                pts[-1], vals[-1] = xc, fc
            else:
                pts = [pts[0] + 0.5 * (q - pts[0]) for q in pts]
                vals = [cost(q) for q in pts]
    best = int(np.argmin(vals))
    return pts[best], vals[best]


def fit_cascade(x, formants, widths, low=100, high=3700):
    """Four resonances fitted to a whisper's averaged spectrum: (Hz, widths, rms dB error)."""
    f, db = welch(x)
    sel = (f > low) & (f < high)
    ff, d = f[sel], db[sel]

    def cost(q):
        e = d - cascade(ff, np.exp(q[0:4]), np.exp(q[4:8])) - q[8] * np.log2(ff / 1000)
        return np.mean((e - e.mean()) ** 2)
    q, c = simplex(cost, list(np.log(formants)) + list(np.log(widths)) + [0], [0.05] * 4 + [0.3] * 4 + [1])
    return np.exp(q[:4]), np.exp(q[4:8]), float(np.sqrt(c))


def track(x, steady_from=0.6, steady_to=0.9):
    """10 ms levels in dB against the steady vowel."""
    t = vo.track(x, 0.01, 0.01)
    return t - np.median(t[int(steady_from * 100):int(steady_to * 100)])


# --- Batches ---------------------------------------------------------------

def lcd():
    """CONS's steps: each value's LCD, grouped by what its CONS cell shows."""
    out = ROOT / "local" / "ref" / "vo-lcd"
    out.mkdir(parents=True, exist_ok=True)
    lines = []
    for v in range(128):
        lines += [f"c{v:03} {ID} 0.05 " + " ".join(map(str, p(cons=v))) + " gate=0.02", f"lcd c{v:03}"]
    with emulator():
        subprocess.run([str(ROOT / "local" / "mdref"), ROMS["mm"], str(out)], input="\n".join(lines) + "\n", text=True,
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    cells, runs = {}, []
    for v in range(128):
        rows = [r.rstrip("\n") for r in open(out / f"c{v:03}.txt") if r.strip()]
        cell = "\n".join(r[48:67] for r in rows[55:63])   # the CONS cell's text
        cells.setdefault(cell, len(cells))
        if runs and runs[-1][2] == cells[cell]:
            runs[-1][1] = v
        else:
            runs.append([v, v, cells[cell]])
    print("CONS: value runs and the glyph each shows (a '-' right after a change is the LCD catching up)")
    print(" ".join(f"{a}-{b}:{g}" for a, b, g in runs))
    print("expected steps v * 21 >> 7:", " ".join(f"{CONS[i]}@{min(v for v in range(128) if v * 21 >> 7 == i)}" for i in range(21)))


def vowels():
    steps = list(range(0, 128, 16)) + [127]
    jobs = [(f"vo-wv1-{v:03}", ID, 1.6, p(voc1=v, voic=127), f"note=36 gate=1.5 {HOLD}") for v in steps]
    jobs += [(f"vo-wv2-{v:03}", ID, 1.6, p(voc2=v, voic=127), f"note=36 gate=1.5 {HOLD}") for v in steps]
    jobs += [(f"vo-wv2lo-{v:03}", ID, 1.6, p(voc1=0, voc2=v, voic=127), f"note=36 gate=1.5 {HOLD}") for v in [104, 120, 124, 127]]
    x = original(jobs)
    print("Whispered vowels (VOIC 127), four resonances in cascade fitted to 100 .. 3700 Hz:")
    for name, _, _, values, _ in jobs:
        f2 = 525 * 2 ** (2.35 * values[1] / 127)
        start = [204 * 2 ** (1.8 * values[0] / 127), min(f2, 2300), 2490 if f2 < 2300 else 2700, 3575]
        hz, widths, error = fit_cascade(vo.segment(x[name], 0.3, 1.4), start, [70, 55, 145, 280])
        hz, widths = zip(*sorted(zip(hz, widths)))
        print(f"  {name:15} F {np.round(hz).astype(int)}  B {np.round(widths).astype(int)}  error {error:.1f} dB"
              f"  level {vo.rms_db(vo.segment(x[name], 0.3, 1.4)):.1f} dB")


def voices():
    jobs = [(f"vo-voic-{v:03}", ID, 1.2, p(voic=v), "note=36 gate=1.0") for v in list(range(0, 128, 16)) + [127]]
    x = original(jobs)
    print("VOIC: the buzz's share of the power (harmonicity at the played 65.4 Hz, a whisper reading 0.18)")
    for name, *_ in jobs:
        s = vo.segment(x[name], 0.4, 0.9)
        h = vo.harmonicity(s, 65.41)
        print(f"  {name:12} {vo.rms_db(s):6.1f} dB  harmonic {h:.2f}  buzz share {(h - 0.18) / (0.93 - 0.18):.2f}")


def consonant_jobs():
    jobs = []
    for c in range(1, 21):
        v = p(cons=cons_value(c), vsw=127)
        jobs += [(f"vo-cons-{c:02}", ID, 1.0, v, f"note=48 gate=0.8 {HOLD}")]
        for clen in [0, 127]:
            w = list(v); w[5] = clen
            jobs += [(f"vo-clen{clen:03}-{c:02}", ID, 1.4, w, f"note=48 gate=1.2 {HOLD}")]
        for cvol in [0, 127]:
            w = list(v); w[6] = cvol
            jobs += [(f"vo-cvol{cvol:03}-{c:02}", ID, 1.0, w, f"note=48 gate=0.8 {HOLD}")]
    return jobs


def consonants():
    jobs = consonant_jobs()
    for clen in [16, 48, 80, 112]:
        jobs += [(f"vo-clen-c14-{clen:03}", ID, 1.2, p(cons=cons_value(14), vsw=127, clen=clen), f"note=48 gate=1.0 {HOLD}")]
    for c in [1, 14]:
        jobs += [(f"vo-vsw000-c{c:02}", ID, 1.0, p(cons=cons_value(c), vsw=0), f"note=48 gate=0.8 {HOLD}")]
    x = original(jobs)
    print("Each consonant at CLEN 64, CVOL 64: 10 ms levels against the vowel, then the spectral centroid (kHz)")
    for c in range(1, 21):
        a = x[f"vo-cons-{c:02}"]
        t = track(a)
        cen = [vo.centroid(a[int(i * 441):int((i + 1) * 441)]) / 1000 for i in range(12)]
        print(f"  {CONS[c]:3}", " ".join("  ." if v < -60 else f"{v:3.0f}" for v in t[:25]),
              "|", " ".join(f"{v:.1f}" for v in cen))
    print("S's vowel arrives (ms) at CLEN 0, 16 ... 112:",
          [int(np.argmax(track(x[n])[:100] > -6) * 10) for n in
           ["vo-clen000-14"] + [f"vo-clen-c14-{v:03}" for v in [16, 48, 80, 112]]])


def tuning():
    jobs = [(f"vo-tune-{v:03}", ID, 0.8, p(tune=v), "note=60 gate=0.6") for v in [0, 32, 48, 56, 63, 64, 65, 72, 96, 127]]
    jobs += [(f"vo-note-{n}", ID, 0.8, p(), f"note={n} gate=0.6") for n in [36, 48, 60, 72, 84]]
    x = original(jobs)
    for name, *_ in jobs:
        print(f"  {name:12} {vo.pitch(vo.segment(x[name], 0.2, 0.55)):8.2f} Hz")


def compare():
    """The original beside voices/vo_6.hpp: vowels' spectra and levels, then the consonants' level tracks."""
    pts = [200, 300, 400, 500, 700, 900, 1200, 1500, 2000, 2500, 3000, 3600]
    jobs = [(f"vo-wv{k}-{v:03}", ID, 1.6, p(**{"voc1" if k == 1 else "voc2": v, "voic": 127}), f"note=36 gate=1.5 {HOLD}")
            for k in (1, 2) for v in (0, 64, 127)]
    x, y = original(jobs), render_machine("VO-6", jobs)
    print("Whispered vowels, dB against each one's peak at", pts, "Hz; original / ours")
    for name, *_ in jobs:
        a, b = vo.segment(x[name], 0.3, 1.4), vo.segment(y[name], 0.3, 1.4)
        (fa, da), (fb, db) = welch(a), welch(b)
        sa, sb = np.convolve(da, np.ones(5) / 5, "same"), np.convolve(db, np.ones(5) / 5, "same")
        print(f"  {name:12} level {vo.rms_db(a):6.1f} / {vo.rms_db(b):6.1f}  ",
              " ".join(f"{np.interp(q, fa, sa) - sa.max():.0f}/{np.interp(q, fb, sb) - sb.max():.0f}" for q in pts))
    variants = ["vo-cons-%02d", "vo-clen000-%02d", "vo-clen127-%02d", "vo-cvol127-%02d", "vo-cvol000-%02d"]
    jobs = consonant_jobs()
    x, y = original(jobs), render_machine("VO-6", jobs)
    print("Consonants: mean |dB| difference of the 10 ms level tracks over 250 ms (700 ms at CLEN 127),")
    print("at CLEN 64, CLEN 0, CLEN 127, CVOL 127, CVOL 0")
    total = []
    for c in range(1, 21):
        errors = []
        for pattern in variants:
            a, b = track(x[pattern % c]), track(y[pattern % c])
            n = 70 if "clen127" in pattern else 25
            m = (a[:n] > -40) | (b[:n] > -40)
            errors.append(float(np.mean(np.abs(np.clip(a[:n][m], -40, 20) - np.clip(b[:n][m], -40, 20)))))
        total += errors
        print(f"  {CONS[c]:3}", " ".join(f"{e:4.1f}" for e in errors))
    print(f"  overall {np.mean(total):.2f} dB")


# --- The consonants, as fitted: each is silence, up to two segments, then the
# vowel. Levels are dB against the vowel (noise at CVOL 64, where measured), times
# in ms at CLEN 0; `stretch` names the segment CLEN lengthens (1 or 2). A
# segment voices (`voice`), aspirates through the formants (`asp`) and/or
# hisses through a band (`fric` at `band` Hz, `q`). The formants sit at the
# locus `f1`/`f2` (None: the vowel's) and glide to the vowel over `glide` ms;
# the vowel starts `overlap` ms before the consonant ends, rises over `rise` ms
# and overshoots by `boost` dB, settling over 80 ms. `trill` is an AM rate, Hz.
CONSONANTS = [
    dict(name="-", rise=55),
    dict(name="B", seg=[dict(ms=50, voice=-14)], f1=300, f2=900, overlap=50, rise=20, glide=25),
    dict(name="D", seg=[dict(ms=6, fric=-14, band=2600, q=1.5), dict(ms=14, voice=-10)], f1=300, f2=1700,
         overlap=6, rise=15, glide=25),
    dict(name="F", stretch=1, seg=[dict(ms=85, fric=-22, band=3300, q=1.2, rise=10)], overlap=10, rise=35, glide=15, boost=3),
    dict(name="G", seg=[dict(ms=12, fric=-9, band=1400, q=2.0), dict(ms=14, asp=-24)], f1=300, f2=1900, rise=6, glide=20),
    dict(name="H", stretch=1, seg=[dict(ms=70, asp=-14, fric=-20, band=2500, q=1.0, rise=30)], rise=10, glide=10, boost=4),
    dict(name="J", delay=50, stretch=1, seg=[dict(ms=40, voice=-4, rise=20)], f1=270, f2=2200, overlap=0, rise=8,
         glide=50, boost=5),
    dict(name="K", seg=[dict(ms=28, fric=-3, band=1400, q=1.5, asp=-3), dict(ms=20, asp=-8)], rise=15, glide=15),
    dict(name="L", delay=50, stretch=1, seg=[dict(ms=70, voice=-2, rise=15)], f1=230, f2=1650, rise=6, glide=30, boost=10),
    dict(name="M", delay=50, stretch=1, seg=[dict(ms=75, voice=-7, rise=15)], f1=150, f2=1000, rise=6, glide=30, boost=6),
    dict(name="N", delay=50, stretch=1, seg=[dict(ms=75, voice=-1, rise=15)], f1=220, f2=1700, rise=6, glide=30, boost=10),
    dict(name="P", seg=[dict(ms=6, fric=-20, band=700, q=1.0), dict(ms=20, asp=-6)], f1=300, f2=900, rise=12, glide=20),
    dict(name="R", delay=30, stretch=1, seg=[dict(ms=85, voice=-21, rise=10)], f1=420, f2=1200, rise=60, glide=40),
    dict(name="RR", delay=20, stretch=1, seg=[dict(ms=85, voice=-9, rise=10)], f1=420, f2=1200, trill=22, rise=25, glide=20),
    dict(name="S", stretch=1, seg=[dict(ms=85, fric=-29, band=3800, q=0.9, rise=30)], overlap=10, rise=35, glide=15, boost=2),
    dict(name="SJ", stretch=1, seg=[dict(ms=80, fric=-18, band=3000, q=1.3, rise=15)], overlap=10, rise=30, glide=15, boost=4),
    dict(name="T", seg=[dict(ms=25, fric=-2, band=2400, q=1.2), dict(ms=35, asp=-12)], rise=15, glide=15),
    dict(name="TJ", stretch=2, seg=[dict(ms=18, fric=-24, band=2000, q=1.5), dict(ms=58, fric=-38, band=4000, q=0.7)],
         rise=25, glide=15, boost=2),
    dict(name="TH", stretch=1, seg=[dict(ms=50, fric=-17, band=3200, q=1.2)], overlap=15, rise=45, glide=15, boost=3),
    dict(name="V", seg=[dict(ms=18, fric=-42, band=3000, q=1.2), dict(ms=22, voice=-12)], f1=300, f2=1100, rise=15,
         glide=20, boost=3),
    dict(name="Z", stretch=1, seg=[dict(ms=72, fric=-24, band=2700, q=1.2, voice=-30, rise=5)], rise=120, glide=15),
]
CLEN_MS = [0, 5, 5, 12, 28, 75, 160, 290, 500]   # CLEN's added length at 0, 16, ..., 112, 127 (S, measured)
VOWEL_RMS = 0.06            # the vowel's output RMS (voices/vo_6.hpp)
NOISE_RMS = 0.577           # full-scale uniform noise


def band_pass(hz, q):
    """An RBJ band-pass (0 dB peak) as Q30 b0, a1, a2 (b1 = 0, b2 = -b0); zeros without a band."""
    import math
    if not hz:
        return 0, 0, 0
    w = 2 * math.pi * hz / 44100
    alpha = math.sin(w) / (2 * q)
    a0 = 1 + alpha
    return round(alpha / a0 * 2 ** 30), round(-2 * math.cos(w) / a0 * 2 ** 30), round((1 - alpha) / a0 * 2 ** 30)


def hiss_power(hz, q):
    """The hiss filter's power gain for white noise: the band-pass, twice."""
    b0, a1, a2 = (v / 2 ** 30 for v in band_pass(hz, q))
    f = np.linspace(0, 22050, 4096, endpoint=False)
    z = np.exp(-2j * np.pi * f / 44100)
    h = (b0 * (1 - z * z) / (1 + a1 * z + a2 * z * z)) ** 2
    return float(np.mean(np.abs(h) ** 2))


def table_cpp():
    """The C++ table voices/vo_6.hpp holds, from CONSONANTS."""
    import math
    ms = lambda t: round(t * 44.1)
    q31 = lambda x: min(0x7fffffff, round(x * 2 ** 31))
    db = lambda d: 0 if d is None else 10 ** (d / 20)
    octv = lambda hz: 0 if hz is None else round(math.log2(hz / 20) * 65536)
    rate = lambda t: 0x7fffffff if ms(t) <= 1 else round(2 ** 31 / ms(t))
    rows = []
    for c in CONSONANTS:
        segs = c.get("seg", []) + [{}] * (2 - len(c.get("seg", [])))
        band = next((s for s in segs if s.get("band")), None)
        fields = []
        for s in segs:
            fric = 0
            if s.get("fric") is not None:
                fric = VOWEL_RMS * db(s["fric"] + 6) / (NOISE_RMS * math.sqrt(hiss_power(s["band"], s["q"])))
            fields.append((ms(s.get("ms", 0)), q31(db(s.get("voice"))), q31(db(s["asp"] + 6) / 2 if s.get("asp") is not None else 0),
                           q31(fric / 8), rate(s.get("rise", 1)),
                           *band_pass(s.get("band"), s.get("q"))))
        rows.append("    {%d, {%s}, %d, {%d, %d}, %d, %d, %d, %d, %du},   // %s" % (
            ms(c.get("delay", 0)),
            ", ".join("{%d, %d, %d, %d, %d, {%d, %d, %d}}" % f for f in fields),
            c.get("stretch", 0), octv(c.get("f1")), octv(c.get("f2")), ms(c.get("overlap", 0)),
            rate(c.get("rise", 1)), rate(c.get("glide", 1)), round(c.get("boost", 0) / 6.0206 * 65536),
            round(c.get("trill", 0) * 64 / 44100 * 2 ** 32), c["name"]))
    return "\n".join(rows)


BATCHES = {"lcd": lcd, "vowels": vowels, "voices": voices, "consonants": consonants, "tuning": tuning,
           "compare": compare, "table": lambda: print(table_cpp())}

if __name__ == "__main__":
    for batch in sys.argv[1:] or ["vowels", "voices", "consonants", "tuning", "compare"]:
        BATCHES[batch]()
