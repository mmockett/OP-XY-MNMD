#!/usr/bin/env python3
"""The SDK's host CPU estimate for one voice module, before any engine holds it.

    python3 reference/cpu_voice.py PI-BD [PI-SD ...]
    python3 reference/cpu_voice.py --kits [--save before.json] [--against before.json]

--kits times every member of the drum kits; --save keeps the figures and
--against prints each beside a saved run, to show what an optimisation bought.

Times the module's render as the SDK's test does (../open-xy/tooling/cpu.py):
its defaults (tools/presets.py, else the design's), then each control at nine
positions with the rest at the defaults, the note started again before each
run, the slowest setting counting. That is scaled by the SDK's reference loop
timed in the same run, to Blackfin instructions per 64-sample block. A kit's
SDK test times only the key its 440 Hz note lands on; this times each machine.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT.parent / "open-xy"))
import catalog  # noqa: E402
from make_engine import defaults, slug, sound_header  # noqa: E402
from test_voices import private_includes  # noqa: E402
from tooling.cpu import HARNESS, REFERENCE, REFERENCE_INSTRUCTIONS  # noqa: E402

BENCH = r'''
#include "@HEADER@.hpp"
''' + HARNESS + r'''
int main() {
    static md::@VOICE@::State voice;
    static md::Common shared;
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = 25479004u;
    static opxy::i32 output[opxy::block_frames];
    volatile opxy::i32 sink = 0;
    md::@VOICE@::reset(voice, shared, 1);
    std::printf("%.1f\n", worst_block([&] { md::@VOICE@::trigger(voice, shared); }, [&](const opxy::i32* parameters, int b) {
        md::@VOICE@::render(voice, shared, output, parameters, pitch);
        sink = sink + output[b & (opxy::block_frames - 1)];
    }));
    return 0;
}
'''


def cxx():
    try:
        prefix = subprocess.run(["brew", "--prefix", "llvm"], capture_output=True, text=True, check=True).stdout.strip()
        return f"{prefix}/bin/clang++"
    except (OSError, subprocess.CalledProcessError):
        return "c++"


def run(source, name, compiler):
    folder = ROOT / "local" / "cpu"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / f"{name}.cpp").write_text(source)
    subprocess.run([compiler, "-std=c++11", "-O2", f"-I{ROOT.parent / 'open-xy' / 'sdk' / 'include'}",
                    f"-I{ROOT / 'lib'}", f"-I{ROOT / 'voices'}", *private_includes(), str(folder / f"{name}.cpp"), "-o", str(folder / name)],
                   check=True)
    return float(subprocess.run([str(folder / name)], capture_output=True, text=True, check=True).stdout)


def estimate(name, compiler):
    labels = catalog.MACHINES[name][2]
    values = defaults(name)
    swept = [i for i, label in enumerate(labels) if label]
    positions = [0x7fffffff * step // 8 for step in range(9)]
    source = (BENCH.replace("@HEADER@", sound_header(name)).replace("@VOICE@", slug(name)).replace("@BLOCKS@", "400")
              .replace("@DEFAULTS@", ", ".join(str(round(v / 127 * 0x7fffffff)) for v in values))
              .replace("@POSITIONS@", ", ".join(str(p) for _ in swept for p in positions))
              .replace("@COUNTS@", ", ".join("9" if label else "0" for label in labels))
              .replace("@POSITION_COUNT@", str(9 * len(swept))))
    return run(source, slug(name), compiler)


def option(flag):
    return sys.argv[sys.argv.index(flag) + 1] if flag in sys.argv else None


def main():
    import json
    compiler = cxx()
    reference = run(REFERENCE, "reference", compiler)
    names = [a for i, a in enumerate(sys.argv[1:], 1) if not a.startswith("--") and sys.argv[i - 1] not in ("--save", "--against")]
    if "--kits" in sys.argv:
        names = [name for kit in ("mdkit", "mde12") for name in catalog.ENGINES[kit]]
    before = json.loads(pathlib.Path(option("--against")).read_text()) if option("--against") else {}
    figures = {}
    for name in names:
        # The host's timing wanders 10-15% run to run: the fastest of three.
        figures[name] = round(min(estimate(name, compiler) / run(REFERENCE, "reference", compiler)
                                  for _ in range(3)) * REFERENCE_INSTRUCTIONS, -2)
        was = f" (was {before[name]:,.0f}, {100 * (figures[name] / before[name] - 1):+.0f}%)" if name in before else ""
        print(f"{name}: about {figures[name]:,.0f} instructions per voice block at its heaviest setting{was}")
    if option("--save"):
        pathlib.Path(option("--save")).write_text(json.dumps(figures, indent=1) + "\n")


if __name__ == "__main__":
    main()
