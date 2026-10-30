#!/usr/bin/env python3
"""Runs the voice modules' own sound checks (voices/tests/<module>.cpp) on the host.

    python3 tools/test_voices.py            # every module with checks
    python3 tools/test_voices.py trx_bd     # some

A kit engine's SDK test (machines/<kit>/tests/sound.cpp) checks that every key
sounds and stays bounded; these check each voice against its fitted measures.
"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from sdk_path import SDK_INCLUDE, platform  # noqa: E402
# Data captured from the owner's ROMs (docs/FAMILIES.md), where this Mac has it;
# otherwise the silent stand-ins in voices/silent.
PRIVATE = pathlib.Path.home() / "Library" / "Application Support" / "OPXY-MD" / "private"


def private_includes():
    return [f"-I{d}" for d in sorted(PRIVATE.glob("*")) if d.is_dir()] + [f"-I{ROOT / 'voices' / 'silent'}"]


def detent_problems():
    """Where an encoder detent would not move a control to its next shown step.

    Stock steps a custom engine's control about 1% a detent, which moves its
    0..127 readout every detent; the SDK answers an enum's choice count, so stock
    steps it a choice a detent and stores the choice's bucket centre (its
    synth_controls.cpp). So every control whose header names its steps
    (make_engine.ENUMS) must be an enum with those labels, the SDK must step
    enums, and each centre must read as its own step."""
    import catalog
    import make_engine
    problems = []
    if "steps[parameter_slots]" not in (platform() / "synth_controls.hpp").read_text():
        problems.append("the SDK steps enums 1% a detent (needs SDK 1.3's stepped enums)")
    for engine, names in catalog.ENGINES.items():
        folder = ROOT / "machines" / engine
        if len(names) != 1 or not (folder / "definition.json").exists():
            continue   # a kit's controls are its keys' machines'
        name = names[0]
        parameters = json.loads((folder / "definition.json").read_text())["parameters"]
        for label, parameter in zip(catalog.MACHINES[name][2], parameters):
            steps = make_engine.ENUMS.get(name, {}).get(label)
            mapping = parameter["mapping"]
            if not steps:
                if mapping["kind"] == "enum":
                    problems.append(f"{engine} {label}: an enum the header does not name")
                continue
            if mapping["kind"] != "enum" or mapping["labels"] != steps:
                problems.append(f"{engine} {label}: {len(steps)} steps, declared {mapping['kind']}")
                continue
            # Stock's centre of each choice's bucket, c q + q / 2 with q = (2^31 - 1) / n, read back
            # as the header reads it.
            n = len(steps)
            q = 0x7fffffff // n
            if any(((c * q + q // 2) >> 16) * n >> 15 != c for c in range(n)):
                problems.append(f"{engine} {label}: a choice's centre reads as another step")
    return problems


def slot_problems():
    """Where an engine's definition still offers an empty slot as a control.

    Every real catalog control is in the definition exactly once, and an empty
    slot is the SDK's absent placeholder (no label, no preset entry). On a
    two-page engine an empty bottom-row slot sits on its encoder's page 2,
    blank and inert; on any other, no encoder reaches it. No empty slots
    follow the last control."""
    sys.path.insert(0, str(ROOT / "tools"))
    import catalog
    from make_engine import control
    problems = []
    for engine, names in catalog.ENGINES.items():
        folder = ROOT / "machines" / engine
        if len(names) != 1 or not (folder / "definition.json").exists():
            continue
        definition = json.loads((folder / "definition.json").read_text())
        paged = definition.get("press") == "page"
        real = [control(label, 0)["id"] for label in catalog.MACHINES[names[0]][2] if label]
        ids = [parameter["id"] for parameter in definition["parameters"] if not parameter.get("absent")]
        if sorted(ids) != sorted(real):
            problems.append(f"{engine}: controls {ids}, not the machine's {real}")
        for i, parameter in enumerate(definition["parameters"]):
            if "spare" in parameter["id"].lower() or parameter["label"].lower() == "spare":
                problems.append(f"{engine} slot {i + 1}: a spare control")
            if parameter.get("absent") and (parameter["label"] or ("control" in parameter and not (paged and i >= 4))):
                problems.append(f"{engine} slot {i + 1}: an empty slot an encoder can reach")
        last = max(i for i, parameter in enumerate(definition["parameters"]) if not parameter.get("absent"))
        if len(definition["parameters"]) > max(4, last + 1):
            problems.append(f"{engine}: empty slots declared after the last control")
        for preset, values in definition["presets"].items():
            if set(values) != {p["id"] for p in definition["parameters"] if not p.get("absent")}:
                problems.append(f"{engine} preset {preset}: not exactly the real controls")
    return problems


def preset_problems():
    """Where a kit preset's region would not load as the key's own machine and
    controls (the SDK's drum_unpack), or its choke group is not the catalog's."""
    sys.path.insert(0, str(ROOT / "tools"))
    import catalog
    import build_firmware
    from make_engine import kit_keys
    problems, rows = [], []
    for engine in catalog.KITS:
        for region, (role, _name, model, controls) in zip(build_firmware.kit_regions(engine), kit_keys(engine)):
            if (region.get("playmode") == "mute1") != (role in catalog.CHOKED) or region["model"] != "fm slap":
                problems.append(f"{engine} {role}: region {region}")
            rows.append(f'{{"{engine} {role}", {{{", ".join(map(str, region["params"]))}}}, {model}, '
                        f'{{{", ".join(map(str, controls))}}}}}')
    out = ROOT / "local" / "voice-tests"
    out.mkdir(parents=True, exist_ok=True)
    (out / "kit_presets.cpp").write_text("""#include "drum_storage.hpp"
#include <cstdio>
using namespace opxy;
using namespace opxy::platform;
struct Row { const char* key; u32 params[4]; u32 model; u8 controls[8]; };
const Row rows[] = {
%s
};
int main() {
    int failed = 0;
    for (const Row& r : rows) {
        i32 words[4];
        for (int i = 0; i < 4; ++i) words[i] = static_cast<i32>(r.params[i] << 16);
        u8 model = 99, controls[8] = {};
        bool ok = drum_unpack(words, &model, controls) && model == r.model;
        for (int i = 0; i < 8; ++i) ok = ok && controls[i] == r.controls[i];
        if (!ok) { std::printf("%%s: does not unpack as its key\\n", r.key); ++failed; }
    }
    return failed;
}
""" % ",\n".join(rows))
    subprocess.run(["c++", "-std=c++11", f"-I{SDK_INCLUDE}", f"-I{platform()}",
                    str(out / "kit_presets.cpp"), "-o", str(out / "kit_presets")], check=True)
    result = subprocess.run([str(out / "kit_presets")], capture_output=True, text=True)
    lines = result.stdout.splitlines()
    return problems + (lines or ([f"kit_presets exited {result.returncode}"] if result.returncode else []))


def main():
    problems = []
    for check, passed, find in (("detents", "every stepped control moves a step a detent", detent_problems),
                                ("slots", "no engine offers an empty slot as a control", slot_problems),
                                ("presets", "every kit preset loads each key as its own sound", preset_problems)):
        found = find()
        for p in found:
            print(f"FAIL {check}: {p}")
        if not found:
            print(f"PASS {check}: {passed}")
        problems += found
    tests = sorted((ROOT / "voices" / "tests").glob("*.cpp"))
    wanted = sys.argv[1:]
    out = ROOT / "local" / "voice-tests"
    out.mkdir(parents=True, exist_ok=True)
    failed = []
    for test in tests:
        if wanted and test.stem not in wanted:
            continue
        exe = out / test.stem
        subprocess.run(["c++", "-std=c++11", "-O2", f"-I{SDK_INCLUDE}", f"-I{ROOT / 'lib'}", f"-I{ROOT / 'voices'}",
                        *private_includes(), str(test), "-o", str(exe)], check=True)
        result = subprocess.run([str(exe)], capture_output=True, text=True)
        print(result.stdout.strip())
        if result.returncode:
            failed.append(test.stem)
    if problems:
        failed.append("detents, slots and presets")
    if failed:
        sys.exit(f"failed: {', '.join(failed)}")


if __name__ == "__main__":
    main()
