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
| `mmdyn` `mmens` `mmpar` `mmpulse` `mmsid` `mmstat` `mmvo6` | Passes the upstream SDK's checks (CI's `upstream` job) | Upstream to discard `.group` records (below), then hardware profiling and a catalog entry |
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

A large inline function that the compiler keeps out of line still comes in a
COMDAT group. The upstream SDK keeps each group's record as an inert
`.group` section, and its linker script doesn't discard them. A firmware
with two of them, such as one engine's voice and display, then has two
`.group` sections, and the module check
refuses it as `Duplicate ELF section`. Adding `.group` to the `/DISCARD/`
list in `linker/opxy-1.2.1.ld` fixes it: with that change, the seven ready
engines compile into one module (224.5 of 2040 KiB). Until upstream takes
that fix, CI's `upstream` job runs without `--firmware`.

## Publishing an engine

From etcetc000/open-xy's [submission steps](https://github.com/etcetc000/open-xy/blob/main/docs/catalog.md#submit-a-machine):

1. Reserve its ID with a catalog-only pull request against `staging`.
2. Profile it on an OP-XY running **OS 1.2.1** ([profiling](https://github.com/etcetc000/open-xy/blob/main/docs/profiling.md)),
   three runs at the commit to be tagged.
3. Tag that commit, then change the entry to `draft` or `published` with
   `repository`, `tag`, `commit`, `path` and the generated `cpu` record.

Engine numbers come from each firmware's layout, so `identity` in
`machine.json` is only a request. Synths share engines 34..62 with every other
catalog machine.
