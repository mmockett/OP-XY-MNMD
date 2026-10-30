#!/usr/bin/env python3
"""How far each kit control moves its part of the screen: in proportion to how
much it changes the sound, never less than a visible minimum.

    python3 tools/art/travel.py              # both kits: measure, then write their tables into voices/mdkit_view.hpp
    python3 tools/art/travel.py mde12        # one kit
    python3 tools/art/travel.py --show       # print the measures only

MD E12 plays ROM samples: its measures need the capture from this Mac's
private folder (docs/FAMILIES.md), which the harness finds as test_voices.py
does; only the measured travel, never sample data, goes into the table. With
the silent stand-in instead, E12's controls measure nothing and the tool
refuses to write its table.

Each kit member is played through the fixed-case harness (voices/tests/cases.hpp)
with one control at 0, 32, 64, 96 and 127, in two settings of the others: the
machine's defaults, and every control at 64 (so a control that only matters once
another is up, such as EFM's MFRQ under MOD, is heard). A step's change is the
mean difference in band energy (dB, 8 bands by 16 windows, the windows weighted
by loudness, bands floored 60 dB under the case's loudest) between neighbouring
values; a control's change is the larger of its two sums. Full travel is FULL_DB
or more; less travel is proportional, down to MINIMUM, so every control still
reads at a glance.
"""
import collections
import json
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from sdk_path import SDK_INCLUDE  # noqa: E402
import catalog  # noqa: E402
from make_engine import defaults, slug, sound_header  # noqa: E402
from test_voices import private_includes  # noqa: E402

FULL_DB = 24.0     # a change this large or larger moves its part all the way
MINIMUM = 0.2      # the least travel, a fifth of the full
STEPS = (0, 32, 64, 96, 127)
VIEW = ROOT / "voices" / "mdkit_view.hpp"


def play(out, kit):
    headers, calls = [], []
    for name in catalog.ENGINES[kit]:
        header = f'#include "{sound_header(name)}.hpp"'
        if header not in headers:
            headers.append(header)
        ns = f"md::{slug(name)}"
        labels = catalog.MACHINES[name][2]
        base = defaults(name)
        middle = [64 if s else 0 for s in labels]
        for setting, values in (("d", base), ("m", middle)):
            for i, label in enumerate(labels):
                if not label:
                    continue
                for v in STEPS:
                    moved = list(values)
                    moved[i] = v
                    calls.append(f'    {{ const int v[8] = {{{", ".join(map(str, moved))}}}; '
                                 f'cases::measure<{ns}::State>("{name}", "{setting}|{i}|{v}", v, {ns}::reset, {ns}::trigger, '
                                 f'{ns}::render); }}')
    out.mkdir(parents=True, exist_ok=True)
    (out / f"travel_{kit}.cpp").write_text("\n".join(headers) + '\n#include "tests/cases.hpp"\nint main() {\n' + "\n".join(calls) + "\n}\n")
    subprocess.run(["c++", "-std=c++11", "-O2", f"-I{SDK_INCLUDE}", f"-I{ROOT / 'lib'}", f"-I{ROOT / 'voices'}",
                    *private_includes(), str(out / f"travel_{kit}.cpp"), "-o", str(out / f"travel_{kit}")], check=True)
    lines = subprocess.run([str(out / f"travel_{kit}")], capture_output=True, text=True, check=True).stdout.splitlines()
    return [json.loads(line) for line in lines]


def change(a, b):
    """Mean band-energy difference between two cases, in dB, loud windows weighted most."""
    top = max(max(max(w) for w in a["bands"]), max(max(w) for w in b["bands"]))
    floor = top - 60
    num = den = 0.0
    for wa, wb in zip(a["bands"], b["bands"]):
        loud = max(max(wa), max(wb))
        if loud < floor:
            continue
        weight = 10 ** ((loud - top) / 40)
        num += weight * sum(abs(max(x, floor) - max(y, floor)) for x, y in zip(wa, wb)) / len(wa)
        den += weight
    return num / den if den else 0.0


def measure(out, kit):
    played = collections.defaultdict(dict)
    for m in play(out, kit):
        setting, slot, v = m["case"].split("|")
        played[(m["machine"], int(slot))][(setting, int(v))] = m
    db = {}
    for (name, slot), cases in played.items():
        sums = [sum(change(cases[(s, a)], cases[(s, b)]) for a, b in zip(STEPS, STEPS[1:])) for s in ("d", "m")]
        db[(name, slot)] = max(sums)
    return db


# Each kit's table and the markers it sits between in voices/mdkit_view.hpp.
TABLES = {"mdkit": ("travel", "travel"), "mde12": ("travel_e12", "travel-e12")}


def main():
    out = pathlib.Path(os.environ.get("TRAVEL_OUT", ROOT / "local" / "travel"))
    kits = [a for a in sys.argv[1:] if not a.startswith("--")] or list(TABLES)
    for kit in kits:
        db = measure(out, kit)
        rows = []
        for name in catalog.ENGINES[kit]:
            labels = catalog.MACHINES[name][2]
            k = [0 if not labels[slot] else round(max(MINIMUM, min(1.0, db[(name, slot)] / FULL_DB)) * 256) for slot in range(8)]
            rows.append((name, k, [f"{labels[s]} {db[(name, s)]:.1f}" for s in range(8) if labels[s]]))
        for name, k, shown in rows:
            print(f"{name:7s} " + "  ".join(shown))
        if "--show" in sys.argv:
            continue
        if max(db.values()) < 0.01:
            sys.exit(f"{kit}: every control measured silent (the silent stand-in?); its table is left as it was")
        table_name, marker = TABLES[kit]
        table = [f"// {marker}:begin (tools/art/travel.py writes this table: edit the tool, then run it)",
                 "// How far each control moves its part, Q8 (256 = all the way), in proportion to",
                 f"// the change it makes to the sound (full at {FULL_DB:.0f} dB of band energy, at least {MINIMUM}).",
                 f"static const opxy::u16 {table_name}[{len(rows)}][8] = {{"]
        table += [f"    {{{', '.join(str(v) for v in k)}}},   // {name}: {', '.join(shown)}" for name, k, shown in rows]
        table += ["};", f"// {marker}:end"]
        text = VIEW.read_text()
        text = re.sub(rf"// {marker}:begin.*?// {marker}:end", lambda _: "\n".join(table), text, flags=re.S)
        VIEW.write_text(text)
        print(f"wrote {kit}'s travel table into {VIEW.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
