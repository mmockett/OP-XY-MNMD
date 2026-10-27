"""Each machine's defaults and presets, 0..127 in its slot order (tools/catalog.py).
Machines not listed here start from the screen design's defaults (docs/design).
A family engine (MD Perc, MNM Wave) uses each member's defaults when MACH picks it."""

PRESETS = {
    "TRX-BD": {
        "defaults": [40, 52, 44, 36, 0, 12, 0, 0],
        "presets": {
            "Round": [40, 52, 44, 36, 0, 12, 0, 0],
            "Boom": [22, 96, 30, 70, 0, 0, 0, 10],
            "Punch": [52, 40, 70, 24, 40, 30, 16, 40],
            "Gabber": [46, 70, 90, 30, 64, 20, 50, 127],
        },
    },
    "TRX-SD": {
        "defaults": [48, 50, 40, 30, 50, 64, 40, 0],
        "presets": {
            "Tight": [48, 40, 40, 30, 50, 64, 40, 0],
            "Fat": [30, 70, 60, 50, 30, 50, 20, 30],
            "Rim": [90, 20, 20, 10, 100, 20, 80, 0],
            "Noise": [60, 80, 10, 10, 70, 120, 40, 60],
        },
    },
    "TRX-CP": {
        "defaults": [80, 64, 64, 40, 64, 40, 50, 64],
        "presets": {
            "Hands": [80, 64, 64, 40, 64, 40, 50, 64],
            "Big": [110, 50, 50, 60, 80, 100, 90, 50],
            "Snap": [20, 90, 110, 20, 30, 10, 20, 90],
            "Crowd": [127, 70, 40, 80, 120, 70, 70, 40],
        },
    },
    "TRX-CH": {
        "defaults": [0, 64, 0, 127, 64, 0, 0, 0],
        "presets": {
            "Closed": [0, 64, 0, 127, 64, 0, 0, 0],
            "Tick": [0, 8, 90, 100, 30, 0, 0, 0],
        },
    },
    # Slots PW PWAD PWRS WAVE / MOD MSRC MFRQ TUNE. PW 64 is a silent full-width
    # pulse on the original, so the default sits at a quarter width; the steps
    # (WAVE TRI SAW PULS MIX NOIS, MOD OFF RING SYNC R+S) keep the original's 0..127.
    "SID-6581": {
        "description": "The Monomachine's SID-6581: triangle, saw, pulse, mixed and noise, with a pulse-width sweep, ring and sync, fitted to the original.",
        "defaults": [32, 0, 0, 38, 0, 0, 64, 64],
        "presets": {
            "Saw": [32, 0, 0, 38, 0, 0, 64, 64],
            "Square": [0, 0, 0, 64, 0, 0, 64, 64],
            "Sweep": [16, 20, 127, 64, 0, 0, 64, 64],
            "Sync": [32, 0, 0, 38, 80, 0, 76, 64],
            "Ring": [32, 0, 0, 10, 40, 0, 92, 64],
            "Mixed": [32, 0, 0, 90, 0, 0, 64, 64],
            "Noise": [32, 0, 0, 115, 0, 0, 64, 64],
        },
    },
    "TRX-OH": {
        "defaults": [32, 64, 0, 127, 64, 0, 0, 0],
        "presets": {
            "Open": [32, 64, 0, 127, 64, 0, 0, 0],
            "Wash": [96, 96, 20, 80, 110, 0, 0, 0],
        },
    },
    "TRX-XT": {
        "defaults": [64, 56, 40, 40, 16, 0, 0, 0],
        "presets": {
            "Tom": [64, 56, 40, 40, 16, 0, 0, 0],
            "Long": [64, 90, 30, 70, 0, 0, 0, 0],
            "Syndrum": [64, 70, 127, 90, 0, 0, 0, 0],
            "Dirty": [64, 60, 50, 40, 20, 90, 40, 0],
        },
    },
    "TRX-XC": {
        "defaults": [64, 48, 30, 30, 30, 0, 0, 0],
        "presets": {
            "Conga": [64, 48, 30, 30, 30, 0, 0, 0],
            "Slap": [80, 30, 20, 20, 90, 0, 0, 0],
            "Low": [30, 64, 40, 40, 10, 0, 0, 0],
            "Wood": [100, 24, 10, 10, 127, 40, 64, 0],
        },
    },
    "TRX-RS": {
        "defaults": [64, 40, 0, 0, 0, 0, 0, 0],
        "presets": {
            "Rim": [64, 40, 0, 0, 0, 0, 0, 0],
            "Ring": [80, 90, 0, 0, 0, 0, 0, 0],
            "Snap": [40, 10, 40, 0, 0, 0, 0, 0],
            "Buzz": [64, 60, 110, 0, 0, 0, 0, 0],
        },
    },
    "TRX-CB": {
        "defaults": [64, 60, 40, 0, 64, 0, 0, 0],
        "presets": {
            "Bell": [64, 60, 40, 0, 64, 0, 0, 0],
            "Agogo": [100, 50, 80, 0, 100, 40, 0, 0],
            "Muted": [64, 40, 20, 100, 40, 0, 0, 0],
            "Clank": [64, 80, 127, 0, 127, 64, 0, 0],
        },
    },
    "TRX-CY": {
        "defaults": [64, 64, 64, 64, 64, 64, 0, 0],
        "presets": {
            "Cymbal": [64, 64, 64, 64, 64, 64, 0, 0],
            "Ride": [64, 80, 80, 64, 100, 90, 0, 0],
            "Crash": [100, 110, 64, 90, 40, 64, 0, 0],
            "Splash": [64, 30, 90, 100, 110, 80, 0, 0],
        },
    },
    "TRX-MA": {
        "defaults": [0, 40, 0, 0, 40, 0, 64, 64],
        "presets": {
            "Shake": [0, 40, 0, 0, 40, 0, 64, 64],
            "Long": [60, 90, 0, 20, 60, 0, 64, 64],
            "Back": [0, 40, 60, 0, 40, 0, 64, 64],
            "Sand": [0, 30, 0, 0, 100, 0, 90, 30],
        },
    },
    "TRX-CL": {
        "defaults": [64, 40, 0, 40, 0, 0, 0, 0],
        "presets": {
            "Clave": [64, 40, 0, 40, 0, 0, 0, 0],
            "Wood": [40, 30, 0, 90, 0, 40, 0, 0],
            "Double": [64, 40, 50, 40, 0, 0, 0, 0],
            "Bell": [90, 80, 0, 20, 100, 0, 0, 0],
        },
    },
    "TRX-B2": {
        "defaults": [64, 60, 48, 20, 40, 10, 0, 0],
        "presets": {
            "909": [64, 60, 48, 20, 40, 10, 0, 0],
            "Long": [56, 90, 40, 64, 30, 0, 0, 0],
            "Hard": [70, 55, 70, 30, 100, 40, 0, 32],
            "Crushed": [64, 64, 50, 20, 60, 20, 100, 0],
        },
    },
    # E12 (voices/e12.hpp): each sample as it is, with a long DEC (8 s) so the kit's
    # DEC macro shortens or opens it; the crossfades of BR, SH and BC halfway, as the design.
    **{name: {"defaults": [64, 110, 0, own, 0, 0, 64, 64]} for name, own in [
        ("E12-SD", 0), ("E12-HT", 0), ("E12-LT", 0), ("E12-CP", 0), ("E12-RS", 0), ("E12-CB", 0),
        ("E12-CH", 0), ("E12-OH", 127), ("E12-RC", 0), ("E12-CC", 0), ("E12-BR", 64), ("E12-TA", 0),
        ("E12-TR", 0), ("E12-SH", 64), ("E12-BC", 64)]},
    "E12-BD": {"defaults": [64, 110, 0, 40, 0, 0, 64, 64]},   # no SNAP; SPLN 40 once the macro adds it
    # VO-6: VOC1 VOC2 V-SW VOIC CONS CLEN CVOL TUNE. VOC1 is the first formant, VOC2
    # the second (reference/README.md); CONS steps every 6: - B D F G H J K L M N P R
    # RR S SJ T TJ TH V Z at 3, 9, 15 ... 124.
    "VO-6": {
        "description": "The Monomachine's VO-6 formant voice: vowels from two formants, twenty consonants before them, fitted to the original.",
        "defaults": [120, 58, 127, 0, 0, 30, 80, 64],
        "presets": {
            "Ah": [120, 58, 127, 0, 0, 30, 80, 64],
            "Ee": [32, 115, 127, 0, 0, 30, 80, 64],
            "Oh": [106, 37, 127, 0, 0, 30, 80, 64],
            "Ma": [120, 58, 127, 0, 57, 30, 80, 64],
            "Sing La": [98, 99, 127, 20, 51, 40, 80, 64],
            "Robot Rr": [64, 80, 127, 0, 82, 50, 100, 64],
            "Whisper": [42, 41, 127, 127, 33, 40, 90, 64],
            "Tick": [64, 64, 0, 0, 100, 0, 127, 64],
            "Hiss": [64, 64, 0, 0, 88, 90, 110, 64],
        },
    },
    # The Monomachine (reference/fit_swave.py, fit_mgnd.py). SWAVE-SAW:
    # UNIL UNIW UNIX - / SUBX SUB1 SUB2 TUNE.
    "SWAVE-SAW": {
        "description": "The Monomachine's SWAVE-SAW: a stack of detuned saws with sub-octaves, fitted to the original.",
        "defaults": [40, 30, 0, 0, 0, 30, 0, 64],
        "presets": {
            "Stack": [40, 30, 0, 0, 0, 30, 0, 64],
            "Super": [100, 60, 80, 0, 0, 40, 0, 64],
            "Wide": [127, 127, 127, 0, 0, 0, 0, 64],
            "Thin": [0, 0, 0, 0, 0, 0, 0, 64],
            "Sub": [30, 20, 0, 0, 90, 60, 40, 64],
        },
    },
    # UNIL UNIW SUB1 SUB2 / PW PWAD PWRS TUNE.
    "SWAVE-PULSE": {
        "description": "The Monomachine's SWAVE-PULSE: detuned pulses with a sweeping width and sub-octaves, fitted to the original.",
        "defaults": [30, 20, 30, 0, 64, 20, 0, 64],
        "presets": {
            "Sweep": [30, 20, 30, 0, 64, 20, 0, 64],
            "Square": [0, 0, 0, 0, 64, 0, 127, 64],
            "Reed": [0, 0, 0, 0, 16, 0, 127, 64],
            "Strings": [90, 70, 0, 0, 40, 12, 127, 64],
            "Bass": [20, 10, 100, 60, 48, 6, 127, 64],
        },
    },
    # PCH2 PCH3 PCH4 WAVE / PW CHRL CHRW TUNE. PCH: 0 is -12 semitones, 62 off,
    # 72 +3, 84 +4, 99 +7, 103 the just 3:2, 123 +12 (voices/swave.hpp).
    "SWAVE-ENS": {
        "description": "The Monomachine's SWAVE-ENS: a four-voice chord of saws or pulses with chorus, fitted to the original.",
        "defaults": [84, 99, 62, 0, 64, 40, 40, 64],
        "presets": {
            "Major": [84, 99, 62, 0, 64, 40, 40, 64],
            "Minor": [72, 99, 62, 0, 64, 40, 40, 64],
            "Fifths": [103, 123, 0, 0, 64, 64, 64, 64],
            "Organ": [123, 0, 103, 127, 32, 20, 30, 64],
            "Seventh": [84, 99, 116, 40, 48, 50, 40, 64],
        },
    },
    # - - - - / - - - TUNE.
    "GND-SIN": {
        "description": "The Monomachine's GND-SIN: a pure sine at the played key, fitted to the original.",
        "defaults": [0, 0, 0, 0, 0, 0, 0, 64],
        "presets": {"Sine": [0, 0, 0, 0, 0, 0, 0, 64]},
    },
    # ST RED STON - / - - - TUNE.
    "GND-NOIS": {
        "description": "The Monomachine's GND-NOIS: white noise turning red, fitted to the original.",
        "defaults": [0, 0, 0, 0, 0, 0, 0, 64],
        "presets": {
            "White": [0, 0, 0, 0, 0, 0, 0, 64],
            "Pink": [0, 48, 0, 0, 0, 0, 0, 64],
            "Red": [0, 127, 0, 0, 0, 0, 0, 64],
            "Loud": [127, 64, 0, 0, 0, 0, 0, 64],
        },
    },
    # The Monomachine's FM+ machines (voices/fmp.hpp, reference/README.md). FRQ
    # steps are the original's 0..127, placed mid-step by make_engine.py:
    # 8 = 1:32, 61 = 1:2, 82 = 1, 93 = 1.5, 104 = 2, 114 = 3, 120 = 3.5, 125 = 4.
    # ENV controls: up to 64 a steady depth, above it a decay, faster as it rises.
    "FM+STAT": {
        "description": "The Monomachine's FM+STAT, fitted to the original: two modulators into one carrier, M1 with its decay and feedback.",
        "defaults": [82, 64, 92, 0, 114, 24, 80, 64],
        "presets": {
            "Tine": [82, 64, 92, 0, 114, 24, 80, 64],
            "Bell": [120, 70, 84, 0, 88, 30, 100, 64],
            "Growl": [82, 64, 64, 56, 61, 48, 64, 64],
            "Wobble": [8, 64, 64, 0, 82, 40, 64, 64],
        },
    },
    "FM+PAR": {
        "description": "The Monomachine's FM+PAR, fitted to the original: three modulators into one carrier, each with its own depth or decay.",
        "defaults": [82, 48, 104, 90, 114, 104, 72, 64],
        "presets": {
            "Keys": [82, 48, 104, 90, 114, 104, 72, 64],
            "Organ": [82, 40, 104, 32, 114, 24, 64, 64],
            "Pluck": [82, 96, 104, 104, 120, 112, 90, 64],
            "Glass": [114, 88, 120, 96, 93, 84, 110, 64],
        },
    },
    "FM+DYN": {
        "description": "The Monomachine's FM+DYN, fitted to the original: M1 with pitch and depth envelopes, M2 with its decay and feedback.",
        "defaults": [64, 64, 40, 92, 64, 88, 0, 64],
        "presets": {
            "Strike": [64, 64, 40, 92, 64, 88, 0, 64],
            "Zap": [64, 104, 48, 64, 90, 0, 0, 64],
            "Bass": [32, 64, 56, 84, 64, 96, 24, 64],
            "Reed": [64, 72, 60, 64, 104, 40, 40, 64],
        },
    },
    # The DigiPRO machines (voices/dpro.hpp, dpro_bbox.hpp): stepped controls sit
    # mid-step. WAVE picks S01..S32 four steps each; WAV1, WAV2 and DENS's WAVE pick
    # D01..D64 two steps each; PCH2..4 step through 32 intervals in fours, OFF at 62..63.
    "DPRO-WAVE": {
        "description": "The Monomachine's DPRO-WAVE: its own S-sets, captured from the owner's ROM, moved along WP.",
        "defaults": [41, 0, 0, 0, 0, 64, 0, 64],
        "presets": {
            "Set 11": [41, 0, 0, 0, 0, 64, 0, 64],
            "Morph": [41, 64, 0, 0, 0, 64, 0, 64],
            "Sweep": [21, 0, 40, 127, 0, 64, 0, 64],
            "Sync": [125, 0, 0, 0, 64, 80, 0, 64],
        },
    },
    "DPRO-DDRW": {
        "description": "The Monomachine's DPRO-DDRW: two of its D-waves, crossfaded, spread and bit-reduced.",
        "defaults": [0, 64, 20, 0, 0, 64, 0, 64],
        "presets": {
            "Double": [0, 64, 20, 0, 0, 64, 0, 64],
            "Organ": [34, 64, 40, 0, 0, 40, 0, 64],
            "Crushed": [10, 40, 8, 0, 100, 64, 110, 64],
            "Voices": [96, 64, 112, 0, 0, 90, 0, 64],
        },
    },
    "DPRO-DENS": {
        "description": "The Monomachine's DPRO-DENS: a chord of up to four D-waves, each with a chorus.",
        "defaults": [86, 101, 62, 32, 0, 40, 40, 64],
        "presets": {
            "Major": [86, 101, 62, 32, 0, 40, 40, 64],
            "Minor": [74, 101, 127, 32, 0, 40, 40, 64],
            "Fifths": [105, 127, 62, 0, 0, 20, 20, 64],
            "Strings": [86, 101, 127, 64, 0, 90, 60, 64],
        },
    },
    "DPRO-BBOX": {
        "description": "The Monomachine's DPRO-BBOX: its 24 drum samples on 24 keys, from the owner's own ROM.",
        "defaults": [64, 0, 0, 0, 0, 64, 0, 0],
        "presets": {
            "Kit": [64, 0, 0, 0, 0, 64, 0, 0],
            "Low": [40, 0, 0, 0, 0, 64, 0, 0],
            "Roll": [64, 0, 0, 0, 4, 24, 0, 0],
            "Glitch": [80, 40, 0, 0, 16, 100, 0, 0],
        },
    },
}
