#!/usr/bin/env python3
"""Copies a firmware build onto the OP-XY over MTP, for docs/DEVICE.md, step 4.

    python3 tools/install_firmware.py            # the latest build: show what would be copied
    python3 tools/install_firmware.py 006        # a given build
    python3 tools/install_firmware.py --copy     # copy it: the OP-XY must be in "mtp mode active"

It needs aft-mtp-cli (`brew install android-file-transfer-linux`). Without
--copy it never connects. With --copy it saves the device's own os.ldr and
op1_factory.json into the build's `replaced/` folder, copies every file of
the build's `candidate/` to the same path on the OP-XY, replacing those files
and keeping everything else, then reads each one back and checks it against
the build. Writing the OS to the boot partition stays on the device: track 2
("exit"), then track 5.
"""
import argparse
import hashlib
import pathlib
import shutil
import subprocess
import sys
import tempfile

BUILDS = pathlib.Path.home() / "Library" / "Application Support" / "OPXY-MD" / "builds"
REPLACED = ("os.ldr", "content/op1_factory.json")
# aft-mtp-cli's device filter; other MTP devices (a phone) may be connected too.
DEVICE = "OP-XY"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def mtp(work, commands):
    """One aft-mtp-cli session in work; relative paths there avoid the spaces in
    the build path. It stops at the first failing command."""
    result = subprocess.run(["aft-mtp-cli", "-d", DEVICE, *commands], cwd=work, capture_output=True, text=True)
    if result.returncode:
        sys.exit(f"aft-mtp-cli failed:\n{result.stdout}{result.stderr}")


def plan(candidate):
    """Device folder and its entries, in copy order: single files by path, each
    preset folder whole, so the device's other presets stay."""
    steps = [("/", ["os.ldr"]), ("/content", ["content/op1_factory.json"])]
    def entries(folder, directories):
        return sorted(p.relative_to(candidate).as_posix() for p in (candidate / "content" / folder).iterdir()
                      if p.is_dir() == directories and not p.name.startswith("."))
    displays = entries("display", False)
    if displays:
        steps.append(("/content/display", displays))
    presets = entries("presets", True)
    if presets:
        steps.append(("/content/presets", presets))
    return steps


def main():
    parser = argparse.ArgumentParser(description="Copy a firmware build onto the OP-XY over MTP")
    parser.add_argument("build", nargs="?", help="build number in the builds folder (default: the latest)")
    parser.add_argument("--copy", action="store_true", help="connect and copy (otherwise only list)")
    args = parser.parse_args()

    builds = sorted((p for p in BUILDS.iterdir() if p.name.isdigit()), key=lambda p: int(p.name)) if BUILDS.exists() else []
    if not builds:
        sys.exit(f"No builds in {BUILDS}: run tools/build_firmware.py first.")
    build = BUILDS / args.build.zfill(3) if args.build else builds[-1]
    candidate = build / "candidate"
    if not (candidate / "os.ldr").exists() or not (candidate / "content" / "op1_factory.json").exists():
        sys.exit(f"{build} has no complete candidate/ folder.")
    files = sorted(p for p in candidate.rglob("*")
                   if p.is_file() and not any(part.startswith(".") for part in p.relative_to(candidate).parts))
    steps = plan(candidate)

    print(f"Build {build.name}: {len(files)} files to the OP-XY")
    for folder, entries in steps:
        for entry in entries:
            print(f"  {entry}  ->  {folder.rstrip('/')}/{pathlib.PurePosixPath(entry).name}{'/' if (candidate / entry).is_dir() else ''}")
    if not args.copy:
        print("\nNothing copied. With the OP-XY in \"mtp mode active\" and OpenMTP closed, add --copy.")
        return

    if not shutil.which("aft-mtp-cli"):
        sys.exit("aft-mtp-cli is missing: brew install android-file-transfer-linux")
    listed = subprocess.run(["aft-mtp-cli", "-d", DEVICE, "-l"], capture_output=True, text=True).stdout.split("\n")
    listed = [line.strip() for line in listed if line.strip()]
    if len(listed) != 1:
        sys.exit(f"Expected one {DEVICE} over MTP, found {len(listed)}: {' '.join(listed) or 'none'}.\n"
                 "Open \"mtp mode active\" on the OP-XY (docs/DEVICE.md, step 2) and close OpenMTP.")
    print(f"\nDevice: {listed[0]}")

    with tempfile.TemporaryDirectory() as temp:
        work = pathlib.Path(temp)
        # Finder's .DS_Store files must not reach the device.
        shutil.copytree(candidate, work / "c", ignore=shutil.ignore_patterns(".*"))
        replaced = build / "replaced"
        if replaced.exists():
            print(f"Keeping the device files saved by the first install of this build, in {replaced}")
        else:
            (work / "r").mkdir()
            mtp(work, [f"get /{name} r/{pathlib.PurePosixPath(name).name}" for name in REPLACED])
            shutil.copytree(work / "r", replaced)
            print(f"Saved the device's os.ldr and op1_factory.json in {replaced}")

        commands = []
        for folder, entries in steps:
            commands.append(f"cd {folder}")
            commands += [f"put c/{entry}" for entry in entries]
        mtp(work, commands)
        print("Copied.")

        for path in files:
            (work / "v" / path.relative_to(candidate)).parent.mkdir(parents=True, exist_ok=True)
        mtp(work, [f"get /{p.relative_to(candidate).as_posix()} v/{p.relative_to(candidate).as_posix()}" for p in files])
        wrong = [p.relative_to(candidate).as_posix() for p in files
                 if not (work / "v" / p.relative_to(candidate)).exists() or sha256(work / "v" / p.relative_to(candidate)) != sha256(p)]
    if wrong:
        sys.exit("These files read back differently; copy them again before writing the OS:\n  " + "\n  ".join(wrong))
    print(f"All {len(files)} files read back exactly. On the OP-XY: track 2 (exit), then track 5 (write os to boot partition).")


if __name__ == "__main__":
    main()
