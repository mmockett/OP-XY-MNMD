"""Measures shared by the EFM and GND fitters (reference/fit_efm_*.py, fit_gnd_*.py).

Renders go through probe.render_many under an exclusive lock, so that several
fitting sessions on one machine take turns with the emulator: set
EMULATOR_LOCK to a file path to share one (default local/emulator.lock).
"""
import contextlib
import fcntl
import os
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import probe  # noqa: E402

RATE = 44100
V = [0, 16, 32, 48, 64, 80, 96, 112, 127]
LOCK = pathlib.Path(os.environ.get("EMULATOR_LOCK", str(probe.ROOT / "local" / "emulator.lock")))


@contextlib.contextmanager
def locked():
    LOCK.parent.mkdir(parents=True, exist_ok=True)
    with open(LOCK, "w") as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


def render_md(jobs):
    with locked():
        return probe.render_many(jobs, "md")


def render_ours(name, jobs):
    with locked():
        return probe.render_machine(name, jobs)


def start(x, threshold=1e-4):
    """The first sample that leaves silence: the hit starts there."""
    return int(np.argmax(np.abs(x) > threshold))


def trim(x):
    return x[start(x):]


def envelope_db(x, smooth_ms=4):
    """The Hilbert envelope in dB, smoothed, from the hit's start."""
    x = trim(x)
    n = 1 << int(np.ceil(np.log2(max(len(x), 2))))
    spec = np.fft.fft(x, n)
    h = np.zeros(n)
    h[0] = 1
    h[1:n // 2] = 2
    h[n // 2] = 1
    env = np.abs(np.fft.ifft(spec * h))[: len(x)]
    k = max(1, int(smooth_ms / 1000 * RATE))
    env = np.convolve(env, np.ones(k) / k, mode="same")
    return 20 * np.log10(env + 1e-12)


def peak_db(x):
    return float(20 * np.log10(np.abs(x).max() + 1e-12))


def t60(x, after=0.01, top=3, bottom=40):
    """Seconds to fall 60 dB, from a line fitted to the envelope between `top`
    and `bottom` dB under its peak (after `after` seconds)."""
    env = envelope_db(x)
    t = np.arange(len(env)) / RATE
    peak = env.max()
    m = (t > after) & (env < peak - top) & (env > peak - bottom)
    if m.sum() < 10:
        return float("nan")
    a, _ = np.polyfit(t[m], env[m], 1)
    return 60 / -a if a < 0 else float("inf")


def time_to(x, db):
    """Seconds from the start until the envelope last sits above `db` under its peak."""
    env = envelope_db(x)
    above = np.nonzero(env > env.max() - db)[0]
    return float(above[-1] / RATE) if len(above) else 0.0


def hz_track(x):
    """Pitch at each half cycle from all zero crossings, from the hit's start."""
    x = trim(x)
    s = np.signbit(x)
    cross = np.nonzero(s[:-1] != s[1:])[0]
    pos = cross + x[cross] / (x[cross] - x[cross + 1])
    half = np.diff(pos)
    mid = (pos[1:] + pos[:-1]) / 2 / RATE
    return mid, RATE / (2 * half)


def hz_at(x, t0, t1):
    t, f = hz_track(x)
    m = (t > t0) & (t < t1)
    return float(np.median(f[m])) if m.any() else float("nan")


def spectrum(x, t0, t1, n=1 << 16):
    seg = trim(x)[int(t0 * RATE):int(t1 * RATE)]
    if len(seg) < 16:
        return np.zeros(1), np.zeros(1)
    seg = seg * np.hanning(len(seg))
    s = np.abs(np.fft.rfft(seg, n))
    return np.fft.rfftfreq(n, 1 / RATE), 20 * np.log10(s + 1e-12)


def peaks(x, t0, t1, count=8, floor=-60, fmin=20):
    """The strongest spectral peaks in a window: [(Hz, dB under the strongest)]."""
    fr, db = spectrum(x, t0, t1)
    if len(db) < 3:
        return []
    is_peak = (db[1:-1] > db[:-2]) & (db[1:-1] >= db[2:]) & (fr[1:-1] > fmin)
    idx = np.nonzero(is_peak)[0] + 1
    top = db.max()
    idx = idx[db[idx] > top + floor]
    idx = idx[np.argsort(-db[idx])][:count]
    return [(float(fr[i]), float(db[i] - top)) for i in sorted(idx)]


def centroid(x, t0, t1):
    fr, db = spectrum(x, t0, t1)
    p = 10 ** (db / 10)
    return float((fr * p).sum() / (p.sum() + 1e-20))


def band_db(x, t0, t1, lo, hi):
    """Energy in a band, dB of the whole window's."""
    fr, db = spectrum(x, t0, t1)
    p = 10 ** (db / 10)
    m = (fr >= lo) & (fr < hi)
    return float(10 * np.log10(p[m].sum() / (p.sum() + 1e-20) + 1e-20))


def rms_db(x, t0, t1):
    seg = trim(x)[int(t0 * RATE):int(t1 * RATE)]
    return float(10 * np.log10(np.mean(seg ** 2) + 1e-20)) if len(seg) else -200.0


def noisiness(x, t0, t1):
    """Spectral flatness in a window, 0 (tones) .. 1 (white noise)."""
    fr, db = spectrum(x, t0, t1, n=1 << 12)
    p = 10 ** (db / 10) + 1e-20
    m = (fr > 50) & (fr < 16000)
    return float(np.exp(np.mean(np.log(p[m]))) / np.mean(p[m]))


def fmt_peaks(ps):
    return " ".join(f"{f:7.0f}({d:4.0f})" for f, d in ps)


def summary(x, label):
    """One line: peak, t60, early and late pitch, centroid early and late, noisiness."""
    return (f"{label:>14}: pk {peak_db(x):6.1f} t60 {t60(x) * 1000:7.0f}ms  -40 at {time_to(x, 40) * 1000:6.0f}ms  "
            f"Hz {hz_at(x, 0.002, 0.012):7.1f} {hz_at(x, 0.1, 0.2):7.1f}  "
            f"cen {centroid(x, 0, 0.02):6.0f} {centroid(x, 0.05, 0.1):6.0f}  nz {noisiness(x, 0, 0.05):.2f}")


# ---- FM: what the EFM fitters measure ------------------------------------------
_TAU = np.linspace(0, np.pi, 2001)


def bessel(n, b):
    """J_n(b), by its integral."""
    return float(np.trapezoid(np.cos(n * _TAU - b * np.sin(_TAU)), _TAU) / np.pi)


_GRID = np.linspace(1e-3, 2.4, 2400)
_J1J0 = np.array([bessel(1, b) / bessel(0, b) for b in _GRID])


def index_of(ratio_j1_j0):
    """The FM index whose first sideband stands `ratio_j1_j0` to the carrier (below 2.4)."""
    return float(_GRID[np.argmin(np.abs(_J1J0 - ratio_j1_j0))])


def line(x, f, t0, t1, width=6):
    """(Hz, linear amplitude) of the strongest line within `width` Hz of f."""
    fr, db = spectrum(x, t0, t1)
    m = (fr > f - width) & (fr < f + width)
    i = np.argmax(db[m])
    return float(fr[m][i]), float(10 ** (db[m][i] / 20))


def index_track(x, fc, fm, times, window=0.03):
    """The FM index over time, from the carrier and the upper first sideband."""
    out = []
    for t in times:
        _, c = line(x, fc, t, t + window)
        _, s = line(x, fc + fm, t, t + window)
        out.append(index_of(s / c))
    return out


def tau_of(times, values, low=0.05):
    """The time constant of an exponential fall through (times, values), from the
    values above `low` of the first."""
    t, v = np.array(times), np.array(values)
    m = v > v[0] * low
    if m.sum() < 2:
        return float("nan")
    a, _ = np.polyfit(t[m], np.log(v[m]), 1)
    return -1 / a if a < 0 else float("inf")


def excess_tau(x, base, t0=0.003, t1=1.0, low=1.0):
    """For a pitch falling exponentially onto `base` Hz: (excess at t=0 in Hz, tau s)."""
    t, f = hz_track(x)
    e = f - base
    m = (t > t0) & (t < t1) & (e > low)
    if m.sum() < 5:
        return 0.0, 0.0
    a, b = np.polyfit(t[m], np.log(e[m]), 1)
    return float(np.exp(b)), float(-1 / a) if a < 0 else float("inf")


def body_rate(x, t0, t1):
    """dB per second the envelope falls between t0 and t1."""
    env = envelope_db(x)
    t = np.arange(len(env)) / RATE
    m = (t > t0) & (t < t1)
    a, _ = np.polyfit(t[m], env[m], 1)
    return float(-a)


# ---- sweeps: what every EFM and GND fitter renders ------------------------------
def sweeps(machine, prefix, bases, seconds=1.0):
    """For each (slot label, slot index, base settings), the control swept over V
    from that base: [(name, machine, seconds, values)], named <prefix>-<label>-<v>."""
    j = []
    for label, slot, base in bases:
        for v in V:
            p = list(base)
            p[slot] = v
            j.append((f"{prefix}-{label}-{v}", machine, seconds, p))
    return j


def windows_db(x, times, width=0.01):
    """RMS dB in windows starting at `times`, under the first window's."""
    first = rms_db(x, times[0], times[0] + width)
    return [rms_db(x, t, t + width) - first for t in times]


def report(x, prefix, labels, extra=None):
    """Prints each sweep's summary() and strongest peaks, and `extra(x)` if given."""
    for label in labels:
        print(f"== {label.upper()}")
        for v in V:
            a = x[f"{prefix}-{label}-{v}"]
            print(summary(a, f"{label} {v}"), "|", fmt_peaks(peaks(a, 0.005, 0.045, 5)))
    if extra:
        extra(x)


DESCRIBE_TIMES = (0.0, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.4, 0.8)


def describe(x, prefix, labels, t0=0.01, t1=0.1):
    """For each sweep: RMS dB (5 ms windows) at DESCRIBE_TIMES under the first,
    the first window's dB, pitch 50-150 ms, centroid and noisiness over t0..t1,
    and the strongest peaks there."""
    print("   times ms:", " ".join(f"{t * 1000:5.0f}" for t in DESCRIBE_TIMES))
    for label in labels:
        print(f"== {label.upper()}")
        for v in V:
            a = x[f"{prefix}-{label}-{v}"]
            w = windows_db(a, DESCRIBE_TIMES, 0.005)
            print(f"  {v:3d}: {rms_db(a, 0, 0.005):6.1f} |" + " ".join(f"{d:5.0f}" for d in w[1:])
                  + f" | {hz_at(a, 0.05, 0.15):7.1f} Hz cen {centroid(a, t0, t1):6.0f} nz {noisiness(a, t0, t1):.2f} | "
                  + fmt_peaks(peaks(a, t0, t1, 6, -30)))
