#!/usr/bin/env python3
"""Sweeps a Monomachine synth's screen through every value of every control
(tools/check_synth_frames.cpp), with the definition's defaults.

    python3 tools/check_synth_frames.py --sdk <open-xy checkout> mmsaw mmsid
    python3 tools/check_synth_frames.py --sdk <open-xy checkout> --machines <folder> mmsaw
    python3 tools/check_synth_frames.py --sdk <open-xy checkout> --amp pad mmsaw   # under the track's amp envelope

Exits 1 if any frame breaks the device's outline rules.
"""
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
# Amp envelopes to sweep under (Q31 attack, decay, sustain, release); unknown is
# what a host before SDK 1.10 reports, and what these screens drew before it.
AMPS = {"unknown": None, "pluck": (0, 0.1, 0, 0), "pad": (0.45, 0.5, 0.7, 0.6)}


def build_and_run(engine, sdk, machines, out, amp="unknown"):
    folder = machines / engine
    d = json.loads((folder / "definition.json").read_text())
    parameters = d["parameters"]
    one_row = len(parameters) <= 4 and not d.get("press")
    entries = [(p["label"].replace(" ", "-"), k, round(p["default"] * 2147483647))
               for k, p in enumerate(parameters) if not p.get("absent")]
    controls = ",".join(f'{{"{n}",{k},{v}}}' for n, k, v in entries)
    exe = out / f"check-{engine}-{amp}"
    flags = ["c++", "-std=c++11", "-O2", "-fwrapv", "-w", f"-I{sdk}/sdk/include", f"-I{folder}", f"-DENGINE={engine}", f"-DCONTROLS={controls}"]
    if not one_row:
        flags.append("-DPAGED")
    if AMPS[amp]:
        flags.append("-DAMP=" + ",".join(str(min(round(x * 2**31), 2**31 - 1)) for x in AMPS[amp]))
    subprocess.run([*flags, str(ROOT / "tools" / "check_synth_frames.cpp"), "-o", str(exe)], check=True)
    print(engine if amp == "unknown" else f"{engine}, {amp} envelope", flush=True)
    return subprocess.run([str(exe)]).returncode


def main():
    a = argparse.ArgumentParser()
    a.add_argument("engines", nargs="+")
    a.add_argument("--sdk", default=str(ROOT.parent / "open-xy"))
    a.add_argument("--machines", default=str(ROOT / "machines"))
    a.add_argument("--out", default=str(ROOT / "local"))
    a.add_argument("--amp", choices=sorted(AMPS), default="unknown", help="the track's amp envelope")
    args = a.parse_args()
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    return 1 if any([build_and_run(e, args.sdk, pathlib.Path(args.machines), out, args.amp) for e in args.engines]) else 0


if __name__ == "__main__":
    sys.exit(main())
