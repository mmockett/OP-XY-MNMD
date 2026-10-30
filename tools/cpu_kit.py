#!/usr/bin/env python3
"""Measures the drum kits' voices on an OP-XY. The SDK's benchmark doesn't drive
a drum kit (its builds run without one), so this holds each voice module in a
temporary synth, which the benchmark's stock voice timer does run, control by
control, as it does for a catalog record (docs/profiling.md in the SDK).

    python3 tools/cpu_kit.py generate           # the synths, in local/cpu-kit
    python3 tools/cpu_kit.py build              # a profiling build of them only
    python3 tools/cpu_kit.py run                # one benchmark over USB MIDI (booted normally)
    python3 tools/cpu_kit.py report             # each voice's figures from the runs

The figures are the voice's own plus the kit's per-block check for silence. The
synths never free a finished voice, so a voice's tail still costs what it costs.
These machines are for measuring: they have no screen of their own and are never
published. Their folder is under local/, as they can hold the private E12 capture.
"""
import json
import pathlib
import shutil
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import catalog  # noqa: E402
from make_engine import copy_private, copy_scoped, defaults, slug, sound_header, with_includes  # noqa: E402
from sdk_path import SDK  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "local" / "cpu-kit"
WORK = pathlib.Path.home() / "Library" / "Application Support" / "OPXY-MD" / "cpu"
STOCK = pathlib.Path.home() / "Library" / "Application Support" / "OPXY-SDK" / "private" / "stock-1.2.1" / "os.ldr"
PYTHON = ROOT.parent / "open-xy" / ".venv" / "bin" / "python"
# Each family's heaviest by the host estimate (reference/cpu_voice.py --kits).
VOICES = ["TRX-CY", "TRX-MA", "TRX-SD", "TRX-OH", "TRX-CB", "TRX-CP",
          "PI-RS", "PI-CC", "PI-ML", "EFM-CY", "EFM-RS", "E12-BR"]


def sdk(*args):
    subprocess.run([str(PYTHON), str(SDK / "sdk.py"), *map(str, args)], cwd=SDK, check=True)


def voice_bytes(folder, engine):
    probe = folder / "size.cpp"
    probe.write_text(f'#include "core.h"\n#include <cstdio>\nint main() {{ std::printf("%zu", sizeof({engine}::Voice)); }}\n')
    binary = OUT / f"size-{engine}"
    subprocess.run(["c++", "-std=c++11", f"-I{SDK / 'sdk' / 'include'}", f"-I{folder}", str(probe), "-o", str(binary)], check=True)
    probe.unlink()
    return int(subprocess.run([str(binary)], capture_output=True, text=True, check=True).stdout)


def generate():
    shutil.rmtree(OUT, ignore_errors=True)
    OUT.mkdir(parents=True)
    for name in VOICES:
        engine = "kc" + slug(name).replace("_", "")
        folder = OUT / engine
        sdk("new", "--controls", 8, "--id", engine, "--name", name, "--out", folder)
        header = sound_header(name)
        headers = with_includes([header])
        for source in sorted((ROOT / "lib").glob("*.hpp")):
            copy_scoped(source, folder, engine)
        for h in headers:
            copy_scoped(ROOT / "voices" / f"{h}.hpp", folder, engine)
        copy_private(headers, folder, engine)
        module = slug(name)
        (folder / "core.h").write_text(f"""#pragma once
#include <opxy/audio.hpp>
#include "{header}.hpp"
namespace {engine} {{
struct Voice {{ md::{module}::State state; opxy::u32 quiet; }};
struct Shared {{ md::Common common; }};
}}
""")
        labels = (list(catalog.MACHINES[name][2]) + [None] * 8)[:8]
        own = "".join(f"\n    own[{i}] = parameters[{i}];" for i, label in enumerate(labels) if label)
        (folder / "engine.cpp").write_text(f"""#include "core.h"
namespace {engine} {{
using opxy::i32;
using opxy::u32;
inline void initialize(Voice& voice, Shared& shared, u32 owner) {{ md::{module}::reset(voice.state, shared.common, owner); voice.quiet = 0; }}
inline void start(Voice& voice, Shared& shared, u32) {{ md::{module}::trigger(voice.state, shared.common); voice.quiet = 0; }}
inline void destroy(Voice&, Shared&, u32) {{}}
inline void render(Voice& voice, Shared& shared, u32, opxy::Output output, opxy::Parameters parameters, opxy::Pitch pitch) {{
    i32 own[8] = {{}};{own}
    md::{module}::render(voice.state, shared.common, output, own, pitch);
    // The kit's check for silence (tools/make_engine.py).
    bool loud = false;
    for (unsigned i = 0; i < opxy::block_frames; ++i) loud = loud || output[i] >= (1 << 14) || output[i] <= -(1 << 14);
    voice.quiet = loud ? 0 : voice.quiet + 1;
}}
}}
""")
        definition = json.loads((folder / "definition.json").read_text())
        for parameter, label, value in zip(definition["parameters"], labels, defaults(name)):
            parameter["label"] = label or "UNUSED"
            parameter["default"] = value / 127
        definition["resources"]["voiceBytes"] = voice_bytes(folder, engine)
        definition["resources"]["sharedBytes"] = 16
        definition["resources"]["voiceAlignment"] = 4
        (folder / "definition.json").write_text(json.dumps(definition, indent=2) + "\n")
        (folder / "tests" / "sound.cpp").write_text('#include "../engine.cpp"\nint main() { return 0; }\n')
        print(f"{engine}: {name}, {definition['resources']['voiceBytes']} bytes a voice")


def machines():
    return sorted(p for p in OUT.iterdir() if (p / "machine.json").exists())


def numbered(prefix):
    return sorted(WORK.glob(f"{prefix}-*"), key=lambda p: int(p.name.rsplit("-", 1)[1]))


def fresh(prefix):
    found = numbered(prefix)
    return WORK / f"{prefix}-{int(found[-1].name.rsplit('-', 1)[1]) + 1 if found else 1:03d}"


def main():
    command = sys.argv[1] if len(sys.argv) > 1 else ""
    if command == "generate":
        generate()
    elif command == "build":
        out = fresh("kit-build")
        out.parent.mkdir(parents=True, exist_ok=True)
        sdk("build", "--profiling", "on", "--stock-os", STOCK, "--out", out, *[a for m in machines() for a in ("--machine", m)])
        print(f"Built {out}")
    elif command == "run":
        out = fresh("kit-run")
        print("The OP-XY must be booted normally with this build, project saved, playback stopped. Don't touch it during the run.")
        sdk("profile", "benchmark", "--package", numbered("kit-build")[-1], "--installed", "--greet", "--out", out)
        print(f"Saved {out}")
    elif command == "report":
        build = numbered("kit-build")[-1]
        captures = [r / "capture.json" for r in numbered("kit-run") if (r / "capture.json").exists()]
        out = fresh("kit-report")
        for m in machines():
            sdk("profile", "report", "--package", build, "--machine", m.name,
                *[a for c in captures for a in ("--capture", c)], "--out", out / m.name)
        for m in machines():
            cpu = json.loads((out / m.name / "catalog-cpu.json").read_text())["cpu"]
            print(f"{m.name}: {cpu['block_percent']:.3f}% at its heaviest, {cpu['midpoint_percent']:.3f}% at its midpoint")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
