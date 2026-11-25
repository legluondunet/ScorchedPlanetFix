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

# More representative immutable perspective/bilinear/modulated/dithered
# wave. The Python float32 incremental reference supplies the expected
# colors; use separate, overlapping draws and one fully discarded texture.
from diagnose_gpu_capture import replay, load

records = [(4, 4, 0), (4, 4, 16), (1, 1, 32)]
tex = tuple(((i*3 & 31) << 11) | ((i*7 & 63) << 5) | (i*5 & 31)
            for i in range(32)) + (0x10000,)
draws2 = []
verts2 = []
for i in range(8):
    x, y = 80 + 19*i, 70 + 9*(i%3)
    flags = 121 | (6 if i%2 else 0)
    tid = 2 if i == 4 else i%2
    draws2.append((tid, flags, x, y, x+51, y+49))
    mods = (157., 133., 192.) if i%2 else (211., 173., 119.)
    vv = ((x, y, .13, .11, .73, *mods),
          (x+50, y+3, .87, .17, 1.27, *(mods if i%2 else (113., 219., 145.))),
          (x+2, y+48, .19, .91, .91, *(mods if i%2 else (193., 101., 237.))))
    verts2.append(tuple(struct.unpack("<8f", struct.pack("<8f", *v)) for v in vv))
before2 = tuple((j*17+0x1234)&65535 for j in range(w*h))
capture = (2, records, draws2, verts2, tex, before2, before2)
after2, writers, _ = replay(capture)
assert sum(j>=0 for j in writers)>1000 and 4 not in writers
data = bytearray(b"M95GPU01")
data += struct.pack("<6I", w, h, len(draws2), len(records), len(tex), 2)
for record in records:
    data += struct.pack("<3I", *record)
for draw in draws2:
    data += struct.pack("<6I", *draw)
for vv in verts2:
    for v in vv:
        data += struct.pack("<8f", *v)
data += struct.pack("<%dI" % len(tex), *tex)
data += struct.pack("<%dH" % len(before2), *before2)
data += struct.pack("<%dH" % len(after2), *after2)
p.with_name("replay-world.bin").write_bytes(data)
