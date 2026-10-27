# Plan: the Machinedrum and the Monomachine on the OP-XY

## 1. What the OP-XY allows

These limits come from the Open-XY SDK (`../open-xy/docs/api.md`). They shape
every decision below.

| Limit | Value | Consequence |
| --- | --- | --- |
| CPU | One Blackfin core, no floating point, no divide, no 64-bit helpers | Rewrite each machine in fixed point; the emulator cannot run on the device |
| Budget | Aim for 15,000 instructions per voice per 64-sample block; over 24,000 risks drop-outs | Per-block coefficients, linear glides between block ends, cheap filters |
| Controls | 4 encoders plus 4 alternates: 8 parameters, saved per track (per key on the drum kind) | One synthesis page (2 rows of 4); a kit's encoders are the selected key's machine (see 2) |
| Keys | The drum kind gives each of 24 keys its own machine and controls; the screen is told the selected key's | A kit's screen shows the selected key's machine |
| Engine numbers | 34 to 62, shared with every other machine in a build: 27 free (36 to 62) | 4 MD kits and 14 Monomachine engines: 18 |
| Memory | 2,040 KiB for the whole build | Ample for the synthesis; the E12 and BBOX samples are the large part |
| After the machine | The OP-XY's own amp envelope, filter, LFOs and sends follow it | The Machinedrum's track filter, EQ and amp settings map onto stock OP-XY pages |
| Identity | A machine's ID and name never change once published | Settle IDs before publishing anywhere (see 6) |

## 2. How the machines map

**The Machinedrum is four kits**, one per family, on the SDK's drum kind as
stock Drumline: MD TRX, MD EFM, MD P-I and MD E12. Each of 24 keys holds its
own machine (Shift and the first encoder) and that machine's eight controls,
saved with the project: the first 16 start as the Machinedrum's default kit
(BD SD HT MT LT CP RS CB CH OH RC CC M1..M4), the last 8 as variations. The
GND machines fill gaps in the EFM and P-I kits. [VOICES.md](VOICES.md) has the
details.

**Each Monomachine machine is its own engine**, played chromatically: the top
row of its SYN page on the encoders, the bottom row on their alternates, empty
slots kept in place. Its amp, filter and LFO pages map onto the OP-XY's own.

**On every engine:**

- **Track pages → OP-XY stock.** The stock amp envelope with full sustain and
  a long release (`presetDefaults` in `machine.json`) lets each machine's own
  decay shape the sound. The stock filter stands in for the MD's track filter
  and the stock LFOs for the LFO pages.
- **SRR and DIST**, the MD's per-track sample-rate reduction and distortion,
  have no stock equivalent. Plan: an `mdfx` send effect with SRR, DIST and EQ.
- **Pitch.** Machines are voiced at middle C. A kit's machine is tuned by its
  own PTCH; a Monomachine engine follows the played note.

## 3. All the machines

`tools/catalog.py` lists every machine with its ID and slots, read back from
the emulated LCD, and which engine holds it.

| Engine | Machines | Approach |
| --- | --- | --- |
| MD TRX | BD, B2, SD, XT, XC, CP, RS, CB, CH, OH, CY, MA, CL | Subtractive; BD, SD, CP, CH and OH are fitted |
| MD EFM | BD, SD, XT, CP, RS, CB, HH, CY, with GND SN, NS, IM | Two-operator FM with pitch and modulation envelopes |
| MD P-I | BD, SD, MT, ML, MA, RS, RC, CC, HH, with GND | Modal resonators and waveguides; the CPU risk |
| MD E12 | BD, SD, HT, LT, CP, RS, CB, CH, OH, RC, CC, BR, TA, TR, SH, BC | Samples captured from the owner's own ROM through the emulator, kept private |
| Monomachine | SWAVE-SAW, PULSE, ENS; SID-6581; DPRO-WAVE, BBOX, DDRW, DENS; FM+STAT, PAR, DYN; VO-6; GND-SIN, NOIS | One engine each; BBOX plays captured samples, DPRO-WAVE and DENS the captured DigiPRO waves |
| Not engines | MD INP, MID, CTR, ROM, RAM; MNM FX-THRU, REVERB, CHORUS, DYNAMIX, RINGMOD, PHASER, FLANGER | Inputs and routing are stock; the MNM effects could become OP-XY send effects later |

## 4. Match the Machinedrum

The machines are written from scratch, then fitted to the real thing with the
[reference loop](../reference/README.md). It renders the Machinedrum from your
own ROM through the emulator, sweeps each parameter, and measures both sides:
decay times, pitch, brightness and noise. Fit each mapping curve to those
measurements, then lock it in with a sound test, so later changes cannot
drift. The ROM and the emulator are only references: nothing of either goes
into the machines.

## 5. On the device

[DEVICE.md](DEVICE.md) has the steps: copy `os.ldr` off the OP-XY once, build
with `tools/build_firmware.py` (it keeps each build's `layout.json` for the
next), install from te-boot, then profile CPU on the device
(`../open-xy/docs/profiling.md`): the host estimates are not device numbers.
Every step on the device needs the owner's say-so.

## 6. Before publishing

- **Names.** "Machinedrum", "Monomachine", "Elektron" and machine family names
  such as TRX are Elektron's. For personal builds this is fine; for the Open-XY
  catalog, choose original names and IDs first, since they can never change.
- **Licence.** Not chosen yet.
- **CPU readout.** The catalog needs measured device numbers for the exact commit.

## 7. Next steps, in order

1. Done: the reference loop for both instruments; TRX BD, SD, CP, CH and OH
   fitted; the bench.
2. One session per family (MD TRX, MD EFM, MD P-I, MD E12 with GND; MNM
   SWAVE with GND, SID, DPRO, FM+, VO-6), each in its own branch: modules,
   fitters and checks, merged back here.
3. Capture the E12 samples and DigiPRO waves privately (`tools/capture`).
4. First device build; profile every engine.
5. `mdfx` (SRR, DIST, EQ); the MNM effects as send effects.
6. Ask upstream for the text-slot fix ([SCREEN.md](SCREEN.md)).
