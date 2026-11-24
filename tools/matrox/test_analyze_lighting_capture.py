#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic-only tests for F12 exact RGB565/Gouraud lighting inputs."""
import struct
import tempfile
import unittest
from pathlib import Path

from test_analyze_undithered_capture import make_case
from analyze_lighting_capture import (
    analyze, ordered_color, undithered_color, parse_lighting, MAGIC, HEADER,
)


def fixture(directory:Path, frame=None, coverage=None, source=None,
            audit=None, nodither=None, lighting=None, header=None, name='sample'):
    n=24
    frame=list(frame if frame is not None else [0xffff]*n)
    coverage=list(coverage if coverage is not None else [5]*n)
    source=list(source if source is not None else [0]*n)
    audit=list(audit if audit is not None else [0]*n)
    nodither=list(nodither if nodither is not None else [0]*n)
    prefix=make_case(directory,name=name,frame=frame,source=source,
                     audit=audit,coverage=coverage,nodither=nodither)
    lighting=lighting or {}
    data=bytearray(n*5)
    for pixel,(sample,mr,mg,mb) in lighting.items():
        struct.pack_into('<HBBB',data,5*pixel,sample,mr,mg,mb)
    hdr=header if header is not None else HEADER.pack(MAGIC,6,4,5,0)
    (directory/f'{name}_lighting.bin').write_bytes(hdr+data)
    return prefix


