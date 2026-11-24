#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic RGB565-only tests of seventh F12 source/lighting attribution."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from analyze_f12_capture import COVERAGE_RGB565, ATLAS_RGB565, WORLD_RGB565
from analyze_final_texel_capture import SOURCE_RGB565
from analyze_source_audit_capture import analyze
from test_analyze_f12_capture import create_bmp


def fixture(folder: Path, *, name='frame', size=(6,4), frame=None,
            coverage=None, texel=None, audit=None, include_audit=True):
    w,h=size; total=w*h
    frame=list(frame if frame is not None else [0xffff]*total)
    coverage=list(coverage if coverage is not None else [5]*total)
    texel=list(texel if texel is not None else [0]*total)
    audit=list(audit if audit is not None else [0]*total)
    assert len(frame)==len(coverage)==len(texel)==len(audit)==total
    imgs={
        'frame':frame,
        'coverage':[COVERAGE_RGB565[i] for i in coverage],
        'world':[WORLD_RGB565[0]]*total,
        'atlas':[ATLAS_RGB565[0]]*total,
        'texel':[SOURCE_RGB565[i] for i in texel],
    }
    if include_audit:
        imgs['sourceaudit']=audit
    for key,data in imgs.items():
        create_bmp(folder/f'{name}_{key}.bmp',data,w=w,h=h)
    return folder/name


