"""Measures for fitting the P-I machines (reference/fit_pi_*.py): onset, level,
decay times, the strongest partials and how fast each falls, brightness.

    from pi_measure import summary, partials, decays
"""
import numpy as np

RATE = 44100


def start(x):
    """The first sample that leaves silence: the hit starts there."""
    return int(np.argmax(np.abs(x) > 1e-4))


def envelope_db(x, ms=4):
    """The level in dB, Hilbert magnitude smoothed over `ms`, from the onset."""
    x = x[start(x):]
    n = 1 << int(np.ceil(np.log2(max(len(x), 2))))
    spec = np.fft.fft(x, n)
    h = np.zeros(n)
    h[0] = 1
    h[1:n // 2] = 2
    h[n // 2] = 1
    env = np.abs(np.fft.ifft(spec * h))[: len(x)]
    k = max(1, int(ms / 1000 * RATE))
    env = np.convolve(env, np.ones(k) / k, mode="same")
    return 20 * np.log10(env + 1e-12)


def decays(x, drops=(20, 40)):
    """Seconds from the peak until the level stays below the peak minus each drop."""
    env = envelope_db(x)
    p = int(np.argmax(env))
    out = []
    for d in drops:
        above = np.nonzero(env[p:] > env[p] - d)[0]
        out.append((above[-1] if len(above) else 0) / RATE)
    return out


def peak_db(x):
    return 20 * np.log10(np.abs(x).max() + 1e-12)


def spectrum(x, t0=0.0, t1=0.1):
    x = x[start(x):]
    seg = x[int(t0 * RATE):int(t1 * RATE)]
    seg = seg * np.hanning(len(seg))
    n = 1 << 17
    s = np.abs(np.fft.rfft(seg, n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    return f, s


def partials(x, t0=0.0, t1=0.1, count=8, floor_hz=20, apart=0.04):
    """The `count` strongest spectral peaks between t0 and t1: [(Hz, dB)], loudest first,
    each at least `apart` (a fraction) from a louder one."""
    f, s = spectrum(x, t0, t1)
    db = 20 * np.log10(s + 1e-12)
    peaks = np.nonzero((db[1:-1] > db[:-2]) & (db[1:-1] >= db[2:]))[0] + 1
    peaks = peaks[f[peaks] > floor_hz]
    peaks = peaks[np.argsort(-db[peaks])]
    chosen = []
    for k in peaks:
        if all(abs(f[k] - f[c]) > apart * f[c] for c in chosen):
            chosen.append(k)
        if len(chosen) == count:
            break
    top = db[chosen[0]] if chosen else 0
    return [(round(float(f[k]), 1), round(float(db[k] - top), 1)) for k in chosen]


def centroid(x, t0=0.0, t1=0.05):
    f, s = spectrum(x, t0, t1)
    return float(np.sum(f * s ** 2) / (np.sum(s ** 2) + 1e-20))


def band_decay(x, hz, width=0.03, hop=0.01, span=1.0, win=0.04):
    """The level of the band around `hz` every `hop` seconds, dB from its first window:
    a partial's own fall."""
    x = x[start(x):]
    n = int(win * RATE)
    out = []
    for i in range(int(span / hop)):
        seg = x[int(i * hop * RATE):int(i * hop * RATE) + n]
        if len(seg) < n:
            break
        seg = seg * np.hanning(n)
        s = np.abs(np.fft.rfft(seg, 1 << 14))
        f = np.fft.rfftfreq(1 << 14, 1 / RATE)
        half = max(hz * width, 3 * RATE / (1 << 14))
        m = (f > hz - half) & (f < hz + half)
        out.append(20 * np.log10(s[m].max() + 1e-12))
    out = np.array(out)
    return out - out[0] if len(out) else out


def t60(x, hz, width=0.03, win=0.04):
    """A partial's -60 dB time, from a straight line through its band level from 3 dB
    under its start down to 45 dB under (or as far as it goes)."""
    lv = band_decay(x, hz, width, span=min(4.0, len(x) / RATE - win - 0.01), win=win)
    t = np.arange(len(lv)) * 0.01
    m = (lv < -3) & (lv > -45)
    if m.sum() < 3:
        m = lv > -45
    if m.sum() < 3:
        return float("nan")
    a, _ = np.polyfit(t[m], lv[m], 1)
    return 60 / -a if a < 0 else float("inf")


def summary(x):
    d20, d40 = decays(x)
    return (f"peak {peak_db(x):6.1f} dB  -20 {d20 * 1000:6.0f} ms  -40 {d40 * 1000:6.0f} ms  "
            f"centroid {centroid(x):6.0f} Hz")


# The P-I machines, their IDs and the screen design's defaults (docs/design), from
# which each control is swept on its own.
MACHINES = {
    "PI-BD": (64, [64, 50, 64, 64, 40, 0, 0, 0]),
    "PI-SD": (65, [64, 50, 64, 0, 40, 64, 40, 0]),
    "PI-MT": (66, [64, 50, 64, 64, 64, 0, 64, 0]),
    "PI-ML": (67, [64, 50, 64, 40, 0, 0, 0, 0]),
    "PI-MA": (68, [64, 50, 40, 0, 64, 64, 0, 0]),
    "PI-RS": (69, [64, 50, 64, 0, 64, 40, 0, 0]),
    "PI-RC": (70, [64, 50, 64, 0, 64, 0, 64, 127]),
    "PI-CC": (71, [64, 50, 64, 0, 64, 0, 64, 127]),
    "PI-HH": (72, [64, 50, 0, 0, 64, 0, 64, 127]),
}
V = [0, 16, 32, 48, 64, 80, 96, 112, 127]


def sweep_jobs(name, seconds=2.5, base=None, values=V):
    """Each control of `name` swept over `values` from `base` (the defaults), the
    rest held: [(job name, id, seconds, slots)]. Job names are pi-<machine>-<label>-<v>."""
    import sys
    import pathlib
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools"))
    import catalog
    ident, defaults = MACHINES[name]
    base = list(base or defaults)
    short = name.split("-")[1].lower()
    jobs = [(f"pi-{short}-default", ident, seconds, base)]
    for i, label in enumerate(catalog.MACHINES[name][2]):
        if not label:
            continue
        for v in values:
            p = list(base)
            p[i] = v
            jobs.append((f"pi-{short}-{label.lower()}-{v}", ident, seconds, p))
    return jobs


def fundamental(x, t0, t1, low=20, high=20000):
    """The strongest peak between `low` and `high` Hz over t0..t1, Hz."""
    f, s = spectrum(x, t0, t1)
    m = (f > low) & (f < high)
    return float(f[m][np.argmax(s[m])])


def report(hits, name, low=20, high=20000, early=(0.0, 0.03), late=(0.15, 0.35), count=5):
    """For each swept control: level, decays, brightness, the strongest peak early and
    late, and the strongest partials, at each value."""
    import pathlib
    import sys
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools"))
    import catalog
    short = name.split("-")[1].lower()
    print(f"######## {name}")
    print(f"  default: {summary(hits[f'pi-{short}-default'])}")
    for label in catalog.MACHINES[name][2]:
        if not label:
            continue
        print(f"== {label}")
        for v in V:
            x = hits.get(f"pi-{short}-{label.lower()}-{v}")
            if x is None:
                continue
            if np.abs(x).max() < 1e-4:
                print(f"  {v:3d}: silent")
                continue
            e = fundamental(x, *early, low, high)
            l = fundamental(x, *late, low, high)
            print(f"  {v:3d}: {summary(x)}  peak Hz {e:7.1f} -> {l:7.1f}  "
                  f"{' '.join(f'{f:.0f}/{d:.0f}' for f, d in partials(x, 0, 0.1, count))}")


def peak_track(x, low, high, win=0.025, hop=0.005, span=0.5):
    """(times, Hz, dB): the strongest peak between `low` and `high` Hz in windows of
    `win` seconds every `hop`, each time the window's centre from the onset."""
    x = x[start(x):]
    n = int(win * RATE)
    size = 1 << 16
    f = np.fft.rfftfreq(size, 1 / RATE)
    m = (f > low) & (f < high)
    w = np.hanning(n)
    times, hz, db = [], [], []
    for i in range(int(span / hop)):
        seg = x[int(i * hop * RATE):int(i * hop * RATE) + n]
        if len(seg) < n:
            break
        s = np.abs(np.fft.rfft(seg * w, size))[m]
        k = int(np.argmax(s))
        times.append(i * hop + win / 2)
        hz.append(float(f[m][k]))
        db.append(20 * np.log10(s[k] + 1e-12))
    return np.array(times), np.array(hz), np.array(db)


def mode_track(x, ratios, low, high, win=0.012, hop=0.004, span=0.4, steps=300):
    """(times, Hz): the frequency f between `low` and `high` whose modes f * ratios
    best explain each short window, by least squares over a grid of f (log steps).
    For a struck membrane whose modes all bend together with its tension."""
    x = x[start(x):]
    n = int(win * RATE)
    t = np.arange(n) / RATE
    grid = np.geomspace(low, high, steps)
    bases = []
    for f in grid:
        cols = []
        for r in ratios:
            cols += [np.cos(2 * np.pi * f * r * t), np.sin(2 * np.pi * f * r * t)]
        a = np.array(cols).T
        bases.append(a @ np.linalg.pinv(a))   # projection onto the modes
    times, hz = [], []
    for i in range(int(span / hop)):
        seg = x[int(i * hop * RATE):int(i * hop * RATE) + n]
        if len(seg) < n:
            break
        energy = [float(seg @ (p @ seg)) for p in bases]
        k = int(np.argmax(energy))
        times.append(i * hop + win / 2)
        hz.append(float(grid[k]))
    return np.array(times), np.array(hz)


# Our machines' output against the Machinedrum's: TRX-BD and TRX-SD at their
# defaults peak 1.5 to 2 dB above the emulator's main mix, so ours aim 2 dB above.
LEVEL_DB = 2.0


def table(md, ours, title, keys, f):
    """One measure `f` (a hit -> text) for each job, the Machinedrum's then ours."""
    print(f"== {title}")
    for k in keys:
        print(f"  {k:24s} {f(md[k]):>28s}   {f(ours[k]):>28s}")


def fit_main(name, jobs, analyse):
    """Renders `jobs` on both sides (ours skipped with --fit-only) and runs `analyse(md, ours)`."""
    import sys
    from probe import render_many, render_machine
    md = render_many(jobs)
    ours = md if "--fit-only" in sys.argv else render_machine(name, jobs)
    analyse(md, ours)


def band(x, low, high):
    """`x` from its onset with everything outside low..high Hz removed (FFT brickwall)."""
    x = x[start(x):]
    s = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / RATE)
    s[(f < low) | (f > high)] = 0
    return np.fft.irfft(s, len(x))


def burst(x, low=600, high=12000, ms=(0.5, 1, 2, 3, 5, 8, 12, 20)):
    """The strike's burst above `low` Hz: its rms in dB in windows ending at `ms`,
    and its frequency from zero crossings over the first 3 ms."""
    y = band(x, low, high)
    edges = [0] + [int(m * RATE / 1000) for m in ms]
    levels = [20 * np.log10(np.sqrt(np.mean(y[a:b] ** 2)) + 1e-9) for a, b in zip(edges, edges[1:])]
    seg = y[: int(0.003 * RATE)]
    crossings = np.sum(np.signbit(seg[1:]) != np.signbit(seg[:-1]))
    return levels, crossings * RATE / len(seg) / 2


def loudness(x, seconds=0.1):
    """The rms of the first `seconds` from the onset, dB: the level balance measure."""
    x = x[start(x):]
    return 20 * np.log10(np.sqrt(np.mean(x[: int(seconds * RATE)] ** 2)) + 1e-12)


def grains(x, ms=1.0, span=0.3):
    """A grainy sound's texture over its first `span` s: the envelope in `ms` windows,
    its peaks per second (grains heard) and its roughness (the envelope's spread, dB)."""
    x = x[start(x):]
    hop = int(ms / 1000 * RATE)
    n = int(span * RATE) // hop
    env = np.sqrt(np.mean(x[: n * hop].reshape(n, hop) ** 2, axis=1)) + 1e-9
    db = 20 * np.log10(env)
    trend = np.convolve(db, np.ones(15) / 15, mode="same")
    peaks = np.sum((db[1:-1] > db[:-2]) & (db[1:-1] > db[2:]) & (db[1:-1] > trend[1:-1] + 3))
    return peaks / span, float(np.std((db - trend)[7:-7]))
