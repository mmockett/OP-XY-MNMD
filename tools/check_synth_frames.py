#!/usr/bin/env python3
"""Sweeps a Monomachine synth's screen through every value of every control
(tools/check_synth_frames.cpp), with the definition's defaults.

    python3 tools/check_synth_frames.py --sdk <open-xy checkout> mmsaw mmsid
    python3 tools/check_synth_frames.py --sdk <open-xy checkout> --machines <folder> mmsaw

Exits 1 if any frame breaks the device's outline rules.
"""
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def build_and_run(engine, sdk, machines, out):
    folder = machines / engine
    d = json.loads((folder / "definition.json").read_text())
    parameters = d["parameters"]
    one_row = len(parameters) <= 4 and not d.get("press")
    entries = [(p["label"].replace(" ", "-"), k, round(p["default"] * 2147483647))
               for k, p in enumerate(parameters) if not p.get("absent")]
    controls = ",".join(f'{{"{n}",{k},{v}}}' for n, k, v in entries)
    exe = out / f"check-{engine}"
    flags = ["c++", "-std=c++11", "-O2", "-fwrapv", "-w", f"-I{sdk}/sdk/include", f"-I{folder}", f"-DENGINE={engine}", f"-DCONTROLS={controls}"]
    if not one_row:
        flags.append("-DPAGED")
    subprocess.run([*flags, str(ROOT / "tools" / "check_synth_frames.cpp"), "-o", str(exe)], check=True)
    print(engine, flush=True)
    return subprocess.run([str(exe)]).returncode


def main():
    a = argparse.ArgumentParser()
    a.add_argument("engines", nargs="+")
    a.add_argument("--sdk", default=str(ROOT.parent / "open-xy"))
    a.add_argument("--machines", default=str(ROOT / "machines"))
    a.add_argument("--out", default=str(ROOT / "local"))
    args = a.parse_args()
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    return 1 if any([build_and_run(e, args.sdk, pathlib.Path(args.machines), out) for e in args.engines]) else 0


if __name__ == "__main__":
    sys.exit(main())