class SourceAuditTests(unittest.TestCase):
    def test_exact_index_and_prelight_conservation(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)
            color=[0xffff]*24;cov=[5]*24;source=[0]*24;audit=[0]*24
            black=[
                (1,1,0x4000|0x0100|1),
                (2,2,0x4000|0x0100|241),
                (3,4,0x4000|0x0100|23),
                (4,4,0x4000|0x0200|0x0400|42),
                (5,4,0x4000|0x0800|42),
                (6,3,0x4000|0x0100|0),
            ]
            for p,kind,metadata in black:
                color[p]=0;cov[p]=2;source[p]=kind;audit[p]=metadata
            color[7]=0x7777;cov[7]=1;source[7]=5
            audit[7]=0x4000|0x0100|0x0800|20
            color[8]=0x8888;cov[8]=1;source[8]=10
            audit[8]=0x4000|1
            prefix=fixture(folder,frame=color,coverage=cov,
                           texel=source,audit=audit)
            out=analyze(prefix,min_region_pixels=1)
            counts=out['counts']
            self.assertEqual(counts['final_black_textured_pixels'],6)
            self.assertEqual(counts['prelight_black_and_final_black'],4)
            self.assertEqual(counts['prelight_nonblack_to_final_black'],2)
            self.assertEqual(counts['prelight_black_to_final_nonblack'],1)
            self.assertEqual(counts['prelight_nonblack_and_final_nonblack'],1)
            self.assertEqual(counts['nonblack_to_black_modulated'],1)
            self.assertEqual(counts['nonblack_to_black_dither_enabled'],1)
            self.assertEqual(counts['nonblack_to_black_alpha_enabled'],1)
            self.assertEqual(out['top_final_black_8bpp_indices'][0],
                {'source_index':42,'final_black':2,'prelight_black':0,
                 'prelight_nonblack':2,'modulated_nonblack_to_black':1})
            self.assertEqual(out['distinct_final_black_8bpp_indices'],5)

    def test_ordered_dither_4x4_rank_denominators_and_black_rates(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);frame=[0xffff]*24;coverage=[5]*24
            source=[0]*24;audit=[0]*24
            # Pixel x=0 y=0 and x=4 y=0 share Bayer rank 0.
            # x=1 y=0 has Bayer rank 8. Each source is nonblack
            # before modulation; the final black mask is synthetic.
            for pos,black in [(0,True),(4,True),(1,False)]:
                coverage[pos]=2 if black else 1
                source[pos]=4 if black else 5
                frame[pos]=0 if black else 0x1234
                audit[pos]=0x4000|0x0200|0x0400|42
            out=analyze(fixture(p,frame=frame,coverage=coverage,
                                texel=source,audit=audit))
            ranks=out['ordered_dither_bayer_ranks']
            self.assertEqual(ranks[0]['textured_modulated_dithered'],2)
            self.assertEqual(ranks[0]['black_final'],2)
            self.assertEqual(ranks[0]['black_rate_percent'],100.0)
            self.assertEqual(ranks[8]['textured_modulated_dithered'],1)
            self.assertEqual(ranks[8]['black_final'],0)
            self.assertEqual(ranks[8]['black_rate_percent'],0.0)
            self.assertIsNone(ranks[15]['black_rate_percent'])
            self.assertEqual(sum(row['black_final'] for row in ranks),2)
            quarters=out['ordered_dither_bayer_quartiles']
            self.assertEqual(quarters[0]['black_final'],2)
            self.assertEqual(quarters[2]['textured_modulated_dithered'],1)

    def test_no_rendered_world_texture(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=analyze(fixture(Path(tmp)))
            self.assertEqual(out['counts']['final_textured_pixels'],0)
            self.assertEqual(out['top_final_black_8bpp_indices'],[])

    def test_hud_overwrite_clears_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            # A former shadow/texel must have no surviving final provenance
            out=analyze(fixture(Path(tmp),coverage=[5]*24,texel=[0]*24,
                                audit=[0]*24))
            self.assertEqual(out['counts']['final_black_textured_pixels'],0)

    def test_4bit_and_16bit_sources(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);frame=[0xffff]*24;coverage=[5]*24
            texel=[0]*24;audit=[0]*24
            for i,kind,source_format in [(1,6,0x2000),(2,8,0x1000)]:
                frame[i]=0;coverage[i]=2;texel[i]=kind
                audit[i]=source_format|0x0100
            out=analyze(fixture(p,frame=frame,coverage=coverage,
                                texel=texel,audit=audit))
            self.assertEqual(out['counts']['final_black_textured_pixels'],2)
            self.assertEqual(out['distinct_final_black_8bpp_indices'],0)

    def test_missing_seventh_layer(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError,'seventh'):
                analyze(fixture(Path(tmp),include_audit=False))

    def test_corrupt_audit_dimensions(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);prefix=fixture(p)
            create_bmp(p/'frame_sourceaudit.bmp',[0]*20,w=5,h=4)
            with self.assertRaisesRegex(ValueError,'dimensions'):
                analyze(prefix)

    def test_reject_packed_unknown_format(self):
        with tempfile.TemporaryDirectory() as tmp:
            audit=[0]*24;audit[0]=0x8000
            with self.assertRaisesRegex(ValueError,'contradictions'):
                analyze(fixture(Path(tmp),audit=audit))

    def test_reject_source_class_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            texel=[0]*24;texel[2]=1
            frame=[0xffff]*24;frame[2]=0
            cov=[5]*24;cov[2]=2
            audit=[0]*24;audit[2]=0x4000|0x0100|241
            with self.assertRaisesRegex(ValueError,'contradictions'):
                analyze(fixture(Path(tmp),frame=frame,coverage=cov,
                                texel=texel,audit=audit))

    def test_reject_dither_without_modulation(self):
        with tempfile.TemporaryDirectory() as tmp:
            texel=[0]*24;texel[2]=4
            frame=[0xffff]*24;frame[2]=0
            cov=[5]*24;cov[2]=2
            audit=[0]*24;audit[2]=0x4000|0x0400|12
            with self.assertRaisesRegex(ValueError,'contradictions'):
                analyze(fixture(Path(tmp),frame=frame,coverage=cov,
                                texel=texel,audit=audit))

    def test_half_open_roi_and_invalid_top_limit(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);prefix=fixture(p)
            out=analyze(prefix,roi='0,0,3,2')
            self.assertEqual(out['roi'],[0,0,3,2])
            with self.assertRaisesRegex(ValueError,'top-indices'):
                analyze(prefix,top_indices=0)
            with self.assertRaisesRegex(ValueError,'contained'):
                analyze(prefix,roi='0,0,7,4')

    def test_reject_final_world_source_without_audit(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);frame=[0xffff]*24;frame[1]=0
            cov=[5]*24;cov[1]=2
            source=[0]*24;source[1]=4
            with self.assertRaisesRegex(ValueError,'contradictions'):
                analyze(fixture(p,frame=frame,coverage=cov,texel=source))


if __name__=='__main__':
    unittest.main()
