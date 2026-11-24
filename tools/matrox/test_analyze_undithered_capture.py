#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic-only RGB565 F12 no-dither counterfactual validation."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from test_analyze_source_audit_capture import fixture
from test_analyze_f12_capture import create_bmp
from analyze_undithered_capture import analyze


def make_case(folder: Path, *, nodither=None, frame=None, source=None,
              audit=None, coverage=None, include_ref=True, name='sample'):
    n=24;frame=list(frame if frame is not None else [0xffff]*n)
    source=list(source if source is not None else [0]*n)
    audit=list(audit if audit is not None else [0]*n)
    coverage=list(coverage if coverage is not None else [5]*n)
    ref=list(nodither if nodither is not None else [0]*n)
    prefix=fixture(folder,name=name,size=(6,4),frame=frame,
                   texel=source,audit=audit,coverage=coverage)
    if include_ref:
        create_bmp(folder/f'{name}_nodither.bmp',ref,w=6,h=4)
    return prefix


class NoDitherTests(unittest.TestCase):
    def test_four_black_counterfactual_cases_and_bayer_histogram(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)
            frame=[0xffff]*24;source=[0]*24;coverage=[5]*24;audit=[0]*24
            ref=[0]*24
            # black caused by dither at index 42
            # stays black no dither at index 1
            # black avoided by dither at index 43
            # nonblack color altered at index 44
            # nonblack identical at index 45
            # alpha blended last-write example (reference holds same underlay)
            # no-dither inactive at index 46; reference MUST be zero there.
            for pixel,index,actual,nodither,alpha in [
                (0,42,0,0x0123,False),
                (1,1,0,0,False),
                (2,43,0x1234,0,False),
                (3,44,0x2222,0x2345,False),
                (4,45,0x4444,0x4444,False),
                (5,42,0,0x1234,True)]:
                frame[pixel]=actual
                coverage[pixel]=2 if actual==0 else 1
                source[pixel]=(
                    (1 if actual==0 else 10) if index==1
                    else (4 if actual==0 else 5)
                )
                audit[pixel]=0x4000|0x0200|0x0400|index
                if alpha:audit[pixel]|=0x0800
                ref[pixel]=nodither
            frame[6]=0x1234;source[6]=5;coverage[6]=1
            audit[6]=0x4000|0x0200|46
            out=analyze(make_case(p,nodither=ref,frame=frame,
                                  source=source,audit=audit,coverage=coverage),
                        min_region_pixels=1)
            c=out['counts']
            self.assertEqual(c['final_dithered_world_texels'],6)
            self.assertEqual(c['actual_black_undithered_nonblack'],2)
            self.assertEqual(c['actual_black_undithered_black'],1)
            self.assertEqual(c['actual_nonblack_undithered_black'],1)
            self.assertEqual(c['actual_nonblack_undithered_nonblack'],2)
            self.assertEqual(c['different_from_no_dither_reference'],4)
            self.assertEqual(c['identical_to_no_dither_reference'],2)
            self.assertEqual(c['nonblack_color_changed_by_dither'],1)
            self.assertEqual(c['alpha_blended_reference_pixels'],1)
            self.assertAlmostEqual(out['dither_created_black_share_percent'],
                                   66.667,places=3)
            self.assertEqual(out['top_indexed_dither_created_black'][0],
                             {'source_index':42,'pixels':2})
            self.assertEqual(out['bayer_ranks'][0]['dither_created_black'],1)

    def test_empty_capture(self):
        with tempfile.TemporaryDirectory() as t:
            out=analyze(make_case(Path(t)))
            self.assertEqual(out['counts']['final_dithered_world_texels'],0)
            self.assertIsNone(out['dither_created_black_share_percent'])

    def test_no_dither_world_write_requires_zero_reference(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t);frame=[0xffff]*24;source=[0]*24
            coverage=[5]*24;audit=[0]*24;ref=[0]*24
            frame[0]=0x1111;source[0]=5;coverage[0]=1
            audit[0]=0x4000|0x0200|40;ref[0]=0x1234
            with self.assertRaisesRegex(ValueError,'nonzero no-dither metadata'):
                analyze(make_case(p,frame=frame,source=source,
                    coverage=coverage,audit=audit,nodither=ref))

    def test_later_hud_write_clears_reference(self):
        with tempfile.TemporaryDirectory() as t:
            out=analyze(make_case(Path(t),
                coverage=[5]*24,audit=[0]*24,nodither=[0]*24))
            self.assertEqual(out['counts']['different_from_no_dither_reference'],0)

    def test_missing_eighth_bmp(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaisesRegex(ValueError,'eighth'):
                analyze(make_case(Path(t),include_ref=False))

    def test_bad_rgb565_dimensions(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t);prefix=make_case(p)
            create_bmp(p/'sample_nodither.bmp',[0]*20,w=5,h=4)
            with self.assertRaisesRegex(ValueError,'dimensions'):
                analyze(prefix)

    def test_roi_count_is_only_selected_pixels(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t);frame=[0xffff]*24;source=[0]*24
            coverage=[5]*24;audit=[0]*24;ref=[0]*24
            for pixel in (0,5):
                frame[pixel]=0;source[pixel]=4;coverage[pixel]=2
                audit[pixel]=0x4000|0x0200|0x0400|44
                ref[pixel]=0x1234
            prefix=make_case(p,frame=frame,source=source,coverage=coverage,
                             audit=audit,nodither=ref)
            out=analyze(prefix,roi='0,0,3,1')
            self.assertEqual(out['counts']['actual_black_undithered_nonblack'],1)

    def test_alpha_reference_black_counts_correctly(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t);frame=[0xffff]*24;source=[0]*24
            coverage=[5]*24;audit=[0]*24;ref=[0]*24
            frame[0]=0;source[0]=4;coverage[0]=2
            audit[0]=0x4000|0x0200|0x0400|0x0800|42
            out=analyze(make_case(p,frame=frame,source=source,
                      coverage=coverage,audit=audit,nodither=ref))
            self.assertEqual(out['counts']['actual_black_undithered_black'],1)
            self.assertEqual(out['counts']['alpha_blended_reference_pixels'],1)


if __name__=='__main__':
    unittest.main()
