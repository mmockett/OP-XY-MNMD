#!/usr/bin/env python3
"""Captures the Machinedrum's sixteen E12 samples from the owner's own ROM,
through the emulator, for the mde12 kit (docs/FAMILIES.md).

    ../open-xy/.venv/bin/python tools/capture_e12.py [--check]

Each E12 machine is played once with its controls where they leave the sample
as it is: PTCH 64 (its own speed), DEC 127 (no decay), HP 0, no start offset,
retrig or bend, and its own control at its neutral end (NEUTRAL). Then the
capture is checked: the same hit played again, and each control one step in
from its neutral end, must sound the same; and track volume must only scale
it. Six machines crossfade to a second sample with their own control (SECOND):
that sample is captured at 127, and a setting between must be exactly a mix of
the two. And STRT's step, which differs from machine to machine, is measured
at STRT 64 and checked at 32. The verdicts are printed; a capture that fails
them is not written.

Writes to ~/Library/Application Support/OPXY-MD/private/e12/ (never to the
repository):

    renders/          the emulator's hits, 32-bit float WAVs
    <name>.wav        each trimmed sample, as captured (<name>-b.wav, the second)
    private_e12.hpp   the samples for voices/e12.hpp: 12 bits each, as the
                      original stores them, in the shape of
                      voices/silent/private_e12.hpp

tools/make_engine.py copies the header into machines/mde12/, where git ignores
it. The ROM is MD_ROM, by default ~/Downloads/elektron_sps1-1uw_os1.63.bin.
Hits already rendered are kept (--fresh renders them all again); --check
re-measures the last renders without rendering.
"""
import argparse
import os
import pathlib
import subprocess
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import catalog  # noqa: E402

PRIVATE = pathlib.Path.home() / "Library" / "Application Support" / "OPXY-MD" / "private" / "e12"
ROM = os.environ.get("MD_ROM", str(pathlib.Path.home() / "Downloads" / "elektron_sps1-1uw_os1.63.bin"))
RATE = 44100
NAMES = [n for n in catalog.MACHINES if n.startswith("E12-")]   # ids 48..63, in order
SECONDS = 2.5

# Each machine's own control (slot 4) where it leaves the sample alone: SNAP
# (BD) does nothing while SPLN is 0, HPQ is off at 0, STOP (OH) cuts the sample
# short below 127. RING, RATL, BELL, REAL, SLEW and BC crossfade from the
# machine's sample at 0 to a second one at 127 (SECOND).
NEUTRAL = {"E12-OH": 127}
SECOND = ["E12-SD", "E12-RS", "E12-RC", "E12-BR", "E12-SH", "E12-BC"]
# Where the mix is checked: RATL crossfades late (reference/README.md).
MIX = {"E12-RS": 120}
# The samples in private_e12.hpp's order: each machine's own (ids 48..63), then the second ones.
SAMPLES = [(n, "") for n in NAMES] + [(n, "b") for n in SECOND]
# Our level against the emulator's main mix at track volume 100: E12-BD then
# peaks with our TRX-BD, as the Machinedrum's own TRX-BD and E12-BD do; the
# kit keeps the Machinedrum's balance between its machines.
LEVEL = 1.2
# The end of a sample: where it stays this far below its peak (12 bits give 72 dB).
FLOOR_DB = 66
FADE = 64
# One step in from a neutral end may move the sound this much (0 is the same, 1 unrelated).
STEP_LIMIT = 0.1   # HPQ's first step is the largest: 0.09


def raw(name):
    p = [64, 127, 0, NEUTRAL.get(name, 0), 0, 0, 64, 64]
    return p


def jobs():
    """(job name, machine, values, options): the capture, then its checks."""
    out = []
    for name in NAMES:
        p = raw(name)
        key = name.lower()
        out.append((f"{key}-raw", name, p, ""))
        out.append((f"{key}-again", name, p, ""))
        out.append((f"{key}-vol", name, p, "cc33=110"))
        # STRT, from the longer sample (the second, where there is one).
        for v in (32, 64):
            q = list(p)
            q[3] = 127 if name in SECOND else q[3]
            q[4] = v
            out.append((f"{key}-strt{v}", name, q, ""))
        if name in SECOND:
            for v in (127, MIX.get(name, 48)):
                q = list(p)
                q[3] = v
                out.append((f"{key}-{'b' if v == 127 else 'mix'}", name, q, ""))
        # One step in from each neutral end.
        # (STRT moves where the sample begins, so one step of it is not compared.)
        for slot, step in ((1, -1), (2, 1), (3, 1 if p[3] == 0 else -1), (5, 1)):
            q = list(p)
            q[slot] += step
            out.append((f"{key}-{catalog.MACHINES[name][2][slot].lower()}", name, q, ""))
    return out


