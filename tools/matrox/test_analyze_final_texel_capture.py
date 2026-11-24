#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthesized only; never rely on copyrighted screenshot fixtures."""
import tempfile
import unittest
from pathlib import Path
from analyze_f12_capture import COVERAGE_RGB565, WORLD_RGB565, ATLAS_RGB565
from analyze_final_texel_capture import analyze, SOURCE_RGB565
from test_analyze_f12_capture import create_bmp

def fixture(path, name='sample', sources=None, frame=None, coverage=None, size=(6,4), source_file=True):
    w,h=size;n=w*h
    sources=list(sources if sources is not None else [0]*n)
    frame=list(frame if frame is not None else [0xffff]*n)
    coverage=list(coverage if coverage is not None else [5]*n)
    values={'frame':frame,
            'coverage':[COVERAGE_RGB565[x] for x in coverage],
            'world':[WORLD_RGB565[0]]*n,
            'atlas':[ATLAS_RGB565[0]]*n}
    if source_file:values['texel']=[SOURCE_RGB565[x] for x in sources]
    for key, pixels in values.items():
        create_bmp(path/f'{name}_{key}.bmp',pixels,w=w,h=h)
    return path/name

class LastTexelTests(unittest.TestCase):
    def test_index1_is_distinct_from_241_and_source_zero(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t);src=[0]*24;frame=[0xffff]*24;cov=[5]*24
            for index,cls in [(1,1),(2,1),(3,2),(4,3),(5,4),(9,6),(10,8)]:
                src[index]=cls;frame[index]=0;cov[index]=2
            for index,cls in [(7,5),(8,7),(11,9),(13,10),(14,11)]:
                src[index]=cls;frame[index]=0x5555;cov[index]=1
            report=analyze(fixture(p,sources=src,frame=frame,coverage=cov),
                           min_region_pixels=1)
            self.assertEqual(report['final_textured_black_pixels'],7)
            self.assertEqual(report['counts']['index1_final_black'],2)
            self.assertEqual(report['counts']['index241_final_black'],1)
            self.assertAlmostEqual(report['final_index1_black_share_percent'],28.571,places=3)

    def test_rejected_key_is_not_a_final_write(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)
            out=analyze(fixture(p))
            self.assertEqual(out['counts']['other_final_msi_write'],24)
            self.assertEqual(out['final_textured_black_pixels'],0)
            self.assertIsNone(out['final_index1_black_share_percent'])

    def test_overwritten_hud_resets_source(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)
            result=analyze(fixture(p,sources=[0]*24,coverage=[5]*24))
            self.assertEqual(result['final_textured_black_pixels'],0)

    def test_final_black_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t);cov=[5]*24;frame=[0xffff]*24
            cov[1]=2;frame[1]=0
            with self.assertRaisesRegex(ValueError,'source provenance black'):
                analyze(fixture(p,frame=frame,coverage=cov))

    def test_source_nonblack_on_black_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t);src=[0]*24;cov=[5]*24;frame=[0xffff]*24
            src[1]=5;cov[1]=2;frame[1]=0
            with self.assertRaisesRegex(ValueError,'conflicts'):
                analyze(fixture(p,sources=src,frame=frame,coverage=cov))

    def test_ownership_zplane_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t);src=[0]*24;src[1]=1;frame=[0xffff]*24;frame[1]=0
            with self.assertRaisesRegex(ValueError,'conflicts'):
                analyze(fixture(p,sources=src,frame=frame))

    def test_missing_layer_and_wrong_dimensions(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)
            with self.assertRaisesRegex(ValueError,'sixth'):
                analyze(fixture(p,source_file=False))
            prefix=fixture(p)
            create_bmp(p/'sample_texel.bmp',[SOURCE_RGB565[0]]*20,w=5,h=4)
            with self.assertRaisesRegex(ValueError,'dimensions'):
                analyze(prefix)

    def test_invalid_roi(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t);prefix=fixture(p)
            with self.assertRaisesRegex(ValueError,'contained'):
                analyze(prefix,'0,0,7,4')
            with self.assertRaisesRegex(ValueError,'max-regions'):
                analyze(prefix,max_regions=0)

if __name__=='__main__':
    unittest.main()
