#!/usr/bin/env python3
"""Draws engines' real screens as SVG: builds tools/frame_json.cpp against each
engine (so it builds and runs host code: run it in CI, or where that is allowed),
plays settings through the display and draws each frame from the engine's
artwork (panel.svg.fragment) under the two-row header, in UniversNextPro-Light.

    python3 tools/frame_svg.py --sdk <open-xy checkout> --out <folder> mmsaw mdkit

Each synth is drawn at its defaults, every control at 0 and every control at 127,
and at its defaults a fifth of a second into a note; each of a kit's machines at
its first key's values (or 64), at 0 and at 127, and with Shift held (its mixer at
the defaults), the first machine with Shift on the second row too. A two-row
header is drawn from the frame's own text slots, as the device places them. The
SVGs reference the font by name only: rasterise them with the face loaded
(OP-XY-Dev media/stock-look-build).
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HIDDEN_X = 640 * 1024
HEAD = ["#000000", "#323232", "#4B4B4B", "#646464", "#7D7D7D", "#969696", "#AFAFAF", "#CDCDCD"]
FONT = "UniversNextPro-Light"


def artwork(folder, engine):
    """The engine's nodes (by number) and palette (by index) from its panel."""
    text = (folder / "panel.svg.fragment").read_text()
    nodes, palette = {}, {}
    for m in re.finditer(r'<(path|circle) id="%s_(node|palette)_(\d+)"([^>]*)/>' % engine, text):
        kind, what, n, attrs = m.group(1), m.group(2), int(m.group(3)), m.group(4)
        a = dict(re.findall(r'([\w-]+)="([^"]*)"', attrs))
        if what == "palette":
            palette[n] = (a.get("fill"), a.get("stroke"), float(a.get("stroke-width", "1")))
        else:
            nodes[n] = (kind, a)
    return nodes, palette


def header(titles, values):
    out = ['<rect x="0" y="0" width="480" height="38" fill="#000"/>']
    for i in range(8):
        out.append(f'<rect x="{60 * i}" y="0" width="60" height="20" fill="{HEAD[i]}"/>')
        ink = "#CBCBCB" if i < 4 else "#000000"
        if i % 2 == 0:
            out.append(f'<text x="{60 * i + 5}" y="10" font-family="{FONT}" font-size="10" fill="{ink}">{titles[i // 2]}</text>')
            out.append(f'<text x="{60 * i + 5}" y="31" font-family="{FONT}" font-size="9" fill="#8A8A8A">{titles[4 + i // 2]}</text>')
        else:
            out.append(f'<text x="{60 * i + 5}" y="17.4" font-family="{FONT}" font-size="20" fill="{ink}">{values[i // 2]}</text>')
            out.append(f'<text x="{60 * i + 5}" y="33" font-family="{FONT}" font-size="12" fill="#BDBDBD">{values[4 + i // 2]}</text>')
    return "".join(out)


def text_slots(folder, engine):
    """The panel's text slots by number: their fill and font size, as authored."""
    slots = {}
    for m in re.finditer(r'<text id="%s_text_(\d+)"([^>]*)>' % engine, (folder / "panel.svg.fragment").read_text()):
        a = dict(re.findall(r'([\w-]+)="([^"]*)"', m.group(2)))
        slots[int(m.group(1))] = (a.get("fill", "#CBCBCB"), a.get("font-size", "10px"))
    return slots


def device_header(frame, slots):
    """The header's text as the frame places it in the panel's text slots."""
    out = []
    for n, (x, y, scale, chars) in enumerate(frame.get("texts", [])):
        if n not in slots or not chars:
            continue
        fill, size = slots[n]
        chars = chars.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        out.append(f'<text transform="matrix({scale / 1024:.4f} 0 0 {scale / 1024:.4f} {x / 1024:.3f} {y / 1024:.3f})" '
                   f'font-family="{FONT}" font-size="{size}" fill="{fill}">{chars}</text>')
    return "".join(out)