class LightingInputTests(unittest.TestCase):
    def test_native_rounding_dither_black_creation_prevention_and_nonuniform(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            frame=[0xffff]*24; coverage=[5]*24; source=[0]*24
            audit=[0]*24;nodither=[0]*24;lighting={}
            # p1: rank8 yields 0, whereas round-to-nearest without dither
            # produces blue=1. Genuine black CREATED by last-write dither.
            p1=1
            frame[p1]=0;coverage[p1]=2;source[p1]=4
            audit[p1]=0x4000|0x0200|0x0400|42
            nodither[p1]=1;lighting[p1]=(1,128,128,128)
            self.assertEqual(ordered_color(1,128,128,128,1,0),0)
            self.assertEqual(undithered_color(1,128,128,128),1)
            # p4: rank0 gives blue=1, undithered round-to-nearest is 0,
            # so dithering PREVENTS black for the final successful write.
            p4=4
            frame[p4]=1;coverage[p4]=1;source[p4]=5
            audit[p4]=0x4000|0x0200|0x0400|43
            nodither[p4]=0;lighting[p4]=(1,64,64,64)
            self.assertEqual(ordered_color(1,64,64,64,4,0),1)
            self.assertEqual(undithered_color(1,64,64,64),0)
            # p2: modulation disabled, neutral modifiers, same RGB565.
            p2=2
            frame[p2]=0x1234;coverage[p2]=1;source[p2]=5
            audit[p2]=0x4000|44
            lighting[p2]=(0x1234,255,255,255)
            # p3: modulation active but no dither, three distinct values.
            p3=3
            frame[p3]=undithered_color(0x1234,100,120,140)
            coverage[p3]=1;source[p3]=5
            audit[p3]=0x4000|0x0200|45
            lighting[p3]=(0x1234,100,120,140)
            # p0: 4x4 rank0, regular dither+undither RGB565 test.
            p0=0
            frame[p0]=ordered_color(0xffff,128,128,128,0,0)
            coverage[p0]=1;source[p0]=5
            audit[p0]=0x4000|0x0200|0x0400|46
            nodither[p0]=undithered_color(0xffff,128,128,128)
            lighting[p0]=(0xffff,128,128,128)
            # p5: alpha underlay not saved, so exact colors are excluded.
            p5=5
            frame[p5]=0x2222;coverage[p5]=1;source[p5]=5
            audit[p5]=0x4000|0x0200|0x0400|0x0800|47
            nodither[p5]=0x3333;lighting[p5]=(0x1234,90,90,90)
            result=analyze(fixture(p,frame,coverage,source,audit,nodither,lighting))
            stats=result['counts']
            self.assertEqual(stats['final_textured_with_light_samples'],6)
            self.assertEqual(stats['final_modulated_textured_pixels'],5)
            self.assertEqual(stats['final_dithered_textured_pixels'],4)
            self.assertEqual(stats['modulated_final_black'],1)
            self.assertEqual(stats['modulated_nonuniform_rgb_coefficients'],1)
            self.assertEqual(stats['black_equal_rgb_coefficients'],1)
            self.assertEqual(sum(bin['dither_created_black']
                                 for bin in result['lighting_bins']),1)
            self.assertEqual(result['top_final_black_8bpp_source_indices'][0],
                             {'source_index':42,'pixels':1})

    def test_official_msi95_sdk_modulation_endpoint_scaling(self):
        """Vendor msitutor's RGB 0 and 255 vertex settings, no asset files."""
        # In MSI95's original TexturingTypes() demonstration, mr/mg/mb
        # values are 0 and 255, not normalized 0 and 1.
        # 255 -> identity; 0 -> zero. Green-only tint preserves green.
        samples=(0x0000,0x0001,0x001f,0x07e0,0xf800,0xffff,0x1234)
        for sample in samples:
            self.assertEqual(undithered_color(sample,255,255,255),sample)
            self.assertEqual(undithered_color(sample,0,0,0),0)
            for y in range(4):
                for x in range(4):
                    self.assertEqual(
                        ordered_color(sample,255,255,255,x,y),sample)
                    self.assertEqual(
                        ordered_color(sample,0,0,0,x,y),0)
        self.assertEqual(undithered_color(0xffff,0,255,0),0x07e0)
        self.assertEqual(undithered_color(0xffff,255,0,0),0xf800)
        self.assertEqual(undithered_color(0xffff,0,0,255),0x001f)
        self.assertEqual(undithered_color(0xffff,1,1,1),0)
        # Modulation never creates an absent channel.
        for rank in range(16):
            x,y=rank%4,rank//4
            out=ordered_color(0x07e0,255,100,255,x,y)
            self.assertEqual(out&0xf81f,0)

    def test_sdk_colored_modulation_in_full_f12_fixture(self):
        """Exercise native capture checks with independent RGB modifiers."""
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            frame=[0xffff]*24;coverage=[5]*24
            source=[0]*24;audit=[0]*24;nodither=[0]*24
            lighting={}
            tests=(
                (0,100,0xffff,0,255,0),
                (1,101,0xffff,255,0,0),
                (2,102,0xffff,0,0,255),
                (3,103,0xffff,255,255,255),
            )
            for pixel,index,sample,mr,mg,mb in tests:
                output=ordered_color(sample,mr,mg,mb,pixel,0)
                frame[pixel]=output
                coverage[pixel]=2 if output==0 else 1
                source[pixel]=4 if output==0 else 5
                audit[pixel]=0x4000|0x0200|0x0400|index
                nodither[pixel]=undithered_color(sample,mr,mg,mb)
                lighting[pixel]=(sample,mr,mg,mb)
            result=analyze(fixture(p,frame,coverage,source,audit,
                                   nodither,lighting))
            self.assertEqual(result['counts']['final_modulated_textured_pixels'],4)
            self.assertEqual(result['counts']['modulated_nonuniform_rgb_coefficients'],3)
            self.assertEqual(result['counts']['modulated_equal_rgb_coefficients'],1)
            self.assertEqual(result['counts']['modulated_final_black'],0)

    def test_ordered_bayer_quantization_bound_all_16_ranks(self):
        """For each channel, ordered dither is either floor or ceil."""
        for sample in (0,1,0x001f,0x07e0,0xf800,0xffff,0xabcdef&0xffff):
            for mr,mg,mb in ((0,0,0),(1,1,1),(127,193,87),(255,255,255)):
                c=((sample>>11)&31,(sample>>5)&63,sample&31)
                m=(mr,mg,mb)
                for y in range(4):
                    for x in range(4):
                        packed=ordered_color(sample,mr,mg,mb,x,y)
                        rgb=((packed>>11)&31,(packed>>5)&63,packed&31)
                        for ch,mod,actual in zip(c,m,rgb):
                            low=(ch*mod)//255
                            high=low+int((ch*mod)%255!=0)
                            self.assertIn(actual,(low,high))

    def test_empty_capture(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=analyze(fixture(Path(tmp)))
            self.assertEqual(out['counts']['final_textured_with_light_samples'],0)
            self.assertIsNone(out['lighting_mean_stats']['mean'])

    def test_reject_wrong_signature_dimensions_and_record_size(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            for header in (HEADER.pack(b'WRONGLIT',6,4,5,0),
                           HEADER.pack(MAGIC,8,3,5,0),
                           HEADER.pack(MAGIC,6,4,6,0),
                           HEADER.pack(MAGIC,6,4,5,1)):
                prefix=fixture(p,header=header)
                with self.assertRaises(ValueError):
                    analyze(prefix)

    def test_reject_truncated_layer_and_missing_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);prefix=fixture(p)
            file=p/'sample_lighting.bin'
            file.write_bytes(file.read_bytes()[:-1])
            with self.assertRaisesRegex(ValueError,'truncated'):
                analyze(prefix)
            file.unlink()
            with self.assertRaisesRegex(ValueError,'ninth'):
                analyze(prefix)

    def test_final_hud_write_clears_lighting_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            with self.assertRaisesRegex(ValueError,'final HUD/flat/empty'):
                analyze(fixture(p,lighting={0:(0x1234,255,255,255)}))

    def test_reject_prelight_black_flag_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            frame=[0xffff]*24;frame[1]=0
            cov=[5]*24;cov[1]=2
            source=[0]*24;source[1]=4
            audit=[0]*24;audit[1]=0x4000|0x0200|0x0400|42
            with self.assertRaisesRegex(ValueError,'prelight-black flag'):
                analyze(fixture(p,frame,cov,source,audit,lighting={
                    1:(0,128,128,128)}))

    def test_reject_wrong_native_modulation_result(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            frame=[0xffff]*24;frame[1]=1
            cov=[5]*24;cov[1]=1
            source=[0]*24;source[1]=5
            audit=[0]*24;audit[1]=0x4000|0x0200|0x0400|42
            reference=[0]*24;reference[1]=1
            with self.assertRaisesRegex(ValueError,'native modulation formula'):
                analyze(fixture(p,frame,cov,source,audit,reference,{
                    1:(1,128,128,128)}))

    def test_reject_wrong_undithered_reference(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            frame=[0xffff]*24;frame[1]=0
            cov=[5]*24;cov[1]=2
            source=[0]*24;source[1]=4
            audit=[0]*24;audit[1]=0x4000|0x0200|0x0400|42
            with self.assertRaisesRegex(ValueError,'undithered F12 reference'):
                analyze(fixture(p,frame,cov,source,audit,lighting={
                    1:(1,128,128,128)}))

    def test_reject_non_neutral_unmodulated_coefficients(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            frame=[0xffff]*24;frame[0]=0x1234
            cov=[5]*24;cov[0]=1
            source=[0]*24;source[0]=5
            audit=[0]*24;audit[0]=0x4000|44
            with self.assertRaisesRegex(ValueError,'neutral RGB'):
                analyze(fixture(p,frame,cov,source,audit,lighting={
                    0:(0x1234,128,255,255)}))

    def test_half_open_roi(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            frame=[0xffff]*24;cov=[5]*24;source=[0]*24
            audit=[0]*24;light={}
            for pos in (0,5):
                frame[pos]=0x1234;cov[pos]=1;source[pos]=5
                audit[pos]=0x4000|44;light[pos]=(0x1234,255,255,255)
            out=analyze(fixture(p,frame,cov,source,audit,lighting=light),
                        roi='0,0,3,1')
            self.assertEqual(out['counts']['final_textured_with_light_samples'],1)


if __name__=='__main__':
    unittest.main()
