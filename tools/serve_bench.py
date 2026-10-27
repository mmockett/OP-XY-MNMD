#!/usr/bin/env python3
"""The kit bench: every machine at once, with a 16-step sequencer, on the Mac.

    python3 tools/serve_bench.py            # build previews, then serve
    python3 tools/serve_bench.py --no-build # serve the last build

It builds each machine's browser preview with the SDK (the same C++ that goes
on the device, compiled to WebAssembly), stages them with the bench page and
the SDK's browser modules in local/bench/, and serves that folder only, on
127.0.0.1. Open http://127.0.0.1:8930/.
"""
import argparse
import functools
import http.server
import json
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SDK = ROOT.parent / "open-xy"
STAGE = ROOT / "local" / "bench"


def llvm_clang():
    try:
        prefix = subprocess.run(["brew", "--prefix", "llvm"], capture_output=True, text=True, check=True).stdout.strip()
        return str(pathlib.Path(prefix) / "bin" / "clang++")
    except (OSError, subprocess.CalledProcessError):
        return "clang++"


def machines():
    return sorted(p.name for p in (ROOT / "machines").iterdir() if (p / "machine.json").exists())


def build(cxx):
    python = SDK / ".venv" / "bin" / "python"
    if not python.exists():
        python = pathlib.Path(sys.executable)
    subprocess.run([sys.executable, str(ROOT / "tools" / "make_engine.py")], check=True)
    for mid in machines():
        out = ROOT / "local" / f"preview-{mid}"
        shutil.rmtree(out, ignore_errors=True)
        # The SDK previews a drum kit only from its machine folder.
        kind = json.loads((ROOT / "machines" / mid / "machine.json").read_text())["kind"]
        source = "--machine" if kind == "drum" else "--source"
        result = subprocess.run([str(python), str(SDK / "sdk.py"), "preview", source, str(ROOT / "machines" / mid),
                                 "--out", str(out), "--cxx", cxx], capture_output=True, text=True)
        if result.returncode:
            sys.exit(f"{mid}: preview failed\n{result.stdout}{result.stderr}")
        print(f"built {mid}")


def stage():
    shutil.rmtree(STAGE, ignore_errors=True)
    (STAGE / "sdk").mkdir(parents=True)
    for name in ("engine.mjs", "display.mjs", "controls.mjs"):
        shutil.copy(SDK / "sdk" / "browser" / name, STAGE / "sdk" / name)
    for item in (ROOT / "bench").iterdir():
        shutil.copy(item, STAGE / item.name)
    subprocess.run([sys.executable, str(ROOT / "tools" / "catalog.py")], check=True, capture_output=True)
    shutil.copy(ROOT / "tools" / "catalog.json", STAGE / "catalog.json")
    listed = []
    for mid in machines():
        source = ROOT / "local" / f"preview-{mid}"
        if not (source / "manifest.json").exists():
            sys.exit(f"{mid}: no preview yet; run without --no-build")
        target = STAGE / "machines" / mid
        target.mkdir(parents=True)
        for name in ("engine.wasm", "panel.svg", "manifest.json"):
            shutil.copy(source / name, target / name)
        listed.append(mid)
    (STAGE / "machines" / "index.json").write_text(json.dumps(listed))


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".mjs": "text/javascript", ".wasm": "application/wasm", ".svg": "image/svg+xml"}

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--port", type=int, default=8930)
    parser.add_argument("--cxx", default=None)
    args = parser.parse_args()
    if not args.no_build:
        build(args.cxx or llvm_clang())
    stage()
    server = http.server.ThreadingHTTPServer(("127.0.0.1", args.port), functools.partial(Handler, directory=str(STAGE)))
    print(f"kit bench on http://127.0.0.1:{args.port}/", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
