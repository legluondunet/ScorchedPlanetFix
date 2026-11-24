#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only last successful MSI world texel-source attribution for F12.

Legacy _world.bmp tracks last attempted source (even rejected-key attempts).
The newer _texel.bmp tracks only the final successful MSI write for a pixel.
This script cross-checks that _texel, _frame and _coverage are consistent.
A black final textured pixel is not proof of wrong art or hardware behavior.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from analyze_f12_capture import (
    COVERAGE_RGB565, decode_debug_pixels, parse_roi,
    read_rgb565_bmp, resolve_inputs, connected_regions,
)

SOURCE_NAMES = (
    'other_final_msi_write',
    'index1_final_black',
    'index241_final_black',
    'index0_final_black',
    'other_index_final_black',
    'other_index_final_nonblack',
    'rgb16_final_black',
    'rgb16_final_nonblack',
    'rgb4_final_black',
    'rgb4_final_nonblack',
    'index1_final_nonblack',
    'index241_final_nonblack',
)
SOURCE_RGB565 = (
    0x0000, 0xFFE0, 0xF81F, 0x07FF, 0xF800, 0x07E0,
    0x8010, 0xAFE5, 0xFC00, 0x83E0, 0xFD20, 0x001F,
)
BLACK_SOURCE_CATEGORIES = frozenset((1,2,3,4,6,8))
NONBLACK_SOURCE_CATEGORIES = frozenset((5,7,9,10,11))

def analyze(prefix: Path, roi: str | None = None,
            min_region_pixels: int = 8, max_regions: int = 8) -> dict:
    if min_region_pixels < 1 or not 1 <= max_regions <= 32:
        raise ValueError('min-region-pixels >= 1 and max-regions in [1,32] required')
    inputs = resolve_inputs(prefix)
    texel_path = prefix.parent / f'{prefix.name}_texel.bmp'
    if not texel_path.is_file():
        raise ValueError('F12 sixth _texel.bmp layer absent; use updated build')
    needed = {key: read_rgb565_bmp(inputs[key]) for key in ('frame','coverage')}
    needed['texel'] = read_rgb565_bmp(texel_path)
    width,height,frame=needed['frame']
    if any((w,h)!=(width,height) for w,h,_ in needed.values()):
        raise ValueError('frame/coverage/texel layers must have same dimensions')
    coverage = decode_debug_pixels(needed['coverage'][2], COVERAGE_RGB565,
                                   'coverage')
    sources = decode_debug_pixels(needed['texel'][2], SOURCE_RGB565, 'texel')
    bounds = parse_roi(roi,width,height) if roi else (0,0,width,height)
    x0,y0,x1,y1=bounds
    indices=[y*width+x for y in range(y0,y1) for x in range(x0,x1)]
    counts={name:0 for name in SOURCE_NAMES}
    conflicts=[]
    for index in indices:
        kind=sources[index]
        counts[SOURCE_NAMES[kind]]+=1
        if kind!=0 and (coverage[index] not in (1,2) or
            (kind in BLACK_SOURCE_CATEGORIES)!=(frame[index]==0)):
            if len(conflicts)<12:
                conflicts.append({'x':index%width,'y':index//width,
                                  'source':SOURCE_NAMES[kind],
                                  'coverage':coverage[index],
                                  'frame_rgb565':f'0x{frame[index]:04x}'})
    if conflicts:
        raise ValueError(f'{len(conflicts)} final texel/coverage/RGB565 '
                         f'conflicts: {conflicts}')
    black_total=sum(frame[i]==0 and coverage[i]==2 for i in indices)
    source_black=sum(counts[SOURCE_NAMES[c]] for c in BLACK_SOURCE_CATEGORIES)
    if black_total!=source_black:
        raise ValueError(f'final world black coverage={black_total} but '
                         f'final source provenance black={source_black}; '
                         'possible missing diagnostic allocations')
    regions={}
    for key in BLACK_SOURCE_CATEGORIES:
        mask=bytearray(1 if s==key else 0 for s in sources)
        regions[SOURCE_NAMES[key]]=connected_regions(
            mask,width,bounds,min_region_pixels,max_regions)
    return {
        'capture':str(prefix),
        'source_size':[width,height],
        'roi':list(bounds),
        'counts':counts,
        'final_textured_black_pixels':black_total,
        'final_index1_black_share_percent':round(
            100*counts['index1_final_black']/black_total,3)
            if black_total else None,
        'regions':regions,
        'warning':('This identifies the last successful MSI writer only. '
                   'CPU framebuffer changes are not instrumented. RGB565 '
                   'black and source index 1 are not by themselves proof '
                   'that original Matrox driver would render differently.'),
    }

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix',type=Path)
    parser.add_argument('--roi',help='half-open x0,y0,x1,y1 in native pixels')
    parser.add_argument('--min-region-pixels',type=int,default=8)
    parser.add_argument('--max-regions',type=int,default=8)
    parser.add_argument('--json',action='store_true')
    args=parser.parse_args()
    try:
        result=analyze(args.prefix,args.roi,args.min_region_pixels,args.max_regions)
    except (OSError,ValueError) as e:
        parser.error(str(e))
    if args.json:
        print(json.dumps(result,sort_keys=True,indent=2))
    else:
        print(f"{result['capture']} final-world-black={result['final_textured_black_pixels']}")
        for name,count in result['counts'].items():
            if count:
                print(f'  {name}: {count}')
        print(result['warning'])
    return 0

if __name__=='__main__':
    raise SystemExit(main())
