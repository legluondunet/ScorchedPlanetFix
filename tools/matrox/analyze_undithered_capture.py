#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""F12 last-write undithered RGB565 reference vs actual dithered framebuffer.

The eighth _nodither.bmp contains RGB565 references ONLY where the seventh
_sourceaudit.bmp says a final successful 3D world texel was modulated and
dithered. A zero in _nodither.bmp can be a valid black reference.

The reference recomputes only the LAST successful MSI pixel write, holding
the previously drawn actual framebuffer as its underlay for alpha. It is NOT
a full-frame replay of the original scene with dithering turned off.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from analyze_f12_capture import read_rgb565_bmp, resolve_inputs, parse_roi, connected_regions
from analyze_source_audit_capture import analyze as check_source_audit, BAYER_4X4


def analyze(prefix: Path, roi: str | None = None, min_region_pixels: int = 8,
            max_regions: int = 8, top_indices: int = 15) -> dict:
    # Full source/coverage/texel/BMP invariants must pass before comparing.
    base = check_source_audit(prefix, roi, min_region_pixels, max_regions, top_indices)
    paths = resolve_inputs(prefix)
    reference_path = prefix.parent / f'{prefix.name}_nodither.bmp'
    if not reference_path.is_file():
        raise ValueError('F12 eighth _nodither.bmp layer absent')
    w,h,displayed = read_rgb565_bmp(paths['frame'])
    wa,ha,audit = read_rgb565_bmp(prefix.parent / f'{prefix.name}_sourceaudit.bmp')
    wr,hr,reference = read_rgb565_bmp(reference_path)
    if len({(w,h),(wa,ha),(wr,hr)}) != 1:
        raise ValueError('frame/sourceaudit/nodither layers must have same dimensions')
    bounds=parse_roi(roi,w,h) if roi else (0,0,w,h)
    x0,y0,x1,y1=bounds
    counts={k:0 for k in (
        'final_dithered_world_texels',
        'identical_to_no_dither_reference',
        'different_from_no_dither_reference',
        'actual_black_undithered_black',
        'actual_black_undithered_nonblack',
        'actual_nonblack_undithered_black',
        'actual_nonblack_undithered_nonblack',
        'nonblack_color_changed_by_dither',
        'alpha_blended_reference_pixels',
    )}
    changed_ranks=[0]*16
    tracked_ranks=[0]*16
    black_created_ranks=[0]*16
    created_indices={}
    changed_mask=bytearray(w*h)
    created_black_mask=bytearray(w*h)
    black_without_dither_mask=bytearray(w*h)
    errors=[]
    for y in range(y0,y1):
        for x in range(x0,x1):
            p=y*w+x
            meta=audit[p]
            active=bool(meta&0x0400)
            old=displayed[p]
            new=reference[p]
            if not active:
                if new and len(errors)<10:
                    errors.append((x,y,'nonzero reference for non-dithered final writer',new))
                continue
            counts['final_dithered_world_texels']+=1
            if meta&0x0800:counts['alpha_blended_reference_pixels']+=1
            rank=BAYER_4X4[((y&3)<<2)|(x&3)]
            tracked_ranks[rank]+=1
            if old!=new:
                counts['different_from_no_dither_reference']+=1
                changed_mask[p]=1
                changed_ranks[rank]+=1
            else:
                counts['identical_to_no_dither_reference']+=1
            if old==0 and new==0:
                counts['actual_black_undithered_black']+=1
                black_without_dither_mask[p]=1
            elif old==0 and new!=0:
                counts['actual_black_undithered_nonblack']+=1
                created_black_mask[p]=1
                black_created_ranks[rank]+=1
                if meta&0x4000:
                    ix=meta&0xff
                    created_indices[ix]=created_indices.get(ix,0)+1
            elif old!=0 and new==0:
                counts['actual_nonblack_undithered_black']+=1
            else:
                counts['actual_nonblack_undithered_nonblack']+=1
                if old!=new:counts['nonblack_color_changed_by_dither']+=1
    if errors:
        raise ValueError(f'{len(errors)} unexpected nonzero no-dither metadata: {errors}')
    if counts['final_dithered_world_texels'] != sum(
        row['textured_modulated_dithered'] for row in base['ordered_dither_bayer_ranks']
    ):
        raise ValueError('sourceaudit/dither-reference final tracked count mismatch')
    if counts['actual_black_undithered_black']+counts['actual_black_undithered_nonblack'] != (
        base['counts']['final_black_dither_enabled']
    ):
        raise ValueError('sourceaudit and no-dither black counts disagree')
    ranks=[
        {'rank':i,'tracked':tracked_ranks[i],
         'different':changed_ranks[i],
         'dither_created_black':black_created_ranks[i],
         'difference_rate_percent':round(100*changed_ranks[i]/tracked_ranks[i],3)
            if tracked_ranks[i] else None}
        for i in range(16)]
    regions={
        'dither_created_black':connected_regions(
            created_black_mask,w,bounds,min_region_pixels,max_regions),
        'black_even_without_dither_last_write':connected_regions(
            black_without_dither_mask,w,bounds,min_region_pixels,max_regions),
        'any_dither_color_change':connected_regions(
            changed_mask,w,bounds,min_region_pixels,max_regions),
    }
    return {
        'capture':str(prefix),
        'source_size':[w,h],
        'roi':list(bounds),
        'counts':counts,
        'dither_created_black_share_percent':round(
            100*counts['actual_black_undithered_nonblack'] /
            (counts['actual_black_undithered_black']+
             counts['actual_black_undithered_nonblack']),3)
            if (counts['actual_black_undithered_black']+
                counts['actual_black_undithered_nonblack']) else None,
        'top_indexed_dither_created_black':[
            {'source_index':i,'pixels':v}
            for i,v in sorted(created_indices.items(),
                              key=lambda row:(-row[1],row[0]))[:top_indices]],
        'bayer_ranks':ranks,
        'regions':regions,
        'interpretation':(
            'The no-dither reference uses the SAME source texel, RGB modulation, '
            'current drawing state, and actual previously drawn framebuffer for '
            'any alpha blending, but changes ONLY ordered dithering of the '
            'last successful MSI world-texture write. This is NOT a global '
            'undithered replay of all overdraw. It cannot prove the native '
            'Mystique implementation uses a different threshold matrix.'
        ),
    }


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix',type=Path)
    parser.add_argument('--roi',help='half-open x0,y0,x1,y1 in source pixels')
    parser.add_argument('--min-region-pixels',type=int,default=8)
    parser.add_argument('--max-regions',type=int,default=8)
    parser.add_argument('--top-indices',type=int,default=15)
    parser.add_argument('--json',action='store_true')
    args=parser.parse_args()
    try:
        out=analyze(args.prefix,args.roi,args.min_region_pixels,
                    args.max_regions,args.top_indices)
    except (ValueError,OSError) as exc:
        parser.error(str(exc))
    if args.json:
        print(json.dumps(out,indent=2,sort_keys=True))
    else:
        print(out['capture'])
        for key,value in out['counts'].items():
            print(f'  {key}: {value}')
        print(f"  dither_created_black_share_percent: {out['dither_created_black_share_percent']}")
        print('Most affected source indices:')
        for item in out['top_indexed_dither_created_black']:
            print(f"  {item['source_index']}: {item['pixels']}")
        print(out['interpretation'])
    return 0

if __name__=='__main__':
    raise SystemExit(main())
