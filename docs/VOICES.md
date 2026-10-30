# Voices: how a machine is built

Each Machinedrum or Monomachine machine is one **voice module**, a header in
`voices/`. Engines are generated from the modules and the catalog by
`tools/make_engine.py`; nothing in `machines/` is edited by hand.

| File | What |
| --- | --- |
| `tools/catalog.py` | Every machine (ID, eight slots as the original shows them), the kits' keys, and which engine holds what |
| `tools/presets.py` | A machine's fitted defaults and presets, 0..127 in slot order; without one, the design's defaults |
| `voices/<module>.hpp` | The sound: one namespace per machine, `md::<slug>` |
| `voices/<family>_view.hpp` | A Monomachine machine's screen view (see [SCREEN.md](SCREEN.md)) |
| `voices/tests/<module>.cpp` | The module's fitted sound checks, run by `tools/test_voices.py` |
| `lib/` | Shared DSP (`md_dsp.hpp`), tables (`md_tables.hpp`, from `tools/gen_tables.py`), notes as keys (`md_kit.hpp`), screen (`md_view.hpp`, `md_rig.hpp`) |

## The module contract

A machine's slug is its name in lower case with the family prefix shortened,
as `make_engine.slug` writes it: TRX-BD is `trx_bd`, EFM-HH `efm_hh`, PI-MT
`pi_mt`, SWAVE-SAW `swave_saw`, SID-6581 `sid_6581`, FM+STAT `fmpstat`. Its header is
`voices/<slug>.hpp` unless `SOUND_HEADER` in `make_engine.py` says otherwise
(TRX-CH and TRX-OH share `trx_hat.hpp`). It defines, in `namespace md::<slug>`:

```cpp
struct State { ... };                                   // all of one voice's state
void reset(State&, Common&, u32 owner);                 // a voice given to a note's owner
void trigger(State&, Common&);                          // the note starts
void render(State&, Common&, opxy::Output out,          // one 64-sample block
            const i32* slots, opxy::Pitch pitch);
```

- `slots` are the machine's eight controls in **its own slot order**
  (catalog.py), Q31, 0 to 0x7fffffff for 0 to 127; empty slots read 0.
- `pitch` is a phase increment per sample. A module is voiced at middle C
  (`md::kit::middle_c`, 25479004): PTCH and TUNE mean what they mean on the
  original at middle C, and the pitch moves them from there.
- `Common` (md_dsp.hpp) is state every voice of an engine shares, such as
  `closed_hats` for the hat choke. Add a field there, never a global.
- Output peaks at most half scale (0x40000000), the SDK's ceiling.
- Machine code is freestanding C++11 for the Blackfin: no floats, no
  division in audio loops, no 64-bit, no signed shifts, no `#if`, no OS headers.
- A module includes `<opxy/audio.hpp>` and what it uses from `lib/`.

`make_engine.available()` treats a machine as built once its header exists
and contains `namespace <slug> ` (with the space); a Monomachine machine also
needs its view (`VIEW_HEADER`). A built machine joins its
engine at the next `python3 tools/make_engine.py`; an engine with an unbuilt
machine is skipped.

## Engines

**MD kits** (`mdkit`, `mde12`): the machines on the SDK 1.9 drum kind, as
stock Drumline, where each key keeps its own machine and settings in the
project. MD KIT holds the TRX, EFM, P-I and GND families' 33 machines, a model
group each (`catalog.GROUPS`); with more than 16 machines the drum kind keeps
slots 6 and 7 at 64 steps, so their stored values round to the nearest even
step (127 at the top). MD E12 holds its 16. The kits share their
voices' code and tables: inline functions and template statics
(`md_tables.hpp`, the E12 samples) link once.

- **Keys.** The SDK gives each of the 24 keys (notes 53 to 76) a machine and
  eight controls, 0..127, in the machine's slot order, so every machine keeps
  all eight. Keys start from `KITS` in catalog.py: the first 16 follow the
  Machinedrum's default kit (BD SD HT MT LT CP RS CB CH OH RC CC M1..M4), the
  last 8 are variations. A key tuned away from its machine is tuned by its PTCH.
