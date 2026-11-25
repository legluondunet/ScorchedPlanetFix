#!/usr/bin/env python3
"""Synthetic local replay fixture; no game assets."""
import struct
import sys
from pathlib import Path

w, h = 640, 400
n = 8
before = [0x1234] * (w * h)
after = before.copy()
draws = bytearray()
vertices = bytearray()
for i in range(n):
    x, y = 20 + 40 * i, 20
    draws += struct.pack("<6I", i % 2, 64, x, y, x + 17, y + 17)
    for vx, vy in ((x, y), (x + 16, y), (x, y + 16)):
        vertices += struct.pack("<8f", vx, vy, 0, 0, 1, 255, 255, 255)
    for yy in range(y, y + 16):
        for xx in range(x, x + 16):
            # Avoid relying on a chosen triangle tie convention; CPU includes
            # diagonal center samples, replay categorizes them as edge pixels.
            if xx - x + yy - y + 1 <= 16:
                after[yy * w + xx] = 0x07e0 if i % 2 else 0xf800
data = bytearray(b"M95GPU01")
data += struct.pack("<6I", w, h, n, 2, 2, 1)
data += struct.pack("<6I", 1, 1, 0, 1, 1, 1)
data += draws + vertices + struct.pack("<2I", 0xf800, 0x07e0)
data += struct.pack("<%dH" % len(before), *before)
data += struct.pack("<%dH" % len(after), *after)
p = Path(sys.argv[1])
p.write_bytes(data)
p.with_suffix(".bad").write_bytes(data[:-1])
