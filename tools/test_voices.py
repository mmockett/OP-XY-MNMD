#!/usr/bin/env python3
"""Runs the voice modules' own sound checks (voices/tests/<module>.cpp) on the host.

    python3 tools/test_voices.py            # every module with checks
    python3 tools/test_voices.py trx_bd     # some
    python3 tools/test_voices.py --record   # take the kits' cases as they sound now

Every run also plays the drum kits' members through their fixed cases
(voices/tests/cases.hpp) and compares them with voices/tests/cases.json: by
level, and by the energy in eight bands across 16 windows, so an optimisation
can't drift a sound. Each case must also fall silent within LIFETIME seconds and
stay silent for two more.

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
    from make_engine import control_count, kit_keys
    problems, rows, engines = [], [], []
    for number, (engine, kits) in enumerate(catalog.ENGINE_KITS.items()):
        counts = [control_count(name) for name in catalog.ENGINES[engine]]
        engines.append(f'{{{len(counts)}, {{{", ".join(map(str, counts))}}}}}')
        for kit in kits:
            for region, (role, _name, model, controls) in zip(build_firmware.kit_regions(engine, kit), kit_keys(engine, kit)):
                if (region.get("playmode") == "mute1") != (role in catalog.CHOKED) or region["model"] != "fm slap":
                    problems.append(f"{engine} {kit} {role}: region {region}")
                rows.append(f'{{"{engine} {kit} {role}", {{{", ".join(map(str, region["params"]))}}}, {model}, '
                            f'{{{", ".join(map(str, controls))}}}, {number}}}')
    out = ROOT / "local" / "voice-tests"
    out.mkdir(parents=True, exist_ok=True)
    (out / "kit_presets.cpp").write_text("""#include "drum_storage.hpp"
#include <cstdio>
using namespace opxy;
using namespace opxy::platform;
// Each engine's machines' control counts (its parameter_count).
struct Engine { u32 models; u8 counts[64]; };
const Engine engines[] = {
%s
};
static const Engine* current;
static u32 count(u32 model) { return current->counts[model]; }
struct Row { const char* key; u32 params[4]; u32 model; u8 controls[8]; u32 engine; };
const Row rows[] = {
%s
};
int main() {
    int failed = 0;
    for (const Row& r : rows) {
        i32 words[4];
        for (int i = 0; i < 4; ++i) words[i] = static_cast<i32>(r.params[i] << 16);
        current = &engines[r.engine];
        const DrumCodec codec = drum_codec(current->models, count);
        u8 model = 99, controls[8] = {};
        bool ok = codec.fits() && drum_unpack(words, &model, controls, codec) && model == r.model;
        for (int i = 0; i < 8; ++i) ok = ok && controls[i] == r.controls[i];
        if (!ok) { std::printf("%%s: does not unpack as its key\\n", r.key); ++failed; }
    }
    return failed;
}
""" % (",\n".join(engines), ",\n".join(rows)))
    subprocess.run(["c++", "-std=c++11", f"-I{SDK_INCLUDE}", f"-I{platform()}",
                    str(out / "kit_presets.cpp"), "-o", str(out / "kit_presets")], check=True)
    result = subprocess.run([str(out / "kit_presets")], capture_output=True, text=True)
    lines = result.stdout.splitlines()
    return problems + (lines or ([f"kit_presets exited {result.returncode}"] if result.returncode else []))


# The kits whose members the fixed cases cover.
CASE_KITS = ("mdkit", "mde12")
CASES = ROOT / "voices" / "tests" / "cases.json"
LIFETIME = 20.0          # seconds a case may sound, at most: TRX-BD at DEC 127 falls 60 dB in 14 s
# Cases that hold, as on the original: they may sound without end, and no other may.
HELD = {("TRX-B2", "DEC 127"), ("GND-SN", "DEC 127"), ("GND-NS", "DEC 127")} | {
    (f"E12-{m}", "RTRG 127") for m in "BD SD HT LT CP RS CB CH OH RC CC BR TA TR SH BC".split()}
PEAK_DB, LEVEL_DB = 1.0, 0.5
BAND_DB = 2.0            # a band's energy in a window, where it is within RANGE_DB of the window's loudest
RANGE_DB = 50.0
QUIET_DB = -90.0         # bands below this are silence, as the lifetime counts it


def run_cases():
    """Plays every kit member's cases; returns {(machine, case): measures}."""
    import catalog
    from make_engine import defaults, slug, sound_header
    headers, calls = [], []
    for kit in CASE_KITS:
        for name in catalog.ENGINES[kit]:
            header = f'#include "{sound_header(name)}.hpp"'
            if header not in headers:
                headers.append(header)
            values = defaults(name)
            ns = f"md::{slug(name)}"
            cases = [("default", values)]
            for i, label in enumerate(catalog.MACHINES[name][2]):
                if label:
                    for end in (0, 127):
                        moved = list(values)
                        moved[i] = end
                        cases.append((f"{label} {end}", moved))
            for label, v in cases:
                calls.append(f'    {{ const int v[8] = {{{", ".join(map(str, v))}}}; '
                             f'cases::measure<{ns}::State>("{name}", "{label}", v, {ns}::reset, {ns}::trigger, '
                             f'{ns}::render); }}')
    out = ROOT / "local" / "voice-tests"
    out.mkdir(parents=True, exist_ok=True)
    source = "\n".join(headers) + '\n#include "tests/cases.hpp"\nint main() {\n' + "\n".join(calls) + "\n}\n"
    (out / "cases.cpp").write_text(source)
    subprocess.run(["c++", "-std=c++11", "-O2", f"-I{SDK_INCLUDE}", f"-I{ROOT / 'lib'}", f"-I{ROOT / 'voices'}",
                    *private_includes(), str(out / "cases.cpp"), "-o", str(out / "cases")], check=True)
    lines = subprocess.run([str(out / "cases")], capture_output=True, text=True, check=True).stdout.splitlines()
    return {(m["machine"], m["case"]): m for m in map(json.loads, lines)}


