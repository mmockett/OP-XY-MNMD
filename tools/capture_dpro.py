#!/usr/bin/env python3
"""Captures the Monomachine's DigiPRO waves and DPRO-BBOX's samples from the
owner's own ROM, through the emulator, for the DigiPRO engines (docs/FAMILIES.md).

    ../open-xy/.venv/bin/python tools/capture_dpro.py          # render (cached), verify, write
    ../open-xy/.venv/bin/python tools/capture_dpro.py --check  # verify the renders only

The ROM is MM_ROM, by default ~/Downloads/elektron_sfx6-60_os1.32b.bin.

Everything it renders and writes stays outside git: renders in local/capture/,
the result in ~/Library/Application Support/OPXY-MD/private/dpro/:

    private_dpro.hpp   the waves, which make_engine.py copies into each engine
    report.txt         what was captured and how well it verified

How a wave is captured: the machine holds one note, low enough that a cycle is
hundreds of samples, with the amp page held open (HOLD and DEC 127) and every
modulation off. The note's exact frequency is found from the render, and the
cycle is the least-squares fit of its harmonics up to 21 kHz over most of a
second, so noise and the onset fall away. Each wave is rendered at two notes an
octave apart: the two fits must agree (the check that a cycle is the wave and
not its playback), and the LCD read back after each note must show the wave
asked for (its number, and for D-waves its name). The playback's own response,
measured on D06 SAW (whose harmonics should fall as 1/k), is divided out.

The D-waves (D01..D64) are played through DPRO-DDRW, one cycle a note; the
S-sets (S01..S32) through DPRO-WAVE, whose tables hold two cycles, at WP 0 and
at the WP steps in S_STEPS (WP moves each set towards the next: WP 127 is the
next set, which the capture checks). Notes on what each control was found to do
are in reference/README.md.
"""
import argparse
import json
import os
import pathlib
import subprocess
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
CACHE = ROOT / "local" / "capture" / "dpro"
PRIVATE = pathlib.Path.home() / "Library" / "Application Support" / "OPXY-MD" / "private" / "dpro"
MM_ROM = pathlib.Path(os.environ.get("MM_ROM", pathlib.Path.home() / "Downloads" / "elektron_sfx6-60_os1.32b.bin"))
RATE = 44100

# Machine IDs (tools/catalog.py) and the amp page held open: CC 57 HOLD, 58 DEC.
DPRO_WAVE, DPRO_BBOX, DPRO_DDRW = 6, 7, 32
HELD = "cc57=127 cc58=127"


# ---- the emulator -----------------------------------------------------------

