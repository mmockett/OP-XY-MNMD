# The screen

The screens follow the design handoff in [docs/design](design/HANDOFF.md)
(`Main.dc.html` is its source): greyscale line art in the OP-XY's language.
Each engine's screen is generated from that design, so the device draws what
the design draws, within the SDK's limits.

## Layout and limits

| Area | Stage coordinates | Screen |
| --- | --- | --- |
| Header, drum kits | | y 0 to 20: four name and value cells, drawn by the SDK from the definition |
| Header, Monomachine | | y 0 to 38: both pages, drawn by the engine with the SDK's `opxy/page_header.hpp` over the stock header |
| Stage | x 0 to 480, y 0 to 160 | y 30 to 190 on a drum kit, y 46 to 206 on a Monomachine engine (`md::view::stage_y`), leaving the same gap under each header; the design's info line below is left out |

The SDK's frame (`opxy::draw::Frame`) allows 4 mutable paths of exactly 64
points, at most 256 nodes in all, 16 palette entries and artwork of at most
64 KB. Artwork may use only `g`, `path`, `circle` and `text`; every path needs
a nonzero key; a node's scale is at most 2. The first 31 nodes are common
(`md_view.hpp`): the 4 paths, 3 rings, 2 handles, a cursor and a 3-digit
7-segment readout. The art starts at node 31.

## The pipeline

1. `tools/art/design.mjs` runs the design's own script in Node and renders any
   machine at any control values.
2. `tools/art/rig.mjs` renders each control at both ends and fits each drawn
   part's change as a move, scale and turn (a similarity fit). Parts that fit
   become **rigged parts**: drawn once, moved on the device. Parts whose shape
   changes are **free layers**, listed by the build as "the view draws ...":
   those are drawn from C++ on the 4 mutable paths. A hit's poses become strike
   motions or variant nodes.
   While a note plays, a screen animates what it shows at rest (its points,
   phase, scale or brightness) and never draws a second trace or overlay over
   it, so the rig leaves out a Monomachine machine's struck copies of a view's
   layers and the layers only a hit draws. A machine's layers that the design
   draws only past a control's middle (E12-BC's tom) are parts shown only there.
