#!/usr/bin/env python3
"""Builds OP-XY firmware with every engine in machines/, ready to install.

    python3 tools/build_firmware.py                 # every engine
    python3 tools/build_firmware.py mdkit mmsaw     # some
    python3 tools/build_firmware.py mdkit ../mine/machine  # a machine folder outside machines/

It runs the SDK's `build` against your own stock OS (the one the SDK patches), copied off your
OP-XY (docs/DEVICE.md), into a new numbered folder outside this checkout,

    ~/Library/Application Support/OPXY-MD/builds/NNN/

then adds a preset folder per engine (content/presets/<engine>/), as the SDK's
browser builder does: the OP-XY lists an engine for a track only once it has
a preset there.

Each build passes the previous build's layout.json to the SDK, so every engine
keeps its engine and page numbers and saved projects keep their machines. It
never touches the device: installing is yours to do, as docs/DEVICE.md says.
"""
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import catalog  # noqa: E402
from make_engine import KIT_FAMILY, kit_keys  # noqa: E402
from sdk_path import SDK, stock_version  # noqa: E402

SUPPORT = pathlib.Path.home() / "Library" / "Application Support"
# The stock OS the SDK's newest platform patches.
STOCK_OS = SUPPORT / "OPXY-SDK" / "private" / f"stock-{stock_version()}" / "os.ldr"
BUILDS = SUPPORT / "OPXY-MD" / "builds"


def kit_bit(k, models, model, controls):
    """Bit k of the 60 a key stores (the SDK's drum_storage.hpp, drum_bit): the eight
    controls (7 bits each) then the model (4). A kit of more than 16 models stores
    slots 6 and 7 as their step, 0..63, in their top six bits, and their lowest
    bits hold the model's bits 4 and 5."""
    if k >= 56:
        return (model >> (k - 56)) & 1
    slot, bit = divmod(k, 7)
    if models <= 16 or slot < 6:
        return (controls[slot] >> bit) & 1
    step = 63 if controls[slot] >= 126 else min(62, (controls[slot] + 1) >> 1)
    return (step >> (bit - 1)) & 1 if bit else (model >> (4 if slot == 6 else 5)) & 1


def kit_region(key, model, controls, choked, models=16):
    """A kit key as a preset region, as the SDK stores it (the SDK's docs/abi-<OS version>.md,
    the drum kind): "fm slap" is Drumline type 7, the SDK's mark; the controls and
    the model fill 15 bits of each param (kit_bit); the play mode mute1 chokes,
    and oneshot, the default, does not."""
    bits = [kit_bit(k, models, model, controls) for k in range(60)]
    params = [sum(bit << b for b, bit in enumerate(bits[w * 15:w * 15 + 15])) for w in range(4)]
    region = {"hikey": key, "model": "fm slap", "params": params}
    return {**region, "playmode": "mute1"} if choked else region


def kit_regions(engine, kit=None):
    """A drum engine's 24 keys from one of its kits (its first, unless named)."""
    models = len(catalog.ENGINES[engine])
    return [kit_region(53 + k, model, controls, role in catalog.CHOKED, models)
            for k, (role, _name, model, controls) in enumerate(kit_keys(engine, kit))]


def preset(entry, amp, params, regions):
    """A preset in the device's version-4 structure, as the SDK's browser builder
    writes its init presets (../open-xy/builder/core.mjs, defaultPreset)."""
    return {
        "engine": {
            "bendrange": 8191, "highpass": 0,
            "modulation": {source: {"amount": 16384, "target": 0} for source in ("aftertouch", "modwheel", "pitchbend", "velocity")},
            "params": list(params), "playmode": "poly",
            "portamento.amount": 0, "portamento.type": 0, "transpose": 0,
            "tuning.root": 0, "tuning.scale": 0, "velocity.sensitivity": 0, "volume": 16384, "width": 0,
        },
        "envelope": {
            "amp": {"attack": 128, "decay": 8192, "release": 32767, "sustain": 32767, **amp},
            "filter": {"attack": 0, "decay": 0, "release": 0, "sustain": 32767},
        },
        "fx": {"active": False, "params": [0, 0, 0, 0, 0, 32767, 0, 0], "type": "svf"},
        "lfo": {"active": False, "params": [0, 0, 0, 0, 0, 0, 0, 0], "type": "value"},
        "octave": 0, "platform": "OP-XY", "regions": regions, "type": entry["type"], "version": 4,
    }


