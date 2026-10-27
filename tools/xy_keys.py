#!/usr/bin/env python3
"""Prints the per-key records of the Drumline and drum tracks in an OP-XY project.

    python3 -I tools/xy_keys.py song.xy [other.xy]

A project (.xy) is one byte-level run-length stream: two equal bytes are
followed by a count of further repeats. The format was worked out by
kmorrill/xy-format (MIT, https://github.com/kmorrill/xy-format); this decoder
is our own few lines. Decoded, each key of a drum-style track is a 128-byte
record (docs/proposals/drum-page-engines.md):

    +0x00  five 32-bit words. A sample pad: start, end, loop start, loop end
           (0, -1, 0, -1 for the whole sample) and the fade word, which
           carries the SDK's pad settings. Drumline: its four sound values
           (0..1 in Q31) in the four marker words, and 0
    +0x14  Drumline's drum model; a sample pad's root note
    +0x16  the key (MIDI note)
    +0x17  class: Drumline 1, 2 or 4; a sample pad 1
    +0x19  Drumline's variant; a sample pad's fine tune
    +0x1c  a sample pad's path; zeros for Drumline

With two files it prints only the records that differ. It reads projects only.
"""
import pathlib
import struct
import sys


def decode(data):
    out, i, prev = bytearray(), 0, -1
    while i < len(data):
        b = data[i]
        i += 1
        out.append(b)
        if b == prev:
            out.extend(bytes([b]) * data[i])
            i += 1
            prev = -1
        else:
            prev = b
    if out[:4] != bytes.fromhex("ddccbbaa"):
        sys.exit("not an OP-XY project")
    return bytes(out)


def records(image):
    """Every run of 24 records, 128 bytes apart, keyed 0x35..0x4c: (offset, note, record)."""
    found = []
    for start in range(0, len(image) - 24 * 128):
        if image[start + 0x16] == 0x35 and all(image[start + 0x16 + 128 * k] == 0x35 + k for k in range(24)):
            if found and start < found[-1][0] + 128:
                continue
            found.extend((start + 128 * k, 0x35 + k, image[start + 128 * k:start + 128 * (k + 1)]) for k in range(24))
    return found


def show(offset, note, r):
    words = struct.unpack_from("<5I", r)
    path = r[0x1c:].split(b"\0")[0].decode("latin-1")
    values = " ".join(f"{w / 2**31:6.4f}" if w < 2**31 else "  -1  " for w in words[:4])
    print(f"{offset:06x} note {note:3d}  id/root {r[0x14]:3d} class {r[0x17]} variant {r[0x19:0x1c].hex()}  "
          f"markers {values}  fade {words[4]:08x}" + (f"  {path}" if path else ""))


def main():
    images = [decode(pathlib.Path(p).read_bytes()) for p in sys.argv[1:]]
    if not images:
        sys.exit(__doc__)
    first = records(images[0])
    if len(images) == 1:
        for item in first:
            show(*item)
        return
    second = {o: r for o, _, r in records(images[1])}
    for offset, note, r in first:
        if second.get(offset) != r:
            show(offset, note, r)
            show(offset, note, second[offset])
            print()


if __name__ == "__main__":
    main()
