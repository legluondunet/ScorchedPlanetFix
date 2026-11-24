#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Analyze the seventh F12 layer: exact final 8bpp index + prelight/FX flags.

This is a packed RGB565 BMP used ONLY as a lossless 16-bit container:
  bit14 = source is original 8bpp, bits 0..7 = exact raw palette index;
  bit13 = 16bpp source; bit12 = 4bpp source (no index in low bits);
  bit8 = post-sampling, PRE-lighting RGB565 texel is black (0x0000);
  bit9 = texture-color modulation was applied;
  bit10 = ordered RGB565 dither was applied (with modulation);
  bit11 = source color was alpha-blended with an earlier framebuffer value.
The three format bits are mutually exclusive; 0 means no FINAL ordinary
MSI world-texture write. Keyed/overdrawn attempts are never final sources.
The underlying screenshot is NOT a native-Matrox-driver ground truth.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from analyze_f12_capture import (
    COVERAGE_RGB565, decode_debug_pixels, read_rgb565_bmp,
    parse_roi, resolve_inputs, connected_regions,
)
from analyze_final_texel_capture import (
    BLACK_SOURCE_CATEGORIES, SOURCE_RGB565, analyze as check_final_texel,
)

FORMATS = {0x4000: 'indexed_8bpp', 0x2000: 'native_16bpp',
           0x1000: 'indexed_4bpp'}
VALID_BIT_MASK = 0x7fff
# Same 4x4 threshold ordering as the opt-in native RGB565 modulator.
BAYER_4X4 = (0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5)


def _expected_class(fmt: int, index: int, black: bool) -> int:
    if fmt == 0x2000:
        return 6 if black else 7
    if fmt == 0x1000:
        return 8 if black else 9
    if index == 1:
        return 1 if black else 10
    if index == 241:
        return 2 if black else 11
    if index == 0 and black:
        return 3
    return 4 if black else 5


