"""Measures for voiced sounds, for reference/fit_vo_6.py: pitch, level tracks,
brightness and how much of a sound is at the harmonics. numpy only."""
import numpy as np

SR = 44100


def segment(x, t0, t1):
    return x[int(t0 * SR):int(t1 * SR)]


def pitch(x, low=40, high=1500):
    """Fundamental by autocorrelation with parabolic interpolation, Hz."""
    x = x - x.mean()
    n = len(x)
    spec = np.fft.rfft(x * np.hanning(n), 2 * n)
    ac = np.fft.irfft(np.abs(spec) ** 2)[:n]
    lo, hi = int(SR / high), int(SR / low)
    k = lo + int(np.argmax(ac[lo:hi]))
    # The first peak within 85% of the best, to avoid octave errors downward.
    best = ac[k]
    for j in range(lo + 1, hi - 1):
        if ac[j] > 0.85 * best and ac[j] >= ac[j - 1] and ac[j] >= ac[j + 1]:
            k = j
            break
    a, b, c = ac[k - 1], ac[k], ac[k + 1]
    shift = 0.5 * (a - c) / (a - 2 * b + c) if a - 2 * b + c else 0
    return SR / (k + shift)


def rms_db(x):
    return 20 * np.log10(max(np.sqrt(np.mean(x * x)), 1e-9))


def track(x, hop=0.005, width=0.01):
    """RMS in dB every `hop` seconds."""
    h, w = int(hop * SR), int(width * SR)
    return np.array([rms_db(x[i:i + w]) for i in range(0, len(x) - w, h)])


def centroid(x):
    n = len(x)
    spec = np.abs(np.fft.rfft(x * np.hanning(n)))
    f = np.fft.rfftfreq(n, 1 / SR)
    return float((spec * f).sum() / max(spec.sum(), 1e-12))


def harmonicity(x, f0):
    """Share of energy at the harmonics of f0 (1 = all voiced, near 0 = noise)."""
    n = len(x)
    spec = np.abs(np.fft.rfft(x * np.hanning(n))) ** 2
    f = np.fft.rfftfreq(n, 1 / SR)
    near = np.abs(((f / f0 + 0.5) % 1) - 0.5) * f0 < max(8, f0 * 0.08)
    sel = (f > 50) & (f < 8000)
    return float(spec[sel & near].sum() / max(spec[sel].sum(), 1e-18))