- **Encoders.** As on stock Drumline, Shift and the first encoder choose the
  key's machine, and Shift and the second its choke group (NONE, 1..3);
  while Shift is held the first two cells show MACHINE and CHOKE with their
  values, and the other two are blank and inert; a kit's presets put
  `catalog.CHOKED` (CH, OH, RC) in group 1 and every other key in none. The four
  encoders hold controls 1..4, and a press switches each to its alternate,
  5..8. The labels and values follow the selected key's machine; a slot the
  machine lacks is blank and does nothing. The track's own eight parameters
  move the eight controls of every key around the middle; step locks lock
  them, the machine never.
- **Pitch.** A machine plays at middle C and its own PTCH tunes it, as on the
  original; the note's pitch is not used.
- **Screen.** The selected key's machine as the design draws one machine,
  from its controls, struck by each note.
- **Generated files**: `kit.h` (each machine's labels, each key's machine and
  controls, shared by engine and screen), `core.h`, `engine.cpp`, `display.*`,
  the art, `definition.json`, `machine.json` and `tests/sound.cpp` (each key
  sounds as its own machine, a lock reaches only its control, every machine
  stays bounded, an unnamed slot changes nothing).
- **Saved values.** Projects saved by builds 011 to 014 hold an earlier
  layout (the machine in value 0); these read those keys as their defaults.

**Monomachine engines** (`mmsaw`, `mmsid` ...): one machine each, played
chromatically. The SYN page's top row is page 1 of the encoders and its
bottom row page 2 (`"press": "page"`): a press of any encoder switches all
four, and the screen draws both pages in its header. The definition is the machine's own eight
controls with its defaults and presets.

**Sample engines** (`mde12`, `mmbbox`) play samples captured from the owner's
own ROMs through the emulator (`tools/capture_e12.py` for `mde12`). The samples
stay outside the repository, in `~/Library/Application Support/OPXY-MD/private/`,
and are built into the owner's own firmware only. Nothing captured is ever
committed or shared. A module includes its `private_<name>.hpp`: generation
copies in the capture where this Mac has it and the silent stand-in in
`voices/silent/` where it does not, so CI and other checkouts build and pass
silent, and the module's tests say which ran. The capture does not fit the
browser preview's 512 KiB: preview and bench a sampled engine silent, and
check its sound with `reference/fit_e12.py` and the module's tests.

## Adding a machine

1. **Measure the original.** Render it through the emulator from your own ROM
   (`reference/probe.py`, `render_many(jobs, "md" | "mm")`): sweep each control
   from a clean setting and measure what it does. `reference/fit_*.py` are the
   worked examples; write `reference/fit_<slug>.py` for yours.
2. **Write the module**, `voices/<slug>.hpp`, to those measures. Reuse
   `md_dsp.hpp`; add shared DSP there only when two families need it.
3. **Check it against the original**: `reference/compare.py --only <NAME>`
   renders the defaults and a sweep of every control on both sides, and
   `reference/probe.py`'s `render_machine` renders your module directly.
4. **Lock it in**: `voices/tests/<slug>.cpp` checks the fitted measures (pitch,
   decay, brightness, bounds at both ends of every control). See
   `voices/tests/trx_bd.cpp`.
5. **Voice it in the kit**: check its key settings in `KITS`, so the kit
   stays balanced as it starts.
6. **Regenerate and check**: `python3 tools/make_engine.py`, then
   `python3 tools/test_voices.py <slug>`, then
   `../open-xy/.venv/bin/python ../open-xy/sdk.py test --machine machines/<engine>`.
   Keep the host CPU estimate under 15,000 instructions per voice block.
7. **Listen** on the bench (`python3 tools/serve_bench.py`).

Where a machine knowingly differs from the original, say so in
`reference/README.md` with the measurement behind it.
