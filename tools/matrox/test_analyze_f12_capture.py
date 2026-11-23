#!/usr/bin/env python3
"""Synthetic fixture tests: absolutely no original game or graphic assets."""
import struct
import tempfile
import unittest
from pathlib import Path
from analyze_f12_capture import (ATLAS_RGB565, COVERAGE_RGB565,
                                 WORLD_RGB565, analyze, parse_roi,
                                 read_rgb565_bmp, resolve_inputs)


def create_bmp(path, pixels, w=2, h=2, top_down=True):
    stride = (w * 2 + 3) & ~3
    header = bytearray(66)
    header[0:2] = b'BM'
    struct.pack_into('<I', header, 2, 66 + stride * h)
    struct.pack_into('<IIiiHHII', header, 10, 66, 40, w,
                     -h if top_down else h, 1, 16, 3, stride * h)
    struct.pack_into('<III', header, 54, 0xF800, 0x07E0, 0x001F)
    content = bytearray()
    ys = range(h) if top_down else range(h-1, -1, -1)
    for y in ys:
        content += struct.pack(f'<{w}H', *pixels[y*w:(y+1)*w])
        content += b'\0' * (stride - 2*w)
    path.write_bytes(header + content)


class CaptureTests(unittest.TestCase):
    def test_top_down_and_bottom_up(self):
        with tempfile.TemporaryDirectory() as t:
            for down in (True, False):
                name = Path(t) / f'frame-{down}.bmp'
                create_bmp(name, [0, 1, 0xFFFF, 0x0102], top_down=down)
                self.assertEqual(read_rgb565_bmp(name),
                                 (2, 2, [0, 1, 0xFFFF, 0x0102]))

    def test_reject_wrong_mask_and_truncated_file(self):
        with tempfile.TemporaryDirectory() as t:
            p = Path(t) / 'bad.bmp'
            create_bmp(p, [0, 0, 0, 0])
            bad = bytearray(p.read_bytes())
            struct.pack_into('<I', bad, 54, 0x7C00)
            p.write_bytes(bad)
            with self.assertRaisesRegex(ValueError, 'unsupported'):
                read_rgb565_bmp(p)
            p.write_bytes(bytes(bad[:69]))
            with self.assertRaises(ValueError):
                read_rgb565_bmp(p)

    def test_semantics_and_alias_roi(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t) / 'matrox_capture_0007'
            # A: index1 black drawn. B: atlas zero skip then world colored.
            # C: atlas zero skip, black and no final MSI write.
            # D: index241 keyed but final Z-plane colored.
            d = {
                'frame': [0, 0xF800, 0, 0x07E0],
                'coverage': [COVERAGE_RGB565[x] for x in (2, 1, 9, 5)],
                'atlas': [ATLAS_RGB565[x] for x in (0, 4, 4, 0)],
                'world': [WORLD_RGB565[x] for x in (12, 2, 0, 10)],
            }
            for kind, vals in d.items():
                name = Path(str(root) + '_' + kind +
                            ('(1)' if kind == 'coverage' else '') + '.bmp')
                create_bmp(name, vals)
            result = analyze(resolve_inputs(root))
            self.assertEqual(result['frame'], {
                'black': 2, 'colored': 2, 'black_without_final_msi_write': 1})
            self.assertEqual(result['world_last_attempt']['indexed8_index1_opaque_preview'], 1)
            self.assertEqual(result['world_last_attempt']['indexed8_index241_rgbkey_discard'], 1)
            self.assertEqual(result['atlas_unmarked_zero_crosscheck']['last_attempt_skips'], 2)
            self.assertEqual(result['atlas_unmarked_zero_crosscheck']['final_frame_colored'], 1)
            self.assertEqual(result['atlas_unmarked_zero_crosscheck']['final_frame_black'], 1)
            self.assertEqual(analyze(resolve_inputs(root), '0,0,1,2')['pixels'], 2)
            self.assertEqual(analyze(resolve_inputs(root), '0,0,1,2')['frame']['black'], 2)
            with self.assertRaisesRegex(ValueError, 'contained'):
                parse_roi('0,0,3,2', 2, 2)

    def test_reject_mismatched_frame(self):
        with tempfile.TemporaryDirectory() as t:
            d = {}
            for kind, val in (('frame', 0), ('coverage', COVERAGE_RGB565[0]),
                              ('atlas', ATLAS_RGB565[0]), ('world', WORLD_RGB565[0])):
                p = Path(t) / (kind + '.bmp')
                create_bmp(p, [val]* (3 if kind == 'world' else 2)*2,
                           w=3 if kind == 'world' else 2)
                d[kind] = p
            with self.assertRaisesRegex(ValueError, 'dimension mismatch'):
                analyze(d)


if __name__ == '__main__':
    unittest.main()
