#!/usr/bin/env python3
"""Checks each engine's screen against the track's amp envelope
(tools/check_envelope.cpp, docs/SCREEN.md "Following the amp envelope").

    python3 tools/check_envelope.py --sdk <open-xy checkout>            # every synth and MD E12
    python3 tools/check_envelope.py --sdk <open-xy checkout> mmsaw mde12
    python3 tools/check_envelope.py --sdk <open-xy checkout> --record   # only from screens known to be right

With the envelope unknown each screen must draw exactly the frames recorded in
tools/envelope_frames.json, from the screens before they read the envelope.
Exits 1 if any engine draws other frames, moves at rest, changes more than 25
nodes a frame on average, or keeps a pluck moving after its sound has gone.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
RECORD = ROOT / "tools" / "envelope_frames.json"
# MD KIT doesn't read the envelope yet; it is changed elsewhere.
ENGINES = sorted(p.name for p in (ROOT / "machines").iterdir() if p.name.startswith("mm")) + ["mde12"]


def run(engine, sdk, machines, out, cxx):
    folder = machines / engine
    kit = json.loads((folder / "machine.json").read_text())["kind"] == "drum"
    exe = out / f"envelope-{engine}"
    flags = [cxx, "-std=c++11", "-O2", "-fwrapv", "-w", f"-I{sdk}/sdk/include", f"-I{folder}", f"-DENGINE={engine}"]
    if kit:
        flags.append("-DKIT")
    subprocess.run([*flags, str(ROOT / "tools" / "check_envelope.cpp"), "-o", str(exe)], check=True)
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    return result.returncode, result.stdout


def main():
    a = argparse.ArgumentParser()
    a.add_argument("engines", nargs="*")
    a.add_argument("--sdk", default=str(ROOT.parent / "open-xy"))
    a.add_argument("--machines", default=str(ROOT / "machines"))
    a.add_argument("--out", default=str(ROOT / "local"))
    a.add_argument("--cxx", default="c++")
    a.add_argument("--record", action="store_true", help="write the unknown envelope's frames as the reference")
    args = a.parse_args()
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    recorded = json.loads(RECORD.read_text()) if RECORD.exists() else {}
    failed = False
    for engine in args.engines or ENGINES:
        code, text = run(engine, args.sdk, pathlib.Path(args.machines), out, args.cxx)
        frames = re.search(r"^unknown ([0-9a-f]{16})$", text, re.M).group(1)
        print(engine)
        print("".join(line + "\n" for line in text.splitlines() if not line.startswith("unknown")), end="")
        if args.record:
            recorded[engine] = frames
            continue
        if recorded.get(engine) != frames:
            print(f"  FAIL: with the envelope unknown the frames differ from the record ({frames}, recorded {recorded.get(engine)})")
            failed = True
        else:
            print("  unknown envelope: the recorded frames, unchanged")
        failed = failed or code != 0
    if args.record:
        RECORD.write_text(json.dumps(dict(sorted(recorded.items())), indent=2) + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
