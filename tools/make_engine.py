#!/usr/bin/env python3
"""Generates the OP-XY engines from the catalog (tools/catalog.py) and the voice
modules (voices/, docs/VOICES.md).

    python3 tools/make_engine.py            # every engine whose machines exist
    python3 tools/make_engine.py mdtrx mmsaw

For each engine it writes machines/<engine>/: machine.json, definition.json,
core.h, engine.cpp, display.h, display.cpp, the artwork (tools/art/build.mjs),
tests/sound.cpp and, for a kit, kit.h; then it copies in the library and module
headers they include. An engine with a machine whose module does not exist yet
is skipped, and reported.
A module that includes private_<name>.hpp (samples captured from the owner's
ROMs, docs/FAMILIES.md) gets the capture from ~/Library/Application Support/
OPXY-MD/private/*/, or else the silent stand-in in voices/silent.
docs/VOICES.md has the contract.
"""
import json
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import catalog  # noqa: E402
from presets import PRESETS  # noqa: E402
from sdk_path import SDK_INCLUDE  # noqa: E402

VOICES = ROOT / "voices"
# Samples and waves captured from the owner's ROMs (docs/FAMILIES.md): never in git.
PRIVATE = pathlib.Path.home() / "Library" / "Application Support" / "OPXY-MD" / "private"


def slug(name):
    return re.sub(r"[^a-z0-9]+", "_", name.lower().replace("+", "p")).strip("_")


# Modules that hold more than one machine.
SOUND_HEADER = {"TRX-CH": "trx_hat", "TRX-OH": "trx_hat", "TRX-XT": "trx_tom", "TRX-XC": "trx_tom",
                "PI-RC": "pi_cymbal", "PI-CC": "pi_cymbal", "PI-HH": "pi_cymbal",
                "SWAVE-SAW": "swave", "SWAVE-PULSE": "swave", "SWAVE-ENS": "swave",
                "FM+STAT": "fmp", "FM+PAR": "fmp", "FM+DYN": "fmp",
                "DPRO-WAVE": "dpro", "DPRO-DDRW": "dpro", "DPRO-DENS": "dpro",
                **{n: "e12" for n in catalog.MACHINES if n.startswith("E12-")}}
# The view header of each Monomachine family: instrument, family -> header.
# (The drum kits draw with trx_view and mdkit_view.)
VIEW_HEADER = {("mm", "SWAVE"): "swave_view", ("mm", "SID"): "sid_view",
               ("mm", "DPRO"): "dpro_view", ("mm", "FM+STAT"): "fm_view", ("mm", "FM+PAR"): "fm_view",
               ("mm", "FM+DYN"): "fm_view", ("mm", "VO"): "vo_view", ("mm", "GND"): "mgnd_view"}
# Stepped controls, shown as their steps' names: machine -> {label: names}. The
# original's 0..127 falls on them evenly (v * len >> 7, read off its LCD), and
# presets.py keeps the original's 0..127, placed at the middle of each step.
ENUMS = {"VO-6": {"V-SW": ["OFF", "ON"],
                  "CONS": ["-", "B", "D", "F", "G", "H", "J", "K", "L", "M", "N", "P", "R", "RR", "S", "SJ", "T", "TJ",
                           "TH", "V", "Z"]}}
ENUMS["SID-6581"] = {"PWRS": ["OFF", "ON"], "WAVE": ["TRI", "SAW", "PULS", "MIX", "NOIS"],
                     "MOD": ["OFF", "RING", "SYNC", "R+S"], "MSRC": ["MFRQ", "PRCH"]}
# FM+STAT and FM+PAR's listed ratios, in the order the sound steps through them
# (reference/README.md: the original's LCD shows 5/32 five steps later).
FM_RATIOS = ["1:64", "1:32", "1:16", "3:32", "1:8", "5:32", "3:16", "1:4", "5:16", "3:8", "7:16", "1:2",
             "5:8", "3:4", "7:8", "1", "1.25", "1.5", "1.75", "2", "2.5", "3", "3.5", "4"]
ENUMS.update({"FM+STAT": {"1FRQ": FM_RATIOS, "2FRQ": FM_RATIOS},
              "FM+PAR": {"1FRQ": FM_RATIOS, "2FRQ": FM_RATIOS, "3FRQ": FM_RATIOS}})
# The DigiPRO waves by number (reference/README.md: S01..S32 four steps each,
# D01..D64 two), and the even on/off and mode switches the sounds read.
OFF_ON = ["OFF", "ON"]
D_WAVES = [f"D{i:02}" for i in range(1, 65)]
ENUMS.update({"DPRO-WAVE": {"WAVE": [f"S{i:02}" for i in range(1, 33)], "WPRS": OFF_ON, "SYNC": ["OFF", "SFRQ", "PRCH"]},
              "DPRO-DDRW": {"WAV1": D_WAVES, "WAV2": D_WAVES},
              "DPRO-DENS": {"WAVE": D_WAVES},
              "SWAVE-PULSE": {"PWRS": OFF_ON},
              "GND-NOIS": {"STON": OFF_ON}})


def family(name):
    return name.split("-")[0]


def sound_header(name):
    return SOUND_HEADER.get(name, slug(name))


def view_header(name):
    return VIEW_HEADER[(catalog.MACHINES[name][0], family(name))]


def available(name):
    """A machine is built once its sound module (and, on the Monomachine, its view) defines its namespace."""
    ns = f"namespace {slug(name)} "
    headers = [sound_header(name)] + ([view_header(name)] if catalog.MACHINES[name][0] == "mm" else [])
    return all((VOICES / f"{h}.hpp").exists() and ns in (VOICES / f"{h}.hpp").read_text() for h in headers)


