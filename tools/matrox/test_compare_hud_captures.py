#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Copyright-clean synthetic RGB565 tests for cross-F12 HUD coverage."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from compare_hud_captures import analyze
from analyze_f12_capture import (
    ATLAS_RGB565, COVERAGE_RGB565, WORLD_RGB565,
)
from test_analyze_f12_capture import create_bmp


class HudComparisonTests(unittest.TestCase):
    def create_capture(self, folder: Path, name: str, coverage: list[int],
                       frame: list[int], atlas: list[int],
                       width: int = 6, height: int = 4) -> Path:
        root = folder / name
        layers = {
            'frame': frame,
            'coverage': [COVERAGE_RGB565[c] for c in coverage],
            'atlas': [ATLAS_RGB565[a] for a in atlas],
            'world': [WORLD_RGB565[0]] * (width*height),
        }
        for kind, pixels in layers.items():
            create_bmp(folder / f'{name}_{kind}.bmp', pixels,
                       w=width, h=height)
        return root

    def test_fixed_hud_mask_across_different_world_scene_colors(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            a_cov = [1] * 24
            for i in (1, 2, 3, 7, 8):
                a_cov[i] = 5
            a_cov[9] = 6
            a_atlas = [0] * 24
            a_atlas[8] = 4  # A zero last-attempt while HUD is actually written.
            a_atlas[10] = 4  # Zero but no final HUD write.
            a_frame = [0x0001] * 24
            for i in (1, 2, 3, 7, 8):
                a_frame[i] = 0x07e0
            a_frame[9] = 0
            first = self.create_capture(path, 'frame05', a_cov, a_frame, a_atlas)
            b_frame = [0xf800] * 24   # Different scene/camera outside HUD.
            for i in (1,2,3,7,8):
                b_frame[i] = 0x07e0
            b_frame[2] = 0x03e0  # One dynamic HUD number.
            b_frame[9] = 0
            second = self.create_capture(path, 'frame06', a_cov, b_frame,
                                         a_atlas)
            result = analyze([first, second])
            row = result['comparisons'][0]
            self.assertTrue(row['hud_mask_identical'])
            self.assertEqual(row['hud_mask_symmetric_difference'], 0)
            self.assertEqual(row['common_hud_pixels'], 6)
            self.assertEqual(row['rgb565_color_diffs_on_common_hud_pixels'], 1)
            self.assertEqual(result['per_capture']['frame05'][
                'atlas_last_zero_skip_over_hud_write'], 1)
            self.assertEqual(result['per_capture']['frame05'][
                'atlas_last_zero_skip_without_hud_write'], 1)
            self.assertEqual(result['per_capture']['frame05'][
                'hud_black_final_pixels'], 1)
            self.assertEqual(analyze([first, second], '0,0,4,2')[
                'comparisons'][0]['common_hud_pixels'], 4)

    def test_detect_hud_mask_shift_and_missing_samples(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            a_cov = [1] * 24
            a_cov[3] = 5
            a_cov[4] = 6
            a_cov[5] = 9
            b_cov = a_cov.copy()
            b_cov[3] = 1   # Disappeared from last MSI HUD ownership.
            b_cov[6] = 5   # Newly written Z-plane.
            a = self.create_capture(folder,'base',a_cov,[0xffff]*24,[0]*24)
            b = self.create_capture(folder,'shift',b_cov,[0xfffe]*24,[0]*24)
            report = analyze([a,b])
            row = report['comparisons'][0]
            self.assertFalse(row['hud_mask_identical'])
            self.assertEqual(row['hud_mask_symmetric_difference'], 2)
            self.assertEqual(row['baseline_only_hud_pixels'], 1)
            self.assertEqual(row['compared_only_hud_pixels'], 1)
            self.assertEqual(row['common_hud_pixels'], 1)
            self.assertEqual(report['per_capture']['base'][
                'missing_texel_last_ownership'], 1)

    def test_dimension_ambiguity_and_invalid_rois(self):
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder)
            a = self.create_capture(p,'a',[1]*24,[0xffff]*24,[0]*24)
            b = self.create_capture(p,'b',[1]*24,[0xffff]*24,[0]*24)
            c = self.create_capture(p,'c',[1]*20,[0xffff]*20,
                                    [0]*20,width=5,height=4)
            with self.assertRaisesRegex(ValueError,'At least two'):
                analyze([a])
            with self.assertRaisesRegex(ValueError,'Duplicate'):
                analyze([a,a])
            with self.assertRaisesRegex(ValueError,'source dimensions'):
                analyze([a,c])
            with self.assertRaisesRegex(ValueError,'contained'):
                analyze([a,b],'1,0,10,4')

    def test_one_quartet_cannot_mix_different_layer_sizes(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)
            a=self.create_capture(p,'a',[1]*24,[0xffff]*24,[0]*24)
            b=self.create_capture(p,'b',[1]*24,[0xffff]*24,[0]*24)
            create_bmp(p / 'b_atlas.bmp',
                       [ATLAS_RGB565[0]]*20,w=5,h=4)
            with self.assertRaisesRegex(ValueError,'same-serial layers'):
                analyze([a,b])

    def test_reject_non_palette_coverage(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)
            a=self.create_capture(p,'a',[1]*24,[0xffff]*24,[0]*24)
            b=self.create_capture(p,'b',[1]*24,[0xffff]*24,[0]*24)
            create_bmp(p / 'b_coverage.bmp',[0x1234]*24,w=6,h=4)
            with self.assertRaisesRegex(ValueError,'unknown RGB565'):
                analyze([a,b])


if __name__ == '__main__':
    unittest.main()