def case_problems(played, recorded):
    """Where a case no longer sounds as recorded, or never falls silent."""
    problems = []
    for key, now in played.items():
        where = f"{key[0]} {key[1]}"
        if key in HELD:
            pass
        elif not now["silent"] or now["lasts"] > LIFETIME:
            problems.append(f"{where}: still sounding after {now['lasts']:.1f} s" if now["silent"] else
                            f"{where}: never falls silent (above -90 dB after {now['lasts']:.1f} s)")
        then = recorded.get(key)
        if then is None:
            problems.append(f"{where}: not recorded (run with --record)")
            continue
        if abs(now["peak"] - then["peak"]) > PEAK_DB or abs(now["level"] - then["level"]) > LEVEL_DB:
            problems.append(f"{where}: peak {then['peak']:.1f} -> {now['peak']:.1f} dB, "
                            f"level {then['level']:.1f} -> {now['level']:.1f} dB")
            continue
        worst = (0.0, 0, 0)
        for w, (a, b) in enumerate(zip(then["bands"], now["bands"])):
            top = max(a)
            for band, (x, y) in enumerate(zip(a, b)):
                if x > max(QUIET_DB, top - RANGE_DB) and abs(x - y) > worst[0]:
                    worst = (abs(x - y), w, band)
        if worst[0] > BAND_DB:
            problems.append(f"{where}: band {worst[2]} in window {worst[1]} moved {worst[0]:.1f} dB")
    for key in recorded:
        if key not in played:
            problems.append(f"{key[0]} {key[1]}: recorded but no longer played")
    return problems


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
    record = "--record" in sys.argv
    played = run_cases()
    if record:
        CASES.write_text("[\n" + ",\n".join(json.dumps(m, separators=(",", ":")) for m in played.values()) + "\n]\n")
        print(f"recorded {len(played)} cases in {CASES.relative_to(ROOT)}")
    recorded = {(m["machine"], m["case"]): m for m in json.loads(CASES.read_text())} if CASES.exists() else {}
    found = case_problems(played, recorded)
    for p in found:
        print(f"FAIL cases: {p}")
    if not found:
        print(f"PASS cases: {len(played)} cases sound as recorded and fall silent within {LIFETIME:.0f} s, but for {len(HELD)} that hold")
    problems += found
    tests = sorted((ROOT / "voices" / "tests").glob("*.cpp"))
    wanted = [a for a in sys.argv[1:] if not a.startswith("--")]
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
        failed.append("detents, slots, presets and cases")
    if failed:
        sys.exit(f"failed: {', '.join(failed)}")


if __name__ == "__main__":
    main()