def svg(frame, nodes, palette, titles, values, slots=None):
    body = ['<rect width="480" height="222" fill="#000"/>']
    for n, (x, y, scale, cos, sin, shade, paint) in enumerate(frame["nodes"]):
        if x >= HIDDEN_X or n not in nodes:
            continue
        kind, a = nodes[n]
        s = scale / 1024
        m = f'matrix({s * cos / 1024:.4f} {s * sin / 1024:.4f} {-s * sin / 1024:.4f} {s * cos / 1024:.4f} {x / 1024:.3f} {y / 1024:.3f})'
        fill, stroke, width = a.get("fill", "none"), a.get("stroke", "none"), a.get("stroke-width", "1")
        if paint in (1, 2) and shade in palette:
            pf, ps, pw = palette[shade]
            stroke, width = ps, pw
            if paint == 2 and fill != "none":
                fill = pf
        if n < 4:
            pts = frame["paths"][n]
            d = "M" + " L".join(f"{pts[2 * i] / 1024:.3f} {pts[2 * i + 1] / 1024:.3f}" for i in range(len(pts) // 2)) + " Z"
            body.append(f'<path transform="{m}" d="{d}" fill="none" stroke="{stroke}" stroke-width="{width}" stroke-linejoin="round"/>')
        elif kind == "circle":
            body.append(f'<circle transform="{m}" cx="{a.get("cx", "0")}" cy="{a.get("cy", "0")}" r="{a["r"]}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>')
        else:
            body.append(f'<path transform="{m}" d="{a["d"]}" fill="{fill}" stroke="{stroke}" stroke-width="{width}" stroke-linejoin="round"/>')
    top = device_header(frame, slots) if slots and any(t[3] for t in frame.get("texts", [])) else header(titles, values)
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 480 222" width="480" height="222">'
            + "".join(body) + top + "</svg>\n")


def labels(folder, engine, model):
    """The header's titles and how to show a value, for an engine (a kit: one of its machines)."""
    if (folder / "kit.h").exists():
        text = (folder / "kit.h").read_text()
        rows = re.findall(r'\{"(\w*)", \d+, \{([^}]*)\}\}', text)
        names = [x.strip().strip('"') for x in rows[model][1].split(",")]
        return [n.lower() for n in names], [None] * 8
    d = json.loads((folder / "definition.json").read_text())
    titles, enums = [""] * 8, [None] * 8
    order = list(range(8))
    params = d["parameters"]
    # Each definition slot's position: the first four on the top row, the rest by "control".
    for k, p in enumerate(params):
        pos = p.get("control", k) if k >= 4 else k
        pos = pos if k < 4 else 4 + (pos % 4)
        if p.get("absent"):
            continue
        titles[pos] = p["label"].lower()
        if p["mapping"].get("kind") == "enum":
            enums[pos] = p["mapping"]["labels"]
        order[pos] = k
    return titles, enums


def main():
    a = argparse.ArgumentParser()
    a.add_argument("engines", nargs="+")
    a.add_argument("--sdk", default=str(ROOT.parent / "open-xy"))
    a.add_argument("--out", required=True)
    a.add_argument("--cxx", default="c++")
    args = a.parse_args()
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for engine in args.engines:
        folder = ROOT / "machines" / engine
        exe = out / f"frame-json-{engine}"
        subprocess.run([args.cxx, "-std=c++11", "-O1", "-fwrapv", "-w", f"-I{args.sdk}/sdk/include", f"-I{folder}",
                        f"-DENGINE={engine}", str(ROOT / "tools" / "frame_json.cpp"), "-o", str(exe)], check=True)
        settings = []
        if (folder / "kit.h").exists():
            text = (folder / "kit.h").read_text()
            names = re.findall(r'//\s*\d+ \w+: ([A-Z0-9-]+)', text)
            key_values = [[int(v) for v in row.split(",")] for row in re.findall(r'^\s*\{([\d, ]+)\},\s*//\s*\d+ ', text, re.M)]
            shown = [int(v) for v in re.search(r"shown\[models\] = \{([^}]*)\}", text).group(1).split(",")]
            models = len(re.findall(r'\{"\w*", \d+, \{', text))
            # The Shift rows' choices: the model's group and its place in it, or the model; choke group none.
            groups = json.loads((folder / "definition.json").read_text())["resources"].get("modelGroups")
            def choices(m):
                if not groups:
                    return [m, 0, 0]
                first = 0
                for g, group in enumerate(groups):
                    if m < first + group["count"]:
                        return [g, m - first, 0]
                    first += group["count"]
                return [0, 0, 0]
            for m in range(models):
                first = shown[m] if m == 0 or shown[m] else None
                defaults = key_values[first] if first is not None else [64] * 8
                for name, v in (("defaults", defaults), ("all0", [0] * 8), ("all127", [127] * 8)):
                    settings.append((f"{m:02d}-{name}", m, v, 0))
                settings.append((f"{m:02d}-shift", m, defaults, 0, [1, 0, *choices(m)]))
                if m == 0:
                    settings.append((f"{m:02d}-shift-row2", m, defaults, 0, [1, 1, *choices(m)]))
        else:
            d = json.loads((folder / "definition.json").read_text())
            defaults = [round(p["default"] * 127) for p in d["parameters"]] + [0] * 8
            defaults = defaults[:8]
            for name, v, frames in (("defaults", defaults, 0), ("all0", [0] * 8, 0), ("all127", [127] * 8, 0), ("hit", defaults, 12)):
                settings.append((name, 0, v, frames))
        settings = [s if len(s) == 5 else (*s, []) for s in settings]
        lines = "".join(f"{m} {' '.join(str(x) for x in v)} {frames} {' '.join(map(str, extra))}\n"
                        for _, m, v, frames, extra in settings)
        result = subprocess.run([str(exe)], input=lines, capture_output=True, text=True, check=True)
        nodes, palette = artwork(folder, engine)
        slots = text_slots(folder, engine)
        for (name, m, v, _, _), line in zip(settings, result.stdout.splitlines()):
            frame = json.loads(line)
            titles, enums = labels(folder, engine, m)
            # The header's values: what each definition slot holds, in its position.
            if (folder / "kit.h").exists():
                values = [str(v[k]) if titles[k] else "" for k in range(8)]
            else:
                d = json.loads((folder / "definition.json").read_text())
                values = [""] * 8
                for k, p in enumerate(d["parameters"]):
                    if p.get("absent"):
                        continue
                    pos = k if k < 4 else 4 + (p.get("control", k) % 4)
                    raw = v[k]
                    labels_ = p["mapping"].get("labels")
                    values[pos] = labels_[min(len(labels_) - 1, raw * len(labels_) >> 7)] if labels_ else str(raw)
            (out / engine).mkdir(exist_ok=True)
            (out / engine / f"{name}.svg").write_text(svg(frame, nodes, palette, titles, values, slots))
            (out / engine / f"{name}.json").write_text(line + "\n")
        print(engine, len(settings), "frames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
