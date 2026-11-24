#!/usr/bin/env python3
"""Report index usage in original 1996 Matrox TITLEM.RWB, without exporting texels.

This is for the historical Criterion TEXU/STNG/RAST/DATA stream, not modern
RenderWare 3.x TXD. Inspect the first 128*128 raw index bytes only; no
assumption about the meaning of the remaining raster payload is needed.
"""
import argparse
import hashlib
import json
from pathlib import Path


def inspect(raw: bytes) -> list[dict]:
    if len(raw) > 32 * 1024 * 1024:
        raise ValueError("TITLEM.RWB exceeds 32 MiB")
    if not raw.startswith(b"ZZZZ"):
        raise ValueError("not the expected original Matrox title container")
    entries = []
    offset = 0
    while (at := raw.find(b"RAST", offset)) >= 0:
        offset = at + 4
        # Each named texture has STNG (8 bytes) directly before its RAST,
        # followed by STRT (16 bytes) and DATA (32768 bytes).
        if at < 16 or raw[at - 16:at - 12] != b"STNG":
            continue
        if int.from_bytes(raw[at - 12:at - 8], "big") != 8:
            continue
        try:
            name = raw[at - 8:at].split(b"\x00")[0].decode("ascii")
        except UnicodeDecodeError:
            continue
        if not name or not name.isprintable():
            continue
        if (len(raw) < at + 40 or raw[at + 8:at + 12] != b"STRT"
                or int.from_bytes(raw[at + 12:at + 16], "big") != 16):
            continue
        w = int.from_bytes(raw[at + 16:at + 20], "big")
        h = int.from_bytes(raw[at + 20:at + 24], "big")
        depth_tag = int.from_bytes(raw[at + 24:at + 28], "big")
        if (w, h) != (128, 128) or raw[at + 32:at + 36] != b"DATA":
            continue
        payload_bytes = int.from_bytes(raw[at + 36:at + 40], "big")
        container_bytes = int.from_bytes(raw[at + 4:at + 8], "big")
        if (payload_bytes != 32768 or container_bytes != 32 + payload_bytes
                or at + 40 + payload_bytes > len(raw)):
            raise ValueError(f"{name}: truncated/invalid RAST/DATA payload")
        indices = raw[at + 40:at + 40 + w * h]
        counts = {str(i): indices.count(i) for i in (0, 1, 241)}
        regions = [
            sum(indices[y * w + x] == 241
                for y in range(y0, y0 + 64)
                for x in range(x0, x0 + 64))
            for x0, y0 in ((0, 0), (64, 0), (0, 64), (64, 64))
        ]
        entries.append({
            "asset_name": name,
            "dimensions": [w, h],
            "raster_depth_tag": depth_tag,
            "first_16384_bytes_sha256": hashlib.sha256(indices).hexdigest(),
            "source_index_samples": counts,
            "source_index_241_quadrants_TL_TR_BL_BR": regions,
        })
        offset = at + 40 + payload_bytes
    if not entries:
        raise ValueError("no historical Matrox RAST entries found")
    return entries


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path,
                        help="Original installed game/TITLEM.RWB")
    args = parser.parse_args()
    raw = args.path.read_bytes()
    print(json.dumps({
        "rwb_sha256": hashlib.sha256(raw).hexdigest(),
        "textures": inspect(raw),
        "notice": "Raw original file indices, NOT final screen pixels",
    }, indent=2))


if __name__ == "__main__":
    main()