def short(name):
    """A machine's name for the header's value, at most 7 characters."""
    fam, rest = name.split("-", 1)
    s = (fam.replace("FM+", "FM") + rest).replace("SWAVE", "SW").replace("DPRO", "DP")
    return s[:7]


def defaults(name):
    if name in PRESETS:
        return list(PRESETS[name]["defaults"])
    # The design's defaults, by label.
    out = subprocess.run(["node", "-e", f"""
import('./tools/art/design.mjs').then(({{defaults}}) => {{
  const d = defaults({json.dumps('mnm' if catalog.MACHINES[name][0] == 'mm' else 'md')}, {json.dumps({'PI-MT': 'PI-XT'}.get(name, name))});
  console.log(JSON.stringify(d));
}});"""], capture_output=True, text=True, cwd=ROOT, check=True).stdout
    d = json.loads(out)
    alias = {"NOISE": "NOIS", "SPLEN": "SPLN", "START": "STRT", "RRTL": "RATL"}
    slots = catalog.MACHINES[name][2]
    values = [0] * 8
    for label, value in zip(d["labels"], d["values"]):
        label = alias.get(label, label)
        if label in slots:
            values[slots.index(label)] = value
    return values


def with_includes(headers):
    """The modules and every voices/ header they include, each once."""
    out = []
    def visit(h):
        if h not in out:
            out.append(h)
            for name in re.findall(r'#include "(\w+)\.hpp"', (VOICES / f"{h}.hpp").read_text()):
                if (VOICES / f"{name}.hpp").exists():
                    visit(name)
    for h in headers:
        visit(h)
    return out


# The stage's top under the two-page header (tools/art/build.mjs PAGED_STAGE_Y).
PAGED_STAGE_Y = 46
# Notes an engine sounds at once, shared by the tracks playing it. Each voice
# costs about 2-3.4% of the OP-XY's audio budget on the heaviest engines, so
# those stop at 8; the lightest, a tenth of their cost, run to 32.
VOICE_CAPACITY = {"mmens": 8, "mmdens": 8, "mmstat": 8, "mmdyn": 8, "mmpar": 8,
                  "mmsin": 32, "mmwave": 32, "mmbbox": 32, "mmnois": 32, "mmsid": 32}


def copy_scoped(source, folder, engine, paged=False):
    """Copies a library or module header into the engine with its namespace md
    made the engine's own (md_<engine>, reached as md): firmware links every
    machine together, so two engines' copies of an inline function must not
    share a name. A paged engine's stage starts under its two-row header."""
    scoped = f"namespace md_{engine} {{}} namespace md = md_{engine}; namespace md_{engine} {{"
    text = source.read_text().replace("namespace md {", scoped)
    if paged and source.name == "md_view.hpp":
        stage = re.search(r"const i32 stage_y = 30;.*", text)
        text = text.replace(stage.group(0), f"const i32 stage_y = {PAGED_STAGE_Y};   // under the two-page header")
    (folder / source.name).write_text(text)


def copy_private(headers, folder, engine):
    """Copies each private_<name>.hpp the modules include into the engine: the
    capture from this Mac's private folder, else the silent stand-in in
    voices/silent. Returns whether every one was a capture."""
    captured = True
    for h in headers:
        for name in sorted(set(re.findall(r'#include "(private_\w+\.hpp)"', (VOICES / f"{h}.hpp").read_text()))):
            found = sorted(PRIVATE.glob(f"*/{name}"))
            captured = captured and bool(found)
            copy_scoped(found[0] if found else VOICES / "silent" / name, folder, engine)
    return captured


def control(label, default, position=None, mapping=None):
    # The SDK's rules: ids start with a letter (FM+'s 1FRQ becomes p1frq); labels
    # are letters, digits and spaces (VO-6's V-SW shows as V SW).
    ident = re.sub(r"[^a-z0-9_-]", "_", label.lower().replace("+", "p"))
    entry = {"id": ident if ident[0].isalpha() else "p" + ident, "label": re.sub(r"[^A-Za-z0-9 ]", " ", label),
             "default": round(default / 127, 6), "mapping": mapping or {"kind": "linear", "minimum": 0, "maximum": 1},
             "display": "enum" if mapping else "percent"}
    if position is not None:
        entry["control"] = position
    return entry


def enum_value(name, label, value):
    """A 0..127 value of a stepped control at the middle of its step, 0..127."""
    names = ENUMS.get(name, {}).get(label)
    if not names:
        return value
    return ((value * len(names) >> 7) + 0.5) / len(names) * 127


def sizes(engine):
    """sizeof Voice, Shared and DisplayState, by compiling a probe on the host."""
    probe = ROOT / "local" / f"sizes-{engine}.cpp"
    probe.parent.mkdir(exist_ok=True)
    probe.write_text(f'#include "core.h"\n#include "display.h"\n#include <cstdio>\nint main() {{ std::printf("%zu %zu %zu", '
                     f'sizeof({engine}::Voice), sizeof({engine}::Shared), sizeof({engine}::DisplayState)); }}\n')
    exe = probe.with_suffix("")
    subprocess.run(["c++", "-std=c++11", f"-I{SDK_INCLUDE}", f"-I{ROOT / 'machines' / engine}", str(probe), "-o", str(exe)], check=True)
    return [int(v) for v in subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.split()]


