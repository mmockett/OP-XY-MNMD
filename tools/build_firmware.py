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
from make_engine import KIT_FAMILY, control_count, kit_keys  # noqa: E402
from sdk_path import SDK, stock_version  # noqa: E402

SUPPORT = pathlib.Path.home() / "Library" / "Application Support"
# The stock OS the SDK's newest platform patches.
STOCK_OS = SUPPORT / "OPXY-SDK" / "private" / f"stock-{stock_version()}" / "os.ldr"
BUILDS = SUPPORT / "OPXY-MD" / "builds"


def kit_value(model, controls, counts):
    """The 60 bits a key stores (the SDK's drum_storage.hpp). A kit of up to 16 models:
    the eight controls (7 bits each), then the model (4). A larger one, by its models'
    control counts: a model with eight stores its rank among those in the top four
    bits over its eight controls; any other 15 there, its model in bits 49..54 and
    its first seven controls."""
    value = sum(c << (7 * i) for i, c in enumerate(controls))
    if len(counts) <= 16:
        return value | model << 56
    full = [m for m, n in enumerate(counts) if n == 8]
    assert len(full) <= 15, f"{len(full)} machines with eight controls; a kit of more than 16 holds 15"
    if model in full:
        return value | full.index(model) << 56
    return (value & ((1 << 49) - 1)) | model << 49 | 15 << 56


def kit_region(key, model, controls, choked, counts=(8,) * 16):
    """A kit key as a preset region, as the SDK stores it (the SDK's docs/abi-<OS version>.md,
    the drum kind): "fm slap" is Drumline type 7, the SDK's mark; the 60 bits
    (kit_value) fill 15 bits of each param; the play mode mute1 chokes, and
    oneshot, the default, does not."""
    value = kit_value(model, controls, counts)
    params = [(value >> (15 * w)) & 0x7fff for w in range(4)]
    region = {"hikey": key, "model": "fm slap", "params": params}
    return {**region, "playmode": "mute1"} if choked else region


def kit_regions(engine, kit=None):
    """A drum engine's 24 keys from one of its kits (its first, unless named)."""
    counts = [control_count(name) for name in catalog.ENGINES[engine]]
    return [kit_region(53 + k, model, controls, role in catalog.CHOKED, counts)
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
        # An engine of several kits names each in a preset, its first too (trx, efm,
        # pi), so every family is in the list; default is the first.
        for kit in kits if len(kits) > 1 else []:
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