def render(todo, fresh):
    renders = PRIVATE / "renders"
    renders.mkdir(parents=True, exist_ok=True)
    mdref = ROOT / "local" / "mdref"
    if not mdref.exists():
        subprocess.run(["sh", str(ROOT / "reference" / "build.sh")], check=True)
    todo = [t for t in todo if fresh or not (renders / f"{t[0]}.wav").exists()]
    if not todo:
        return
    lines = [f"{job} {catalog.MACHINES[name][1]} {SECONDS} " + " ".join(str(v) for v in p) + (f" {opt}" if opt else "")
             for job, name, p, opt in todo]
    subprocess.run([str(mdref), ROM, str(renders)], input="\n".join(lines) + "\n", text=True, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def read(job):
    return np.frombuffer((PRIVATE / "renders" / f"{job}.wav").read_bytes()[44:], dtype="<f4").astype(np.float64)


def trimmed(x):
    """From the first sound (FLOOR_DB under the peak) to where it stays that far under, faded out over FADE samples."""
    floor = np.abs(x).max() * 10 ** (-FLOOR_DB / 20)
    loud = np.nonzero(np.abs(x) > floor)[0]
    x = x[max(0, loud[0] - 1):min(len(x), loud[-1] + FADE)].copy()
    x[-FADE:] *= np.linspace(1, 0, FADE)
    return x


def aligned(a, b):
    """a and b from their starts, b moved by whole samples to match a best, cut to the same length."""
    a, b = trimmed(a), trimmed(b)
    n = min(len(a), len(b), 4096)
    shifts = range(-32, 33)
    best = max(shifts, key=lambda k: np.dot(a[32:n - 32], b[32 + k:n - 32 + k]))
    b = b[best:] if best >= 0 else np.concatenate([np.zeros(-best), b])
    n = min(len(a), len(b))
    return a[:n], b[:n]


def difference(a, b):
    """How unlike two hits are, after aligning their starts and matching levels: 0 is the same."""
    a, b = aligned(a, b)
    g = np.dot(a, b) / np.dot(a, a)
    return float(np.linalg.norm(b - g * a) / np.linalg.norm(b)), float(g)


def write_wav(path, x):
    data = x.astype("<f4").tobytes()
    import struct
    head = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 3, 1, RATE, RATE * 4, 4, 32)
    path.write_bytes(head + b"data" + struct.pack("<I", len(data)) + data)


def header(samples, steps):
    """private_e12.hpp: every sample in 12 bits, the top 8 in `high`, the low 4 in `low` (two to a byte),
    and each machine's STRT step (Q8 frames)."""
    codes, start, level = [], [0], []
    for x in samples:
        peak = np.abs(x).max()
        q = np.clip(np.round(x / peak * 2047), -2048, 2047).astype(np.int64)
        codes.append(q)
        start.append(start[-1] + len(q))
        # The voice plays code * 2^19 (Q31, half of full scale) times level: the
        # emulator's peak, at our scale.
        level.append(int(round(2 * peak * LEVEL / (2047 / 2048) * 2 ** 31)))
    q = np.concatenate(codes + [np.zeros(1, np.int64)])   # one more, for the interpolation past the end
    u = q & 0xfff
    high = (u >> 4).astype(np.int64)
    high = np.where(high >= 128, high - 256, high)
    nib = (u & 15).astype(np.int64)
    if len(nib) % 2:
        nib = np.append(nib, 0)
    low = nib[0::2] | (nib[1::2] << 4)
    assert max(level) < 2 ** 31

    def rows(values):
        values = [str(int(v)) for v in values]
        return "\n".join("    " + ", ".join(values[i:i + 24]) + "," for i in range(0, len(values), 24))

    return f"""#pragma once
// The Machinedrum's E12 samples, captured from the owner's own ROM through the
// emulator by tools/capture_e12.py. Private: never commit or share this file.
// Frames: {", ".join(f"{n}{'-' + b if b else ''} {len(c)}" for (n, b), c in zip(SAMPLES, codes))};
// {len(q)} in all, {len(high) + len(low)} bytes.
#include <opxy/types.hpp>

namespace md {{
namespace e12_data {{
const opxy::u32 captured = 1;
// Template statics, so every engine that plays the samples links one copy.
template <int> struct samples {{
    static const signed char high[{len(high)}];
    static const unsigned char low[{len(low)}];
}};
template <int N> const signed char samples<N>::high[{len(high)}] = {{
{rows(high)}
}};
template <int N> const unsigned char samples<N>::low[{len(low)}] = {{
{rows(low)}
}};
static const signed char (&high)[{len(high)}] = samples<0>::high;
static const unsigned char (&low)[{len(low)}] = samples<0>::low;
const opxy::u32 start[{len(start)}] = {{{", ".join(str(s) for s in start)}}};
const opxy::i32 level[{len(level)}] = {{{", ".join(str(v) for v in level)}}};
const opxy::u32 strt[{len(steps)}] = {{{", ".join(str(round(v * 256)) for v in steps)}}};
}}
}}
"""