def generate(engine, index):
    """A Monomachine engine: one machine, its eight controls on the encoders and their alternates."""
    (name,) = catalog.ENGINES[engine]
    if not available(name):
        return None, [name]
    folder = ROOT / "machines" / engine
    (folder / "tests").mkdir(parents=True, exist_ok=True)
    s = slug(name)
    headers = [sound_header(name), view_header(name)]
    enums = ENUMS.get(name, {})
    slots = catalog.MACHINES[name][2]
    named = [label or "" for label in (list(slots) + [""] * 8)[:8]]
    real = [i for i, label in enumerate(named) if label]
    # A machine with at most four controls has one row: no pages, on the stock
    # header. Each control keeps its column on the SYN page when no two share
    # one (GND-NOIS's TUNE under its fourth), else they go left to right.
    # `layout` is the machine slot of each definition slot (None: empty), and
    # `own` rebuilds the machine's eight slots for the sound and the screen.
    one_row = len(real) <= 4
    if not one_row:
        layout = [i if named[i] else None for i in range(max(real[-1] + 1, 4))]
    elif len({i % 4 for i in real}) == len(real):
        layout = [next((i for i in real if i % 4 == c), None) for c in range(4)]
    else:
        layout = real + [None] * (4 - len(real))
    remapped = any(m is not None and m != k for k, m in enumerate(layout))
    own = "".join(f"""
    own[{m}] = {{source}}[{k}];""" for k, m in enumerate(layout) if m is not None)
    if one_row:
        # The stock header: a stepped control shows its step's name, an empty slot nothing.
        captions = "".join(f"""
    static const char* const steps{k}[{len(enums[named[m]])}] = {{{", ".join(f'"{n}"' for n in enums[named[m]])}}};
    opxy::draw::copy(frame.header[{k}], opxy::draw::header_capacity,
                     steps{k}[(static_cast<u32>(md::positive(input.parameters[{k}])) >> 16) * {len(enums[named[m]])}u >> 15]);"""
                           for k, m in enumerate(layout) if m is not None and named[m] in enums)
        captions += "".join(f"""
    opxy::draw::copy(frame.header[{k}], opxy::draw::header_capacity, " ");""" for k, m in enumerate(layout) if m is None)
        header = captions
    else:
        # The two-page header (opxy/page_header.hpp): page 1 is the SYN page's top
        # row, page 2 its bottom row, under it. A slot the machine lacks is blank
        # on either row, and its encoder turns nothing. Each value reads 0..127,
        # or a stepped control's step name, as the original's does.
        second = [4 + c for c in range(4)]
        titles = ", ".join(f'"{control(label, 0)["label"].lower() if label else ""}"' for label in named)
        captions = "".join(f"""
    static const char* const steps{i}[{len(enums[label])}] = {{{", ".join(f'"{n}"' for n in enums[label])}}};
    opxy::draw::copy(values[{i}], opxy::draw::header_capacity,
                     steps{i}[(static_cast<u32>(md::positive(input.slots[{i}])) >> 16) * {len(enums[label])}u >> 15]);"""
                           for i, label in enumerate(named) if label in enums)
        captions += "".join(f"""
    values[{i}][0] = 0;""" for i, label in enumerate(named) if not label)
        header = f"""
    // The header last: both pages, the one the encoders edit on its bar.
    static const char* const titles[8] = {{{titles}}};
    static const opxy::u8 pages[2][4] = {{{{0, 1, 2, 3}}, {{{", ".join(map(str, second))}}}}};
    char values[8][opxy::draw::header_capacity];
    for (u32 i = 0; i < 8; ++i) md::view::value127(values[i], opxy::draw::header_capacity, input.slots[i]);{captions}
    opxy::draw::page_header(state.header, input, frame, titles, values, header_node, pages);"""

    # --- core.h
    (folder / "core.h").write_text(f"""#pragma once
// Generated by tools/make_engine.py from tools/catalog.py: edit the voice modules
// (voices/) or the generator, then regenerate.
#include <opxy/audio.hpp>
#include "{headers[0]}.hpp"

namespace {engine} {{
using opxy::i32;
using opxy::u32;

struct Voice {{
    md::{s}::State state;
}};
struct Shared {{
    md::Common common;
}};
}}
""")

    # --- engine.cpp
    (folder / "engine.cpp").write_text(f"""#include "core.h"

// {name}: generated by tools/make_engine.py. The sound is in voices/{headers[0]}.hpp.
namespace {engine} {{
inline void initialize(Voice& voice, Shared& shared, u32 owner) {{ md::{s}::reset(voice.state, shared.common, owner); }}
inline void start(Voice& voice, Shared& shared, u32) {{ md::{s}::trigger(voice.state, shared.common); }}
inline void destroy(Voice&, Shared&, u32) {{}}
inline void render(Voice& voice, Shared& shared, u32, opxy::Output output, opxy::Parameters parameters, opxy::Pitch pitch) {{{f'''
    // The machine's eight slots from the definition's {len(layout)} (definition.json).
    i32 own[8] = {{}};{own.format(source="parameters")}''' if remapped else ""}
    md::{s}::render(voice.state, shared.common, output, {"own" if remapped else "parameters"}, pitch);
}}
}}
""")

    # --- artwork
    subprocess.run(["node", "tools/art/build.mjs", *(["--one-row"] if one_row else []), engine, f"mm:{name}"], cwd=ROOT, check=True)

    # --- display.h / display.cpp
    (folder / "display.h").write_text(f"""#pragma once
// Generated by tools/make_engine.py.
#include <opxy/draw.hpp>
#include <opxy/page_header.hpp>
#include "md_view.hpp"
#include "art.h"

namespace {engine} {{
using opxy::i32;
using opxy::u32;
// The art's nodes, then the two-page header's eight bar steps; its 16 text slots.
const u32 header_node = md::view::art_first + art::node_count;
using Frame = opxy::draw::Frame<4, 64, header_node + opxy::draw::page_header_nodes, opxy::draw::page_header_texts, 16>;
struct DisplayState {{
    md::view::Hit hit;
    opxy::draw::PageHeader header;
}};
}}
""" if not one_row else f"""#pragma once
// Generated by tools/make_engine.py.
#include <opxy/draw.hpp>
#include "md_view.hpp"
#include "art.h"

namespace {engine} {{
using opxy::i32;
using opxy::u32;
// One row of controls, on the stock header.
using Frame = opxy::draw::Frame<4, 64, md::view::art_first + art::node_count, 0, 16>;
struct DisplayState {{
    md::view::Hit hit;
}};
}}
""")
    (folder / "display.cpp").write_text(f"""#include "display.h"
#include "{headers[1]}.hpp"

// {name}: generated by tools/make_engine.py. The screen: md_rig.hpp draws the
// design's parts from art.h; voices/{headers[1]}.hpp draws what changes shape.
namespace {engine} {{
inline void display_reset(DisplayState& state) {{
    state.hit.initialized = 0;
    state.hit.age = 1 << 20;{"" if one_row else '''
    state.header.reset();'''}
}}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {{
    md::view::begin(frame, input);
    state.hit.update(input);
    const md::view::Moment moment = {{state.hit.age, input.reduced_motion, state.hit.quiet}};{f'''
    i32 own[8] = {{}};{own.format(source="input.slots")}''' if remapped else ""}
    md::rig::draw(frame, art::parts, art::effects, art::members, art::machines[0], {"own" if remapped else "input.slots"}, moment);
    md::{s}::view(frame, {"own" if remapped else "input.slots"}, moment);{header}
}}
}}
""")

    # --- headers: the library and the modules
    modules = with_includes(headers)
    for source in sorted((ROOT / "lib").glob("*.hpp")) + [VOICES / f"{h}.hpp" for h in modules]:
        copy_scoped(source, folder, engine, paged=not one_row)
    captured = copy_private(modules, folder, engine)

    # --- definition.json
    d = defaults(name)
    mapping = lambda label: {"kind": "enum", "labels": enums[label]} if label in enums else None
    # Each definition slot holds its layout's control. An empty slot before a
    # real one keeps its place, absent (the SDK's placeholder: no label, no
    # preset), so every control sits where the original has it; on the bottom
    # row of two it is its encoder's page 2, blank, rather than page 1 again.
    empty = lambda k: {"id": f"empty{k + 1}", "label": "", "default": 0, "mapping": {"kind": "linear", "minimum": 0, "maximum": 1},
                       "display": "percent", "absent": True, **({"control": k - 4} if k >= 4 and not one_row else {})}
    params = [control(named[m], enum_value(name, named[m], d[m]), None if k < 4 else k - 4, mapping(named[m])) if m is not None else
              empty(k) for k, m in enumerate(layout)]
    presets = {key: {p["id"]: round(enum_value(name, named[m], v[m]) / 127, 6) for p, m in zip(params, layout) if m is not None}
               for key, v in PRESETS.get(name, {}).get("presets", {}).items()}
    # Two rows: a press of any encoder switches all four between the SYN page's rows.
    definition = {"schema": 1, "id": engine, "name": re.sub(r"[^A-Za-z0-9 _-]", " ", name),   # FM+STAT shows as FM STAT
                  **({} if one_row else {"press": "page"}), "parameters": params, "presets": presets,
                  "resources": {"voiceBytes": 4, "voiceAlignment": 4, "voiceCapacity": VOICE_CAPACITY.get(engine, 16),
                                "sharedBytes": 16, "outputPeakQ31": 1073741824,
                                "display": {"stateBytes": 16, "stateAlignment": 4, "paths": 4, "points": 64,
                                            "nodes": 0, "texts": 0, "palette": 16}}}
    (folder / "definition.json").write_text(json.dumps(definition, indent=2) + "\n")
    voice_bytes, shared_bytes, display_bytes = sizes(engine)
    # The 31 common nodes, the art's, then the two-page header's eight and its 16 text slots.
    nodes = int(re.search(r"node_count = (\d+);", (folder / "art.h").read_text()).group(1)) + 31 + (0 if one_row else 8)
    definition["resources"].update({"voiceBytes": voice_bytes, "sharedBytes": shared_bytes})
    definition["resources"]["display"].update({"stateBytes": display_bytes, "nodes": nodes, "texts": 0 if one_row else 16})
    (folder / "definition.json").write_text(json.dumps(definition, indent=2) + "\n")

    # --- machine.json
    (folder / "machine.json").write_text(json.dumps({
        "schema": 1, "sdk": "1.3", "kind": "synth",
        "description": (PRESETS.get(name, {}).get("description") or f"The Monomachine's {name}, fitted to the original.")[:160],
        "identity": {"engine": 36 + index, "page": 84 + index},
        "presetDefaults": {"amp": {"attack": 0, "decay": 32767, "sustain": 32767, "release": 4096}},
    }, indent=2) + "\n")

    # --- tests: the module's own checks run through tools/test_voices.py; this checks the engine
    (folder / "tests" / "sound.cpp").write_text(f"""// A check of {name}'s engine, written by tools/make_engine.py: it sounds at
// middle C, stays within the declared peak at its controls' ends and across
// four octaves. The voice's fitted checks: voices/tests/{s}.cpp.
// {"Built with the data captured from the owner's ROM." if captured else "Built without the captured data (docs/FAMILIES.md): silent, so it is not asked to sound."}
#include "../engine.cpp"
#include <cstdio>

int main() {{
    static {engine}::Voice voice;
    static {engine}::Shared shared;
    opxy::i32 out[opxy::block_frames];
    int failures = 0;
    for (int octave = -2; octave <= 2; ++octave) {{
        opxy::u32 pitch[opxy::block_frames];
        for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = octave < 0 ? 25479004u >> -octave : 25479004u << octave;
        for (int extreme = 0; extreme < 3; ++extreme) {{
            opxy::i32 p[8];
            for (int i = 0; i < 8; ++i) p[i] = extreme == 0 ? 0x40000000 : extreme == 1 ? 0 : 0x7fffffff;
            {engine}::initialize(voice, shared, 1);
            {engine}::start(voice, shared, 1);
            bool sounding = false, bounded = true;
            for (unsigned b = 0; b < 690; ++b) {{
                {engine}::render(voice, shared, 1, out, p, pitch);
                for (unsigned i = 0; i < opxy::block_frames; ++i) {{
                    sounding = sounding || out[i] != 0;
                    bounded = bounded && out[i] >= -1073741824 && out[i] <= 1073741824;
                }}
            }}
            if (!bounded) {{ std::printf("FAIL octave %d, setting %d, above the declared peak\\n", octave, extreme); ++failures; }}
            if ({"true" if captured else "false"} && extreme == 0 && octave == 0 && !sounding) {{ std::puts("FAIL silent at the middle setting"); ++failures; }}
        }}
    }}
    if (!failures) std::puts("PASS {engine}{"" if captured else " (silent: no captured data)"}");
    return failures ? 1 : 0;
}}
""")
    return [name], []