def run(jobs, lcd=False):
    """jobs: [(name, machine, seconds, [8 values], options)] -> {name: samples}.
    Rendered once and cached by settings; with lcd, each job's LCD is kept as
    <name>.txt (mdref's 128 x 64 dump) and read back with the render."""
    CACHE.mkdir(parents=True, exist_ok=True)
    index_path = CACHE / "index.json"
    index = json.loads(index_path.read_text()) if index_path.exists() else {}
    todo = []
    for name, machine, seconds, values, options in jobs:
        key = [machine, seconds, list(values), options, lcd]
        if index.get(name) != key or not (CACHE / f"{name}.wav").exists():
            todo.append(f"{name} {machine} {seconds} " + " ".join(str(int(v)) for v in values) + (" " + options if options else ""))
            if lcd:
                todo.append(f"lcd {name}")
            index[name] = key
    if todo:
        mdref = ROOT / "local" / "mdref"
        if not mdref.exists():
            subprocess.run(["sh", str(ROOT / "reference" / "build.sh")], check=True)
        print(f"rendering {sum(1 for t in todo if not t.startswith('lcd'))} notes through the emulator", flush=True)
        subprocess.run([str(mdref), str(MM_ROM), str(CACHE)], input="\n".join(todo) + "\n", text=True, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        index_path.write_text(json.dumps(index))
    return {j[0]: np.frombuffer((CACHE / f"{j[0]}.wav").read_bytes()[44:], dtype="<f4").astype(np.float64) for j in jobs}


# ---- reading the LCD --------------------------------------------------------

# The Monomachine's 3 x 5 type as its LCD shows it, rows top to bottom (read
# off the emulated LCD). Numbers use the same shapes: S is 5 there, O is 0.
FONT = {
    "###/#.#/###/#.#/#.#": "A", "##./#.#/##./#.#/##.": "B", "###/#.#/#../#.#/###": "C", "##./#.#/#.#/#.#/##.": "D",
    "###/#../###/#../###": "E", "###/#../###/#../#..": "F", ".##/#../#.#/#.#/.##": "G", "#.#/#.#/###/#.#/#.#": "H",
    "###/.#./.#./.#./###": "I", "#../#../#../#../###": "L", "#.#/###/#.#/#.#/#.#": "M", "##./#.#/#.#/#.#/#.#": "N",
    "###/#.#/#.#/#.#/###": "O", "##./#.#/##./#../#..": "P", ".#./#.#/#.#/#.#/.##": "Q", "###/#.#/##./#.#/#.#": "R",
    "###/#../###/..#/###": "S", "###/.#./.#./.#./.#.": "T", "#.#/#.#/#.#/#.#/###": "U", "#.#/#.#/#.#/###/#.#": "W",
    "#.#/#.#/###/..#/###": "Y", "###/..#/.#./#../###": "Z", ".#./.#./#.#/###/#.#": "Å", "#.#/.#./#.#/###/#.#": "Ä",
    "#.#/###/#.#/#.#/###": "Ö", "##./.#./.#./.#./###": "1", "###/..#/###/#../###": "2", "###/..#/###/..#/###": "3",
    "#.#/#.#/###/..#/..#": "4", "###/#../###/#.#/###": "6", "###/..#/..#/..#/..#": "7", "###/#.#/###/#.#/###": "8",
    "###/#.#/###/..#/..#": "9", ".../.../.../.../...": " ",
}
DIGIT = {"S": "5", "O": "0"}


def lcd(name):
    rows = [l.rstrip("\n") for l in open(CACHE / f"{name}.txt") if l.strip()]
    return np.array([[c == "#" for c in r] for r in rows], dtype=bool)


def glyph(grid, x, y):
    return "/".join("".join("#" if grid[y + r, x + c] else "." for c in range(3)) for r in range(5))


def field(grid, x0, x1, y, number=False):
    """The text of one line of a parameter cell, columns x0..x1, glyph tops at row y:
    the LCD centres it, so it starts at the first lit column. ? marks a glyph not in FONT."""
    cols = [x for x in range(x0, x1) if grid[y:y + 5, x].any()]
    if not cols:
        return ""
    chars = "".join(FONT.get(glyph(grid, cols[0] + 4 * i, y), "?") for i in range((cols[-1] - cols[0] + 2) // 4))
    return "".join(DIGIT.get(ch, ch) for ch in chars) if number else chars


# The cells' columns on the SYN page (128 x 64 LCD): a name line and a number line.
CELL_X = [(49, 69), (69, 89), (89, 109), (109, 129)]
NAME_Y, NUMBER_Y = 12, 26


def shown_wave(name, cell):
    """(name, number) shown in a SYN-page cell after job `name`, e.g. ("SINE", "D01")."""
    g = lcd(name)
    x0, x1 = CELL_X[cell]
    return field(g, x0, x1, NAME_Y), field(g, x0, x1, NUMBER_Y)[:1] + field(g, x0, x1, NUMBER_Y, True)[1:]


# ---- fitting a cycle ----------------------------------------------------------

def frequency(x, guess):
    """The frequency of a held note near `guess`, to a few parts per million: found
    on the strongest of its first three harmonics, as a table may hold no first."""
    t = np.arange(len(x)) / RATE
    w = np.hanning(len(x))
    found = []
    for k in (1, 2, 3):
        best, power = guess * k, 0.0
        for span in (0.03, 0.003, 0.0003, 0.00003):
            fs = np.linspace(best * (1 - span), best * (1 + span), 61)
            p = [abs(np.sum(w * x * np.exp(-2j * np.pi * f * t))) for f in fs]
            best, power = fs[int(np.argmax(p))], max(p)
        found.append((power, best / k))
    return max(found)[1]


def harmonics(x, f, count):
    """Complex amplitudes of harmonics 1..count of f in x (Hann-weighted projection)."""
    t = np.arange(len(x)) / RATE
    w = np.hanning(len(x))
    s = w.sum()
    return np.array([2 * np.sum(w * x * np.exp(-2j * np.pi * f * k * t)) / s for k in range(1, count + 1)])


def held(x, start=0.15, end=1.05):
    return x[int(start * RATE):int(end * RATE)]


def note_hz(note, octave=0):
    """The Monomachine's note at A440, `octave` octaves up (each machine has its own)."""
    return 440 * 2 ** ((note - 69) / 12 + octave)


TABLE = 512                      # samples a stored table: up to 255 harmonics
D_WAVES, S_SETS = 64, 32
S_STEPS = [0, 16, 48, 80, 112]   # WP where each S-set is captured; WP 127 is the next set
LOW, HIGH = 24, 36               # the two notes each wave is fitted at


def jobs():
    """Every note the capture renders: (name, machine, seconds, slots, options)."""
    hold = f"{HELD} gate=1.1"
    out = []
    for n in (LOW, HIGH):
        # D-waves through DPRO-DDRW: WAV1 = WAV2 = the wave, MIX 0, BR1 BR2 0, WID centred.
        out += [(f"d{w:02d}n{n}", DPRO_DDRW, 1.2, [2 * w, 0, 2 * w, 0, 0, 64, 0, 64], f"note={n} {hold}") for w in range(D_WAVES)]
        # S-sets through DPRO-WAVE at WP 0, no WPM, sync off.
        out += [(f"s{k:02d}n{n}", DPRO_WAVE, 1.2, [4 * k + 1, 0, 0, 0, 0, 0, 0, 64], f"note={n} {hold}") for k in range(S_SETS)]
    out += [(f"s{k:02d}wp{wp}n{LOW}", DPRO_WAVE, 1.2, [4 * k + 1, wp, 0, 0, 0, 0, 0, 64], f"note={LOW} {hold}")
            for k in range(S_SETS) for wp in S_STEPS[1:] + [127]]
    return out


SAW = 5      # D06, whose harmonics are 1/k: the reference for the playback's response
RND = 7      # D08, noise that drifts from cycle to cycle: the modules make it, so it is not stored


def response(renders):
    """The playback's gain at a frequency, from D06 SAW at the low note (its kth
    harmonic is 1/k of the first): a function of Hz, 1 below 500 Hz. (GND-SIN's
    level up the keyboard falls far faster above 3 kHz, so it is not the DigiPRO's path.)"""
    x = held(renders[f"d{SAW:02d}n{LOW}"])
    f = frequency(x, note_hz(LOW))
    count = int(20000 / f)
    m = np.abs(harmonics(x, f, count)) * np.arange(1, count + 1)
    m = np.convolve(m, np.ones(9) / 9, mode="same")     # smooth over neighbouring harmonics
    fs = f * np.arange(1, count + 1)
    m /= np.median(m[(fs > 100) & (fs < 500)])
    keep = slice(4, count - 4)
    return lambda hz: np.interp(hz, fs[keep], m[keep]), (fs[keep], m[keep])


def cycle(x, guess, gain, size=TABLE):
    """One period of a held note as `size` samples, its harmonics corrected by the
    track's gain (at most 12 dB), and the fraction of its power the harmonics miss."""
    seg = held(x)
    f = frequency(seg, guess)
    count = min(size // 2 - 1, int(20000 / f))
    h = harmonics(seg, f, count)
    t = np.arange(len(seg)) / RATE
    k = np.arange(1, count + 1)
    fitted = np.real(np.exp(2j * np.pi * f * np.outer(t, k)) @ h)
    missed = np.mean((seg - fitted) ** 2) / np.mean(seg ** 2)
    h = h / np.maximum(gain(k * f), 0.25)
    u = np.arange(size) / size
    return np.real(np.exp(2j * np.pi * np.outer(u, k)) @ h), f, missed


def canonical(table, cycles=1):
    """`table` turned so that the note's first harmonic rises through zero at its
    start. The original's oscillators run freely from note to note, but every
    D-wave's first harmonic sits at the same phase against its table's start (the
    phases measured at note-on fall on one line set by the order the notes were
    played in), so this lines the tables up as the original's are: DDRW's
    crossfade depends on it. An S-set holds two cycles: of its two turns, the one
    whose own first harmonic also rises at the start."""
    spectrum = np.fft.rfft(table)
    k = np.arange(len(spectrum))
    best = None
    for half in range(cycles):
        shift = (-np.pi / 2 - np.angle(spectrum[cycles])) / (2 * np.pi * cycles) + half / cycles   # in tables
        turned = np.fft.irfft(spectrum * np.exp(2j * np.pi * k * shift), len(table))
        score = np.real(np.fft.rfft(turned)[1] * 1j) if cycles > 1 else 0
        if best is None or score > best[0]:
            best = (score, turned)
    return best[1]


def align(table, reference):
    """`table` turned (circularly) to match `reference` best, and that match's correlation."""
    x = np.fft.ifft(np.fft.fft(reference) * np.conj(np.fft.fft(table))).real
    i = int(np.argmax(x))
    return np.roll(table, i), x[i] / np.sqrt((table ** 2).sum() * (reference ** 2).sum())


def fix_names(names):
    """The LCD draws 5 and S alike: SYNS after SYN4 is SYN5."""
    out = list(names)
    for i in range(1, len(out)):
        if out[i][-1:] == "S" and out[i - 1][-1:] == "4" and out[i][:-1] == out[i - 1][:-1]:
            out[i] = out[i][:-1] + "5"
    return out


def capture():
    """Renders, fits and verifies every wave -> (d tables, s tables, names, report lines, failures)."""
    renders = run(jobs(), lcd=True)
    gain, (fs, gains) = response(renders)
    report = ["The playback's response (D06 SAW against 1/k): " + ", ".join(
        f"{hz} Hz {20 * np.log10(gain(hz)):+.1f} dB" for hz in (1000, 2000, 4000, 6000, 8000, 10000, 12000))]
    failures = []
    # D-waves: one cycle a note.
    d_tables, names = [], []
    for w in range(D_WAVES):
        shown = [shown_wave(f"d{w:02d}n{n}", c) for n in (LOW, HIGH) for c in (0, 2)]
        if any(s[1].strip() != f"D{w + 1:02d}" for s in shown) or len({s[0] for s in shown}) != 1:
            failures.append(f"D{w + 1:02d}: the LCD showed {shown}")
        names.append(shown[0][0].strip())
        low, f, missed = cycle(renders[f"d{w:02d}n{LOW}"], note_hz(LOW), gain)
        high, _, _ = cycle(renders[f"d{w:02d}n{HIGH}"], note_hz(HIGH), gain)
        _, agree = align(high, low)
        if w == RND:
            x = held(renders[f"d{w:02d}n{LOW}"])
            period = int(round(RATE / f))
            agree = np.corrcoef(x[:-period], x[period:])[0, 1]   # its drift: each cycle against the last
            low = np.zeros(TABLE)
        elif agree < 0.99:
            failures.append(f"D{w + 1:02d}: the two notes' cycles agree to only {agree:.3f}")
        d_tables.append(canonical(low))
        report.append(f"D{w + 1:02d} {shown[0][0]}  f {f:.3f} Hz  notes agree {agree:.4f}  outside the harmonics {10 * np.log10(missed + 1e-12):.0f} dB")
    names = fix_names(names)
    # S-sets: two cycles a table, at each WP step, turned to follow on from the step before.
    s_tables = []
    for k in range(S_SETS):
        for n in (LOW, HIGH):
            shown = shown_wave(f"s{k:02d}n{n}", 0)
            if shown[1].strip() != f"S{k + 1:02d}":
                failures.append(f"S{k + 1:02d}: the LCD showed {shown}")
        low, f, missed = cycle(renders[f"s{k:02d}n{LOW}"], note_hz(LOW, -1), gain)
        high, _, _ = cycle(renders[f"s{k:02d}n{HIGH}"], note_hz(HIGH, -1), gain)
        _, agree = align(high, low)
        if agree < 0.97:   # the brightest sets alias more at the higher note
            failures.append(f"S{k + 1:02d}: the two notes' tables agree to only {agree:.3f}")
        # Each set turned to follow on from the last set's last step, and each
        # step from the one before, so the module's crossfades along WP never cancel.
        steps = [canonical(low, 2) if not s_tables else align(low, s_tables[-1][-1])[0]]
        for wp in S_STEPS[1:]:
            t, _, _ = cycle(renders[f"s{k:02d}wp{wp}n{LOW}"], note_hz(LOW, -1), gain)
            steps.append(align(t, steps[-1])[0])
        s_tables.append(steps)
        report.append(f"S{k + 1:02d}  f {f:.3f} Hz  notes agree {agree:.4f}  outside the harmonics {10 * np.log10(missed + 1e-12):.0f} dB")
    # WP 127 is the next set at WP 0: a check on the order of the sets. S32 has no
    # next set: what it reaches at 127 is stored as its own last step.
    for k in range(S_SETS - 1):
        end, _, _ = cycle(renders[f"s{k:02d}wp127n{LOW}"], note_hz(LOW, -1), gain)
        _, same = align(end, s_tables[k + 1][0])
        report.append(f"S{k + 1:02d} at WP 127 against S{k + 2:02d} at WP 0: {same:.4f}")
        if same < 0.96:
            failures.append(f"S{k + 1:02d}: WP 127 is not the next set ({same:.3f})")
    end, _, _ = cycle(renders[f"s{S_SETS - 1:02d}wp127n{LOW}"], note_hz(LOW, -1), gain)
    last = align(end, s_tables[-1][-1])[0]
    _, wraps = align(end, s_tables[0][0])
    report.append(f"S32 at WP 127 against S01 at WP 0: {wraps:.4f} (stored as S32's own end)")
    return d_tables, s_tables + [[last]], names, report, failures


# ---- DPRO-BBOX's samples --------------------------------------------------------

# 24 samples on 24 keys from C, repeating every two octaves; each key also raises
# the pitch a quarter semitone, so 24 keys up plays the same sample 6 semitones
# higher (measured). Sample i is key n with n % 24 == i. Stored at the rate they
# play at note 60 (their 1.0 here), each captured on its key in 36..59, where it
# plays at or below that rate, and checked against the same key two octaves down.
# Above note 60 the original stops behaving as a resampler (key 20 on note 68
# repeats exactly hit to hit yet matches note 44 only 0.33), so nothing is taken from there.
# The original does not only resample: two octaves down a sample's body is
# smoother and its tail longer (in the sample's own time), so the check is on
# the first 50 ms below 2 kHz, where the two must still agree.
BBOX = 24
BBOX_TRIM = 10 ** (-60 / 20)     # a sample ends where it stays 60 dB under its peak


def bbox_note(i):
    return 36 + (i - 12) % 24


def bbox_rate(note):
    return 2 ** ((note - 60) / 48)


def bbox_jobs():
    hold = f"{HELD} gate=0.9"
    out = [(f"bb{i:02d}n{n}", DPRO_BBOX, 1.0, [64, 0, 0, 0, 0, 64, 0, 0], f"note={n} {hold}")
           for i in range(BBOX) for n in (bbox_note(i), bbox_note(i) - 24)]
    # Each key a second time: a sample whose two hits differ makes its noise as it plays.
    return out + [(f"bb{i:02d}n{bbox_note(i)}again", DPRO_BBOX, 1.0, [64, 0, 0, 0, 0, 64, 0, 0], f"note={bbox_note(i)} {hold}")
                  for i in range(BBOX)]


def native(x, rate):
    """A render of a sample playing at `rate` (from the note's onset to where it
    stays 60 dB down) as the sample at rate 1: band-limited resampling, 4 x
    oversampled (zero-padded, so nothing wraps round) then interpolated."""
    a = np.abs(x)
    start = max(0, int(np.argmax(a > a.max() * 1e-3)) - 4)
    end = int(np.where(a > a.max() * BBOX_TRIM)[0][-1]) + 16
    x = x[start:end]
    n = len(x)
    up = np.fft.irfft(np.fft.rfft(x, 2 * n), 8 * n)[:4 * n] * 4
    positions = np.arange(0, (n - 1) * rate, 1.0) / rate * 4
    return np.interp(positions, np.arange(4 * n), up)


def waveform_agree(low, lower):
    """How alike two captures of one sample are over their first 50 ms below 2 kHz,
    within 2 ms of each other (the onsets are found to within a few frames)."""
    m = min(len(low), len(lower), RATE // 20)
    a, b = np.fft.rfft(low[:m], 2 * m), np.fft.rfft(lower[:m], 2 * m)
    band = np.fft.rfftfreq(2 * m, 1 / RATE) > 2000
    a[band] = 0
    b[band] = 0
    x = np.fft.irfft(a * np.conj(b))
    x = np.concatenate([x[-88:], x[:89]])
    return x.max() / np.sqrt(np.fft.irfft(a * np.conj(a))[0] * np.fft.irfft(b * np.conj(b))[0])


def bands_agree(a, b):
    """Correlation of two sounds' levels in third-octave bands, 100 Hz to 12 kHz (dB)."""
    m = min(len(a), len(b))
    fa, fb = np.abs(np.fft.rfft(a[:m])) ** 2, np.abs(np.fft.rfft(b[:m])) ** 2
    hz = np.fft.rfftfreq(m, 1 / RATE)
    edges = 100 * 2 ** (np.arange(0, 21) / 3)
    la = [10 * np.log10(fa[(hz >= lo) & (hz < hi)].sum() + 1e-12) for lo, hi in zip(edges[:-1], edges[1:])]
    lb = [10 * np.log10(fb[(hz >= lo) & (hz < hi)].sum() + 1e-12) for lo, hi in zip(edges[:-1], edges[1:])]
    return np.corrcoef(la, lb)[0, 1]


def capture_bbox():
    """-> (samples at rate 1, report lines, failures)."""
    renders = run(bbox_jobs(), lcd=True)
    report, failures, samples = [], [], []
    label = None
    for i in range(BBOX):
        n = bbox_note(i)
        low = native(renders[f"bb{i:02d}n{n}"], bbox_rate(n))
        lower = native(renders[f"bb{i:02d}n{n - 24}"], bbox_rate(n - 24))
        again = native(renders[f"bb{i:02d}n{n}again"], bbox_rate(n))
        agree, kind = waveform_agree(low, lower), "waveform two octaves down"
        if agree < 0.8:
            # Noise in a sample is not resampled as it is played: it must agree in
            # its spectrum, a third-octave at a time.
            again_agree = waveform_agree(low, again)
            agree = bands_agree(low, lower)
            kind = (f"spectrum two octaves down (a second hit's waveform agrees {again_agree:.2f}: "
                    f"{'its noise differs every hit' if again_agree < 0.9 else 'it repeats'})")
            if agree < 0.9:
                failures.append(f"BBOX {i:02d}: its two notes agree to only {agree:.3f}")
        a = np.abs(low)
        sample = low.copy()
        sample[-64:] *= np.linspace(1, 0, 64)
        samples.append(sample)
        # The machine on the LCD: every BBOX note shows the same machine label.
        g = lcd(f"bb{i:02d}n{n}")[54:63, 0:47]
        label = g if label is None else label
        if not np.array_equal(g, label):
            failures.append(f"BBOX {i:02d}: the LCD shows another machine")
        report.append(f"BBOX {i:02d} (key {n % 24}) {len(sample) / RATE * 1000:.0f} ms, peak {a.max():.3f}, "
                      f"notes agree {agree:.4f} by {kind}")
    return samples, report, failures


# Captured levels: a BBOX int16 is the emulator's level x 2^15; the module reads it x 2^16.
BBOX_TO_INT16 = 2 ** 15


def bbox_header(samples):
    q = [np.clip(np.round(x * BBOX_TO_INT16), -32767, 32767).astype(int) for x in samples]
    starts = np.cumsum([0] + [len(x) for x in q])
    return f"""#pragma once
// Generated by tools/capture_dpro.py from the owner's own Monomachine ROM, through
// the emulator: DPRO-BBOX's 24 samples. Private: never commit or share it
// (docs/FAMILIES.md).
#include <opxy/types.hpp>

namespace md {{
namespace bbox_data {{
const bool captured = true;
const opxy::u32 samples = {BBOX};
const opxy::u32 frames = {int(starts[-1])};
// Each sample's first frame and length, at the rate it plays at note 60.
static const opxy::u32 start[{BBOX}] = {{{", ".join(str(int(v)) for v in starts[:-1])}}};
static const opxy::u32 length[{BBOX}] = {{{", ".join(str(len(x)) for x in q)}}};
static const opxy::i16 data[{int(starts[-1])}] = {{
{rows([v for x in q for v in x])}
}};
}}
}}
"""


# ---- the header ---------------------------------------------------------------

# Captured levels to the module's: a table's int16 is the emulator's level x 2^16, so
# the module's output (int16 x 2^15, Q31) is the emulator's level, and the
# loudest wave (D05 SQR, peaking near 0.45) stays inside the SDK's half scale.
TO_INT16 = 2 ** 16


def rows(values, per=16):
    return ",\n".join("    " + ", ".join(str(int(v)) for v in values[i:i + per]) for i in range(0, len(values), per))


def braced(tables):
    """Tables as rows of an array of arrays, each in its own braces."""
    return ",\n".join("{\n" + rows(t) + "}" for t in tables)


def norm(table):
    return int(round(2 ** 30 / max(int(np.abs(table).max()), 256)))


def header(d_tables, s_tables, names):
    q = lambda t: np.clip(np.round(np.asarray(t) * TO_INT16), -32767, 32767).astype(int)
    d = [q(t) for t in d_tables]
    s = [[q(t) for t in steps] for steps in s_tables[:-1]]
    end = q(s_tables[-1][0])
    s_rows = ",\n".join("{" + braced(steps) + "}" for steps in s)
    return f"""#pragma once
// Generated by tools/capture_dpro.py from the owner's own Monomachine ROM, through
// the emulator. Private: never commit or share it (docs/FAMILIES.md).
#include <opxy/types.hpp>

namespace md {{
namespace dpro_data {{
const bool captured = true;
const opxy::u32 table_bits = 9;                 // {TABLE} samples a table
const opxy::u32 d_waves = {D_WAVES}, s_waves = {S_SETS};
const opxy::u32 s_steps = {len(S_STEPS)};                   // WP {", ".join(map(str, S_STEPS))}; 127 is the next set
// The waves are a template's static members, which the linker keeps once for
// every translation unit and engine that includes this header (a static array
// would be copied into each).
template <int> struct waves {{
    static const opxy::i16 d[{D_WAVES}][{TABLE}];
    static const opxy::i16 s[{S_SETS}][{len(S_STEPS)}][{TABLE}];
    static const opxy::i16 end[{TABLE}];
}};
// D01..D64: {", ".join(names)}
template <int N> const opxy::i16 waves<N>::d[{D_WAVES}][{TABLE}] = {{
{braced(d)}
}};
// S01..S32, each at its WP steps: two cycles a table.
template <int N> const opxy::i16 waves<N>::s[{S_SETS}][{len(S_STEPS)}][{TABLE}] = {{
{s_rows}
}};
// Where S32 arrives at WP 127 (the others arrive at the next set).
template <int N> const opxy::i16 waves<N>::end[{TABLE}] = {{
{rows(end)}
}};
static const opxy::i16 (&d_table)[{D_WAVES}][{TABLE}] = waves<0>::d;
static const opxy::i16 (&s_table)[{S_SETS}][{len(S_STEPS)}][{TABLE}] = waves<0>::s;
static const opxy::i16 (&s_end)[{TABLE}] = waves<0>::end;
// For the views: 2^30 / each table's peak, so md::multiply(sample x 65536, norm) is Q15 at its peak.
static const opxy::i32 d_norm[{D_WAVES}] = {{{", ".join(str(norm(t) if w != RND else 65536) for w, t in enumerate(d))}}};
static const opxy::i32 s_norm[{S_SETS}] = {{{", ".join(str(norm(steps[0])) for steps in s)}}};
// Each D-wave's peak, for DDRW's bit reduction, which works on the wave's own scale
// (RND's, made by the modules, peaks at 16384).
static const opxy::i16 d_peak[{D_WAVES}] = {{{", ".join(str(max(int(np.abs(t).max()), 256) if w != RND else 16384) for w, t in enumerate(d))}}};
}}
}}
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="verify the renders without writing the header")
    args = ap.parse_args()
    if not MM_ROM.exists():
        sys.exit(f"needs the owner's Monomachine ROM at {MM_ROM}")
    d_tables, s_tables, names, report, failures = capture()
    peak = max(np.abs(np.array(d_tables)).max(), max(np.abs(t).max() for steps in s_tables for t in steps)) * TO_INT16
    report.append(f"largest sample {peak:.0f} of 32767")
    if peak > 32767:
        failures.append(f"a table peaks at {peak:.0f}, beyond int16 at this scale")
    samples, bbox_report, bbox_failures = capture_bbox()
    report += bbox_report
    failures += bbox_failures
    report += [f"FAIL {f}" for f in failures]
    print("\n".join(report[-12:]))
    if failures:
        sys.exit(f"{len(failures)} checks failed: nothing written")
    if args.check:
        return
    PRIVATE.mkdir(parents=True, exist_ok=True)
    (PRIVATE / "private_dpro.hpp").write_text(header(d_tables, s_tables, names))
    (PRIVATE / "private_bbox.hpp").write_text(bbox_header(samples))
    (PRIVATE / "report.txt").write_text("\n".join(report) + "\n")
    size = (D_WAVES + S_SETS * len(S_STEPS) + 1) * TABLE * 2
    frames = sum(len(x) for x in samples)
    print(f"wrote {PRIVATE / 'private_dpro.hpp'}: {size / 1024:.0f} KiB of tables "
          f"({D_WAVES * TABLE * 2 / 1024:.0f} KiB of D-waves, {S_SETS * len(S_STEPS) * TABLE * 2 / 1024:.0f} KiB of S-sets)")
    print(f"wrote {PRIVATE / 'private_bbox.hpp'}: {frames * 2 / 1024:.0f} KiB of samples ({frames / RATE:.2f} s)")


if __name__ == "__main__":
    main()
