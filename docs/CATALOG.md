# The public builder

[Open-XY](https://open-xy.com) builds firmware from a user's own `os.ldr` with
the machines in etcetc000/open-xy's [catalog](https://github.com/etcetc000/open-xy/blob/main/docs/catalog.md).
The catalog pins each machine to a tagged commit of its own repository and
builds it against etcetc000's SDK for **OS 1.2.1**, not the fork this
repository's other tools use. Each engine here can join as its own entry,
`path: machines/<id>`.

## Where each engine stands

| Engines | Ready? | What it needs |
| --- | --- | --- |
| `mmdyn` `mmens` `mmpar` `mmpulse` `mmsid` `mmstat` `mmvo6` | Yes: submitted in etcetc000/open-xy#168 at `v1.1.2`, with two-page controls and hardware CPU records from that commit | Review and merge |
| `mmsaw` `mmsin` `mmnois` | No | An empty slot before a real control, which only the fork's `absent` field allows |
| `mdtrx` `mdefm` `mdpi` | No | The drum kind, etcetc000/open-xy#121 and #143, merged and ported to OS 1.2.1 |
| `mde12` `mmbbox` `mmddrw` `mmdens` `mmwave` | Never | Samples and waves captured from the owner's ROM, which can't be published. `mmbbox`, `mmdens` and `mmwave` also have `absent` slots |

## Linking with other machines

Firmware links every machine's objects together, and the upstream SDK turns
each weak definition into a global one, so a definition that two objects
repeat fails to link. `tools/make_engine.py` therefore gives each engine its
own copy of the library's namespace (`md_<engine>`, still reached as `md`),
and `lib/md_tables.hpp` keeps its tables as `static` arrays, one copy in each
translation unit that reads them.

A large inline function that the compiler keeps out of line comes in a COMDAT
group. Until etcetc000/open-xy#165, the SDK left each group's record in the
linked module, so a firmware with two of them refused to build as
`Duplicate ELF section`. CI's `upstream` job checks without `--firmware`,
since its Linux runner has no release of the pinned Blackfin tools.

## Outlines

The OP-XY draws streaks from an outline with no area: a line traced out and
back over itself, or one that turns straight back at a point
(etcetc000/open-xy#167, which `sdk.py test` will warn about). Every view
writes its paths through `outline()` in `lib/md_view.hpp`, which makes a
retraced line a thin ribbon and a spike a sliver. It also starts each
outline where both its edges are longest: on the device, a closing edge only
an eighth of a pixel long (a ribbon's end on a level or upright line, as
SID-6581's steps) draws as a long sliver along the next edge.

## Publishing an engine

From etcetc000/open-xy's [submission steps](https://github.com/etcetc000/open-xy/blob/main/docs/catalog.md#submit-a-machine):

1. Reserve its ID with a catalog-only pull request against `staging`.
2. Profile it on an OP-XY running **OS 1.2.1** ([profiling](https://github.com/etcetc000/open-xy/blob/main/docs/profiling.md)),
   three runs at the commit to be tagged.
3. Tag that commit, then change the entry to `draft` or `published` with
   `repository`, `tag`, `commit`, `path` and the generated `cpu` record.

## Measuring CPU

`tools/profile_cpu.py` runs upstream's [profiling](https://github.com/etcetc000/open-xy/blob/main/docs/profiling.md)
for the ready engines. It uses a checkout of etcetc000/open-xy's `staging` at
`../open-xy-staging` and the Python environment in `../open-xy/.venv`, with
`python-rtmidi` installed. Builds, runs and reports go in
`~/Library/Application Support/OPXY-MD/cpu`.

1. Update the OP-XY to OS 1.2.1 with the official update. Open "mtp mode
   active", close OpenMTP, then run `python3 tools/profile_cpu.py stock`. It
   copies the device's stock `os.ldr` and checks it's exactly 1.2.1.
2. Run `python3 tools/profile_cpu.py build`, from the commit you'll tag. It
   refuses uncommitted machine changes.
3. In "mtp mode active", run `python3 tools/profile_cpu.py install --copy`.
   On the OP-XY, press track 2 (exit) then track 5 (write os), then switch it
   off and on.
4. Boot normally, with USB connected, the project saved and playback stopped.
   Run `python3 tools/profile_cpu.py run` three times without touching the
   device, then restart it without saving.
5. `python3 tools/profile_cpu.py report` writes each engine's
   `catalog-cpu.json`. Its `cpu` member goes in the catalog entry. Keep the
   runs private, because they hold the device's serial number.

The firmware holds only the ready engines, so the drum kits and other
engines aren't on the device while it's installed. They need the fork's
OS 1.2.0 build anyway.

Engine numbers come from each firmware's layout, so `identity` in
`machine.json` is only a request. Synths share engines 34..62 with every other
catalog machine.
