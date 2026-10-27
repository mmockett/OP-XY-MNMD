#!/usr/bin/env python3
"""Turns mdref's LCD text dumps into PNGs, 4x, green on black like an MD.
    python3 reference/lcd_png.py local/ref/probe/*.txt"""
import struct
import sys
import zlib


def png(path, rows, scale=4):
    w, h = len(rows[0]) * scale, len(rows) * scale
    raw = b""
    for row in rows:
        line = b"".join((b"\x9c\xff\x5a" if c == "#" else b"\x10\x18\x10") * scale for c in row)
        raw += (b"\x00" + line) * scale
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(data)


for name in sys.argv[1:]:
    rows = [l.rstrip("\n") for l in open(name) if l.strip()]
    png(name[:-4] + ".png", rows)
