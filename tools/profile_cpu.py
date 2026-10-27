#!/usr/bin/env python3
"""Measures the catalog-ready engines' CPU on an OP-XY running OS 1.2.1, for
the Open-XY catalog's `cpu` records (docs/CATALOG.md, "Measuring CPU").

    python3 tools/profile_cpu.py stock          # copy the device's stock 1.2.1 os.ldr (in "mtp mode active")
    python3 tools/profile_cpu.py build          # a profiling build of the engines, from upstream's SDK
    python3 tools/profile_cpu.py install        # show what would be copied; --copy to copy it
    python3 tools/profile_cpu.py run            # one benchmark over USB MIDI (booted normally); run it three times
    python3 tools/profile_cpu.py report         # each engine's catalog-cpu.json from the runs

Each command works on the latest build in ~/Library/Application Support/
OPXY-MD/cpu (or the one --build names). The build must come from the commit
that the catalog will pin, so build refuses uncommitted machine changes.

Unlike tools/install_firmware.py, install deletes nothing: an SDK package has
no presets, so it leaves every preset on the device as it is. Writing the OS
to the boot partition stays on the device: track 2 ("exit"), then track 5.
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

from install_firmware import DEVICE, mtp, sha256

ROOT = pathlib.Path(__file__).resolve().parents[1]
SUPPORT = pathlib.Path.home() / "Library" / "Application Support"
STOCK = SUPPORT / "OPXY-SDK" / "private" / "stock-1.2.1" / "os.ldr"
WORK = SUPPORT / "OPXY-MD" / "cpu"
# The engines the catalog can take (docs/CATALOG.md); CI's upstream job checks the same list.
READY = ["mmdyn", "mmens", "mmpar", "mmpulse", "mmsid", "mmstat", "mmvo6"]
# Upstream's SDK, with the .group fix until etcetc000/open-xy merges it.
SDK = ROOT.parent / "open-xy-group"
PYTHON = ROOT.parent / "open-xy" / ".venv" / "bin" / "python"


def sdk(*args):
    print("$ sdk.py " + " ".join(map(str, args)))
    if subprocess.run([str(PYTHON), "sdk.py", *map(str, args)], cwd=SDK).returncode:
        sys.exit("sdk.py failed.")


def numbered(prefix):
    """The folders WORK/<prefix>-NNN, oldest first."""
    return sorted(WORK.glob(f"{prefix}-[0-9][0-9][0-9]")) if WORK.exists() else []


def fresh(prefix):
    found = numbered(prefix)
    return WORK / f"{prefix}-{int(found[-1].name[-3:]) + 1 if found else 1:03d}"


def package(args):
    found = [WORK / args.build] if args.build else numbered("build")
    if not found or not (found[-1] / "candidate" / "os.ldr").exists():
        sys.exit(f"No profiling build in {WORK}: run build first.")
    return found[-1]


def plan(candidate):
    """Device folder and its files, in copy order: an SDK package has the OS,
    the instrument menu and the synths' screens."""
    screens = sorted(p.relative_to(candidate).as_posix() for p in (candidate / "content" / "display").glob("*.svg"))
    return [("/", ["os.ldr"]), ("/content", ["content/op1_factory.json"]), ("/content/display", screens)]


def one_device():
    if not shutil.which("aft-mtp-cli"):
        sys.exit("aft-mtp-cli is missing: brew install android-file-transfer-linux")
    listed = [line.strip() for line in subprocess.run(["aft-mtp-cli", "-d", DEVICE, "-l"], capture_output=True,
                                                      text=True).stdout.split("\n") if line.strip()]
    if len(listed) != 1:
        sys.exit(f"Expected one {DEVICE} over MTP, found {len(listed)}. Open \"mtp mode active\" and close OpenMTP.")


def stock(args):
    expected = json.loads((SDK / "profiles" / "opxy-1.2.1.json").read_text())["stock_sha256"]
    if STOCK.exists() and sha256(STOCK) == expected:
        print(f"Already have the stock OS 1.2.1: {STOCK}")
        return
    one_device()
    with tempfile.TemporaryDirectory() as temp:
        mtp(pathlib.Path(temp), ["get /upgrade/firmware/blackfin/bin/os.ldr os.ldr"])
        got = pathlib.Path(temp) / "os.ldr"
        if sha256(got) != expected:
            sys.exit("The device's upgrade os.ldr isn't stock OS 1.2.1. Install the official 1.2.1 update and try again.")
        STOCK.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(got, STOCK)
    print(f"Saved the stock OS 1.2.1 in {STOCK}")


