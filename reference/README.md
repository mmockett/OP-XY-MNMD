# The reference loop

Renders the real Machinedrum and Monomachine, from your own ROMs, through the
[gearmulator-md-mm](https://github.com/joelanders/gearmulator-md-mm) emulator,
and compares them hit by hit with this repository's voice modules.

```bash
../open-xy/.venv/bin/python reference/compare.py
```

That builds what it needs on first run (`reference/build.sh` makes a headless
build of the emulator's MD library in `../gearmulator-md-mm/temp/headless` and
links `local/mdref`), renders each machine's defaults and a sweep of every
parameter on both sides, about three minutes in all, and writes:

| File | What |
| --- | --- |
| `local/ref/report.md` | Per machine and setting: peak, decay to −20 and −40 dB, early and late pitch, brightness, noisiness. Each cell is Machinedrum / ours |
| `local/ref/report.json` | The same numbers, for fitting mapping curves |
| `local/ref/pairs/<machine>-<setting>.wav` | The Machinedrum hit, a gap, then ours: listen to these |
| `local/ref/md/`, `local/ref/xy/` | The raw renders, 32-bit float WAVs |

`--only TRX-BD` limits it to one machine (by its catalog name) and
`--no-render` re-measures the last renders after changing a measure. Our side
is the voice module itself (`render_machine` in `probe.py` compiles
`render_hits.cpp` against `voices/<module>.hpp`), so a machine can be fitted
before any engine holds it.

## Fitting a machine

The comparison says how far apart two sounds are; the fitters say why. Each
renders clean sweeps (one control moving, the rest at a neutral setting),
measures what that control really does on the Machinedrum, and prints the same
measures for our machine beside it:

| Fitter | What it found on the Machinedrum |
| --- | --- |
| `reference/fit_bd.py` | TRX-BD is one sine: a base of 30.6 Hz x 2^(1.98 PTCH), a fixed thump falling linearly about 150 Hz over 15 to 20 ms, and RAMP adding up to 730 Hz (its cube) that falls at RDEC's rate. A fixed 5 ms click opens it. DEC is two-stage; CLIP is a hard clip into a 30 Hz high-pass |
| `reference/fit_sd.py` | TRX-SD's body is a sine near 342 Hz that PTCH barely moves; the wires (TONE) are a 6.98 kHz sine ring-modulated by high-passed noise; SNAP is 2 to 7 kHz noise held 15 ms; TUNE has no measurable effect |
| `reference/fit_cp.py` | TRX-CP's hands are 0.5 to 8 kHz noise, brightened by HARD and lengthened by RICH; RATE spreads them; the room is a narrow band that RTUN moves from 550 Hz to 3 kHz |
| `reference/fit_hh.py` | TRX-CH/OH are metallic partials from 3.8 to 9.3 kHz; DEC is a hold, GAP the fall after it; TRX-OH holds about 3 times as long |
| `reference/fit_tom.py` | TRX-XT and TRX-XC share a model: a downward click (52 and 29 samples), then one sine at 72.5 Hz (XC 86.5 Hz) x 2^(1.99 PTCH), plus RAMP's extra Hz (linear, up to 356 / 366 Hz whatever PTCH is) falling at RDEC's rate (tau 23 ms .. 0.6 s); DEC -60 dB in 75 ms .. 10 s (XC .. 2.5 s). DAMP lowers the body linearly to half and doubles the click. DIST clips the sine before its envelope, down to a third, into a 30 Hz high-pass; DTYP raises that high-pass |
| `reference/fit_rs.py` | TRX-RS: a tone at 172 Hz + 171 Hz x PTCH (linear in Hz), its level following its pitch, ringing for DEC (-60 dB in 92 ms .. 3.8 s), under a rim: a free-running 477 Hz sawtooth-like burst (harmonics to the 9th, a line at 2.46 kHz) held 6 ms, gone in 25. DIST adds odd harmonics about level to the 7th, keeping the tone's level |
| `reference/fit_cb.py` | TRX-CB: a free-running pulse near 1117 Hz beating against a sine near 1051 Hz, with a line at 1752 Hz. TONE is the pulse's brightness (3rd harmonic -29 .. -11 dB), ENH the second voice's level (-15 dB .. level) and some even harmonics, DAMP takes the second voice and the line away; DEC -60 dB in 9 ms .. 3.1 s. PTCH and BUMP have no measurable effect |
| `reference/fit_cy.py` | TRX-CY: a fixed set of lines, the odd harmonics of free-running squares near 432, 513, 551, 760, 774 and 848 Hz (and 1160, 1547), weighed and never tuned: SIZE a steep body high-pass (380 Hz .. 1.4 kHz), TOP and PEAK the band above 5 kHz (-10 .. +5 and +7 dB), TTUN its steep upper edge, RICH the lower squares (with 9 dB). DEC -60 dB in 1 .. 6 s, the top faster |
| `reference/fit_ma.py` | TRX-MA: a gated burst of noise grains in a 5 .. 15 kHz band, flickering 6 to 9 dB per millisecond: SUS its length (10 .. 190 ms), ATT a rise from 35 dB down (to 220 ms), REV a return shake 50 .. 170 ms on, TONE and HARD move the band up and down, RATL thickens the grains and leaves a faint rattle, DAMP darkens it. RTYP has no measurable effect |
| `reference/fit_cl.py` | TRX-CL: a pure sine at 942 Hz + 2188 Hz x PTCH, DEC -60 dB in 58 ms .. 2.3 s; ENH a harder strike (+4 dB, and a 1.5 .. 4 dB transient); DUAL a first strike 8 dB quieter up to 15 ms before the main one; TUNE a second tone on PTCH's scale, opposite in phase (-22 dB at PTCH 0, -13 at 64). CLIC has no measurable effect |
| `reference/fit_b2.py` | TRX-B2: one sine, base 10 Hz (PTCH 0) .. 106 Hz (127), plus RAMP's extra Hz (up to 457) falling with a fixed 38 ms constant; the level swells 4 dB over 20 ms, holds for HOLD (none .. 9.5 s), falls for DEC (55 ms .. 20 s, none at 127). TICK a downward spike; NOIS a noise burst above 1 kHz (70 ms); DIST a hard clip, full from 32; DIRT rounds to coarser steps, three levels at 127 (3rd harmonic gone, 5th -14, 7th -17 dB) |
| `reference/fit_pi_bd.py` | PI-BD is two head modes, f and 1.595 f, f = 28.9 Hz x 2^(1.97 PTCH); TENS stretches both up to 2.7 times at the strike, relaxing in 27 ms (less at low PTCH, none at 127, never above 157 Hz); DEC: -60 dB in 31.3 ms per step; a 4 ms burst of the head's upper modes near 2.2 kHz that HAMR lowers. HARD and DAMP do nothing measurable |
| `reference/fit_pi_sd.py` | PI-SD's head rings at 1, 1.21, 1.77 (and 0.59, 3) times 86 .. 340 Hz; DEC 20 .. 276 ms; TENS +-6 %, HARD +3 %; RING a mode at 2.75 f, ringing longer; RVOL (squared) and RDEC (20 ms .. 0.68 s) the wires; a 5 ms noise burst at the strike |
| `reference/fit_pi_rs.py` | PI-RS: nine rim modes at fixed 144 .. 668 Hz that PTCH does not move but rebalances (measured at each step), PI-SD's head under them, DEC 0.2 .. 1.66 s; the crack is 5 ms of noise near 1.7 kHz moving with PTCH, louder with HARD; RING and the wires as PI-SD's |
| `reference/fit_pi_mt.py` | PI-MT: head modes at 1, 1.1, 1.99 times 69 Hz x 4^PTCH, and TUNE's at 1.01 .. 2.09 times (POS: its level, -14 .. -4 dB); the shell is a tube, harmonics of 1 / (1.063 ms + 33.1 us x SIZE), which PTCH does not move; DEC 29.3 ms per step (shell 1.43 times longer); DAMP shortens the shell, its upper harmonics most |
| `reference/fit_pi_ml.py` | PI-ML: seven modes at 0.58 .. 2.07 times 372 Hz x 2^(1.99 PTCH), their balance different every hit; DEC 30 ms .. 1.8 s; HARD 0 is 14 dB quieter, mostly the upper pair; TENS bends the strike up to twice the pitch, relaxing in 20 .. 120 ms |
| `reference/fit_pi_ma.py` | PI-MA is a shaker: the shake falls over DEC, seeds hit at random (GRNS: more, smoother, louder), each ringing GLEN long (smoother); the shell's band brightens with SIZE (3.2 .. 6 kHz centroid) and darkens with HARD (5.2 .. 2.3 kHz) |
| `reference/fit_pi_cymbal.py` | PI-RC, PI-CC, PI-HH: sixteen partials each (the crash and the hat share theirs), struck afresh every hit; each control's effect on level, ring and brightness read at every step and added; GRAB below 80 grabs the cymbal after 40 .. 135 ms; on the hat CLOS 127 is closed, lower values ring until the pedal closes it with a chick, 35 ms .. 2.25 s in |
| `reference/fit_e12.py` | The E12 machines play 22 ROM samples (below). PTCH is (v - 64) / 32 octaves; DEC an exponential fall, none at 127; HP a two-pole high-pass, 45 Hz to 6.3 kHz, with HPQ its resonance; STRT a fixed step per machine; RTRG RTRG - 1 repeats every RTIM, in 32-sample blocks; BEND an octave either way, drifting back over about 20 s |
| `reference/fit_sid_6581.py` | SID-6581 (Monomachine) plays MIDI pitch, within 1.3 cents from note 24 to 108. TRI, SAW (falling) and PULS share one amplitude; PW is symmetric, duty 1 - \|PW - 64\| / 128, so PW 64 is silent; PWAD sweeps PW at 40 to 332 steps a second, wrapping, from PW at the note with PWRS on; MIX is one fixed shape 10 dB under the saw; NOIS is noise held 16 times a cycle. TUNE is +-2 semitones. MFRQ puts the modulator at 2^((MFRQ - 32) / 32) x 261.6 Hz on every key; RING ignores WAVE (the triangles and 1.1 x their product, its difference tones high-passed near 200 Hz, 8.4 dB under the saw); SYNC restarts WAVE at the modulator's pitch half way through its cycle each cycle of the note (R+S: at its start). The output rolls off above 2 kHz |
| `reference/fit_vo_6.py` | VO-6 is a buzz of impulses (or noise) through four resonances in cascade, each with unity gain at DC; VOC1 is the first formant, VOC2 the second; the level is held whatever the formants. Twenty consonants on CONS, lengthened by CLEN and scaled by CVOL. [VO-6](#vo-6) has the details |
| `reference/fit_swave.py` | SWAVE-SAW is a main saw, a pair at +-UNIW (0 to 53.7 cents, linear) at UNIL's level and an inner pair at half the detune at UNIX x UNIL, all starting in phase; SUBX is a saw an octave down, SUB1 and SUB2 sines one and two octaves down. SWAVE-PULSE's duty runs 1% to 99% over PW; PWAD sweeps it on, wrapping, at 0.0422 cycles a second per step, and PWRS restarts the sweep at the note. SWAVE-ENS's PCH2..4 pick 33 sorted intervals (-12 to +12 semitones, just 6:5, 5:4, 4:3, 3:2 both ways, off at 61..63); WAVE crossfades the saw into a pulse an octave down whose duty is PW / 128 |
| `reference/fit_mgnd.py` | GND-SIN is a pure sine; GND-NOIS is white noise rolled off above 7 kHz, turning red (a 1-pole near 170 Hz) with RED and loudest near its middle; ST adds up to +2.9 dB while STON is off |
| `reference/fit_fmp.py` | FM+STAT, FM+PAR and FM+DYN (Monomachine; note 60 = 261.9 Hz) are one sine carrier at the note, true-FM modulated by two (STAT, DYN) or three (PAR) sine modulators in parallel at ratios of it. A depth control (and an ENV control up to 64) is an index of (pi/2)(v/64)^2 radians to 64, then a line to about 14 at 127; an ENV above 64 is an exponential decay of the index (about 22 s at 72, 95 ms at 96, 7.6 ms at 124) that does not follow the key. STAT and PAR's FRQ steps through 24 listed ratios, 1:64 to 4; DYN's 1FRQ is v/64 and 2FRQ (v/64)^2, 127 counting as 128. DYN's 1FEN sweeps M1's ratio and 1VEN its depth at the start, both signed about 64. Feedback (1FB, 2FB) adds harmonics to 32 and from about 48 shifts the carrier's pitch up (1FB 64: 323 Hz). TONE is a low-pass on the modulators that darkens high ratios and high notes first. TUNE is +-2 semitones |
| `reference/fit_dpro.py` | The Monomachine's DigiPRO machines; below |
| `reference/fit_efm_bd.py` | EFM-BD is a sine carrier, 25.7 Hz x 2^(3.96 PTCH), frequency-modulated by a sine at MFRQ / 16 of it (0 plays as 1/16) with a deviation of MOD / 16 times the carrier; RAMP starts the pitch RAMP / 16 times higher, settling at RDEC's rate (1.8 ms to 2.4 s); MDEC is the deviation's decay; MFB feeds the modulator back one sample late, which pulls the carrier down (102 Hz to 23 Hz by MFB 48) and goes chaotic near the top. DEC falls over a quiet tail from -20 dB |
| `reference/fit_efm_xt.py` | EFM-XT is EFM-BD's voice over two octaves (43 to 171 Hz) without the tail, through a one-pole high-pass near 125 Hz (8 dB quieter at PTCH 0 than 127), with about half the deviation; CLIC starts the carrier up to 0.19 cycle in, which the high-pass turns into a click |
| `reference/fit_efm_sd.py` | EFM-SD's tone (122 to 680 Hz) has its modulator fed back at full strength, so MOD spreads it into a band of lines around its pitch; NOIS adds noise in proportion, coloured by about two poles of 2.5 kHz high-pass, decaying at NDEC's rate (25 ms to 1.7 s); HPF is one pole from off to 3.3 kHz over both |
| `reference/fit_efm_cp.py` | EFM-CP is EFM-SD's tone (122 Hz to 5.4 kHz) under CLPS / 8 claps (none at 0), CDEC apart (1 ms to 41 ms), each falling 18 dB before the next, then a tail at DEC's rate |
| `reference/fit_efm_rs.py` | EFM-RS is two of SD's tones: the rim (244 Hz to 3.8 kHz, MOD a sixth as strong) and the snare (SPTC as SD's PTCH, SMOD three quarters), the snare in proportion to SNAR |
| `reference/fit_efm_cb.py` | EFM-CB is two equal sines 1.48 apart (172 to 960 Hz), each with its own modulator at MFRQ / 16 and half BD's deviation; FB feeds each back on itself (second harmonics -8 dB at 64, a buzz at the top of the band from 96); SNAP takes up to 78% of the level away over 50 ms |
| `reference/fit_efm_hh.py` | EFM-HH's noise is a click of random height every 32 samples, falling to 0.4 and 0.1 of it over the next two; under it an FM carrier 345 Hz to 1.4 kHz through a high-pass near 5 kHz, its feedback buzzing at the top of the band from FB 48; TREM is a tremolo at TFRQ's rate (0.95 to 59 Hz); DEC as BD's |
| `reference/fit_efm_cy.py` | EFM-CY is six sines at 1, 1.41, 1.80, 2.70 (level) and 3.50, 4.94 (-11 dB) of 244 Hz to 1.36 kHz; MOD's modulation is wide (most of the sound above 5 kHz by MOD 64), and MDEC takes little of it away; FB buzzes from 80 |
| `reference/fit_gnd.py` | GND-SN is a sine an octave every 16 steps (43 Hz to 10.6 kHz) whose start is (RAMP / 16)^2 times the pitch above it, falling back at RDEC's rate; GND-NS white noise; GND-IM two steps below zero (UP and DOWN samples long, UVAL and DVAL squared deep) through a 35 Hz high-pass |

The TRX fitters share `reference/trx.py`: its `live` renders as `render_many`
does, then renders again any hit that came back dead. About one hit in 25 the
emulated machine sounds only a step that drains away, as if its oscillator
never ran; the same settings rendered again sound normally. Its `onset` finds
a hit's start from the hit's own level: `mdref` leaves a second between jobs,
so a long hit's tail can still be sounding, 60 dB down, when the next starts,
and the first sample off silence is then the previous hit's.

### Where ours knowingly differs (TRX)

| Machine | Difference, and the measure behind it |
| --- | --- |
| TRX-XT, XC | DTYP: the Machinedrum's waveform at DIST 127, DTYP 127 is a ramp-and-jump that keeps its peak and raises the 5th and 7th harmonics to 6 and 8 dB under the fundamental; ours raises the high-pass after the clip, which matches the 3rd harmonic (-6 dB) and the 4.5 dB fall in level but leaves the 5th and 7th 8 to 14 dB lower |
| TRX-RS | DIST's odd harmonics match at PTCH 64 (-39, -28, -21, -16 dB at DIST 32 .. 127) but not at PTCH 0, where the Machinedrum's are 8 dB stronger. The rim's per-millisecond level swings more than the Machinedrum's (it is one sawtooth, theirs is smoother); lifted 12.5 dB |
| TRX-CB | PTCH and BUMP do nothing measurable on the Machinedrum (1109 .. 1117 Hz across PTCH); here PTCH tunes the bell a fourth either way and BUMP bends its strike up to a fifth over 10 ms. Lifted 9 dB |
| TRX-CY | The 4 kHz band sits about 6 dB over the Machinedrum's and the top octave (16 kHz) 10 dB over; TTUN 127 opens 11 kHz a little further. Squares are band-limited to their 13th harmonic: naive ones folded aliases into the top octave 25 dB over the Machinedrum's. Lifted 15 dB |
| TRX-MA | RTYP does nothing measurable on the Machinedrum; here it lengthens the grains. The band follows TONE and HARD with wider reaches than the measured centres move, since a one-pole low-pass is soft near Nyquist. The grains peak near the ceiling (lifted 26 dB) |
| TRX-CL | CLIC does nothing measurable on the Machinedrum; here it adds a millisecond of noise above 3 kHz. The second tone's slight intermodulation (lines at 2 f1 - f2, 35 dB down) is left out |
| TRX-B2 | TICK's spike is 1.7 x the body (2 x on the Machinedrum) and DIST's peak stops at the SDK's half-scale ceiling: lifted 7 dB, both would pass it |

`reference/pi_measure.py` holds the P-I fitters' measures (partials, mode decays,
bursts, grain texture, the mode tracker for bending strikes) and their sweeps.
`reference/cpu_voice.py PI-BD ...` gives the SDK's host CPU estimate for one module
before any engine holds it: a kit's own SDK test times only the key its 440 Hz
note lands on.

