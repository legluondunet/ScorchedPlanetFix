#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only F12 fifth-layer shadow overlay; never diagnoses missing artwork alone.

The optional _shadow.bmp is produced only by diagnostic builds newer than
the four-layer F12 capture. Pixel classes represent the LAST SUCCESSFUL MSI
write. An opaque dark flat without a recognized textured underlay is a
candidate for a visible stain, not proof of a bug: the world may intentionally
draw untextured background geometry or write the framebuffer via the CPU.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from analyze_f12_capture import (
    COVERAGE_RGB565, connected_regions, decode_debug_pixels,
    parse_roi, read_rgb565_bmp, resolve_inputs,
)

SHADOW = {
    0: 'other_final_msi_write',
    1: 'blended_with_textured_world_underlay',
    2: 'opaque_no_recorded_textured_underlay',
    3: 'other_dark_flat_with_textured_underlay',
    4: 'other_dark_flat_no_recorded_underlay',
    5: 'unusual_blended_shadow_sentinel',
}
SHADOW_RGB565 = (0x0000, 0x07e0, 0xf800, 0xffe0, 0x07ff, 0xf81f)


def analyze(prefix: Path, roi: str | None = None, min_region_pixels: int = 8,
            max_regions: int = 8) -> dict:
    if min_region_pixels < 1 or not 1 <= max_regions <= 32:
        raise ValueError('min-region-pixels must be >=1 and max-regions in [1,32]')
    quartet = resolve_inputs(prefix)
    shadow_file = prefix.parent / f'{prefix.name}_shadow.bmp'
    if not shadow_file.is_file():
        raise ValueError(f'{shadow_file}: F12 fifth shadow layer absent (old build?)')
    layers = {key: read_rgb565_bmp(path) for key, path in quartet.items()}
    layers['shadow'] = read_rgb565_bmp(shadow_file)
    width, height, frame = layers['frame']
    if any((w, h) != (width, height)
           for w, h, _ in layers.values()):
        raise ValueError('Five F12 diagnostic bitmaps must have the same dimensions')
    coverage = decode_debug_pixels(layers['coverage'][2], COVERAGE_RGB565,
                                   'coverage')
    shadow = decode_debug_pixels(layers['shadow'][2], SHADOW_RGB565, 'shadow')
    bounds = parse_roi(roi, width, height) if roi else (0, 0, width, height)
    x0, y0, x1, y1 = bounds
    indices = [y * width + x for y in range(y0, y1) for x in range(x0, x1)]
    counts = {name: 0 for name in SHADOW.values()}
    for i in indices:
        counts[SHADOW[shadow[i]]] += 1
    incongruent = sum(shadow[i] != 0 and coverage[i] not in (3, 4)
                      for i in indices)
    if incongruent:
        raise ValueError(f'{incongruent} shadow final-write pixels have '
                         'non-flat MSI ownership; diagnostic layers mismatch')
    regions = {}
    for category in (1, 2, 3, 4):
        mask = bytearray(1 if value == category else 0 for value in shadow)
        regions[SHADOW[category]] = connected_regions(
            mask, width, bounds, min_region_pixels, max_regions)
    return {
        'capture': str(prefix),
        'source_size': [width, height],
        'roi': list(bounds),
        'shadow_final_write_counts': counts,
        'shadow_final_rgb565_black': sum(
            shadow[i] != 0 and frame[i] == 0 for i in indices),
        'shadow_provenance_coverage_conflicts': incongruent,
        'four_connected_regions': regions,
        'interpretation': (
            'This is final-write attribution, not a pixel history. '
            'Rejected/repeated shadow attempts and overwritten shadows '
            'are absent. No textured underlay recorded does not mean '
            'no original geometry nor an error in the Mystique driver.'
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', type=Path)
    parser.add_argument('--roi', help='x0,y0,x1,y1 native-pixel rectangle')
    parser.add_argument('--min-region-pixels', type=int, default=8)
    parser.add_argument('--max-regions', type=int, default=8)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    try:
        out = analyze(args.prefix, args.roi, args.min_region_pixels,
                      args.max_regions)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if args.json:
        print(json.dumps(out, indent=2, sort_keys=True))
    else:
        print(f"{out['capture']} ROI={out['roi']}")
        for name, count in out['shadow_final_write_counts'].items():
            print(f'  {name}: {count} pixels')
        for name, region in out['four_connected_regions'].items():
            print(f'  {name}: {region["components_above_minimum"]} regions; '
                  f'largest={region["reported"][:3]}')
        print('NOTE:', out['interpretation'])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