def build(args):
    if not STOCK.exists():
        sys.exit(f"No {STOCK}: run stock first.")
    dirty = subprocess.run(["git", "status", "--porcelain", "--", "machines"], cwd=ROOT, capture_output=True,
                           text=True).stdout
    if dirty:
        sys.exit("Commit the machines first: the catalog's cpu record names the measured commit.\n" + dirty)
    out = fresh("build")
    out.parent.mkdir(parents=True, exist_ok=True)
    sdk("build", "--profiling", "on", "--stock-os", STOCK, "--out", out,
        *[a for m in args.engines or READY for a in ("--machine", ROOT / "machines" / m)])
    commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    (out / "mnmd-commit.txt").write_text(commit + "\n")
    print(f"\nBuilt {out.name} from OP-XY-MNMD {commit[:7]}. Next: install --copy, in \"mtp mode active\".")


def install(args):
    build = package(args)
    candidate = build / "candidate"
    files = sorted(p for p in candidate.rglob("*") if p.is_file() and not p.name.startswith("."))
    steps = plan(candidate)
    print(f"{build.name}: {len(files)} files to the OP-XY, nothing deleted")
    for folder, entries in steps:
        for entry in entries:
            print(f"  {entry}  ->  {folder.rstrip('/')}/{pathlib.PurePosixPath(entry).name}")
    if not args.copy:
        print("\nNothing copied. With the OP-XY in \"mtp mode active\" and OpenMTP closed, add --copy.")
        return
    one_device()
    with tempfile.TemporaryDirectory() as temp:
        work = pathlib.Path(temp)
        shutil.copytree(candidate, work / "c", ignore=shutil.ignore_patterns(".*"))
        replaced = build / "replaced"
        if not replaced.exists():
            (work / "r").mkdir()
            mtp(work, ["get /os.ldr r/os.ldr", "get /content/op1_factory.json r/op1_factory.json"])
            shutil.copytree(work / "r", replaced)
            print(f"Saved the device's os.ldr and op1_factory.json in {replaced}")
        mtp(work, [c for folder, entries in steps for c in (f"cd {folder}", *[f"put c/{e}" for e in entries])])
        for path in files:
            (work / "v" / path.relative_to(candidate)).parent.mkdir(parents=True, exist_ok=True)
        mtp(work, [f"get /{p.relative_to(candidate).as_posix()} v/{p.relative_to(candidate).as_posix()}" for p in files])
        wrong = [p.relative_to(candidate).as_posix() for p in files
                 if not (work / "v" / p.relative_to(candidate)).exists() or sha256(work / "v" / p.relative_to(candidate)) != sha256(p)]
    if wrong:
        sys.exit("These files read back differently; copy them again before writing the OS:\n  " + "\n  ".join(wrong))
    print(f"All {len(files)} files read back exactly. On the OP-XY: track 2 (exit), then track 5 (write os to boot "
          "partition), then switch it off and on and boot normally.")


def run(args):
    build = package(args)
    out = fresh("run")
    print("The OP-XY must be booted normally with this build, on USB, project saved and playback stopped. "
          "Don't touch it while the run is active.")
    sdk("profile", "benchmark", "--package", build, "--installed", "--greet", "--out", out)
    print(f"\nSaved {out.name}. Three runs, then report.")


def report(args):
    build = package(args)
    captures = [r / "capture.json" for r in numbered("run") if (r / "capture.json").exists()]
    captures = captures[-args.runs:]
    if len(captures) < 3:
        sys.exit(f"Found {len(captures)} runs with a capture; the catalog needs at least three.")
    out = fresh("report")
    for engine in args.engines or READY:
        sdk("profile", "report", "--package", build, "--machine", engine,
            *[a for c in captures for a in ("--capture", c)], "--out", out / engine)
    for engine in args.engines or READY:
        cpu = json.loads((out / engine / "catalog-cpu.json").read_text())["cpu"]
        print(f"{engine}: {cpu['block_percent']:.3f}% of the block budget over {cpu['runs']} runs")
    print(f"\nEach engine's catalog-cpu.json is in {out}.")


def main():
    parser = argparse.ArgumentParser(description="Measure the catalog-ready engines' CPU on an OP-XY")
    parser.add_argument("command", choices=["stock", "build", "install", "run", "report"])
    parser.add_argument("engines", nargs="*", help=f"engine folders in machines/ (default: {' '.join(READY)})")
    parser.add_argument("--build", help="a build folder's name in the cpu folder (default: the latest)")
    parser.add_argument("--copy", action="store_true", help="install: connect and copy (otherwise only list)")
    parser.add_argument("--runs", type=int, default=3, help="report: how many of the latest runs to use")
    args = parser.parse_args()
    globals()[args.command](args)


if __name__ == "__main__":
    main()
