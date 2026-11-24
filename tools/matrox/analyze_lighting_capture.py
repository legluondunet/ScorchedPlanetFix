#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Analyze exact lighting coefficients in last successful Matrox MSI world writes.

Ninth capture layer _lighting.bin (no copyrighted game assets) is a lossless
stream of five bytes per pixel with little-endian header "M95LIGHT", width u32,
height u32, record bytes u32=5, reserved u32=0. Each record is:
prelight_rgb565 u16, applied mr u8, mg u8, mb u8.
The seventh _sourceaudit.bmp is authoritative for final writer and flags;
non-textured/HUD/flat writes erase their previous lighting records.
RGB565 samples are after optional filtering, before MSI modulation or alpha.
"""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from collections import Counter

from analyze_f12_capture import read_rgb565_bmp, resolve_inputs, parse_roi
from analyze_undithered_capture import analyze as verify_previous_layers
from analyze_source_audit_capture import BAYER_4X4

MAGIC=b'M95LIGHT'
HEADER=struct.Struct('<8sIIII')
RECORD_SIZE=5
LIGHT_BINS=tuple((i,i+31) for i in range(0,256,32))


def ordered_color(sample: int, mr: int, mg: int, mb: int, x: int, y: int) -> int:
    """Mirror the native 4x4 Bayer rounding logic without interpolation."""
    rank=BAYER_4X4[((y&3)<<2)|(x&3)]
    threshold=((2*rank+1)*255)//32
    def component(c: int, m: int, limit: int) -> int:
        total=c*m
        q,rem=divmod(total,255)
        if rem>threshold and q<limit:
            q+=1
        return q
    r=component((sample>>11)&31,mr,31)
    g=component((sample>>5)&63,mg,63)
    b=component(sample&31,mb,31)
    return (r<<11)|(g<<5)|b


def undithered_color(sample: int, mr: int, mg: int, mb: int) -> int:
    r=((((sample>>11)&31)*mr)+127)//255
    g=((((sample>>5)&63)*mg)+127)//255
    b=(((sample&31)*mb)+127)//255
    return (r<<11)|(g<<5)|b


def parse_lighting(path: Path, width: int, height: int) -> memoryview:
    raw=path.read_bytes()
    if len(raw)<HEADER.size:
        raise ValueError('lighting layer is shorter than its 24-byte header')
    magic,w,h,record_bytes,reserved=HEADER.unpack_from(raw)
    if magic!=MAGIC or record_bytes!=RECORD_SIZE or reserved!=0:
        raise ValueError('invalid M95LIGHT signature/version/record length')
    if (w,h)!=(width,height):
        raise ValueError('lighting layer dimensions do not match RGB565 frame')
    if len(raw)!=HEADER.size+width*height*RECORD_SIZE:
        raise ValueError('lighting layer truncated or contains trailing bytes')
    return memoryview(raw)[HEADER.size:]


def analyze(prefix: Path, roi: str | None = None,
            min_region_pixels: int = 8, max_regions: int = 8,
            top_indices: int = 15) -> dict:
    verified=verify_previous_layers(
        prefix,roi,min_region_pixels,max_regions,top_indices)
    paths=resolve_inputs(prefix)
    width,height,frame=read_rgb565_bmp(paths['frame'])
    wa,ha,audit=read_rgb565_bmp(
        prefix.parent/f'{prefix.name}_sourceaudit.bmp')
    _,_,nodither=read_rgb565_bmp(
        prefix.parent/f'{prefix.name}_nodither.bmp')
    if (wa,ha)!=(width,height):
        raise ValueError('sourceaudit dimensions differ from frame')
    path=prefix.parent/f'{prefix.name}_lighting.bin'
    if not path.is_file():
        raise ValueError('F12 ninth _lighting.bin layer absent')
    raw=parse_lighting(path,width,height)
    bounds=parse_roi(roi,width,height) if roi else (0,0,width,height)
    x0,y0,x1,y1=bounds
    total=0;modulated=0;dithered=0;final_black=0
    black_bins=[0]*8;all_bins=[0]*8
    black_created_bins=[0]*8;black_survivor_bins=[0]*8
    equal_channels=0;unequal_channels=0
    black_uniform=0;black_nonuniform=0
    min_lighting=256;max_lighting=-1
    sum_lighting=0
    source_idx_black=Counter()
    mismatches=[]
    def issue(x:int,y:int,reason:str,expected:int|None=None,actual:int|None=None):
        if len(mismatches)<12:
            mismatches.append({'x':x,'y':y,'reason':reason,
                               'expected':expected,'actual':actual})
    for y in range(y0,y1):
        for x in range(x0,x1):
            p=y*width+x
            start=p*RECORD_SIZE
            sample=raw[start]|(raw[start+1]<<8)
            mr,mg,mb=raw[start+2],raw[start+3],raw[start+4]
            meta=audit[p]
            texture=bool(meta&0x7000)
            if not texture:
                if any((sample,mr,mg,mb)):
                    issue(x,y,'nonzero lighting data for final HUD/flat/empty pixel')
                continue
            total+=1
            if bool(meta&0x0100)!=(sample==0):
                issue(x,y,'sourceaudit prelight-black flag disagrees with sampled RGB565')
            use_modulate=bool(meta&0x0200)
            use_dither=bool(meta&0x0400)
            alpha=bool(meta&0x0800)
            if not use_modulate:
                if (mr,mg,mb)!=(255,255,255):
                    issue(x,y,'unmodulated texture must have neutral RGB coefficients')
                computed=sample
            else:
                modulated+=1
                if use_dither:
                    dithered+=1
                    computed=ordered_color(sample,mr,mg,mb,x,y)
                else:
                    computed=undithered_color(sample,mr,mg,mb)
            if not alpha and frame[p]!=computed:
                issue(x,y,'actual framebuffer differs from native modulation formula',
                      computed,frame[p])
            if use_dither and not alpha:
                expected=undithered_color(sample,mr,mg,mb)
                if nodither[p]!=expected:
                    issue(x,y,'undithered F12 reference differs from native formula',
                          expected,nodither[p])
            if not use_modulate:
                continue
            mean=(mr+mg+mb)//3
            bin_idx=min(mean//32,7)
            all_bins[bin_idx]+=1
            min_lighting=min(mean,min_lighting)
            max_lighting=max(mean,max_lighting)
            sum_lighting+=mean
            equal=(mr==mg==mb)
            if equal:equal_channels+=1
            else:unequal_channels+=1
            if frame[p]==0:
                final_black+=1
                black_bins[bin_idx]+=1
                if equal:black_uniform+=1
                else:black_nonuniform+=1
                if (meta&0x4000):
                    source_idx_black[meta&255]+=1
                if use_dither:
                    if nodither[p]==0:
                        black_survivor_bins[bin_idx]+=1
                    else:
                        black_created_bins[bin_idx]+=1
    if mismatches:
        raise ValueError(f'{len(mismatches)} lighting RGB565 invariant failures: '
                         f'{mismatches}')
    # The earlier layers verify all world-texture ownership. This audit
    # independently verifies the subset with both modulation and dither.
    if dithered!=verified['counts']['final_dithered_world_texels']:
        raise ValueError('modulated+dithered lighting count differs from F12 provenance')
    if sum(black_created_bins)!=verified['counts']['actual_black_undithered_nonblack']:
        raise ValueError('dither-created black count differs from eighth F12 layer')
    if sum(black_survivor_bins)!=verified['counts']['actual_black_undithered_black']:
        raise ValueError('modulation-only black count differs from eighth F12 layer')
    bins=[]
    for i,(lower,upper) in enumerate(LIGHT_BINS):
        bins.append({
            'lighting_mean_range':[lower,upper],
            'modulated_textured_pixels':all_bins[i],
            'final_black':black_bins[i],
            'dither_created_black':black_created_bins[i],
            'still_black_without_last_write_dither':black_survivor_bins[i],
            'black_rate_percent':round(100*black_bins[i]/all_bins[i],3)
                                 if all_bins[i] else None,
        })
    return {
        'capture':str(prefix),'source_size':[width,height],'roi':list(bounds),
        'counts':{
            'final_textured_with_light_samples':total,
            'final_modulated_textured_pixels':modulated,
            'final_dithered_textured_pixels':dithered,
            'modulated_final_black':final_black,
            'modulated_equal_rgb_coefficients':equal_channels,
            'modulated_nonuniform_rgb_coefficients':unequal_channels,
            'black_equal_rgb_coefficients':black_uniform,
            'black_nonuniform_rgb_coefficients':black_nonuniform,
        },
        'lighting_mean_stats':{
            'minimum':min_lighting if modulated else None,
            'maximum':max_lighting if modulated else None,
            'mean':round(sum_lighting/modulated,3) if modulated else None,
        },
        'lighting_bins':bins,
        'top_final_black_8bpp_source_indices':[
            {'source_index':k,'pixels':v}
            for k,v in sorted(source_idx_black.items(),
                              key=lambda item:(-item[1],item[0]))[:top_indices]],
        'interpretation':(
            'Exact RGB565 prelight source and actual per-channel 0..255 Gouraud '
            'modifiers are preserved for the last successful MSI world draw. '
            'The analyzer verifies native modulation and Bayer rounding when '
            'the last write was not alpha-blended. This proves internal '
            'software arithmetic consistency, NOT original Matrox Mystique '
            'hardware parity, gamma correctness, or original driver color.'
        ),
    }


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix',type=Path)
    parser.add_argument('--roi',help='half-open x0,y0,x1,y1 native rectangle')
    parser.add_argument('--min-region-pixels',type=int,default=8)
    parser.add_argument('--max-regions',type=int,default=8)
    parser.add_argument('--top-indices',type=int,default=15)
    parser.add_argument('--json',action='store_true')
    args=parser.parse_args()
    try:
        out=analyze(args.prefix,args.roi,args.min_region_pixels,
                    args.max_regions,args.top_indices)
    except (OSError,ValueError) as exc:
        parser.error(str(exc))
    if args.json:
        print(json.dumps(out,indent=2,sort_keys=True))
    else:
        print(out['capture'])
        for k,v in out['counts'].items():
            print(f'  {k}: {v}')
        print(f"  Lighting mean: {out['lighting_mean_stats']}")
        for row in out['lighting_bins']:
            print('  {lighting_mean_range}: {final_black}/{modulated_textured_pixels}'
                  ' black; {dither_created_black} from dither'.format(**row))
        print(out['interpretation'])
    return 0


if __name__=='__main__':
    raise SystemExit(main())
