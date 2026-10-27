# Handoff: OP-XY–style screen for Machinedrum + Monomachine

Prepared from a Claude Design session. Design canvas: https://claude.ai/artifact/KZwW6Dbx5VaeggAtJ23617
`Main.dc.html` (this folder) is the full, current source of the single interactive artboard (520×548: the 480×222 screen plus a hardware control deck).

## What it is
An interactive mock of an OP-XY display and controls, re-imagined to drive Elektron Machinedrum (MD) and Monomachine (MNM) voices. Greyscale, line-art, OP-XY visual language. Web Audio approximations of every machine (not emulations).

## Hardware model (constraints the design follows)
- Screen: 480×222 px (retailer spec; TE doesn't publish a pixel count).
- 4 rotary encoders, clickable. Shift (latched) swaps dials to params 5–8 ("set B").
- Dial: turn = change value · click = next engine/machine for the current track · shift+click = switch MD ⇄ MNM (and releases shift).
- Function keys: shift, tempo (dials become bpm / swing / volume), play.
- 16 step keys = sequencer; shift + step key selects track (MD 16 tracks, MNM 6).

## Screen layout (480×222)
- y 0–28: 8 header cells alternating name/value, black→white grey ramp (`SHADES`), text flips light→dark at cell 4. Shows the 4 dial params.
- y 30–190: "stage" (480×160) — one bespoke view per machine, built in JS as path layers + auto-placed labels.
- y 194: info line (machine, track, family · set A/B · bpm). y 211: 16 step pips.
- Envelope / filter / FX params are intentionally NOT visualised (OP-XY puts those on separate screens); they still exist on the dials where they are part of a machine's 8 params.

## Machines (all names from the official manuals)
MD (0–127), clicking cycles within the track's role (`ROLE`/`OPTS` in source):
- TRX, EFM, E12, P-I, GND families — 49 machines with exact manual parameter lists in `MSPEC` (e.g. TRX-BD: PTCH DEC RAMP RDEC STRT NOIS HARM CLIP).
- Default kit: BD SD HT MT LT CP RS CB CH OH RC CC M1–M4 (M1 GND-SN, M2 TRX-MA, M3 E12-BC, M4 E12-SH).

MNM (0–127), 14 machines in `MNMSPEC`: SWAVE-SAW/PULSE/ENS, SID-6581, DPRO-WAVE/BBOX/DDRW/DENS, FM+STAT/PAR/DYN, VO-6, GND-SIN, GND-NOIS. Enum params render as text (`mdisp`): SID wave/mod/msrc, sync, PCH2–4 (off/+n/just ratios), CONS, listed FM ratios, signed TUNE/PW/WID.
Guesses (manual shows these only in figures): exact dial order where not stated, FM+ internal routing, PCH value stepping.

## Views (stage)
- MD: left = iso instrument illustration per class (kick, snare, tom/conga, clap, rim/claves, cowbell/triangle/metal bar, hats, cymbal, maracas/egg/tambourine, sine orb, noise/impulse); right = family panel showing source params only:
  TRX pitch contour + wave sample (hats: metal partials; cymbal: partials; clap: clap train; maracas: grains) · EFM operators + sidebands · E12 sample strip with start / retrig / bend · P-I modal spectrum · GND impulse/noise.
- MNM: full-width per-machine views (oscillator rows for SuperWave, chord keyboard for ENS/DENS, 8-bit stepped SID wave, iso wavetable for DPRO-WAVE, sample strip + key map for BBOX, dual bit-reduced waves + mix fader for DDRW, operator diagrams for FM+, vowel chart + syllable + consonant timeline for VO-6).
- Hit animation: `hitAnim()` (rAF, 520 ms; 1100 ms for shakers). Shaker grains use per-grain bounce/drift (`grainSim`).
- Labels: `placeLabels()` rasterises all drawn paths into a 2 px occupancy grid and nudges each label to a clear spot; cached during animation to avoid jitter. Suffix `!` on the align arg pins a label.

## Code map (inside the `<script type="text/x-dc">` block of Main.dc.html)
- Data: `MSPEC`, `CLS`, `DEFN`, `MACH`, `ROLE`, `OPTS`, `DEF_ENG`, `MNMSPEC`, `MNM`, `ORDER`, `info()`.
- Sound: `voice()` (MD path inline; MNM via `mnmVoice()`), `tick()` sequencer w/ swing, `ensureAudio()`.
- Views: toolkit (`mkB`, `PL`, `CIR`, `ELL`, `RCT`, `ARCE`, `STK`…), `INST.*`, panels `pTrx*`, `pEfm`, `pE12`, `pPi`, `pGrains`, `pNoise`, `pImpulse`, `mnmView`, `tempoView`, `buildView`.
- UI state/handlers: `Component` (DCLogic class) — `paramsNow`, dial pointer/wheel/key handlers, `renderVals`.
- Markup uses Claude Design's `.dc.html` runtime (`<x-dc>`, `<sc-for>`, `<sc-if>`, `{{holes}}`, `./support.js`). Porting to a normal web app means replacing that template layer (e.g. React/Svelte) — the JS logic is framework-agnostic.

## Open items / next steps
- Port off the .dc runtime into the repo's stack.
- Separate envelope / filter / FX screens (MD: AMD/AMF/EQF/EQG/FLTF/FLTW/FLTQ/SRR, DIST/VOL/PAN/DEL/REV/LFO; MNM: ATK/HOLD/DEC/REL, filter, FX pages).
- Verify MNM dial order against hardware; optional MIDI out to real MD/MNM (CC/NRPN) instead of Web Audio.
