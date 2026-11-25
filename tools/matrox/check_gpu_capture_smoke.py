#!/usr/bin/env python3
"""Validate actual M95GPU01 output from the synthetic Win32 MSI95 smoke."""
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
assert data[:8] == b"M95GPU01"
w, h, n, nt, words, frame = struct.unpack_from("<6I", data, 8)
assert (w, h, n, nt, words) == (640, 400, 32, 1, 4096)
assert len(data) == 32 + 12 * nt + 120 * n + 4 * words + 4 * w * h
assert struct.unpack_from("<3I", data, 32) == (64, 64, 0)
draw_offset = 44
vertex_offset = draw_offset + 24 * n
tex_offset = vertex_offset + 96 * n
for i in range(n):
    tid, flags, x0, y0, x1, y1 = struct.unpack_from("<6I", data, draw_offset + 24 * i)
    x = 8 + 38 * (i // 2)
    assert tid == 0 and flags == 70, (i, tid, flags)
    assert (x0, y0, x1, y1) == (x, 20, x + 17, 37)
    for j in range(3):
        xyuv = struct.unpack_from("<8f", data, vertex_offset + 96 * i + 32 * j)
        assert xyuv[2:] == (.0625, .5, 1., 255., 255., 255.), xyuv
texels = struct.unpack_from("<%dI" % words, data, tex_offset)
assert texels[4] == 0xffff and texels[12] == 0x8410
before_offset = tex_offset + 4 * words
before = struct.unpack_from("<%dH" % (w*h), data, before_offset)
after = struct.unpack_from("<%dH" % (w*h), data, before_offset + 2*w*h)
assert not any(before), "expected native clear"
changed = 0
for yy in range(h):
    for xx in range(w):
        inside = 20 <= yy < 36 and any(8 + 38*j <= xx < 24 + 38*j for j in range(16))
        expected = 0xffff if inside else 0
        assert after[yy*w+xx] == expected, (xx, yy, after[yy*w+xx], expected)
        changed += bool(expected)
assert changed == 4096
print(f"MSI_GPU_CAPTURE_SMOKE_PASS frame={frame} draws={n} textures={nt} changed={changed}")
