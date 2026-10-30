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

### The kits' sections (MD KIT, MD E12)

MD KIT and MD E12 draw one thing, the whole screen: the sounding part of the
instrument in section, as on an engraved patent plate, centred and widened
under the header. They draw no instrument from the design, no plot or panel,
and nothing of the rig: their art.h holds no parts, and `md_rig`'s hit motion
and the clap's lit hands have nothing to move. Each control moves one physical part, the same way
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

Every machine is its own drawing, so it can be told apart without a caption,
and each family shares a trait so the kit reads too:

| | TRX | EFM | P-I |
| --- | --- | --- | --- |
| Shell hardware | tube lugs | exciter coil and magnet (an animated spring on BD, XT) | rope tensioning |
| Striker | round felt ball, round stick tip | square head, flat nylon tip | big felt head, mallet stick |
| Cymbals | wing nut | exciter coil on the rod (HH's spring) | felt washers |

Machines differ by their shell's depth (kick 148, floor tom 136, rack tom 122,
snare 112, rimshot 104), a rack tom's mount, a floor tom's legs and slack head
(TRX-XC), a kick's pillow (TRX-B2), a ride's flat bow and big bell, a crash's
steep bow and small bell, a china's upturned edge (EFM-CY), an egg shaker
(PI-MA) beside a maraca (TRX-MA). Each E12 tape carries its own recording
(`fig_e12::takes`: a kick's wide bands, a hat's fine stripes, a clap's bursts,
a triangle's even ones) and the machine's number, plus one, as notches filed in
the block's end.

How far a control moves its part follows how much it changes the sound:
`tools/art/travel.py` plays each member with each control swept (at its
defaults and with every control at 64), measures the band-energy change and
writes the tables (MD KIT's and MD E12's) in `voices/mdkit_view.hpp` (the square root of its share of 24 dB,
at least half of full travel). Run it again after a voice changes.

The sections are drawn on the four paths (`voices/mdkit_view.hpp`, namespaces
`fig` and `fig_e12`), laid out about x 362 and drawn shifted to the centre
(`fig::shift`); every path keeps within 128 px of its origin, the most a path
at scale 2 may reach. Their fixed parts (rings, a rod, a hand, clamps, a
loudspeaker, wavefronts, beads; E12's block, slots, blade and patches:
`tools/art/fig.mjs`) take the common nodes' places, which these kits never use
otherwise; `build.mjs` drops every design part for the kits in `FIG_KITS`
(MD KIT and MD E12, each with its parts in `FIGS`). MD KIT is 71 nodes and about 16 KB of artwork (its hardware parts
follow the common nodes, and each view hides them first); MD E12 is 39 nodes
and about 11 KB. 
MD E12 draws its sample as a strip of tape on a splicing block, seen from
above (namespace `fig_e12`), its recording developed into stripes across it,
each machine its own recording:

| Control | What it moves |
| --- | --- |
| PTCH | the stripes closer together (the tape played faster) |
| DEC | the stripes fading along the tape |
| HP | the stripes' lower halves thinned |
| STRT | the cut-off start lying above the block; the stripes begin part way into the sound |
| RTRG, RTIM | splicing-tape patches where the start is spliced in again, RTIM apart |
| BEND | the first stripes crowded or spread, drifting back |
| RING, RATL, BELL, REAL, SLEW, BC | a second sample's tape spliced in on the diagonal, nearer the start as it rises |
| STOP | the end cut shorter the sooner it stops |
| SNAP, SPLN | the start saturated, its stripes clipped flat, for SPLN's length |
| HPQ | the tape's edge creased |

A hit runs the tape left through the block. Its travel table needs the E12
capture on this Mac (`tools/art/travel.py` refuses to write it from the silent
stand-in); only the measured travel goes in the table, never sample data.

### Keeping the kits' lines whole, and their controls alive

Lessons from the device (build 039), for any view on a reshaping path:

- **A line is a real thin closed shape.** `fig::draw(..., ribbon)` offsets both
  edges a quarter of a local pixel apart. A line traced out and back over itself
  (`polyline`), most of all one that turns sharply, broke up on the device even
  when every host check passed.
- **Never draw several separate strokes on one path.** TRX-BD's NOIS spray broke
  up that way: the strokes and the joins between them doubled back at every tip.
  Use fixed nodes (`spark0`..), which cannot change shape.
- **An outline must not cross itself.** A beater's ball started from the far side
  of the ball: the crossing cancelled its area at some settings ("has no area",
  TRX-B2 at RAMP 80..82), and drew a stray line through the ball at all of them.
- **Count the points a shape can need at its extreme.** A zigzag that runs out of
  points ends in a straight line. Fit the spacing to the points, not the reverse.
- **Keep every point within 128 px of its path's origin** (64 local px at scale 2),
  and everything below the header: art draws over it, so a hanging rod crossed the
  controls. `check_kit_frames` checks every frame with the SDK's
  `opxy::draw::valid`.
- **Sweep all 128 values** of every control, struck, not a few: `check_kit_frames`.

So that a control is never timid or dead:

- **Travel is proportional but never small:** the square root of the sound's change,
  at least half of the full travel (`travel.py`).
- **An amount that starts from nothing** (a spray, teeth, clamps, a second bell) grows
  from 0 (`Ctl::a`); it must not jump in at the middle of its range.
- **A count needs a continuous part too:** hammer marks deepen, the newest ray grows
  out, splices draw closer, or the control is a two-state switch between its counts.
- **A shade fades** through the palette's greys (`fig::grey`), never dark to white.
- **Motion eases.** A hit's `pose` swings up over 33 ms and eases back; `arc` runs out
  and comes home. Nothing jumps at the hit's start or end.
- **Measure it:** `tools/check_kit_frames.cpp` walks each control in steps of 8 and
  fails one with more than a fifth of its steps changing nothing in the frame.

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
