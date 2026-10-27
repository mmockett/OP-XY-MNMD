# Family work

The machines still to build, split by family so each can be built in its own
session and branch, then merged here. Each family follows
[VOICES.md](VOICES.md) (the module contract and how to add a machine) and
[SCREEN.md](SCREEN.md).

| Family | Machines | Engines | Notes |
| --- | --- | --- | --- |
| MD TRX | XT, XC, RS, CB, CY, MA, CL, B2 | `mdtrx` | BD, SD, CP, CH, OH are done: match their fitting and level balance. Voice the toms (HT MT LT on XT, LC HC on XC) across the kit |
| MD EFM + GND | EFM BD, SD, XT, CP, RS, CB, HH, CY; GND SN, NS, IM | `mdefm` (GND also plays in `mdpi`) | Two-operator FM with pitch and modulation envelopes |
| MD P-I | BD, SD, MT, ML, MA, RS, RC, CC, HH | `mdpi` | Physical models: the CPU risk, so measure early |
| MD E12 | the 16 E12 machines | `mde12` | Samples captured from the owner's own ROM (below); E12's controls over them |
| MNM SWAVE + GND | SWAVE-SAW, PULSE, ENS; GND-SIN, NOIS | `mmsaw`, `mmpulse`, `mmens`, `mmsin`, `mmnois` | Each needs its view (`swave_view.hpp`, `mgnd_view.hpp`) |
| MNM SID | SID-6581 | `mmsid` | `sid_view.hpp` |
| MNM DPRO | DPRO-WAVE, DDRW, DENS, BBOX | `mmwave`, `mmddrw`, `mmdens`, `mmbbox` | The DigiPRO waves and BBOX samples are captured from the owner's ROM (below); `dpro_view.hpp` |
| MNM FM+ | FM+STAT, PAR, DYN | `mmstat`, `mmpar`, `mmdyn` | `fm_view.hpp` |
| MNM VO-6 | VO-6 | `mmvo6` | `vo_view.hpp` |

## Rules for a family branch

- **Commit** the family's voice modules and views (`voices/`), their tests
  (`voices/tests/`), its fitters (`reference/fit_<module>.py`), and its
  entries in `tools/catalog.py` (kit voicing),
  `tools/presets.py` and `make_engine.py` (`SOUND_HEADER`).
- **Don't commit `machines/`.** Engines are regenerated here after each
  merge; two families share `mdpi` through GND, so generated files would
  conflict. Regenerate locally to test.
- **Shared files** (`lib/`, `reference/mdref.cpp`, `reference/probe.py`,
  `tools/art/`, `tools/make_engine.py`'s generators): change them only where
  the family needs it, keep each change small and additive, and list every
  change in the final report.
- **Fit to the original**: measure with the emulator from the owner's ROM,
  lock the measures in a test, and record in `reference/README.md` what each
  control was found to do and where ours knowingly differs.
- **CPU**: keep the host estimate under 15,000 instructions per voice block;
  say which settings are heaviest.
- **No Elektron data in git**, and nothing captured from a ROM, ever.

## Captured samples and waves

E12 and DPRO need data from the ROMs: E12's 16 samples, the DigiPRO waves and
the BBOX samples. A capture tool renders them through the emulator from the
owner's ROM into `~/Library/Application Support/OPXY-MD/private/` and writes
a C++ header there. Make generation copies that header into the engine
folder, and the folder's copy is ignored by git. Without it the engine builds
silent, so CI and other checkouts still pass. The captures go only into the
owner's own firmware.