### The DigiPRO machines

Measured on the emulated Monomachine (OS 1.32b), with the amp page held open
(CC 57 HOLD and CC 58 DEC at 127; CC 56..59 are ATK HOLD DEC REL, CC 72/73
the filter's BASE and WIDTH, open by default). `tools/capture_dpro.py`
captures the waves and samples privately; `reference/fit_dpro.py` compares.

- **The waves.** The DigiPRO reads its tables raw: high notes alias (the
  inharmonic part of S01 rises from -44 dB at note 24 to -4 dB at note 96), and
  the path after it is flat within 1.5 dB to 6 kHz (D06 SAW against 1/k).
  Ours reads 512-sample tables the same way, linearly interpolated.
  D01..D64 (SINE, HSIN, TRI, ZOID, SQR, SAW, ISAW, RND, SYN1-8, ORG1-9, ORGA-D,
  ELP1-3, STR1-8, WND1-8, MAL and FEM vowels) are one cycle each, two CC steps
  apiece; their fits at notes 24 and 36 agree to 0.97 or better (most 0.999).
  D08 RND is noise that drifts: at note 24 one cycle correlates 0.96 with the
  last, about 512 random points a cycle; ours makes it (a fresh set every 8
  cycles, crossfaded: 0.94), from 512 random levels each voice holds. The oscillators run freely from note to note, and every D-wave's
  first harmonic sits at the same phase against its table's start, so the
  tables are stored lined up that way: DDRW's crossfade depends on it.
- **DPRO-WAVE** plays S01..S32 (four CC steps each), tables of two cycles at
  the played pitch (S32's is one slow cycle, an octave down). WP moves a set
  towards the next: its shape changes in plateaus (8..24, 32..48, 64..80,
  96..120 look alike), and at 127 it is the next set (0.96 or better for all
  31, mostly 0.99); S32 reaches a wave of its own. Ours crossfades captures at
  WP 0, 16, 48, 80 and 112 and the next set: harmonic shapes match 0.96 to
  0.998 across WP (S01 0.93 to 0.99, levels within 14%). WPM is a triangle sweep of the whole WP
  range, 0.38 Hz at 32 and 1.33 Hz at 127 (ours 1.33 (WPM/127)^0.9 Hz); WPRS
  restarts it at the note. SYNC SFRQ restarts the wave at a rate that does not
  follow the note: 51 Hz at SFRQ 64 on notes 36 and 48 alike, 29 Hz x
  2^((SFRQ - 32) / 39) (ours repeats every 853 samples at 64, the original 864).
  SYNC PRCH syncs to the previous track, which the OP-XY lacks: ours plays
  unsynced there. TUNE is +-2 semitones (measured -2.01, +1.97).
- **DPRO-DDRW** crossfades WAV1 to WAV2 with MIX (the first harmonic of SINE to
  SQR stays at 0.183 throughout, ours 0.182). WID detunes the two apart in
  cents, WAV1 down and WAV2 up, scaled by how evenly they are mixed: +-27 cents
  at WID 64 and +-53 at 127 at MIX 64, none at MIX 0 or 127, the same at notes
  36, 48 and 60 (ours -53/+53 against -53/+55). BR1 and BR2 cut each wave's
  bits on its own scale, 16 at 0 to 1 at 127 (shapes 0.98, levels 0.89 to 1.0).
  TIME changed nothing we could measure on a held note, so ours ignores it.
- **DPRO-DENS** adds up to three voices of one D-wave, each a quarter of DDRW's
  level. PCH2..4 step in fours, read off the sound (the LCD rounds them two CC
  steps away): 0..61 are -12..-1 semitones with the just 2/3, 3/4, 4/5, 5/6 in
  pitch order, 62..63 OFF, 64..127 +1..+12 with 6/5, 5/4, 4/3, 3/2. CHRL and
  CHRW are its chorus: CHRL deepens a slow detuning of each voice and CHRW
  spreads it in stereo, which partly cancels in the mono mix; ours detunes a
  copy +-15 cents at 0.6 Hz, mixed CHRL (1 - 0.7 CHRW). This is the roughest
  fit: the original's copies sit nearer +-10 cents.
- **DPRO-BBOX** has 24 samples on 24 keys, repeating every two octaves from C;
  each key also adds a quarter semitone, so two octaves up is the same sample 6
  semitones higher (6.00 measured). PTCH spans -20 to +21.8 semitones, STRT
  starts up to 0.37 of the sample in (5.7 frames a step on a 57 ms sample, 47.5
  on a 295 ms one), RTRG restarts it that many times, RTIM apart: about
  1000 ms / (RTIM + 1) (990 ms at 0, 110 at 8, 58 at 16, 15.6 at 64, 8 at 127).
  Key 10 makes fresh noise at every hit on the original (two hits of one note
  differ); ours plays the one captured. Keys 20 and 21 repeat hit to hit, but
  their noise does not follow the pitch, so they are checked by spectrum. The
  original does not only resample across keys: two octaves down a sample's
  body is smoother and its tail longer in the sample's own time, and above
  note 60 it stops behaving as a resampler at all (key 20 on note 68 matches
  note 44 only 0.33). Ours resamples what was captured on keys 36..59, so above
  about 15 kHz keys 12..23 are empty, and at middle C (sample 12) it matches
  the original 0.81 in waveform, 78 against 85 ms, peaking 0.49 against 0.37.

`reference/efm_measure.py` holds the EFM and GND fitters' measures, and renders
under an exclusive lock on `EMULATOR_LOCK` (default `local/emulator.lock`) so
several fitting sessions can share one emulator. `reference/cpu_voice.py` is
the SDK's host CPU estimate for one voice module.

### Where the EFM and GND machines knowingly differ

- On the emulator, a job whose swept control sits at 0 sometimes starts a few
  milliseconds late or quiet; those rows are not fitted.
- EFM-SD, CP and RS: the Machinedrum's MOD spreads the tone into a band whose
  lines we match in spacing and level, not one by one; our carrier drifts up
  with MOD a little more (EFM-SD's 293 Hz becomes 317 Hz at MOD 32, the
  Machinedrum's 293).
- EFM-XT: CLIC 127 starts at 1.75 times the body's swing, the Machinedrum's at 1.45.
- EFM-HH: past FB 48 the Machinedrum's buzz sits almost all in the top octave
  (-20.7 dB RMS); ours spreads lower and is 7 dB quieter. The clicks below
  500 Hz stand up to 8 dB louder than the Machinedrum's.
- EFM-CY: the -11 dB partials (3.50, 4.94) are not frequency-modulated, to keep
  the voice under 15,000 instructions; FB feeds the partials' sum back, not each
  partial; partials are table sines without interpolation (errors near -60 dB).
- GND-SN: RAMP past about 90 sets a start above half the sample rate, which the
  Machinedrum aliases and ours holds at the top.
- All are 2 dB over the Machinedrum, as the TRX machines are.

`reference/curves.py`, `show_env.py` and `show_peaks.py` print pitch tracks,
envelopes and per-millisecond peaks for any pair of renders. Renders are cached
in `local/ref/probe-md/` by their settings, and ours in `local/ref/probe-<module>/`.

## The P-I machines: where ours differs

Each P-I module is fitted to the measures in its fitter, and levels sit 2 dB over
the emulator's main mix (`LEVEL_DB` in `pi_measure.py`, from TRX-BD and TRX-SD),
except where said. Known differences, with the measurement behind each:

- **PI-BD**: the stretch is fitted on the head's track (x1.2 .. x2.7 at PTCH 64);
  at PTCH 0 ours stretches a little less at TENS 0 and more at TENS 64 (x1.07 /
  x1.47 against the Machinedrum's x1.21 / x1.34). The strike's burst is one tone,
  as the Machinedrum's appears to be. HARD and DAMP do nothing on OS 1.63 (every measure identical over their range,
  also at DEC 127 and HAMR 0 or 127), so ours ignore them too.
- **PI-SD**: the head's modes start in phase and beat a little deeper in the first
  15 ms. The wires are filtered noise; the Machinedrum's carry a little more low
  rattle (its 20 to 1000 Hz band decays slower at RVOL 127).
- **PI-RS**: the crack is noise in a band, so its crest is higher: at HARD 112 and
  over ours touches the SDK's ceiling for a few samples where the Machinedrum peaks
  at -11 dB. At PTCH 0 the Machinedrum's crack lingers (-26 dB at 12 ms, ours -47).
- **PI-MT**: which shell harmonic is strongest varies with SIZE on the
  Machinedrum (the second at SIZE 32 and 112, the fourth at 96); ours keeps the
  first strongest. TUNE's mode is fitted in pitch only: on the Machinedrum it grows
  as it nears the shell's pitch (TUNE 112 and 127), ours does not. The first 50 ms
  are darker (centroid 430 Hz against 757 Hz). HARD does nothing here; the
  Machinedrum's moves the first 5 ms a little.
- **PI-ML**: the Machinedrum's envelope is smooth (+-1 dB in 2 ms windows) where
  ours beats between its seven modes, so ours peaks 4 to 5 dB higher for the same
  loudness. The hit-to-hit variation is ours: random levels (-5 .. 0 dB) and phases.
- **PI-MA**: the shake is random, so each measure is noisy hit to hit on both sides;
  ours sits 6 dB over the usual level (8 dB over the emulator's main mix) so the
  maracas sit in a kit.
- **PI-RC, PI-CC, PI-HH**: our partials stop at 6.2 kHz, so the brightest settings
  (AU or BR near 127) reach a centroid of 5 to 5.5 kHz where the Machinedrum's
  reaches 7 to 8 kHz. RING does nothing measurable on the ride; AG and AU nothing on
  the crash. The Machinedrum's cymbals and hat sit far under its drums (the crash
  30 dB under PI-BD at the design's settings), so ours sit 10 dB over the usual
  level (12 dB over the emulator's main mix), and the kit voices them louder still. Ours add one thing: a closed
  hat (CLOS 127, DEC under 64) chokes open ones, as the TRX hats do.

CPU, host estimates (`reference/cpu_voice.py`, instructions per voice block at the
heaviest setting): PI-BD 1,500, PI-SD 5,100, PI-MT 4,500, PI-ML 3,800, PI-MA 7,100,
PI-RS 9,300, PI-RC 4,500, PI-CC 4,500, PI-HH 4,100. The heaviest are PI-RS (twelve
modes, with the crack and the wires sounding) and PI-MA at high GRNS.

## The E12 samples

`tools/capture_e12.py` captures the sixteen E12 machines' samples from your
own ROM into `~/Library/Application Support/OPXY-MD/private/e12/` (never into
a checkout) and writes `private_e12.hpp` there, 12 bits a sample as the
original stores them; `make_engine.py` copies it into `machines/mde12/`, where
git ignores it. A sample is played with every control where it leaves the sound
alone: PTCH 64, DEC 127, HP 0, STRT, RTRG and BEND neutral, and the machine's
own control at its neutral end. The tool checks that the hit played again is
the same (about 0.002 apart), that track volume 110 scales it by exactly
(110/100)^2 and nothing else, and that one step in from each neutral end
barely moves it (HPQ's first step moves it most, 0.09). Six machines' own
controls crossfade, linearly (the second sample's share is v / 128, all of it
at 127), to a second sample, captured at 127: RING (SD), BELL (RC), REAL (BR),
SLEW (SH) and BC (BC), and RATL (RS) along a steep curve (4 % at 80, 35 % at 112).
The tool checks a setting between is exactly that mix (within 1 %). STRT's
step differs from machine to machine (6.2 frames on BC, 254 on OH) and is
measured at STRT 64 and checked at 32. 22 samples, 9.5 s, 616 KiB.

What each control was found to do, and where ours knowingly differs:

- **PTCH**: (v - 64) / 32 octaves from the sample's own speed, within 0.005
  octave of ours from PTCH 0 to 127 (`fit_e12.py ptch`).
- **DEC**: a fall in dB linear in time, -60 dB after about 15 ms at DEC 0..16,
  120 ms at 48, 300 ms at 64, 2.6 s at 96, 10 s at 112, none at 127. Ours is
  within about 1 dB from 48 up; at 120 to 126 the Machinedrum falls slower than
  our longest fall (24 s), under 2 dB by 700 ms.
- **HP**: a two-pole (12 dB an octave) high-pass, cutoff (-3 dB) about 45 Hz
  at 0, 120 Hz at 16, 380 Hz at 32, 850 Hz at 48, 1.6 kHz at 64, 3 kHz at 80,
  5 kHz at 96 and 6.3 kHz at 112 and up, where it lifts the top octave by up to
  10 dB. HP 0 is in the capture. Ours is within about 1 dB above the
  Machinedrum's own floor, which sits 30 to 60 dB down.
- **HPQ**: the high-pass's resonance: a peak of +11 dB at HPQ 16 to +16 dB at
  127 at the cutoff; at HP 0 it rings much harder, +17 dB at 16 to +34 dB at
  127 near 45 Hz. Ours follows to +20 dB there and stops: the filter is not
  stable with less damping.
- **STRT**: a fixed step per machine (above), in frames whatever the pitch.
- **RTRG / RTIM**: RTRG plays the sample again from STRT RTRG - 1 times, at
  full level and without end at 126 and 127; DEC runs on across the repeats.
  RTIM is the period: about 0.47 ms a step up to 64 (30.3 ms), then 1.41 times
  longer each 16 steps (120 ms at 127), always whole 32-sample blocks.
- **BEND**: the pitch starts (v - 64) / 64 octaves away and drifts back, about
  a tenth of the way by 600 ms; ours within 1 % at every setting.
- **STOP** (E12-OH): the sample ends 5.8 ms a step after the hit (372 ms at
  64), in about 1.5 ms; never at 127.
- **SNAP / SPLN** (E12-BD, in the HP and own slots): SNAP drives the sample
  1 + 2 SNAP times as hard (3 times at 127) into a clip, for SPLN's time (none
  at SPLN 0, about 10 ms at 16, 260 ms at 64, held at 127). The clip is at
  about 0.3 on the Machinedrum's scale, from the level of the clipped windows;
  its clip is softer than ours and peaks higher, to 0.46.

Our level is the Machinedrum's times 1.2, so E12-BD peaks as our TRX-BD does;
the kit keeps the Machinedrum's balance between its machines.

### Where SID-6581 knowingly differs

- **MIX** is an original formula (nothing for half a cycle, then a rippled
  raised-cosine hump), not the Monomachine's wave: its shape correlates 0.84
  with the measured one and its level matches, but its 2nd and 4th harmonics
  are 3 and 15 dB weaker (`fit_sid_6581.py`, `mix`).
- **MSRC = PRCH** takes the previous track's oscillator on the Monomachine.
  The OP-XY has no previous track to read, so PRCH rings and syncs with the
  pitch of the note played before this one (`md::Common::previous_pitch`).
- **The roll-off** is two one-pole low-passes at 6 kHz: within 1 to 2 dB to
  8 kHz; above that the Monomachine falls faster (-26 to -50 dB at 16.7 kHz).
  The Monomachine also plays high notes quieter (about -3.6 dB at 2 kHz,
  -6.8 dB at 4 kHz); ours keeps the level.
- **The screen** draws three cycles in 47 steps where the design draws four
  finer ones (four 64-point paths cannot hold more), draws the ring as the
  design does (the wave times the modulator's sine) without its dashed
  modulator line (no path is left wide enough), and shows the pulse's true
  width (PW 64 full) where the design maps PW 64 to a half-width pulse. It
  leaves out the design's words over the wave ("saw · ring", "mod from previous
  track"): they change with the steps, and the device's SVG parser takes only
  fixed text or one node per stroke (docs/SCREEN.md); the header names WAVE,
  MOD, MSRC and PWRS's steps instead.

## VO-6

Measured on the emulated Monomachine (OS 1.32b) by `reference/fit_vo_6.py`,
with the amp page held (`cc57=127`) where a steady level matters.

- **The vowel** is a cascade of four two-pole resonances with unity gain at DC,
  fitted to whispered vowels within 1.6 dB rms from 100 Hz to 3.7 kHz. VOC1
  moves the first formant from 195 Hz to 708 Hz as 195 x 2^(1.86 VOC1/127);
  VOC2 the second from 499 Hz to 2675 Hz, about 0.3 octave every 16. The third
  sits at 2490 Hz and the fourth at 3575 Hz, except that the third keeps 1.11
  times above the second once VOC2 passes about 110. The widths hardly move:
  70, 55, 145 and 280 Hz. So the design's guess that VOC1 and VOC2 are the two
  formants of a vowel chart holds, though not its linear scales.
- **Level.** The Monomachine holds a vowel's power whatever its formants: the
  bare cascade swings 18 dB across VOC1 and VOC2, the original 3 dB (rising
  2.9 dB across VOC1). Ours divides out the cascade's noise gain from a 9 x 9
  table (`power` in `voices/vo_6.hpp`).
- **The buzz** is flat in spectrum, like an impulse train, and keeps its power
  at every pitch. Ours places each impulse between two samples by its phase
  and scales it by the square root of the period. It tracks equal temperament
  from middle C (note 60 is 261.6 Hz).
- **VOIC** fades the buzz into noise of the same power, through the same
  formants: the buzz's share of the power is 1, 0.99, 0.94, 0.84, 0.55, 0.39,
  0.09, about 0.01 and 0 at 0, 16, ... 112, 127.
- **V-SW** is a switch, OFF below 64: off, a consonant plays alone and the
  vowel never comes (with CONS at `-` the vowel plays either way).
- **CONS** has 21 steps of six values each (v x 21 >> 7, read off the LCD):
  `- B D F G H J K L M N P R RR S SJ T TJ TH V Z`. The definition declares it
  and V-SW as enums (`ENUMS` in `tools/make_engine.py`). Each consonant is
  modelled as silence (J L M N wait 50 ms, R 30, RR 20), up to two segments
  that voice through a locus of formants, aspirate through the formants, or
  hiss through a band, then the vowel's rise, its formants' glide from the
  locus and, after most continuants, an overshoot of 2 to 10 dB that settles
  in about 80 ms. The table is fitted to the original's level tracks: 2.0 dB
  mean difference over the first 250 ms across CLEN 0, 64, 127 and CVOL 0, 127.
- **CLEN** lengthens only the continuants (F H J L M N R RR S SJ TJ TH Z), by
  about 500 ms x CLEN^3.6 (S: the vowel arrives at 95, 100, 100, 110, 135, 185,
  270, 395 and 595 ms for CLEN 0, 16, ... 127). The plosives (B D G K P T V)
  keep their length.
- **CVOL** scales the noise of a consonant (hiss and aspiration), linearly,
  down to about 22 dB under full at 0; voicing is not scaled.
- **TUNE** is +-2 semitones, (TUNE - 64) / 32 semitones; 63 and 64 both play
  261 Hz.

Where ours knowingly differs:

- The original's buzz leaves an inharmonic floor 45 to 60 dB under the vowel
  above 4 kHz (its harmonics there are 80 to 100 dB down: aliasing or rounding,
  not filtered buzz); ours is clean above the fourth formant. At high notes the
  original also has subharmonic dirt (note 84: 40 dB down at 300 Hz) that ours
  has not.
- The consonants are a model, not a copy: their timing and levels follow the
  original within about 2 dB, but their spectra only roughly (our hiss's
  centroid within 10 to 20%, the original's fricatives showing the vowel's
  third and fourth formants). B's rise does not lengthen with CLEN as the
  original's does slightly.
- A harmonic landing on a narrow formant stands up to 13 dB over the usual
  level (it does on the original too, which has more headroom): ours turns the
  excitation down when a block peaks above 0.36 of full scale, so the
  controls' ends stay under 0.41. Below C1 the impulses stop growing taller,
  so the very lowest notes are quieter than the original's.
- The screen's chart keeps the design's axes (first formant 250 .. 900 Hz
  across, second 700 .. 2500 Hz up) and its vowel letters; the handle and
  crosshair sit at the real formants, so they reach the chart's edges only
  where the original's range does. The design's "vowel" and "cons · clen"
  labels are left out (no free path for text), as are its grains: the
  consonant's burst is drawn as a trace before the vowel's wave.

## The Monomachine

Fitted with the AMP page's HOLD at 127 (`cc57=127`, held while the note is,
with `cc56=0 cc58=127 cc59=10`): the machine's initial pages otherwise decay
the note within half a second. The AMP page is CC 56 to 63 (ATK HOLD DEC REL
DIST VOL PAN PORT); the filter's BASE and WDTH are CC 72 and 73 (open at the
initial 0 and 127); CC 80 and 81 are the EQ. Two things to know:

- **A job keeps the last one's free-running state.** PWAD's sweep of the
  pulse width runs on between notes and between jobs; a pulse job with PWAD 0
  plays at whatever width the last left. Reset it with PWRS (slot 7 at 127)
  and run PWAD's own jobs last. Other machines' oscillators restart at the note.
- **A PCH value of 0 is -12 semitones on SWAVE-ENS**, not off: off is 61..63.

Each Monomachine engine has its own level (the fitters' `GAIN`): SAW x 2.2,
PULSE x 1.6, ENS x 2.4, GND-SIN x 2 and GND-NOIS x 1.2 of the Monomachine's
output, so each one's loudest settings stay under the SDK's half scale. The
Monomachine's own levels differ much more (SWAVE-SAW's plain saw is 22 dB
under GND-SIN); within each machine the levels are the original's.

Where ours knowingly differs, by measurement:

- **Pitch.** The Monomachine's SWAVE-SAW and ENS play 0.6 to 0.9 cents sharp
  (261.71 Hz for middle C); ours plays the key exactly.
- **SWAVE-SAW's tone.** The original's saws alias heavily: what is not
  harmonic is only 8 dB under the harmonics at middle C (13 dB at C2, 3 dB at
  C6), with a notch near 5.2 kHz. Ours is a plain saw, cleaner, through a
  high-pass at 1.9 x the fundamental (the original's thin fundamental, 2 dB
  over the 2nd harmonic instead of a saw's 6) and a low-pass near 3.4 kHz:
  its first eight harmonics match within 1.5 dB from C2 to C5 and within
  2.5 dB at C6.
- **SWAVE-PULSE** matches within 1 dB at its first eight harmonics; the even
  harmonics of a square are about 75 dB down on both.
- **SWAVE-ENS's chorus** is two copies per voice detuned 6 to 16 cents either
  way by CHRW, drifting +-3 cents over two seconds. The original's beats about
  3 Hz at middle C and drifts the pitch +-9 cents over a few seconds; CHRW
  barely changes it. The levels match within 5%.
- **GND-NOIS** keeps 15 dB more above 12.8 kHz than the original, and about
  3 dB more under 100 Hz when red. The original's ST is presumably a stereo
  control; heard in mono it is the level change above.

### Where FM+ knowingly differs

- **The ratio list** is the order the sound steps through. The Monomachine's
  LCD shows 5:32 five steps later than the sound plays it; ours names each
  step by what it plays.
- **Depth** matches within a few tenths of a dB at 2VOL 32 (H2 -13.4 against
  -13.6 dB); at 64 our H2 is 1.7 dB stronger and H3 1.6 dB stronger.
- **Feedback** shifts the pitch as the original's does at 1FB 64 (322 against
  323 Hz); between 48 and 64 it is trimmed to three measured shifts (+26,
  +52, +61 Hz) and can run a little higher in between.
- **The widest settings**: the carrier's deviation is held within 6.2 kHz (or
  the carrier's own pitch, if higher), as the original's seems to saturate.
  That keeps most aliasing under about 8 kHz, but the limit is an estimate.
- **The screen** leaves the design's ratio labels ("x3.5") to the header,
  which names each FRQ step: they change with the step, and the device takes
  only fixed text (docs/SCREEN.md). FM+PAR's output wave is drawn in 32
  points on its one free path, so three modulators at high ratios draw jagged.
  Its carriers grow with their ENV but keep their resting fill: the design
  fills one past ENV 0.3, which the rig cannot switch.

## The ROMs

The Machinedrum ROM is read from `MD_ROM`, or
`~/Downloads/elektron_sps1-1uw_os1.63.bin` by default (exactly the MD UW OS
1.63 image), and the Monomachine's from `MM_ROM`, or
`~/Downloads/elektron_sfx6-60_os1.32b.bin` (OS 1.32b). `mdref` tells them
apart and only reads them. `render_many(jobs, "md" | "mm")` picks one. The
ROMs are Elektron's copyright: they stay outside every repository, and
nothing rendered from them is committed. Samples captured from them for the
E12 and BBOX engines stay in `~/Library/Application Support/OPXY-MD/private/`.

## How a hit is made

`mdref` boots the Machinedrum (about 20 emulated seconds, including the UW's
first-run flash set-up). For each job it assigns the machine to track 1 with
the OS 1.63 SysEx *assign machine* message (`F0 00 20 3C 02 00 5B 00 <id> 00 F7`).
It then sets the eight synthesis parameters over CC 16 to 23 and gives the
track neutral settings: no AM, flat EQ, open filter, no SRR, distortion,
sends or LFO, volume 100, level 100. Finally it presses trig 1 and records the
main mix. Machine IDs, as read back from the emulated LCD:

| ID | Machine | ID | Machine | ID | Machine |
| --- | --- | --- | --- | --- | --- |
| 0 | GND-EMPTY | 18 | TRX-XT | 24 | TRX-CY |
| 1 | GND-SN | 19 | TRX-CP | 25 | TRX-MA |
| 2 | GND-NS | 20 | TRX-RS | 26 | TRX-CL |
| 3 | GND-IM | 21 | TRX-CB | 27 | TRX-XC |
| 16 | TRX-BD | 22 | TRX-CH | 28 | TRX-B2 |
| 17 | TRX-SD | 23 | TRX-OH | 64..72 | PI-BD, SD, MT, ML, MA, RS, RC, CC, HH |

On the Monomachine, `mdref` assigns the machine to track 1 with
`F0 00 20 3C 03 00 5B <track> <id> 01 F7`, which also gives it the machine's
initial pages, sets the SYN page over CC 48 to 55 (other pages by `ccN=V`),
and plays a note on the track's channel; `note=` and `gate=` set it. Machine IDs are in
`tools/catalog.py`.

`mdref` also writes the LCD on request (`lcd <name>` in its job list), and
`reference/lcd_png.py` turns that into a PNG: the way to check which machine
and parameter page a job really got.

## Licence

`mdref.cpp` links the GPL-3 emulator and is GPL-3 like it. The OP-XY machines
take no code or data from the emulator or the ROM: the comparison only
measures sound.
