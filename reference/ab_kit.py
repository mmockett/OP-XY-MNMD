#!/usr/bin/env python3
"""A/B of MD KIT's models against another checkout (a release), for changes meant to
cost less and sound the same.

    python3 reference/ab_kit.py <other-checkout> [--wav DIR] [--only PI-RS ...]

Renders each model with every control at 64, 0 and 127, as the device benchmark
does, and at its defaults, struck once and again after 145 ms (a retrigger on
running state), through both checkouts' own modules on the host. For each it
prints whether the two are bit-identical; otherwise level, peak, attack (the first
20 ms), the worst 5 ms window, the worst third-octave band, and the null, all in
dB. Host renders of the same integer code give the device's samples; nothing here
says what a change costs. --wav writes each changed model's pair, A then a gap then B.
"""
import argparse
import os
import pathlib
import struct
import subprocess
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import catalog  # noqa: E402
from make_engine import defaults, slug, sound_header  # noqa: E402
from sdk_path import SDK  # noqa: E402

RATE, SECONDS, RESTRIKE = 44100, 2.0, 100   # blocks between the strikes, as the benchmark's
HARNESS = """
#include <cstdio>
@HEADERS@
template <class State, class Reset, class Trigger, class Render>
void play(const int* values, Reset reset, Trigger trigger, Render render) {
    static State voice;
    md::Common shared = {};
    opxy::i32 p[8];
    for (int i = 0; i < 8; ++i) p[i] = values[i] >= 127 ? 0x7fffffff : values[i] * 16909320;
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = 25479004u;
    reset(voice, shared, 1);
    trigger(voice, shared);
    static opxy::i32 out[opxy::block_frames];
    for (unsigned b = 0; b < @BLOCKS@; ++b) {
        if (b == @RESTRIKE@) trigger(voice, shared);
        render(voice, shared, out, p, pitch);
        fwrite(out, sizeof(out), 1, stdout);
    }
}
int main() {
@CALLS@
    return 0;
}
"""


def cases(only):
    found = []
    for name in catalog.ENGINES["mdkit"]:
        if only and name not in only:
            continue
        labels = catalog.MACHINES[name][2]
        for level in (64, 0, 127):
            found.append((name, str(level), [level if label else 0 for label in (list(labels) + [None] * 8)[:8]]))
        found.append((name, "default", list(defaults(name))))
    return found


def render(root, found, work, tag):
    headers = sorted({sound_header(name) for name, _label, _values in found})
    calls = []
    for name, _label, values in found:
        ns = f"md::{slug(name)}"
        calls.append(f'    {{ const int v[8] = {{{", ".join(map(str, values))}}}; '
                     f'play<{ns}::State>(v, {ns}::reset, {ns}::trigger, {ns}::render); }}')
    blocks = int(SECONDS * RATE) // 64
    source = HARNESS.replace("@HEADERS@", "\n".join(f'#include "{h}.hpp"' for h in headers)) \
        .replace("@CALLS@", "\n".join(calls)).replace("@BLOCKS@", str(blocks)).replace("@RESTRIKE@", str(RESTRIKE))
    work.mkdir(parents=True, exist_ok=True)
    (work / f"{tag}.cpp").write_text(source)
    subprocess.run(["c++", "-std=c++11", "-O1", "-w", f"-I{SDK / 'sdk' / 'include'}", f"-I{root / 'lib'}", f"-I{root / 'voices'}",
                    str(work / f"{tag}.cpp"), "-o", str(work / tag)], check=True)
    raw = subprocess.run([str(work / tag)], capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype="<i4").reshape(len(found), blocks * 64).astype(float) / 2147483648.0


def db(x):
    return 20 * np.log10(max(float(x), 1e-12))


def rms(x):
    return float(np.sqrt(np.mean(x * x)))


def thirds(x):
    window = np.hanning(len(x))
    power = np.abs(np.fft.rfft(x * window)) ** 2
    hz = np.fft.rfftfreq(len(x), 1 / RATE)
    edges = 40 * 2 ** (np.arange(0, 29) / 3)
    return np.array([power[(hz >= lo) & (hz < hi)].sum() for lo, hi in zip(edges[:-1], edges[1:])])


def compare(a, b):
    """The differences in dB, between two renders of one strike. Windows and bands
    within 50 dB of the loudest count, as anything further down is under the sound."""
    if np.array_equal(a, b):
        return None
    w = int(0.005 * RATE)
    n = len(a) // w
    ea = np.sqrt(np.mean(a[:n * w].reshape(n, w) ** 2, axis=1))
    eb = np.sqrt(np.mean(b[:n * w].reshape(n, w) ** 2, axis=1))
    audible = ea > ea.max() * 10 ** (-50 / 20)
    window = max((abs(db(x) - db(y)) for x, y in zip(ea[audible], eb[audible])), default=0.0)
    ta, tb = thirds(a), thirds(b)
    keep = ta > ta.max() * 1e-5
    band = float(np.max(np.abs(10 * np.log10((tb[keep] + 1e-30) / (ta[keep] + 1e-30)))))
    attack = int(0.02 * RATE)
    return dict(level=db(rms(b)) - db(rms(a)), peak=db(np.abs(b).max()) - db(np.abs(a).max()),
                attack=db(rms(b[:attack])) - db(rms(a[:attack])), window=window, band=band,
                null=db(rms(a - b)) - db(rms(a)))


def write_wav(path, a, b):
    joined = np.concatenate([a, np.zeros(RATE // 2), b])
    data = (np.clip(joined * 2, -1, 1) * 32767).astype("<i2").tobytes()
    path.write_bytes(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16)
                     + b"data" + struct.pack("<I", len(data)) + data)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("other", type=pathlib.Path)
    parser.add_argument("--wav", type=pathlib.Path)
    parser.add_argument("--only", nargs="*", default=[])
    args = parser.parse_args()
    found = cases(args.only)
    work = ROOT / "local" / "ab-kit" / str(os.getpid())
    a, b = render(args.other.resolve(), found, work, "a"), render(ROOT, found, work, "b")
    identical, changed = [], {}
    print(f"{'case':16} {'level':>6} {'peak':>6} {'attack':>6} {'5ms':>6} {'band':>6} {'null':>7}")
    for i, (name, label, _values) in enumerate(found):
        result = compare(a[i], b[i])
        if result is None:
            identical.append(f"{name} {label}")
            continue
        changed.setdefault(name, []).append(label)
        print(f"{name + ' ' + label:16} {result['level']:6.2f} {result['peak']:6.2f} {result['attack']:6.2f} "
              f"{result['window']:6.2f} {result['band']:6.2f} {result['null']:7.1f}")
        if args.wav:
            args.wav.mkdir(parents=True, exist_ok=True)
            write_wav(args.wav / f"{name}-{label}.wav", a[i], b[i])
    print(f"{len(identical)} of {len(found)} cases bit-identical; models changed: {', '.join(changed) or 'none'}")


if __name__ == "__main__":
    main()
