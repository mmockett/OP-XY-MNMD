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
SDK_INCLUDE = ROOT.parent / "open-xy" / "sdk" / "include"
# Data captured from the owner's ROMs (docs/FAMILIES.md), where this Mac has it;
# otherwise the silent stand-ins in voices/silent.
PRIVATE = pathlib.Path.home() / "Library" / "Application Support" / "OPXY-MD" / "private"


def private_includes():
    return [f"-I{d}" for d in sorted(PRIVATE.glob("*")) if d.is_dir()] + [f"-I{ROOT / 'voices' / 'silent'}"]


def detent_problems():
    """Where an encoder detent would not move a control to its next shown step.

    Stock steps a custom engine's control about 1% a detent, which moves its
    0..127 readout every detent; the SDK steps an enum a choice a detent to the
    choice's midpoint (its synth_controls.cpp). So every control whose header
    names its steps (make_engine.ENUMS) must be an enum with those labels, the
    SDK must step enums, and each midpoint must read as its own step."""
    sys.path.insert(0, str(ROOT / "tools"))
    import catalog
    import make_engine
    problems = []
    if "choices[parameter_slots]" not in (ROOT.parent / "open-xy" / "sdk" / "platform" / "opxy-1.2.0" / "synth_controls.hpp").read_text():
        problems.append("the SDK steps enums 1% a detent (needs its branch drum-synthesis-enum-detents)")
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
            # The SDK's midpoint of each choice, (2^30 / n)(2c + 1), read back as the header reads it.
            n = len(steps)
            if any((((0x40000000 // n) * (2 * c + 1)) >> 16) * n >> 15 != c for c in range(n)):
                problems.append(f"{engine} {label}: a choice's midpoint reads as another step")
    return problems


def slot_problems():
    """Where an engine's definition still offers an empty slot as a control.

    An empty catalog slot is the SDK's absent placeholder (no label, no
    encoder position, no preset entry) when a real slot follows it, else left
    off; a real slot is never absent."""
    sys.path.insert(0, str(ROOT / "tools"))
    import catalog
    problems = []
    for engine, names in catalog.ENGINES.items():
        folder = ROOT / "machines" / engine
        if len(names) != 1 or not (folder / "definition.json").exists():
            continue
        definition = json.loads((folder / "definition.json").read_text())
        slots = catalog.MACHINES[names[0]][2]
        for i, parameter in enumerate(definition["parameters"]):
            if "spare" in parameter["id"].lower() or parameter["label"].lower() == "spare":
                problems.append(f"{engine} slot {i + 1}: a spare control")
            if slots[i] and parameter.get("absent"):
                problems.append(f"{engine} slot {i + 1}: {slots[i]} is absent")
            if not slots[i] and not (parameter.get("absent") and parameter["label"] == "" and "control" not in parameter):
                problems.append(f"{engine} slot {i + 1}: an empty slot an encoder can reach")
        if len(definition["parameters"]) > max(4, max(i for i, s in enumerate(slots) if s) + 1):
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
    subprocess.run(["c++", "-std=c++11", f"-I{SDK_INCLUDE}", f"-I{ROOT.parent / 'open-xy' / 'sdk' / 'platform' / 'opxy-1.2.0'}",
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
