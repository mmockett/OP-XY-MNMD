"""Every Machinedrum and Monomachine synthesis machine, as the emulated
instruments themselves show them (OS 1.63 and OS 1.32b, read back from the
LCD after assigning each ID: reference/README.md). This is the source of truth
for IDs and for which of the eight slots holds which control; None is an empty
slot. A slot's index is also its controller: Machinedrum track 1 uses CC 16+i,
the Monomachine CC 48+i.

How the machines are packaged for the OP-XY (ENGINES below):

- The Machinedrum is four drum kits, one per family (TRX, EFM, P-I, E12), on
  the SDK's drum kind, as the OP-XY's Drumline: each key holds its own machine
  (Shift and the first encoder) and that machine's eight controls, saved with
  the project and starting from KITS.
- Each Monomachine machine is an engine of its own, played chromatically, with
  its eight controls on the encoders and their alternates.

18 engines in all, inside the 27 free engine numbers (36..62).
"""

_ = None

# name: (instrument, id, eight slots)
MACHINES = {
    # Machinedrum: GND
    "GND-SN":  ("md", 1,  ["PTCH", "DEC", "RAMP", "RDEC", _, _, _, _]),
    "GND-NS":  ("md", 2,  ["DEC", _, _, _, _, _, _, _]),
    "GND-IM":  ("md", 3,  ["UP", "UVAL", "DOWN", "DVAL", _, _, _, _]),
    # Machinedrum: TRX
    "TRX-BD":  ("md", 16, ["PTCH", "DEC", "RAMP", "RDEC", "STRT", "NOIS", "HARM", "CLIP"]),
    "TRX-SD":  ("md", 17, ["PTCH", "DEC", "BUMP", "BENV", "SNAP", "TONE", "TUNE", "CLIP"]),
    "TRX-XT":  ("md", 18, ["PTCH", "DEC", "RAMP", "RDEC", "DAMP", "DIST", "DTYP", _]),
    "TRX-CP":  ("md", 19, ["CLPY", "TONE", "HARD", "RICH", "RATE", "ROOM", "RSIZ", "RTUN"]),
    "TRX-RS":  ("md", 20, ["PTCH", "DEC", "DIST", _, _, _, _, _]),
    "TRX-CB":  ("md", 21, ["PTCH", "DEC", "ENH", "DAMP", "TONE", "BUMP", _, _]),
    "TRX-CH":  ("md", 22, ["GAP", "DEC", "HPF", "LPF", "MTAL", _, _, _]),
    "TRX-OH":  ("md", 23, ["GAP", "DEC", "HPF", "LPF", "MTAL", _, _, _]),
    "TRX-CY":  ("md", 24, ["RICH", "DEC", "TOP", "TTUN", "SIZE", "PEAK", _, _]),
    "TRX-MA":  ("md", 25, ["ATT", "SUS", "REV", "DAMP", "RATL", "RTYP", "TONE", "HARD"]),
    "TRX-CL":  ("md", 26, ["PTCH", "DEC", "DUAL", "ENH", "TUNE", "CLIC", _, _]),
    "TRX-XC":  ("md", 27, ["PTCH", "DEC", "RAMP", "RDEC", "DAMP", "DIST", "DTYP", _]),
    "TRX-B2":  ("md", 28, ["PTCH", "DEC", "RAMP", "HOLD", "TICK", "NOIS", "DIRT", "DIST"]),
    # Machinedrum: EFM
    "EFM-BD":  ("md", 32, ["PTCH", "DEC", "RAMP", "RDEC", "MOD", "MFRQ", "MDEC", "MFB"]),
    "EFM-SD":  ("md", 33, ["PTCH", "DEC", "NOIS", "NDEC", "MOD", "MFRQ", "MDEC", "HPF"]),
    "EFM-XT":  ("md", 34, ["PTCH", "DEC", "RAMP", "RDEC", "MOD", "MFRQ", "MDEC", "CLIC"]),
    "EFM-CP":  ("md", 35, ["PTCH", "DEC", "CLPS", "CDEC", "MOD", "MFRQ", "MDEC", "HPF"]),
    "EFM-RS":  ("md", 36, ["PTCH", "DEC", "MOD", "HPF", "SNAR", "SPTC", "SDEC", "SMOD"]),
    "EFM-CB":  ("md", 37, ["PTCH", "DEC", "SNAP", "FB", "MOD", "MFRQ", "MDEC", _]),
    "EFM-HH":  ("md", 38, ["PTCH", "DEC", "TREM", "TFRQ", "MOD", "MFRQ", "MDEC", "FB"]),
    "EFM-CY":  ("md", 39, ["PTCH", "DEC", "FB", "HPF", "MOD", "MFRQ", "MDEC", _]),
    # Machinedrum: E12 (ROM samples)
    "E12-BD":  ("md", 48, ["PTCH", "DEC", "SNAP", "SPLN", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-SD":  ("md", 49, ["PTCH", "DEC", "HP", "RING", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-HT":  ("md", 50, ["PTCH", "DEC", "HP", "HPQ", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-LT":  ("md", 51, ["PTCH", "DEC", "HP", "HPQ", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-CP":  ("md", 52, ["PTCH", "DEC", "HP", "HPQ", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-RS":  ("md", 53, ["PTCH", "DEC", "HP", "RATL", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-CB":  ("md", 54, ["PTCH", "DEC", "HP", "HPQ", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-CH":  ("md", 55, ["PTCH", "DEC", "HP", "HPQ", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-OH":  ("md", 56, ["PTCH", "DEC", "HP", "STOP", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-RC":  ("md", 57, ["PTCH", "DEC", "HP", "BELL", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-CC":  ("md", 58, ["PTCH", "DEC", "HP", "HPQ", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-BR":  ("md", 59, ["PTCH", "DEC", "HP", "REAL", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-TA":  ("md", 60, ["PTCH", "DEC", "HP", "HPQ", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-TR":  ("md", 61, ["PTCH", "DEC", "HP", "HPQ", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-SH":  ("md", 62, ["PTCH", "DEC", "HP", "SLEW", "STRT", "RTRG", "RTIM", "BEND"]),
    "E12-BC":  ("md", 63, ["PTCH", "DEC", "HP", "BC", "STRT", "RTRG", "RTIM", "BEND"]),
    # Machinedrum: P-I (physical models)
    "PI-BD":   ("md", 64, ["PTCH", "DEC", "HARD", "HAMR", "TENS", "DAMP", _, _]),
    "PI-SD":   ("md", 65, ["PTCH", "DEC", "HARD", "RING", "TENS", "RVOL", "RDEC", _]),
    "PI-MT":   ("md", 66, ["PTCH", "DEC", "HARD", "HAMR", "TUNE", "DAMP", "SIZE", "POS"]),
    "PI-ML":   ("md", 67, ["PTCH", "DEC", "HARD", "TENS", _, _, _, _]),
    "PI-MA":   ("md", 68, ["GRNS", "DEC", "GLEN", _, "SIZE", "HARD", _, _]),
    "PI-RS":   ("md", 69, ["PTCH", "DEC", "HARD", "RING", "RVOL", "RDEC", _, _]),
    "PI-RC":   ("md", 70, ["PTCH", "DEC", "HARD", "RING", "AG", "AU", "BR", "GRAB"]),
    "PI-CC":   ("md", 71, ["PTCH", "DEC", "HARD", "RING", "AG", "AU", "BR", "GRAB"]),
    "PI-HH":   ("md", 72, ["PTCH", "DEC", "CLSN", "RING", "AG", "AU", "BR", "CLOS"]),
    # Monomachine
    "GND-SIN":     ("mm", 1,  [_, _, _, _, _, _, _, "TUNE"]),
    "GND-NOIS":    ("mm", 2,  ["ST", "RED", "STON", _, _, _, _, "TUNE"]),
    "SID-6581":    ("mm", 3,  ["PW", "PWAD", "PWRS", "WAVE", "MOD", "MSRC", "MFRQ", "TUNE"]),
    "SWAVE-SAW":   ("mm", 4,  ["UNIL", "UNIW", "UNIX", _, "SUBX", "SUB1", "SUB2", "TUNE"]),
    "SWAVE-PULSE": ("mm", 5,  ["UNIL", "UNIW", "SUB1", "SUB2", "PW", "PWAD", "PWRS", "TUNE"]),
    "DPRO-WAVE":   ("mm", 6,  ["WAVE", "WP", "WPM", "WPRS", "SYNC", "SFRQ", _, "TUNE"]),
    "DPRO-BBOX":   ("mm", 7,  ["PTCH", "STRT", _, _, "RTRG", "RTIM", _, _]),
    "FM+STAT":     ("mm", 8,  ["1FRQ", "1FIN", "1ENV", "1FB", "2FRQ", "2VOL", "TONE", "TUNE"]),
    "FM+PAR":      ("mm", 9,  ["1FRQ", "1ENV", "2FRQ", "2ENV", "3FRQ", "3ENV", "TONE", "TUNE"]),
    "FM+DYN":      ("mm", 10, ["1FRQ", "1FEN", "1VOL", "1VEN", "2FRQ", "2ENV", "2FB", "TUNE"]),
    "VO-6":        ("mm", 11, ["VOC1", "VOC2", "V-SW", "VOIC", "CONS", "CLEN", "CVOL", "TUNE"]),
    "SWAVE-ENS":   ("mm", 14, ["PCH2", "PCH3", "PCH4", "WAVE", "PW", "CHRL", "CHRW", "TUNE"]),
    "DPRO-DDRW":   ("mm", 32, ["WAV1", "MIX", "WAV2", "TIME", "BR1", "WID", "BR2", "TUNE"]),
    "DPRO-DENS":   ("mm", 33, ["PCH2", "PCH3", "PCH4", "WAVE", _, "CHRL", "CHRW", "TUNE"]),
}

# Not synthesis machines, so not engines here: Machinedrum INP-GA/GB/FA/FB/EA/EB
# (80-85, audio inputs), ROM/RAM (UW table), CTR and MID; Monomachine FX-THRU,
# REVERB, CHORUS, DYNAMIX, RINGMOD, PHASER, FLANGER (12, 13, 15..19), which could
# become OP-XY send effects.

# The OP-XY packaging.

# The kits: 24 keys from C, as the OP-XY's keyboard. The first 16 follow the
# Machinedrum's default kit (docs/design/HANDOFF.md): BD SD HT MT LT CP RS CB
# CH OH RC CC M1..M4; the last 8 are variations. Each key: (role, machine,
# {label: value} over the machine's defaults (tools/presets.py, else the
# design's)).
KITS = {
    "mdtrx": [
        ("BD", "TRX-BD", {}), ("SD", "TRX-SD", {}),
        ("HT", "TRX-XT", {"PTCH": 90, "DEC": 50}), ("MT", "TRX-XT", {"PTCH": 66}), ("LT", "TRX-XT", {"PTCH": 40, "DEC": 64}),
        ("CP", "TRX-CP", {}), ("RS", "TRX-RS", {}), ("CB", "TRX-CB", {}),
        ("CH", "TRX-CH", {}), ("OH", "TRX-OH", {}),
        ("RC", "TRX-CY", {"DEC": 80, "TOP": 72, "SIZE": 100, "PEAK": 84}),
        ("CC", "TRX-CY", {"RICH": 100, "DEC": 110, "TTUN": 90, "SIZE": 40}),
        ("M1", "TRX-MA", {}), ("M2", "TRX-CL", {}), ("M3", "TRX-XC", {"PTCH": 80, "DEC": 36, "DAMP": 70}),
        ("M4", "TRX-B2", {}),
        ("BD2", "TRX-BD", {"PTCH": 22, "DEC": 96, "RAMP": 30, "RDEC": 70, "NOIS": 0, "CLIP": 10}),
        ("SD2", "TRX-SD", {"PTCH": 90, "DEC": 20, "BUMP": 20, "BENV": 10, "SNAP": 100, "TONE": 20, "TUNE": 80}),
        ("LC", "TRX-XC", {"PTCH": 60}), ("HC", "TRX-XC", {"PTCH": 92}),
        ("CP2", "TRX-CP", {"CLPY": 110, "TONE": 50, "HARD": 50, "RICH": 60, "RATE": 80, "ROOM": 100, "RSIZ": 90, "RTUN": 50}),
        ("PH", "TRX-CH", {"DEC": 8, "HPF": 90, "LPF": 100, "MTAL": 30}),
        ("CB2", "TRX-CB", {"PTCH": 127, "ENH": 70, "TONE": 90}), ("B22", "TRX-B2", {"PTCH": 56, "DEC": 70, "TICK": 90, "DIRT": 80}),
    ],
    "mdefm": [
        ("BD", "EFM-BD", {"PTCH": 40, "DEC": 60, "RAMP": 56, "RDEC": 36, "MOD": 24, "MFRQ": 32, "MDEC": 40}),
        ("SD", "EFM-SD", {"PTCH": 60, "DEC": 40, "NOIS": 80, "NDEC": 56, "MOD": 20, "MFRQ": 48, "MDEC": 40, "HPF": 16}),
        ("HT", "EFM-XT", {"PTCH": 100, "DEC": 56, "RAMP": 30, "RDEC": 36, "MOD": 16, "MFRQ": 32, "MDEC": 40, "CLIC": 40}),
        ("MT", "EFM-XT", {"PTCH": 76, "DEC": 58, "RAMP": 30, "RDEC": 36, "MOD": 16, "MFRQ": 32, "MDEC": 40, "CLIC": 40}),
        ("LT", "EFM-XT", {"PTCH": 52, "DEC": 60, "RAMP": 30, "RDEC": 36, "MOD": 16, "MFRQ": 32, "MDEC": 40, "CLIC": 40}),
        ("CP", "EFM-CP", {"PTCH": 72, "DEC": 48, "CLPS": 32, "CDEC": 60, "MOD": 64, "MDEC": 64, "HPF": 32}),
        ("RS", "EFM-RS", {"PTCH": 72, "DEC": 30, "MOD": 40, "HPF": 24, "SNAR": 40, "SDEC": 30, "SMOD": 40}),
        ("CB", "EFM-CB", {"PTCH": 82, "DEC": 44, "SNAP": 40, "FB": 24, "MOD": 8}),
        ("CH", "EFM-HH", {"DEC": 24, "MOD": 48, "MFRQ": 96, "MDEC": 64}),
        ("OH", "EFM-HH", {"DEC": 80, "MOD": 48, "MFRQ": 96, "MDEC": 64}),
        ("RC", "EFM-CY", {"DEC": 90, "MOD": 40, "MFRQ": 80, "MDEC": 80, "HPF": 40}),
        ("CC", "EFM-CY", {"DEC": 105, "MOD": 64, "MFRQ": 100, "MDEC": 90, "HPF": 60}),
        ("M1", "GND-SN", {"PTCH": 56, "DEC": 56, "RAMP": 24, "RDEC": 24}), ("M2", "GND-NS", {"DEC": 48}),
        ("M3", "GND-IM", {}), ("M4", "EFM-BD", {"PTCH": 48, "DEC": 56, "MOD": 100, "MFB": 80, "MDEC": 48}),
        # The variations: a long low kick, a noisy snare, a bright tom, a long clap,
        # a rim with more snare, the cowbell a fifth up, a tremolo hat, a high blip.
        ("BD2", "EFM-BD", {"PTCH": 24, "DEC": 100, "RAMP": 48, "RDEC": 48, "MOD": 12}),
        ("SD2", "EFM-SD", {"PTCH": 70, "DEC": 30, "NOIS": 110, "NDEC": 64, "MOD": 40, "MFRQ": 64, "HPF": 30}),
        ("XT2", "EFM-XT", {"PTCH": 80, "DEC": 64, "RAMP": 40, "MOD": 110, "MFRQ": 48, "MDEC": 48}),
        ("CP2", "EFM-CP", {"PTCH": 72, "DEC": 70, "CLPS": 110, "CDEC": 56, "MOD": 64, "MDEC": 64, "HPF": 32}),
        ("RS2", "EFM-RS", {"PTCH": 72, "DEC": 30, "MOD": 40, "HPF": 24, "SNAR": 100, "SDEC": 48, "SMOD": 40}),
        ("CB2", "EFM-CB", {"PTCH": 112, "DEC": 44, "SNAP": 40, "FB": 24, "MOD": 8}),
        ("HH2", "EFM-HH", {"DEC": 70, "TREM": 90, "TFRQ": 80, "MOD": 48, "MFRQ": 96, "MDEC": 64}),
        ("SN2", "GND-SN", {"PTCH": 100, "DEC": 48, "RAMP": 16, "RDEC": 16}),
    ],
    "mdpi": [
        ("BD", "PI-BD", {"DEC": 36, "TENS": 56}), ("SD", "PI-SD", {"RVOL": 100, "RDEC": 96}),
        ("HT", "PI-MT", {"PTCH": 92, "SIZE": 44, "DEC": 40}), ("MT", "PI-MT", {"PTCH": 64, "DEC": 40}),
        ("LT", "PI-MT", {"PTCH": 36, "SIZE": 88, "DEC": 44}),
        ("CP", "GND-NS", {"DEC": 30}), ("RS", "PI-RS", {}), ("CB", "PI-ML", {"PTCH": 76, "TENS": 24, "DEC": 36}),
        ("CH", "PI-HH", {"CLOS": 127}), ("OH", "PI-HH", {"CLOS": 127, "DEC": 92}),
        ("RC", "PI-RC", {"DEC": 100, "HARD": 88, "BR": 44}), ("CC", "PI-CC", {"DEC": 104, "HARD": 80}),
        ("M1", "PI-MA", {}), ("M2", "GND-SN", {}), ("M3", "GND-IM", {}), ("M4", "PI-ML", {"PTCH": 100}),
        ("BD2", "PI-BD", {"PTCH": 24, "DEC": 100, "TENS": 90}), ("SD2", "PI-SD", {"RING": 110, "RVOL": 64, "RDEC": 64}),
        ("XT2", "PI-MT", {"SIZE": 110, "PTCH": 50}), ("RS2", "PI-RS", {"RING": 100}),
        ("ML2", "PI-ML", {"PTCH": 40}), ("MA2", "PI-MA", {"GLEN": 100}),
        ("RC2", "PI-RC", {"GRAB": 56, "DEC": 110}), ("CC2", "PI-CC", {"GRAB": 40, "DEC": 120, "HARD": 100}),
    ],
    "mde12": [
        ("BD", "E12-BD", {}), ("SD", "E12-SD", {}),
        ("HT", "E12-HT", {}), ("MT", "E12-HT", {"PTCH": 51}), ("LT", "E12-LT", {}),
        ("CP", "E12-CP", {}), ("RS", "E12-RS", {}), ("CB", "E12-CB", {}),
        ("CH", "E12-CH", {"DEC": 80}), ("OH", "E12-OH", {}), ("RC", "E12-RC", {}), ("CC", "E12-CC", {}),
        ("M1", "E12-BR", {}), ("M2", "E12-TA", {}), ("M3", "E12-SH", {}), ("M4", "E12-BC", {}),
        # The variations: a driven kick, the ringing snare, a deep tom, a flam clap, the
        # bell of the ride, a thin crash, the other brush.
        ("BD2", "E12-BD", {"PTCH": 51, "SNAP": 50, "SPLN": 60}), ("SD2", "E12-SD", {"RING": 127}),
        ("LT2", "E12-LT", {"PTCH": 51, "HPQ": 40}), ("CP2", "E12-CP", {"RTRG": 3, "RTIM": 36}),
        ("TR", "E12-TR", {}), ("RC2", "E12-RC", {"BELL": 127}), ("CC2", "E12-CC", {"PTCH": 69, "HP": 40}),
        ("BR2", "E12-BR", {"PTCH": 53, "REAL": 127}),
    ],
}

# The keys a kit preset puts in one choke group, so the hats cut each other off;
# the others choke nothing. Shift and the second encoder change a key's group.
CHOKED = ("CH", "OH", "RC")

ENGINES = {
    **{kit: sorted({key[1] for key in keys}, key=list(MACHINES).index) for kit, keys in KITS.items()},
    # The Monomachine: a machine per engine.
    "mmsaw": ["SWAVE-SAW"], "mmpulse": ["SWAVE-PULSE"], "mmens": ["SWAVE-ENS"], "mmsid": ["SID-6581"],
    "mmwave": ["DPRO-WAVE"], "mmbbox": ["DPRO-BBOX"], "mmddrw": ["DPRO-DDRW"], "mmdens": ["DPRO-DENS"],
    "mmstat": ["FM+STAT"], "mmpar": ["FM+PAR"], "mmdyn": ["FM+DYN"], "mmvo6": ["VO-6"],
    "mmsin": ["GND-SIN"], "mmnois": ["GND-NOIS"],
}
# Engines that play samples captured from the owner's own ROMs (tools/capture).
SAMPLED = {"mde12", "mmbbox"}


def controls(name):
    """A machine's controls in slot order, without the empty slots."""
    return [c for c in MACHINES[name][2] if c]


assert len(ENGINES) <= 27, len(ENGINES)
assert all(len(keys) == 24 for keys in KITS.values())
assert {m for ms in ENGINES.values() for m in ms} == set(MACHINES), "every machine in an engine"
for _kit, _keys in KITS.items():
    assert set(CHOKED) <= {key[0] for key in _keys}, _kit
    for _role, _name, _over in _keys:
        assert set(_over) <= set(MACHINES[_name][2]), (_kit, _role, _over)


if __name__ == "__main__":
    # tools/catalog.json, for the JS tools (tools/art).
    import json
    import pathlib
    out = pathlib.Path(__file__).with_name("catalog.json")
    out.write_text(json.dumps({"machines": {n: {"instrument": i, "id": k, "slots": s} for n, (i, k, s) in MACHINES.items()},
                               "engines": ENGINES, "kits": KITS}, indent=1) + "\n")
    print(f"wrote {out}")
