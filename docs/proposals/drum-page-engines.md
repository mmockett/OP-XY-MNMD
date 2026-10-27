# Proposal: drum-page engines with per-key settings

A proposal for the Open-XY SDK: let a machine work like stock Drumline. Each
key plays its own sound with its own settings, saved with the project, and
the screen shows the key being played or edited. It is written from what the
OP-XY's own project files show. Nothing here is built yet, and every claim
about the stock firmware's behaviour beyond those files is marked as a test
to run on the device.

## Why

The MD kits here (`mdtrx` and the rest) play one machine per key, but a synth
engine has only its eight track parameters, so the encoders are kit-wide
macros, and the screen is never told which key played. Per-key settings on
a synth engine would need new state, a new load path and a voice-to-track
mapping (route B in our scoping). Stock Drumline already does what we want
on the device, through the per-key records that drum tracks save, so the
cheaper route is to let SDK machines use the same records.

## What a project file shows

A project (`.xy`) is a byte-level run-length stream over a serialised image;
[kmorrill/xy-format](https://github.com/kmorrill/xy-format) (MIT) worked out
the codec. Decoded, a drum-style track holds 24 records of 128 bytes, one per
key, notes 53 to 76. `tools/xy_keys.py` prints them. Four saves of one
project, each with one change, give this layout:

| Offset | Sample drum pad | Drumline key |
| --- | --- | --- |
| +0x00 | start, end, loop start, loop end: `0, -1, 0, -1` for a whole sample | four sound values, 0..1 in Q31 (the first is pitch) |
| +0x10 | the fade word, which carries the SDK's 16-bit pad settings | 0 |
| +0x14 | root note (60) | drum model, a small number (17 to 74); three keys with the same model share most values |
| +0x16 | key | key |
| +0x17 | 1 | class: 4 (the two kicks), 2 (a trio of related keys), 1 (the rest) |
| +0x19 | fine tune, flags | variant bytes |
| +0x1c | the sample's path | zeros |

The edits that placed these:

- **A key's pitch, outside step mode** (the hi-hat, note 73, to 11): only that
  key's first word changed, from `0x4a3d0000` (0.58) to `0x0f0a3d6b`
  (0.1175). The low bits survived the save, so the words are stored as
  integers, not through binary32 as the fade word is.
- **Two step locks**: only the track's step-lock table changed, a table the
  sample drum track has too. Step locks are per track and generic.

So Drumline keeps **four 32-bit sound values per key in the words a sample pad
uses for its markers**, and stock saves them with the project for keys that
have no sample.

## What the SDK has today

- Sample machines on page 7: notes map to 24 pads, and stock draws the drum
  page.
- `padSettings`: 16 machine bits per pad, in the fade word.
- `drumControls`: three machine knobs on the drum page's Shift row, plus a
  panel (pitch, direction, level, two captions).
- The outer markers reach the machine as `region.begin` and `region.end`.
- No custom screen for a sample machine, and nothing that tells any screen
  which key played.

## The proposal

### 1. Pad words for keys without a sample

A sample machine declares `"padWords": 4`. On a pad with no sample, the SDK
gives the machine that pad's four marker words as its own storage, as
Drumline uses them:

```cpp
struct PadState { u32 words[4]; PadSettings settings; };
PadState pad_defaults(u32 key);                  // a key that has never been set
void drum_control(PadState&, const DrumControl&);
// SampleBlock gains: const PadState* pad;        // the playing key's state
```

128 bits a key, plus the 16 the SDK has, is enough for a whole Machinedrum
page per key: a machine number (6 bits) and eight controls of 7 bits is 62.

### 2. Every machine control on the selected key

The drum page edits the selected pad. Let a drum-page machine declare up to
eight knobs, on the main and Shift rows, editing its `PadState`, instead of
three on the Shift row. Stock's pitch, start/end and volume would give way on
a pad without a sample.

### 3. A screen of the machine's own

A drum-page machine supplies a screen, as synths do (`display.cpp`, artwork),
in place of stock's waveform. `opxy::draw::Input` gains the selected pad and
the last note played. The second also serves synth engines (our kits), and
the SDK already reads stock's held-note map each frame
(`note_activity.cpp`), so it is the smallest piece.

### 4. Keys that need no sample

A sample machine declares `"samples": "none"`: every key starts a voice
whether or not a file is assigned, and the sample browser is not needed to
make a key sound.

## To test on the device first

1. **Voices without samples**: does a key with no sample start a voice for an
   SDK sample machine? A probe machine with today's SDK (page 7,
   `padSettings`, `drumControls`, synthesised output) answers this, and
   whether its 16-bit settings survive a save on such a key.