def folder_of(engine):
    """An engine name in machines/, or a machine folder given as a path."""
    return (ROOT / engine).resolve() if "/" in engine else ROOT / "machines" / engine


def write_presets(out, engines):
    """content/presets/<engine>/: default.json, the engine's catalog defaults, and
    one preset per definition preset. Returns the files written."""
    factory = json.loads((out / "candidate" / "content" / "op1_factory.json").read_text())
    entries = {e["type"]: e for e in factory["engine_types"]}
    written = []
    for engine in engines:
        folder = folder_of(engine)
        engine = folder.name
        if engine not in entries:
            continue
        machine = json.loads((folder / "machine.json").read_text())
        definition = json.loads((folder / "definition.json").read_text())
        amp = machine.get("presetDefaults", {}).get("amp", {})
        target = out / "candidate" / "content" / "presets" / engine
        target.mkdir(parents=True, exist_ok=True)
        files = {"default": entries[engine]["default_params"]}
        ids = [p["id"] for p in definition["parameters"]]
        for name, values in definition.get("presets", {}).items():
            params = [round(values.get(i, p["default"]) * 32767) for i, p in zip(ids, definition["parameters"])]
            files[name.lower().replace(" ", "_")] = params + [0] * (8 - len(params))
        kits = catalog.ENGINE_KITS.get(engine, [])
        regions = kit_regions(engine) if kits else []
        for name, params in files.items():
            path = target / f"{name}.json"
            path.write_text(json.dumps(preset(entries[engine], amp, params, regions), indent=2) + "\n")
            written.append(path)
        # A drum engine's other kits, a preset each, named for the family (efm, pi).
        for kit in kits[1:]:
            path = target / f"{KIT_FAMILY[kit].lower()}.json"
            path.write_text(json.dumps(preset(entries[engine], amp, files["default"], kit_regions(engine, kit)), indent=2) + "\n")
            written.append(path)
    return written


def main():
    parser = argparse.ArgumentParser(description="Build OP-XY firmware with this repository's engines")
    parser.add_argument("engines", nargs="*", help="engine folders in machines/ (default: all)")
    parser.add_argument("--stock-os", default=str(STOCK_OS), help="your OP-XY's own stock os.ldr")
    parser.add_argument("--profiling", choices=("on", "off"), default="on",
                        help="the stock per-voice CPU profiling (on, the SDK's default, for development)")
    parser.add_argument("--layout", help="the engine and page numbers to keep (default: the last build's layout.json)")
    args = parser.parse_args()

    stock = pathlib.Path(args.stock_os)
    if not stock.exists():
        sys.exit(f"No stock OS at {stock}.\nCopy os.ldr off your OP-XY first: docs/DEVICE.md, step 2.")
    engines = args.engines or sorted(p.name for p in (ROOT / "machines").iterdir() if (p / "machine.json").exists())
    missing = [e for e in engines if not (folder_of(e) / "machine.json").exists()]
    if missing:
        sys.exit(f"Not machine folders: {', '.join(missing)}")

    BUILDS.mkdir(parents=True, exist_ok=True)
    previous = sorted((p for p in BUILDS.iterdir() if p.name.isdigit() and (p / "layout.json").exists()), key=lambda p: int(p.name))
    out = BUILDS / f"{(int(previous[-1].name) + 1) if previous else 1:03d}"
    python = SDK / ".venv" / "bin" / "python"
    command = [str(python if python.exists() else sys.executable), str(SDK / "sdk.py"), "build",
               "--stock-os", str(stock), "--out", str(out), "--profiling", args.profiling]
    for engine in engines:
        command += ["--machine", str(folder_of(engine))]
    keep = pathlib.Path(args.layout) if args.layout else previous[-1] / "layout.json" if previous else None
    if keep:
        command += ["--layout", str(keep)]
    print(f"building {', '.join(engines)} into {out}" + (f", keeping the numbers in {keep}" if keep else ""))
    result = subprocess.run(command)
    if result.returncode:
        sys.exit("The build failed; nothing was written.")

    write_presets(out, engines)
    layout = json.loads((out / "layout.json").read_text())
    print(f"\nBuilt {out}. Install these from {out / 'candidate'} (docs/DEVICE.md, step 4),")
    print("including the preset folders:")
    for item in sorted((out / "candidate").rglob("*")):
        if item.is_file():
            print(f"  {item.relative_to(out / 'candidate')}  ->  /{item.relative_to(out / 'candidate')}")
    print("\nLayout (engine and page numbers):")
    print(json.dumps(layout, indent=1)[:2000])


if __name__ == "__main__":
    main()
