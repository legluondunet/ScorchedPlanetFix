#!/usr/bin/env python3
"""Synthetic fixture tests: absolutely no original game or graphic assets."""
import struct
import tempfile
import unittest
from pathlib import Path
from analyze_f12_capture import (ATLAS_RGB565, COVERAGE_RGB565,
                                 WORLD_RGB565, analyze, parse_roi,
                                 read_rgb565_bmp, resolve_inputs,
                                 connected_regions)


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
                'world': [WORLD_RGB565[x] for x in (12, 13, 0, 10)],
            }
            for kind, vals in d.items():
                name = Path(str(root) + '_' + kind +
                            ('(1)' if kind == 'coverage' else '') + '.bmp')
                create_bmp(name, vals)
            result = analyze(resolve_inputs(root))
            self.assertEqual(result['frame'], {
                'black': 2, 'colored': 2, 'black_without_final_msi_write': 1})
            self.assertEqual(result['world_last_attempt']['indexed8_index1_opaque_preview'], 1)
            self.assertEqual(result['world_last_attempt']['indexed8_icon01_index241_opaque'], 1)
            self.assertEqual(result['world_last_attempt']['indexed8_index241_rgbkey_discard'], 1)
            self.assertEqual(result['atlas_unmarked_zero_crosscheck']['last_attempt_skips'], 2)
            self.assertEqual(result['atlas_unmarked_zero_crosscheck']['final_frame_colored'], 1)
            self.assertEqual(result['atlas_unmarked_zero_crosscheck']['final_frame_black'], 1)
            self.assertEqual(analyze(resolve_inputs(root), '0,0,1,2')['pixels'], 2)
            self.assertEqual(analyze(resolve_inputs(root), '0,0,1,2')['frame']['black'], 2)
            with self.assertRaisesRegex(ValueError, 'contained'):
                parse_roi('0,0,3,2', 2, 2)


    def test_connected_clusters_four_way_not_diagonally(self):
        mask = bytearray(4 * 3)
        mask[0] = mask[5] = 1  # Diagonal neighbors do not connect.
        found = connected_regions(mask, 4, (0, 0, 4, 3), minimum=1)
        self.assertEqual(found['total_components_all_sizes'], 2)
        self.assertEqual([r['bbox'] for r in found['reported']],
                         [[0, 0, 1, 1], [1, 1, 2, 2]])
        self.assertEqual(found['reported'][0]['pixels'], 1)

    def test_spatial_regions_are_bounded_and_do_not_change_existing_counts(self):
        w, h = 6, 5
        frame = [0xffff] * (w*h)
        cov = [1] * (w*h)
        world = [0] * (w*h)
        atlas = [0] * (w*h)

        def paint(x, y, f, c, a, src):
            i = y*w + x
            frame[i], cov[i], atlas[i], world[i] = f, c, a, src

        # A valid connected key-discard cluster and a separate single pixel.
        for x, y in ((1, 1), (2, 1), (2, 2), (5, 4)):
            paint(x, y, 0xf800, 7, 0, 10)
        # Actual black world, not an absent pixel: original index1 preview.
        for x, y in ((4, 0), (4, 1)):
            paint(x, y, 0, 2, 0, 12)
        # Candidate unowned black atlas skip, not evidence of missing art.
        for x, y in ((0, 3), (0, 4)):
            paint(x, y, 0, 9, 4, 0)

        with tempfile.TemporaryDirectory() as t:
            paths = {}
            for kind, values in (
                ('frame', frame),
                ('coverage', [COVERAGE_RGB565[i] for i in cov]),
                ('atlas', [ATLAS_RGB565[i] for i in atlas]),
                ('world', [WORLD_RGB565[i] for i in world]),
            ):
                p = Path(t) / (kind + '.bmp')
                create_bmp(p, values, w=w, h=h)
                paths[kind] = p

            old = analyze(paths)
            new = analyze(paths, regions=True, min_region_pixels=1,
                          max_regions=1)
            self.assertNotIn('spatial_regions', old)
            self.assertEqual(
                {k: v for k, v in new.items() if k != 'spatial_regions'}, old)
            self.assertEqual(new['frame']['black'], 4)
            result = new['spatial_regions']
            keyed = result['world_index241_keyed_only']
            self.assertEqual(keyed['candidate_pixels_in_roi'], 4)
            self.assertEqual(keyed['total_components_all_sizes'], 2)
            self.assertEqual(keyed['components_above_minimum'], 2)
            self.assertEqual(keyed['unreported_above_minimum'], 1)
            self.assertEqual(keyed['reported'][0]['bbox'], [1, 1, 3, 3])
            self.assertEqual(keyed['reported'][0]['pixels'], 3)
            self.assertFalse(keyed['reported'][0]['touches_roi_boundary'])
            opaque = result['visible_world_index1_black_preview']
            self.assertEqual(opaque['candidate_pixels_in_roi'], 2)
            self.assertEqual(opaque['reported'][0]['bbox'], [4, 0, 5, 2])
            self.assertTrue(opaque['reported'][0]['touches_roi_boundary'])
            atlas_black = result['zplane_unmarked_zero_black_unowned']
            self.assertEqual(atlas_black['candidate_pixels_in_roi'], 2)
            self.assertEqual(atlas_black['reported'][0]['bbox'], [0, 3, 1, 5])
            self.assertEqual(
                result['black_without_final_msi_write']['candidate_pixels_in_roi'], 2)

            # Restricting to a central panel does not include right-edge
            # island, top index1 black or left-edge atlas skip.
            panel = analyze(paths, '1,1,4,4', regions=True,
                            min_region_pixels=2)
            self.assertEqual(panel['spatial_regions'][
                'world_index241_keyed_only']['candidate_pixels_in_roi'], 3)
            self.assertEqual(panel['spatial_regions'][
                'world_index241_keyed_only']['reported'][0]['bbox'],
                [1, 1, 3, 3])
            self.assertEqual(panel['spatial_regions'][
                'visible_world_index1_black_preview']['candidate_pixels_in_roi'],
                0)
            with self.assertRaisesRegex(ValueError, 'min_region_pixels'):
                analyze(paths, regions=True, min_region_pixels=0)
            with self.assertRaisesRegex(ValueError, 'max_regions'):
                analyze(paths, regions=True, max_regions=33)

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
