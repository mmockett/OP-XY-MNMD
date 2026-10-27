"""Renders Machinedrum hits on demand, cached by name, for the fitting scripts.

    from probe import render
    x = render("bd-ramp-064", 16, 1.0, [40, 64, 64, 64, 0, 0, 0, 0])

Hits are kept in local/ref/probe-md/<name>.wav; a hit already there with the
same settings is not rendered again. Batch calls with render_many.
"""
import json
import os
import pathlib
import subprocess

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
DIR = ROOT / "local" / "ref" / "probe-md"
ROMS = {"md": os.environ.get("MD_ROM", str(pathlib.Path.home() / "Downloads" / "elektron_sps1-1uw_os1.63.bin")),
        "mm": os.environ.get("MM_ROM", str(pathlib.Path.home() / "Downloads" / "elektron_sfx6-60_os1.32b.bin"))}
ROM = ROMS["md"]


def _read(path):
    return np.frombuffer(path.read_bytes()[44:], dtype="<f4").astype(np.float64)


def render_many(jobs, instrument="md"):
    """jobs: [(name, machine id, seconds, [8 values], options...)] -> {name: samples}.
    `instrument` picks the ROM: "md" the Machinedrum, "mm" the Monomachine. An
    optional fifth item is extra mdref options, such as "gate=0.5 cc56=0"."""
    DIR.mkdir(parents=True, exist_ok=True)
    index_path = DIR / "index.json"
    index = json.loads(index_path.read_text()) if index_path.exists() else {}
    todo = []
    for job in jobs:
        name, machine, seconds, p = job[:4]
        options = job[4] if len(job) > 4 else ""
        key = [instrument, machine, seconds, list(p), options]
        if index.get(name) != key or not (DIR / f"{name}.wav").exists():
            todo.append(f"{name} {machine} {seconds} " + " ".join(str(int(v)) for v in p) + (" " + options if options else ""))
            index[name] = key
    if todo:
        mdref = ROOT / "local" / "mdref"
        if not mdref.exists():
            subprocess.run(["sh", str(ROOT / "reference" / "build.sh")], check=True)
        subprocess.run([str(mdref), ROMS[instrument], str(DIR)], input="\n".join(todo) + "\n", text=True, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        index_path.write_text(json.dumps(index))
    return {job[0]: _read(DIR / f"{job[0]}.wav") for job in jobs}


def render(name, machine, seconds, p):
    return render_many([(name, machine, seconds, p)])[name]


def render_machine(name, jobs):
    """The same jobs through our voice module for machine `name` (tools/catalog.py),
    machine id ignored; job values are the machine's eight slots, and a job's
    "note=N" option plays that MIDI note (middle C otherwise) -> {name: samples}."""
    import sys
    sys.path.insert(0, str(ROOT / "tools"))
    from make_engine import slug, sound_header
    from test_voices import private_includes
    voice, header = slug(name), sound_header(name)
    out = ROOT / "local" / "ref" / f"probe-{voice}"
    out.mkdir(parents=True, exist_ok=True)
    hits = ROOT / "local" / f"hits-{voice}"
    newest = max(p.stat().st_mtime for p in [*(ROOT / "voices").glob("**/*.hpp"), *(ROOT / "lib").glob("*.hpp"),
                                              ROOT / "reference" / "render_hits.cpp",
                                              *pathlib.Path.home().joinpath("Library", "Application Support", "OPXY-MD", "private").glob("*/*.hpp")])
    if not hits.exists() or hits.stat().st_mtime < newest:
        subprocess.run(["c++", "-std=c++17", "-O2", f"-I{ROOT.parent / 'open-xy' / 'sdk' / 'include'}",
                        f"-I{ROOT / 'lib'}", f"-I{ROOT / 'voices'}", *private_includes(), f'-DHEADER="{header}.hpp"', f"-DVOICE={voice}",
                        str(ROOT / "reference" / "render_hits.cpp"), "-o", str(hits)], check=True)
    def note(job):
        found = [o[5:] for o in (job[4] if len(job) > 4 else "").split() if o.startswith("note=")]
        return f" {found[0]}" if found else ""
    lines = [f"{job[0]} {job[2]} " + " ".join(f"{v:.3f}" for v in job[3]) + note(job) for job in jobs]
    subprocess.run([str(hits), str(out)], input="\n".join(lines) + "\n", text=True, check=True)
    return {job[0]: _read(out / f"{job[0]}.wav") for job in jobs}