KIT_FAMILY = {"mdtrx": "TRX", "mdefm": "EFM", "mdpi": "PI", "mde12": "E12"}
SHAKERS = ("TRX-MA", "PI-MA", "E12-SH", "E12-TA")   # the design's 1100 ms hit


def kit_keys(engine):
    """Each key of a kit: (role, machine, its model in the kit, its eight controls 0..127)."""
    machines = catalog.ENGINES[engine]
    keys = []
    for role, name, over in catalog.KITS[engine]:
        values = defaults(name)
        for label, v in over.items():
            values[catalog.MACHINES[name][2].index(label)] = v
        raw = [int(round(v)) if s else 0 for v, s in zip(values, catalog.MACHINES[name][2])]
        keys.append((role, name, machines.index(name), raw))
    return keys


def generate_drum_kit(engine, index):
    """A drum kit on the SDK's drum kind: each key picks its own machine and
    holds that machine's controls, which the SDK saves with the project."""
    machines = catalog.ENGINES[engine]
    missing = [m for m in machines if not available(m)]
    if missing:
        return None, missing
    folder = ROOT / "machines" / engine
    (folder / "tests").mkdir(parents=True, exist_ok=True)
    # The kit's family names it; its GND machines keep their family in their names.
    fam = KIT_FAMILY[engine]
    title = f"MD {fam.replace('PI', 'P-I')}"
    headers = sorted({sound_header(m) for m in machines})
    member = {m: k for k, m in enumerate(machines)}
    q31 = lambda v: 0x7fffffff if v >= 127 else round(v * 16909320)

    # --- kit.h: shared by the engine and the screen
    models, rows, key_models, shown = [], [], [], {}
    for name in machines:
        slots = catalog.MACHINES[name][2]
        count = max(i for i, s in enumerate(slots) if s) + 1   # an empty slot inside stays unlabelled
        labels = [f'"{s}"' if s else '""' for s in slots]
        short = name.split("-", 1)[1] if family(name) == fam else name
        models.append(f'    {{"{short}", {count}, {{{", ".join(labels)}}}}},')
    for k, (role, name, model, raw) in enumerate(kit_keys(engine)):
        rows.append(f"    {{{', '.join(map(str, raw))}}},   // {k:2d} {role}: {name}")
        key_models.append(model)
        shown.setdefault(name, k)
    (folder / "kit.h").write_text(f"""#pragma once
// Generated by tools/make_engine.py from tools/catalog.py (KITS): edit
// the catalog, then regenerate. The engine and the screen both read this table.
#include "md_kit.hpp"

namespace {engine} {{
namespace kit {{
using opxy::i32;
using opxy::u32;
const u32 keys = 24;
const u32 models = {len(machines)};
// A machine on the drum kind's eight controls, in its own slot order: slots
// 0..count-1 are its controls, the rest empty; an unnamed slot inside the
// count is a gap in the machine's layout, blank and inert. Shift and the first encoder
// choose it (the drum kind's model).
struct Model {{
    const char* name;   // as the header shows it: without the family, which the kit names
    unsigned char count;
    const char* labels[8];
}};
const Model model[models] = {{
{chr(10).join(models)}
}};
// Each key's machine, and its eight controls, 0..127, as the drum kind stores them.
const unsigned char key_model[keys] = {{{", ".join(map(str, key_models))}}};
const unsigned char key_values[keys][8] = {{
{chr(10).join(rows)}
}};
// The key each machine is first on.
const unsigned char shown[models] = {{{", ".join(str(shown[m]) for m in machines)}}};

// A machine's eight slots (Q31) from the drum kind's eight controls (Q31,
// 0..127 << 24), on the scale the voice modules are voiced on.
inline void controls(u32 m, const i32* values, i32* own) {{
    const Model& d = model[m < models ? m : 0];
    for (u32 s = 0; s < 8; ++s) {{
        const u32 raw = static_cast<u32>(md::positive(values[s])) >> 24;
        own[s] = !d.labels[s][0] ? 0 : raw >= 127 ? 0x7fffffff : static_cast<i32>(raw * 16909320u);
    }}
}}
// A key's stored controls as the SDK hands them to render.
inline void key_controls(u32 key, i32* values) {{
    for (u32 c = 0; c < 8; ++c) values[c] = static_cast<i32>(static_cast<u32>(key_values[key][c]) << 24);
}}
}}
}}
""")

    # --- core.h
    includes = "\n".join(f'#include "{h}.hpp"' for h in headers)
    state = "\n".join(f"        md::{slug(m)}::State m{member[m]};   // {m}" for m in machines)
    (folder / "core.h").write_text(f"""#pragma once
// Generated by tools/make_engine.py from tools/catalog.py: edit the voice modules
// (voices/) or the generator, then regenerate.
#include <opxy/audio.hpp>
#include "kit.h"
{includes}

namespace {engine} {{
using opxy::i32;
using opxy::u32;

struct Voice {{
    u32 machine;   // the member sounding
    u32 next;      // the played key's machine, from drum_select
    u32 owner;
    u32 pending;   // a note started
    union {{
{state}
    }} u;
}};
struct Shared {{
    md::Common common;
}};

// The drum kind's callbacks (the SDK's docs/api.md, synthesis drum kits).
void drum_defaults(u32 key, opxy::u8* model, opxy::u8* controls);
const char* model_name(u32 model);
const char* parameter_name(u32 model, u32 slot);
u32 parameter_count(u32 model);
}}
""")

    # --- engine.cpp
    cases = lambda verb, args, indent="    ": "\n".join(
        f"{indent}case {member[m]}: md::{slug(m)}::{verb}(voice.u.m{member[m]}, {args}); break;" for m in machines)
    (folder / "engine.cpp").write_text(f"""#include "core.h"

// {title}: generated by tools/make_engine.py. Each key holds its own machine,
// chosen with Shift and the first encoder, and that machine's eight controls
// (kit.h), which the SDK saves with the project. The sound is in the voice
// modules: {", ".join(f"{m} (voices/{sound_header(m)}.hpp)" for m in machines)}.
namespace {engine} {{
void drum_defaults(u32 key, opxy::u8* model, opxy::u8* controls) {{
    const u32 k = key >= 53 && key < 53 + kit::keys ? key - 53 : 0;
    *model = kit::key_model[k];
    for (u32 c = 0; c < 8; ++c) controls[c] = kit::key_values[k][c];
}}
const char* model_name(u32 model) {{ return kit::model[model < kit::models ? model : 0].name; }}
const char* parameter_name(u32 model, u32 slot) {{
    return slot < 8 ? kit::model[model < kit::models ? model : 0].labels[slot] : "";
}}
u32 parameter_count(u32 model) {{ return kit::model[model < kit::models ? model : 0].count; }}

inline void begin_member(Voice& voice, Shared& shared) {{
    switch (voice.machine) {{
{cases("reset", "shared.common, voice.owner")}
    default: return;
    }}
    switch (voice.machine) {{
{cases("trigger", "shared.common")}
    default: break;
    }}
}}

inline void initialize(Voice& voice, Shared&, u32 owner) {{
    voice.machine = 0;
    voice.next = 0;
    voice.owner = owner;
    voice.pending = 0;
}}
inline void start(Voice& voice, Shared&, u32) {{ voice.pending = 1; }}
inline void destroy(Voice&, Shared&, u32) {{}}
inline void drum_select(Voice& voice, Shared&, u32, u32 model) {{ voice.next = model < kit::models ? model : 0; }}

// The machines are voiced at middle C and tuned by their own PTCH, as on the
// original, so the note's pitch is not used.
inline void render(Voice& voice, Shared& shared, u32, opxy::Output output, opxy::Parameters parameters, opxy::Pitch) {{
    if (voice.pending || voice.next != voice.machine) {{
        voice.pending = 0;
        voice.machine = voice.next;
        begin_member(voice, shared);
    }}
    i32 own[8];
    kit::controls(voice.machine, parameters, own);
    u32 tuned[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) tuned[i] = ::md::kit::middle_c;
    switch (voice.machine) {{
{cases("render", "shared.common, output, own, tuned")}
    default: for (unsigned i = 0; i < opxy::block_frames; ++i) output[i] = 0; break;
    }}
}}
}}
""")

    # --- artwork: each machine full size, as the design draws one machine
    subprocess.run(["node", "tools/art/build.mjs", "--pads", engine, *[f"md:{m}" for m in machines]], cwd=ROOT, check=True)
    view_headers = ["trx_view"] + (["mdkit_view"] if fam != "TRX" else [])
    views = "\n".join(f"    case {k}: md::{name.lower().replace('-', '_')}::view(frame, own, moment); break;"
                       for k, name in enumerate(machines))
    shaker = " || ".join(f"selected == {k}" for k, name in enumerate(machines) if name in SHAKERS) or "false"

    # --- display.h / display.cpp
    (folder / "display.h").write_text(f"""#pragma once
// Generated by tools/make_engine.py.
#include <opxy/draw.hpp>
#include "md_view.hpp"
#include "kit.h"
#include "art.h"
{chr(10).join(f'#include "{h}.hpp"' for h in view_headers)}

namespace {engine} {{
using opxy::i32;
using opxy::u32;
using Frame = opxy::draw::Frame<4, 64, md::view::art_first + art::node_count, 0, 16>;
struct DisplayState {{
    md::view::Hit hit;
}};
}}
""")
    (folder / "display.cpp").write_text(f"""#include "display.h"

// {title}: generated by tools/make_engine.py. The screen: the selected key's
// machine as the design draws one machine (art.h): its instrument on the left,
// its family's panel on the right, at that key's values (a held step's locks
// included), struck by each note.
namespace {engine} {{
inline void display_reset(DisplayState& state) {{
    state.hit.initialized = 0;
    state.hit.age = 1 << 20;
}}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {{
    md::view::begin(frame, input);
    state.hit.update(input);
    const md::view::Moment moment = {{state.hit.age, input.reduced_motion, state.hit.quiet}};
    const u32 selected = input.model < kit::models ? input.model : 0;
    i32 own[8];
    kit::controls(selected, input.slots, own);
    for (u32 m = 0; m < kit::models; ++m)
        if (m != selected) md::rig::hide_all(frame, art::members, art::machines[m]);
    md::rig::draw(frame, art::parts, art::effects, art::members, art::machines[selected], own, moment,
                  {shaker} ? md::rig::shaker : md::rig::drum);
    switch (selected) {{
{views}
    default: break;
    }}
    // A slot the machine lacks shows nothing (an empty header keeps the stock value).
    for (u32 i = 0; i < 4; ++i)
        if (!kit::model[selected].labels[input.shown[i] & 7u][0])
            opxy::draw::copy(frame.header[i], opxy::draw::header_capacity, " ");
}}
}}
""")

    # --- headers
    for source in sorted((ROOT / "lib").glob("*.hpp")) + [VOICES / f"{h}.hpp" for h in [*headers, *view_headers]]:
        copy_scoped(source, folder, engine)
    captured = copy_private(headers, folder, engine)

    # --- definition.json: the eight controls; their defaults are the first key's
    first = [int(v) for v in rows[0].split("{")[1].split("}")[0].split(", ")]
    params = [control(f"CTRL {c + 1}", first[c], None if c < 4 else c - 4) for c in range(8)]
    definition = {"schema": 1, "id": engine, "name": title, "parameters": params, "presets": {},
                  "resources": {"voiceBytes": 4, "voiceAlignment": 4, "voiceCapacity": 16, "sharedBytes": 16,
                                "outputPeakQ31": 1073741824, "modelCount": len(machines),
                                "display": {"stateBytes": 16, "stateAlignment": 4, "paths": 4, "points": 64,
                                            "nodes": 0, "texts": 0, "palette": 16}}}
    (folder / "definition.json").write_text(json.dumps(definition, indent=2) + "\n")
    voice_bytes, shared_bytes, display_bytes = sizes(engine)
    nodes = int(re.search(r"node_count = (\d+);", (folder / "art.h").read_text()).group(1)) + 31
    definition["resources"].update({"voiceBytes": voice_bytes, "sharedBytes": shared_bytes})
    definition["resources"]["display"].update({"stateBytes": display_bytes, "nodes": nodes})
    (folder / "definition.json").write_text(json.dumps(definition, indent=2) + "\n")

    # --- machine.json
    (folder / "machine.json").write_text(json.dumps({
        # SDK 1.9: drum kits, numberless; each build's layout gives the engine and page.
        "schema": 1, "sdk": "1.9", "kind": "drum",
        "description": f"The Machinedrum's {fam.replace('PI', 'P-I')} machines as a drum kit: each key picks its own machine and keeps its own controls."[:160],
        "presetDefaults": {"amp": {"attack": 0, "decay": 32767, "sustain": 32767, "release": 32767}},
    }, indent=2) + "\n")

    # --- tests: every key's own sound, every machine at its controls' ends
    (folder / "tests" / "sound.cpp").write_text(f"""// Checks for {title}, written by tools/make_engine.py: each key's stored
// machine and controls sound as that machine, within the declared peak; every
// machine stays bounded with its controls at both ends; the names fit, and
// the header leaves an unnamed slot blank.
// The voices' own checks: tools/test_voices.py.
#include "../engine.cpp"
#include "../display.cpp"
#include <cstdio>
#include <cstring>

namespace {{
int failures = 0;
// Without the private samples a sampled kit is silent: its sounds go unchecked.
const bool captured = {"true" if captured else "false"};
// The adapter's whole path for one note (drum_voice.cpp.in): the played key's
// defaults, its stored model and controls over them, the track's eight offsets
// around 64 on the eight controls, clamped, then Q31.
void device(opxy::u32 key, const opxy::u8* stored_model, const opxy::u8* stored, const opxy::i32* track,
            opxy::u8& model, opxy::i32* values) {{
    opxy::u8 raw[8];
    {engine}::drum_defaults(key, &model, raw);
    if (stored) {{ model = *stored_model; for (int c = 0; c < 8; ++c) raw[c] = stored[c]; }}
    if (model >= {engine}::kit::models) {engine}::drum_defaults(key, &model, raw);
    for (int c = 0; c < 8; ++c) {{
        opxy::i32 v = static_cast<opxy::i32>(raw[c]) + static_cast<opxy::i32>(static_cast<opxy::u32>(track[c]) >> 24) - 64;
        v = v < 0 ? 0 : v > 127 ? 127 : v;
        values[c] = v == 127 ? 0x7fffffff : v << 24;
    }}
}}
const opxy::i32 neutral[8] = {{0x40000000, 0x40000000, 0x40000000, 0x40000000,
                              0x40000000, 0x40000000, 0x40000000, 0x40000000}};
{engine}::Voice voice;
{engine}::Shared shared;
// Renders a note on the voice (fresh unless reused) and returns a fingerprint of its sound.
opxy::u32 play(opxy::u32 model, const opxy::i32* values, bool& bounded, bool& sounding, bool reuse = false) {{
    opxy::i32 out[opxy::block_frames];
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = ::md::kit::middle_c;
    if (!reuse) {{
        voice = {engine}::Voice();
        shared = {engine}::Shared();
        {engine}::initialize(voice, shared, 1);
    }}
    {engine}::start(voice, shared, 1);
    sounding = false;
    bounded = true;
    opxy::u32 print = 0;
    for (unsigned b = 0; b < 690; ++b) {{
        {engine}::drum_select(voice, shared, 1, model);
        {engine}::render(voice, shared, 1, out, values, pitch);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {{
            sounding = sounding || out[i] != 0;
            bounded = bounded && out[i] >= -1073741824 && out[i] <= 1073741824;
            print = print * 31u + static_cast<opxy::u32>(out[i]);
        }}
    }}
    return print;
}}
}}

int main() {{
    // Every key keeps its own stored machine: keys storing different machines
    // sound different, and a voice taken over by another key changes machine.
    {{
        opxy::u32 prints[24];
        opxy::u8 models[24], stored[24][8];
        for (opxy::u32 k = 0; k < 24; ++k) {{
            models[k] = static_cast<opxy::u8>((k * 5u) % {engine}::kit::models);
            for (int c = 0; c < 8; ++c) stored[k][c] = 64;
            opxy::u8 model;
            opxy::i32 values[8];
            device(53 + k, &models[k], stored[k], neutral, model, values);
            if (model != models[k]) {{ std::printf("FAIL key %u renders another key's machine\\n", 53 + k); ++failures; }}
            bool bounded, sounding;
            prints[k] = play(model, values, bounded, sounding);
        }}
        for (opxy::u32 a = 0; a < 24; ++a)
            for (opxy::u32 b = a + 1; b < 24; ++b)
                if (captured && models[a] != models[b] && prints[a] == prints[b]) {{
                    std::printf("FAIL keys %u and %u sound the same with different machines\\n", 53 + a, 53 + b); ++failures;
                }}
        opxy::u8 first_model, second_model;
        opxy::i32 first[8], second[8];
        device(53, &models[0], stored[0], neutral, first_model, first);
        device(54, &models[1], stored[1], neutral, second_model, second);
        bool bounded, sounding;
        play(first_model, first, bounded, sounding);
        play(second_model, second, bounded, sounding, true);
        if (voice.machine != models[1]) {{ std::printf("FAIL a reused voice keeps the previous key's machine\\n"); ++failures; }}
        // A step lock on control c moves that control only, and the model never.
        for (int c = 0; c < 8; ++c) {{
            opxy::i32 locked[8];
            for (int i = 0; i < 8; ++i) locked[i] = neutral[i];
            locked[c] = 0x40000000 + (10 << 24);
            opxy::u8 model;
            opxy::i32 values[8];
            device(53, &models[0], stored[0], locked, model, values);
            for (int i = 0; i < 8; ++i)
                if ((static_cast<opxy::u32>(values[i]) >> 24) != (i == c ? 74u : 64u) || model != models[0]) {{
                    std::printf("FAIL a lock on control %d reaches control %d\\n", c, i); ++failures;
                }}
        }}
    }}
    for (opxy::u32 key = 53; key < 77; ++key) {{
        opxy::u8 model;
        opxy::i32 values[8];
        device(key, nullptr, nullptr, neutral, model, values);
        if (model != {engine}::kit::key_model[key - 53]) {{ std::printf("FAIL key %u defaults to another machine\\n", key); ++failures; }}
        bool bounded, sounding;
        play(model, values, bounded, sounding);
        if (captured && !sounding) {{ std::printf("FAIL key %u is silent\\n", key); ++failures; }}
        if (!bounded) {{ std::printf("FAIL key %u above the declared peak\\n", key); ++failures; }}
    }}
    for (opxy::u32 m = 0; m < {engine}::kit::models; ++m) {{
        const opxy::u32 count = {engine}::parameter_count(m);
        if (count < 1 || count > 8) {{ std::printf("FAIL machine %u has %u controls\\n", m, count); ++failures; }}
        for (opxy::u32 s = 0; s < 8; ++s) {{
            const char* label = {engine}::parameter_name(m, s);
            if (std::strlen(label) > 8 || (s >= count && label[0]) || (s == 0 && !label[0]) ||
                !std::strncmp(label, "SPARE", 5) || !std::strncmp(label, "CTRL", 4)) {{
                std::printf("FAIL machine %u slot %u label\\n", m, s); ++failures;
            }}
            // An unnamed slot is blank on the device and must do nothing: its
            // control at either end sounds as at the middle.
            if (label[0]) continue;
            opxy::i32 middle[8], moved[8];
            for (int c = 0; c < 8; ++c) middle[c] = moved[c] = 64 << 24;
            bool bounded, sounding;
            const opxy::u32 still = play(m, middle, bounded, sounding);
            for (int end = 0; end < 2; ++end) {{
                moved[s] = end ? 0x7fffffff : 0;
                if (play(m, moved, bounded, sounding) != still) {{
                    std::printf("FAIL %s's unnamed slot %u changes its sound\\n", {engine}::model_name(m), s); ++failures;
                }}
            }}
        }}
        for (int end = 0; end < 2; ++end) {{
            opxy::i32 values[8];
            for (int c = 0; c < 8; ++c) values[c] = end ? 0x7fffffff : 0;
            bool bounded, sounding;
            play(m, values, bounded, sounding);
            if (!bounded) {{ std::printf("FAIL %s above the declared peak with its controls at %s\\n", {engine}::model_name(m), end ? "127" : "0"); ++failures; }}
        }}
        // The screen's header: a number for each named slot, nothing for an unnamed one.
        for (opxy::u32 page = 0; page < 2; ++page) {{
            static {engine}::DisplayState state;
            static {engine}::Frame frame;
            opxy::draw::Input input = opxy::draw::Input();
            input.model = m;
            for (opxy::u32 i = 0; i < 4; ++i) input.shown[i] = static_cast<opxy::u8>(page * 4 + i);
            {engine}::display_reset(state);
            {engine}::display(state, input, frame);
            for (opxy::u32 i = 0; i < 4; ++i) {{
                const bool named = {engine}::parameter_name(m, page * 4 + i)[0] != 0;
                const char* shown = frame.header[i];
                if (named ? !(shown[0] >= '0' && shown[0] <= '9') : std::strcmp(shown, " ") != 0) {{
                    std::printf("FAIL %s's header for slot %u reads \\"%s\\"\\n", {engine}::model_name(m), page * 4 + i, shown); ++failures;
                }}
            }}
        }}
    }}
    if (!failures) std::puts("PASS {engine}");
    return failures ? 1 : 0;
}}
""")
    return list(machines), []


def main():
    wanted = sys.argv[1:] or list(catalog.ENGINES)
    for index, engine in enumerate(catalog.ENGINES):
        if engine not in wanted:
            continue
        make = generate_drum_kit if engine in catalog.KITS else generate
        members, missing = make(engine, index)
        if members is None:
            print(f"{engine}: skipped" + (f" ({', '.join(missing)})" if missing else ""))
        else:
            print(f"{engine}: {', '.join(members)}" + (f"; still to build: {', '.join(missing)}" if missing else ""))


if __name__ == "__main__":
    main()