def analyze(prefix: Path, roi: str | None = None,
            min_region_pixels: int = 8, max_regions: int = 8,
            top_indices: int = 15) -> dict:
    if not 1 <= top_indices <= 256:
        raise ValueError('top-indices must be in [1,256]')
    base = check_final_texel(prefix, roi, min_region_pixels, max_regions)
    needed = resolve_inputs(prefix)
    audit_path = prefix.parent / f'{prefix.name}_sourceaudit.bmp'
    if not audit_path.is_file():
        raise ValueError('F12 seventh _sourceaudit.bmp layer absent')
    width,height,frame=read_rgb565_bmp(needed['frame'])
    w2,h2,coverage_image=read_rgb565_bmp(needed['coverage'])
    w3,h3,texel_image=read_rgb565_bmp(
        prefix.parent / f'{prefix.name}_texel.bmp')
    w4,h4,audit=read_rgb565_bmp(audit_path)
    if len({(width,height),(w2,h2),(w3,h3),(w4,h4)}) != 1:
        raise ValueError('all F12 RGB565 layers must have same dimensions')
    coverage=decode_debug_pixels(coverage_image,COVERAGE_RGB565,'coverage')
    source=decode_debug_pixels(texel_image,SOURCE_RGB565,'texel')
    bounds=parse_roi(roi,width,height) if roi else (0,0,width,height)
    x0,y0,x1,y1=bounds
    indices=(y*width+x for y in range(y0,y1) for x in range(x0,x1))
    table={k:0 for k in (
        'final_textured_pixels','final_black_textured_pixels',
        'prelight_black_and_final_black','prelight_nonblack_to_final_black',
        'prelight_black_to_final_nonblack','prelight_nonblack_and_final_nonblack',
        'final_black_modulate_enabled','final_black_dither_enabled',
        'final_black_alpha_enabled','nonblack_to_black_modulated',
        'nonblack_to_black_without_modulate',
        'nonblack_to_black_dither_enabled',
        'nonblack_to_black_alpha_enabled')}
    by_idx={}
    # Derived from final successful textured writes only: do not use
    # candidate draws or dark-flat shadow overlays for the denominator.
    bayer_dithered=[0]*16
    bayer_black=[0]*16
    other_black_mask=bytearray(width*height)
    new_black_mask=bytearray(width*height)
    raw_black_mask=bytearray(width*height)
    malformed=[]
    for p in indices:
        word=audit[p]
        fmt=word&0x7000
        if word&0x8000 or (word and fmt not in FORMATS):
            malformed.append((p,'unsupported packed source format',word))
            continue
        if word==0:
            if source[p]!=0:
                malformed.append((p,'missing source audit for final texel',source[p]))
            continue
        idx=word&0xff
        raw_black=bool(word&0x0100)
        modulate=bool(word&0x0200)
        dither=bool(word&0x0400)
        alpha=bool(word&0x0800)
        final_black=frame[p]==0
        if (coverage[p] not in (1,2) or
            source[p]!=_expected_class(fmt,idx,final_black) or
            (fmt!=0x4000 and idx!=0) or (dither and not modulate)):
            malformed.append((p,'source/coverage/FX mismatch',word))
            continue
        table['final_textured_pixels']+=1
        if modulate and dither:
            rank=BAYER_4X4[((p//width)&3)*4 + ((p%width)&3)]
            bayer_dithered[rank]+=1
            if final_black:bayer_black[rank]+=1
        if final_black:
            table['final_black_textured_pixels']+=1
            if modulate:table['final_black_modulate_enabled']+=1
            if dither:table['final_black_dither_enabled']+=1
            if alpha:table['final_black_alpha_enabled']+=1
            if fmt==0x4000:
                bucket=by_idx.setdefault(idx,{'final_black':0,
                    'prelight_black':0,'prelight_nonblack':0,
                    'modulated_nonblack_to_black':0})
                bucket['final_black']+=1
                if raw_black:bucket['prelight_black']+=1
                else:bucket['prelight_nonblack']+=1
                if modulate and not raw_black:
                    bucket['modulated_nonblack_to_black']+=1
                if idx not in (0,1,241):other_black_mask[p]=1
            if raw_black:
                table['prelight_black_and_final_black']+=1
                raw_black_mask[p]=1
            else:
                table['prelight_nonblack_to_final_black']+=1
                new_black_mask[p]=1
                if modulate:table['nonblack_to_black_modulated']+=1
                else:table['nonblack_to_black_without_modulate']+=1
                if dither:table['nonblack_to_black_dither_enabled']+=1
                if alpha:table['nonblack_to_black_alpha_enabled']+=1
        elif raw_black:
            table['prelight_black_to_final_nonblack']+=1
        else:
            table['prelight_nonblack_and_final_nonblack']+=1
    if malformed:
        example=malformed[:4]
        raise ValueError(f'{len(malformed)} source audit contradictions: {example}')
    if table['final_textured_pixels']!=base['counts']['other_index_final_nonblack'] + sum(
        count for key,count in base['counts'].items()
        if key not in ('other_final_msi_write','other_index_final_nonblack')):
        raise ValueError('final textured ownership count inconsistent')
    if table['final_black_textured_pixels']!=base['final_textured_black_pixels']:
        raise ValueError('final source-audit black pixel count inconsistent')
    ranking=sorted(by_idx.items(),key=lambda x:(-x[1]['final_black'],x[0]))
    bayer_ranks=[
        {'rank':rank,'textured_modulated_dithered':bayer_dithered[rank],
         'black_final':bayer_black[rank],
         'black_rate_percent':round(100*bayer_black[rank]/bayer_dithered[rank],3)
               if bayer_dithered[rank] else None}
        for rank in range(16)]
    bayer_quartiles=[]
    for q in range(4):
        first=q*4
        count=sum(bayer_dithered[first:first+4])
        dark=sum(bayer_black[first:first+4])
        bayer_quartiles.append({
            'rank_range':[first,first+3],
            'textured_modulated_dithered':count,
            'black_final':dark,
            'black_rate_percent':round(100*dark/count,3) if count else None
        })
    if sum(bayer_dithered)!=table['final_black_dither_enabled']+sum(
            bayer_dithered)-sum(bayer_black):
        raise ValueError('Bayer black numerator diverges from dithered finals')
    region_args=(width,bounds,min_region_pixels,max_regions)
    return {
        'capture':str(prefix),
        'source_size':[width,height],
        'roi':list(bounds),
        'counts':table,
        'top_final_black_8bpp_indices':[
            {'source_index':index,**counts}
            for index,counts in ranking[:top_indices]],
        'distinct_final_black_8bpp_indices':len(by_idx),
        'ordered_dither_bayer_ranks':bayer_ranks,
        'ordered_dither_bayer_quartiles':bayer_quartiles,
        'regions':{
            'other_indices_final_black':connected_regions(other_black_mask,*region_args),
            'prelight_black_remains_black':connected_regions(raw_black_mask,*region_args),
            'prelight_nonblack_becomes_black':connected_regions(new_black_mask,*region_args),
        },
        'interpretation':('Prelight = sampled texture RGB565 before optional '
            'MSI modulation, ordered dither and source-over blending. Under '
            'nearest filtering this is the palette-resolved original color; '
            'optional bilinear changes this input. The dither/modulate/alpha '
            'flags identify active operations, not proof of correct original '
            'Mystique silicon behavior. This only tracks final successful '
            'MSI writes, not later CPU framebuffer modifications.'),
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
    except (OSError,ValueError) as e:
        parser.error(str(e))
    if args.json:
        print(json.dumps(out,indent=2,sort_keys=True))
    else:
        print(out['capture'])
        for name,n in out['counts'].items():
            print(f'  {name}: {n}')
        print('Leading final black source indices:')
        for row in out['top_final_black_8bpp_indices']:
            print('  index {source_index}: final_black={final_black}, '
                  'prelight_black={prelight_black}, '
                  'prelight_nonblack={prelight_nonblack}'.format(**row))
        print('Bayer 4x4 final-black rates (only modulated+dithered world texture):')
        for row in out['ordered_dither_bayer_quartiles']:
            print('  ranks {}-{}: {}/{} black ({}%)'.format(
                  *row['rank_range'],row['black_final'],
                  row['textured_modulated_dithered'],row['black_rate_percent']))
        print(out['interpretation'])
    return 0

if __name__=='__main__':
    raise SystemExit(main())
