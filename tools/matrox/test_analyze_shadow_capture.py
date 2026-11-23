#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic-only F12 shadow provenance and color/coverage regression cases."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from analyze_f12_capture import ATLAS_RGB565, COVERAGE_RGB565, WORLD_RGB565
from analyze_shadow_capture import analyze, SHADOW_RGB565
from test_analyze_f12_capture import create_bmp


def fixture(folder: Path, name: str, *, shadow=None, coverage=None,
            size=(6,4), include_shadow=True) -> Path:
    width,height = size
    pixels=width*height
    shadow = list(shadow) if shadow is not None else [0]*pixels
    coverage = list(coverage) if coverage is not None else [1]*pixels
    assert len(shadow)==len(coverage)==pixels
    values={
        'frame': [0xffff] * pixels,
        'coverage': [COVERAGE_RGB565[x] for x in coverage],
        'world': [WORLD_RGB565[0]] * pixels,
        'atlas': [ATLAS_RGB565[0]] * pixels,
        'shadow': [SHADOW_RGB565[x] for x in shadow],
    }
    prefix=folder/name
    for key,v in values.items():
        if key=='shadow' and not include_shadow:
            continue
        create_bmp(folder/f'{name}_{key}.bmp', v, w=width,h=height)
    return prefix


class ShadowF12Tests(unittest.TestCase):
    def test_shadow_counts_and_four_connected_islands(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary)
            shadow=[0]*24
            for index in (1, 2, 7, 9, 14, 20):
                shadow[index]=2
            for index in (3, 4, 10):
                shadow[index]=1
            shadow[16]=3
            shadow[17]=4
            cov=[1]*24
            for i,v in enumerate(shadow):
                if v:
                    cov[i]=3
            prefix=fixture(folder,'sample',shadow=shadow,coverage=cov)
            out=analyze(prefix,min_region_pixels=1)
            counts=out['shadow_final_write_counts']
            self.assertEqual(counts['opaque_no_recorded_textured_underlay'],6)
            self.assertEqual(counts['blended_with_textured_world_underlay'],3)
            self.assertEqual(counts['other_dark_flat_with_textured_underlay'],1)
            self.assertEqual(counts['other_dark_flat_no_recorded_underlay'],1)
            regions=out['four_connected_regions']['opaque_no_recorded_textured_underlay']
            self.assertGreater(regions['total_components_all_sizes'],1)
            self.assertEqual(sum(r['pixels'] for r in regions['reported']),6)
            self.assertEqual(out['shadow_provenance_coverage_conflicts'],0)
            crop=analyze(prefix,'0,0,3,2',min_region_pixels=1)
            self.assertEqual(crop['shadow_final_write_counts'][
                'opaque_no_recorded_textured_underlay'],2)

    def test_pixel_written_then_overlaid_is_not_shadow(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary)
            shadow=[0]*24  # final HUD write replaces an earlier shadow
            cov=[5]*24
            prefix=fixture(folder,'overwritten',shadow=shadow,coverage=cov)
            result=analyze(prefix)
            self.assertEqual(result['shadow_final_write_counts'][
                'other_final_msi_write'],24)

    def test_reject_falsely_claimed_shadow_on_zplane(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary)
            shadow=[0]*24
            shadow[5]=2
            prefix=fixture(folder,'bad',shadow=shadow,coverage=[5]*24)
            with self.assertRaisesRegex(ValueError,'non-flat MSI ownership'):
                analyze(prefix)

    def test_reject_legacy_four_layer_capture(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary)
            prefix=fixture(folder,'legacy',include_shadow=False)
            with self.assertRaisesRegex(ValueError,'fifth shadow layer absent'):
                analyze(prefix)

    def test_invalid_roi_and_max_regions(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary)
            prefix=fixture(folder,'valid')
            with self.assertRaisesRegex(ValueError,'contained'):
                analyze(prefix,'0,0,7,4')
            with self.assertRaisesRegex(ValueError,'max-regions'):
                analyze(prefix,max_regions=0)

    def test_reject_non_rgb565_shadow_palette_and_mismatch(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary)
            prefix=fixture(folder,'bad_palette')
            create_bmp(folder/'bad_palette_shadow.bmp',[0x1234]*24,w=6,h=4)
            with self.assertRaisesRegex(ValueError,'unknown RGB565'):
                analyze(prefix)
            prefix=fixture(folder,'bad_size')
            create_bmp(folder/'bad_size_shadow.bmp',
                       [SHADOW_RGB565[0]]*20,w=5,h=4)
            with self.assertRaisesRegex(ValueError,'same dimensions'):
                analyze(prefix)


if __name__=='__main__':
    unittest.main()