2. **Marker words kept**: SDK sample machines borrow the multisampler payload.
   Does stock save and reload the marker words of a sample-less pad on that
   path exactly, as it does for Drumline, or does it clamp them to the
   sample's length on load?
3. **Presets**: are a pad's words saved in user presets (`.preset`, not yet
   decoded)?
4. **The drum page with no sample**: does stock's page still let a pad be
   selected and edited?

### Results, build 005

Tests 1 and 4 ran on a page-7 test machine that ignored its samples and
synthesised a drum on every key, its pad settings on three machine knobs.

- **Test 1 passes.** Every key with no sample sounds: stock starts a voice
  for an SDK sample machine without a sample.
- **Test 4 fails without a sample.** On a pad with no sample, neither the
  Shift knobs nor encoder 4 change the captions or the sound. With a sample
  assigned, Shift + encoders 2 and 3 work. The SDK bridge edits a pad only
  through `pad_region(track, key)`, which evidently returns no record for an
  empty pad, so the edit is dropped. Part 1 has to give such a pad a record
  the bridge can edit, and the save steps (3 and 4 above) wait on it.
- **The test machine in build 005 numbered its knobs wrongly.** `DrumControl.knob` is
  the encoder's index (1 and 2 on Shift, 3 on encoder 4), as Timestretch
  uses it, not a count from 0, so tone was unreachable and encoder 4 was
  ignored. With build 006 all three knobs edit a pad that has a sample, so
  the only gap is the empty pad.

## Effort and risk

| Part | Effort | Risk |
| --- | --- | --- |
| A test machine, today's SDK | Small: a day and one device test | Low: existing SDK paths |
| 1. Pad words | Small to medium | Medium until tests 2 and 4 pass; the project file is untouched beyond what stock writes |
| 2. Eight knobs | Medium: more of stock's drum page to map | Medium |
| 3. Own screen, selected and last key | Medium (the last-note part alone is small) | Low to medium: drawing only |
| 4. No-sample keys | Small to medium | Medium: voice allocation for sample machines |

None of it changes the audio contract or project files beyond fields stock
already saves. Each part is a separate change and can land on its own.

## For this repository

With parts 1 to 3, an MD kit becomes one drum-page engine: each key any MD
machine with its own eight controls, saved with the project, the encoders on
the selected key, and the screen showing that key's instrument from our
art. The voice modules carry over unchanged; only the engine wrapper and
the screen change.

## On PR #121's drum kind, build 012

Instead of parts 1 to 4, MD TRX (`mdtrx`) runs on the synthesis drum kind
of [PR #121](https://github.com/etcetc000/open-xy/pull/121) (SDK 1.3), with
the TRX machines on Drumline's per-key records. As of build 012, on the
device: each key plays its own machine and controls, edits survive saving,
a power cycle and reloading, and a loaded kit plays without clicks. Step
locks reach the sound of their step.

That needs four local fixes on top of the PR, each a branch of
`../open-xy` on the one before:

| Branch | Fix |
| --- | --- |
| `drum-synthesis-weak` | The runtime scan rejected weak symbols, which inline C++ functions and template statics emit, so no collection of ours linked |
| `drum-synthesis-note-index` | Note start tests for Drumline (`8d593d84`) before packing the played pad's index into the note; without the alias every key played pad 1's values |
| `drum-synthesis-project-load` | Project load resets a Drumline track's parameters to neutral offsets (`8d587560`); a loaded kit kept zero, so every value lost 64 and notes clicked |
| `drum-synthesis-step-lock` | The screen showed the key's stored values, not a held step's locks; screen and voice now decode the same values (works on the device, build 013) |
| `drum-synthesis-enum-detents` | An owned synth's enum stepped about 1% a detent; it now steps a choice a detent (build 014) |
| `drum-synthesis-eight-controls` | As on Drumline, Shift and the first encoder choose a key's machine; the eight encoders' slots are the machine's eight controls, stored with the machine in the four words and marked by Drumline type 7 (build 015) |

## Next steps

Superseded by PR #121's drum kind (above).
