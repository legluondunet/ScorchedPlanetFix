#!/usr/bin/env python3
"""Synthetic 1996 RWB fixture tests; no copyrighted game bytes."""
import unittest
from inspect_titlem_atlases import inspect


def fixture(name="icon01", width=128, height=128, short=False):
    indices = bytearray([5] * (128 * 128))
    for x, y in ((1, 1), (70, 1), (1, 70), (70, 70)):
        indices[y * 128 + x] = 241
    indices[12] = 1
    indices[13] = 0
    raster = bytes(indices) + b"\xcc" * 16384
    name_bytes = name.encode("ascii").ljust(8, b"\0")
    stng = b"STNG" + (8).to_bytes(4, "big") + name_bytes
    strt = (b"STRT" + (16).to_bytes(4, "big") +
            width.to_bytes(4, "big") + height.to_bytes(4, "big") +
            (16).to_bytes(4, "big") + (256).to_bytes(4, "big"))
    data = b"DATA" + (32768).to_bytes(4, "big") + raster
    rast = b"RAST" + (32 + 32768).to_bytes(4, "big") + strt + data
    return b"ZZZZ" + stng + (rast[:-1] if short else rast)


class RwbTitleTests(unittest.TestCase):
    def test_original_128_index_quadrants(self):
        entries = inspect(fixture())
        self.assertEqual(len(entries), 1)
        e = entries[0]
        self.assertEqual(e["asset_name"], "icon01")
        self.assertEqual(e["dimensions"], [128, 128])
        self.assertEqual(e["source_index_samples"], {"0": 1, "1": 1, "241": 4})
        self.assertEqual(e["source_index_241_quadrants_TL_TR_BL_BR"], [1, 1, 1, 1])

    def test_invalid_header_or_dimensions(self):
        with self.assertRaisesRegex(ValueError, "container"):
            inspect(b"other" + fixture())
        with self.assertRaisesRegex(ValueError, "no historical"):
            inspect(fixture(width=64))
        with self.assertRaisesRegex(ValueError, "truncated"):
            inspect(fixture(short=True))

    def test_no_raster_payload_leaks(self):
        e = inspect(fixture())[0]
        self.assertEqual(set(e), {
            "asset_name", "dimensions", "raster_depth_tag",
            "first_16384_bytes_sha256", "source_index_samples",
            "source_index_241_quadrants_TL_TR_BL_BR"})
        self.assertEqual(len(e["first_16384_bytes_sha256"]), 64)


if __name__ == "__main__":
    unittest.main()