3. `tools/art/build.mjs` writes `panel.svg.fragment` (the nodes and palette)
   and `art.h` (each part's pivot, shade and effects). Labels are drawn in an
   original stroke font (`tools/art/font.mjs`), not text slots (the
   Monomachine engines' header takes all 16). Parts that
   move together merge, and identical parts are shared between machines.
4. On the device, `md::rig::draw` (`lib/md_rig.hpp`) places each part from
   the controls, and a view module draws the free layers.

## Machinedrum kits

The kits (`mdkit`, `mde12`: the SDK's drum kind) have a
machine per key and the encoders on that key, so each shows **the selected
key's machine** as the design draws one machine: the instrument large on the
left, the family's panel on the right, at the key's values, with a held
step's locks applied. The panels are the design's `pTrxVoice`, `pTrxHat`,
`pTrxCym`, `pTrxRoom` and `pGrains` (`voices/trx_view.hpp`), and `pEfm`,
`pPi`, `pE12`, `pNoise` and `pImpulse` (`voices/mdkit_view.hpp`). Each note
moves the instrument itself: a head flexes, a cowbell swings, a panel's wave
swells (the SDK does not say which key played).

`node tools/art/build.mjs --pads` draws every machine at full size without
labels, sparks or the panels' struck poses, which the views draw live, and
draws GND-NS's noise on the paths: a static label would show for every
machine, and the rest would not fit. TRX takes 176 nodes, EFM 203,
P-I 181 and E12 212. The header names the machine while Shift is
held.

### The kits' right side (MD KIT)

MD KIT draws the right side as fig. 2 of an engraved patent plate: the
sounding part of the instrument in section, beside the playing drawing on the
left, with no plot or panel. Each control moves one physical part, the same way
across the families:

| Kind of control | What it moves |
| --- | --- |
| Pitch | a head's standing waves; a cymbal's, bell's or bar's size; a fork's tines |
| Decay | how far the waves carry from the strike; how many rings leave an edge |
| Ramp, bump, tension | the dent the striker presses into the head |
| Striker (STRT, SNAP, HAMR, HARD) | how high the beater, stick or mallet is lifted, or its head's size |
| Tone, filters | snare wires pressed on or hanging clear, a cymbal's thickness, the teeth of its rim, a bell's wall |
| Metal, harmonics, FM | hammer marks, the overtone riding the head, an exciter's spring |
| Room | an echo chamber's depth, ceiling and reflections |
| Distortion | clamps closing on the head, shown only while it is up |

How far a control moves its part follows how much it changes the sound:
`tools/art/travel.py` plays each member with each control swept (at its
defaults and with every control at 64), measures the band-energy change and
writes the table in `voices/mdkit_view.hpp` (full travel at 24 dB, at least a
fifth of it). Run it again after a voice changes.

The sections are drawn on the four paths (`voices/mdkit_view.hpp`, namespace
`fig`). The kit's artwork must stay under the SDK's 64 KB and the instruments
take about 61 KB of it, so the right side's 27 fixed parts (rings, a rod, a
hand, clamps, a loudspeaker, wavefronts, beads: `tools/art/fig.mjs`) take the
common nodes' places, which MD KIT never uses otherwise; `build.mjs` drops the
design's panel parts for the kits in `FIG_KITS`. A hit animates in sixteen
steps, so the right side redraws a few times a hit rather than every frame.
MD E12 still draws the design's pE12 panel.

## Monomachine engines

One machine per engine, drawn full width as the design draws it (oscillator
rows for SuperWave, a stepped wave for SID, an iso wavetable for DPRO-WAVE,
operator diagrams for FM+, a vowel chart for VO-6). The rigged parts come
from the build; the view module `voices/<family>_view.hpp` defines
`md::<slug>::view(frame, slots, moment)` and draws the free layers on the 4
paths. `voices/trx_view.hpp` is the worked example: contours, waves and
partials from the controls, at stage scale, with the readout for numbers in
labels such as "gap 40".

## Checking a screen

- `python3 tools/serve_bench.py`, then http://127.0.0.1:8930/?track=N. With
  `&flash=1` the screen shows a hit without audio, for screenshots.
- Headless: `"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
  --headless=new --window-size=1300,900 --screenshot=out.png
  "http://127.0.0.1:8930/?track=0"`.
- The SDK's `test` runs the drawing smoke checks; `tools/check_frame.cpp`
  validates a frame on the host, including what the device does beyond the
  SDK's rules: neighbouring points within an eighth of a pixel merge, and a
  closing edge under a quarter of a pixel draws as a sliver.

## What the device's SVG parser takes

The OP-XY's own SVG loader crashed on build 001 (`svg_loader.cpp`, at
launch) although every SDK check passed. That build's paths held up to 81
subpaths and 288 points each; every machine known to run on the device draws
**one subpath per path, at most 64 points**. So the build now:

- writes every art node as one subpath of at most 64 points, splitting a part
  into several nodes that share its pivot and movement;
- writes fixed labels as static `<text>` in the device font
  (`UniversNextPro-Light`, screen coordinates, baseline in the transform), as
  the SDK's own header does; only labels that move use the stroke font, and
  each of their strokes costs a node;
- draws outlines as one line, retracing edges;
- draws a design circle (an FM operator, a drum's beater) as a `<circle>`
  node, round at any size, pivoted on its own centre unless its part scales
  or turns; and keeps more points on the curved runs of other outlines (an
  ellipse, a drum's hoop), which Douglas-Peucker would otherwise cut to a
  polygon: 0.12 px from the design on a curve, 0.6 px on straight runs;
- draws a filled design part as its outline's stroke when the design draws a
  view's layer over it: a view's paths come before every art node, and a
  node's paint changes a fill's colour but never removes it.

Views keep to two more: a mutable path is never filled (the SDK authors it
without a fill, and paint only recolours one), so a view draws strokes only;
and a node's stroke scales with it, so a ring at 1/8 scale is a 0.2 px hairline.

The rig leaves some of the design out (`LEAVE_OUT` in `tools/art/rig.mjs`):
labels that repeat a header name or say what the drawing already shows, and
layers that would mislead on the device.

Keep to this in any view or art change; the host checks do not catch it.

## Known SDK issues

- **Text slots.** Before etcetc000/open-xy#125, `tooling/native.py` tested
  `{'x', 'y'} & node.keys()` on an ElementTree element, which failed for any
  text slot. The fix is on `staging` and the fork's `two-page-synths`, and the
  Monomachine engines' two-page header uses 16.