def mixed(a, b, x, reach=200, n=6000):
    """x as a mix of a and b, which the original plays from the same moment: how
    far x is from the best mix, the mix, and b moved by whole samples to start
    with a (a's trim and b's need not agree)."""
    a, b, x = trimmed(a), trimmed(b), trimmed(x)
    pad = lambda y, k: (y[k:] if k >= 0 else np.concatenate([np.zeros(-k), y]))
    n = min(n, len(a), len(b), len(x) - reach)
    shift = max(range(-reach, reach), key=lambda k: np.dot(a[:n], pad(x, k)[:n]))
    x = pad(x, shift)[:n]
    best = None
    for k in range(-reach, reach):
        m = np.stack([a[:n], pad(b, k)[:n]], 1)
        w = np.linalg.lstsq(m, x, rcond=None)[0]
        e = float(np.linalg.norm(m @ w - x) / np.linalg.norm(x))
        if best is None or e < best[0]:
            best = (e, w, k)
    return best[0], best[1], pad(b, best[2])


def offset(sample, x, n=4096):
    """Where in `sample` the hit x begins, in whole frames (matched over n of them: the low ones need many)."""
    x = trimmed(x)
    n = min(n, len(x))
    x = x[:n]
    c = np.correlate(sample, x, "valid")
    e2 = np.convolve(sample * sample, np.ones(n), "valid")
    return int(np.argmin(e2 - 2 * c))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true", help="re-measure the last renders")
    ap.add_argument("--fresh", action="store_true", help="render every hit again, not only those missing")
    args = ap.parse_args()
    todo = jobs()
    if not args.check:
        render(todo, args.fresh)
    ok, captured, steps_of = True, {}, {}
    for name in NAMES:
        key = name.lower()
        x = read(f"{key}-raw")
        again, _ = difference(x, read(f"{key}-again"))
        vol, g = difference(x, read(f"{key}-vol"))
        steps = {job.split("-", 2)[2]: difference(x, read(job))[0] for job, n, _, _ in todo
                 if n == name and job.split("-", 2)[2] not in ("raw", "again", "vol", "b", "mix", "strt32", "strt64")}
        # The same hit again must be the same; volume 110 must scale it by (110/100)^2
        # and nothing else; one step in from each end must stay close.
        verdict = again < 1e-2 and vol < 1e-2 and abs(g - 1.21) < 0.01 and all(v < STEP_LIMIT for v in steps.values())
        captured[(name, "")] = trimmed(x)
        line = (f"{name}: {len(captured[(name, '')])} frames, peak {np.abs(x).max():.3f}; again {again:.1e}, "
                f"volume x{g:.3f} ({vol:.1e}); one step in: " + ", ".join(f"{k} {v:.3f}" for k, v in steps.items()))
        if name in SECOND:
            b = read(f"{key}-b")
            residue, w, captured[(name, "b")] = mixed(x, b, read(f"{key}-mix"))
            verdict = verdict and residue < 0.02
            line += f"; second sample {len(captured[(name, 'b')])} frames, {MIX.get(name, 48)} = {w[0]:.3f} + {w[1]:.3f} of them ({residue:.3f} off)"
        longer = captured[(name, "b")] if name in SECOND else captured[(name, "")]
        k32, k64 = offset(longer, read(f"{key}-strt32")), offset(longer, read(f"{key}-strt64"))
        steps_of[name] = k64 / 64
        verdict = verdict and abs(k32 - k64 / 2) <= max(4, k64 / 100)   # the original rounds its start
        line += f"; STRT {k64 / 64:.2f} frames a step (32: {k32}, 64: {k64})"
        ok = ok and verdict
        print(line + ("" if verdict else "  <- CHECK"))
    samples = [captured[s] for s in SAMPLES]
    total = sum(len(t) for t in samples) + 1
    print(f"{len(samples)} samples, {total} frames ({total / RATE:.2f} s): {total * 1.5 / 1024:.0f} KiB at 12 bits "
          f"({total * 2 / 1024:.0f} KiB at 16)")
    if not ok:
        sys.exit("capture_e12: a check failed; nothing written")
    for (name, part), t in zip(SAMPLES, samples):
        write_wav(PRIVATE / f"{name.lower()}{'-' + part if part else ''}.wav", t)
    (PRIVATE / "private_e12.hpp").write_text(header(samples, [steps_of[n] for n in NAMES]))
    print(f"wrote {PRIVATE / 'private_e12.hpp'}")


if __name__ == "__main__":
    main()
