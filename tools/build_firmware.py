#!/usr/bin/env python3
"""Builds OP-XY firmware with every engine in machines/, ready to install.

    python3 tools/build_firmware.py                 # every engine
    python3 tools/build_firmware.py mdtrx mmsaw     # some
    python3 tools/build_firmware.py mdtrx ../mine/machine  # a machine folder outside machines/

It runs the SDK's `build` against your own stock OS 1.2.0, copied off your
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
SDK = ROOT.parent / "open-xy"
SUPPORT = pathlib.Path.home() / "Library" / "Application Support"
STOCK_OS = SUPPORT / "OPXY-SDK" / "private" / "stock-1.2.0" / "os.ldr"
BUILDS = SUPPORT / "OPXY-MD" / "builds"


def preset(entry, amp, params):
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
        "octave": 0, "platform": "OP-XY", "regions": [], "type": entry["type"], "version": 4,
    }


def folder_of(engine):
    """An engine name in machines/, or a machine folder given as a path."""
    return (ROOT / engine).resolve() if "/" in engine else ROOT / "machines" / engine


def write_presets(out, engines):
    """content/presets/<engine>/: default.json, the engine's catalog defaults, and
    one preset per definition preset. Returns the files written."""
    catalog = json.loads((out / "candidate" / "content" / "op1_factory.json").read_text())
    entries = {e["type"]: e for e in catalog["engine_types"]}
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
        for name, params in files.items():
            path = target / f"{name}.json"
            path.write_text(json.dumps(preset(entries[engine], amp, params), indent=2) + "\n")
            written.append(path)
    return written


def main():
    parser = argparse.ArgumentParser(description="Build OP-XY firmware with this repository's engines")
    parser.add_argument("engines", nargs="*", help="engine folders in machines/ (default: all)")
    parser.add_argument("--stock-os", default=str(STOCK_OS), help="your OP-XY's own OS 1.2.0 os.ldr")
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
