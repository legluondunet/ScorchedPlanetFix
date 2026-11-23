#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare FINAL Z-plane/HUD ownership across existing MSI95 F12 RGB565 quartets.

This is an offline diagnostic, not an automatic missing-art detector.
Different viewpoints are allowed: compare colors ONLY where both frames
have last MSI Z-plane ownership. Last-attempt atlas provenance != final write.
No screenshots, original game assets, or rendering libraries are bundled.

Usage:
  python tools/matrox/compare_hud_captures.py captures/matrox_capture_0005 \
      captures/matrox_capture_0006 captures/matrox_capture_0007 --json
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from analyze_f12_capture import (
    ATLAS_RGB565, COVERAGE_RGB565, decode_debug_pixels,
    parse_roi, read_rgb565_bmp, resolve_inputs,
)

HUD_COVERAGE = frozenset((5, 6))
HUD_ATLAS_UNMARKED_ZERO_SKIP = 4
HUD_MISSING_TEXEL = 9
HUD_KEY_DISCARD_ONLY = 8


def load_capture(prefix: Path) -> dict:
    paths = resolve_inputs(prefix)
    layers = {name: read_rgb565_bmp(paths[name])
              for name in ('frame', 'coverage', 'atlas', 'world')}
    width, height, _ = layers['frame']
    if any((layer[0], layer[1]) != (width, height)
           for layer in layers.values()):
        raise ValueError(f'{prefix}: all four same-serial layers must have equal dimensions')
    return {
        # Include the directory: separate sessions often reuse capture_0001.
        # Two unrelated captures must never overwrite one per_capture entry.
        'name': str(prefix),
        'width': width,
        'height': height,
        'frame': layers['frame'][2],
        'coverage': decode_debug_pixels(layers['coverage'][2],
                                        COVERAGE_RGB565, 'coverage'),
        'atlas': decode_debug_pixels(layers['atlas'][2], ATLAS_RGB565, 'atlas'),
    }


def region_stats(capture: dict, indices: list[int]) -> dict:
    cov = capture['coverage']
    atlas = capture['atlas']
    frame = capture['frame']
    hud = sum(cov[i] in HUD_COVERAGE for i in indices)
    return {
        'hud_final_write_pixels': hud,
        'hud_black_final_pixels': sum(
            cov[i] in HUD_COVERAGE and frame[i] == 0 for i in indices),
        'atlas_last_zero_skip_over_hud_write': sum(
            cov[i] in HUD_COVERAGE and
            atlas[i] == HUD_ATLAS_UNMARKED_ZERO_SKIP for i in indices),
        'atlas_last_zero_skip_without_hud_write': sum(
            cov[i] not in HUD_COVERAGE and
            atlas[i] == HUD_ATLAS_UNMARKED_ZERO_SKIP for i in indices),
        'missing_texel_last_ownership': sum(
            cov[i] == HUD_MISSING_TEXEL for i in indices),
        'hud_key_discard_only': sum(
            cov[i] == HUD_KEY_DISCARD_ONLY for i in indices),
    }


def compare_one(base: dict, other: dict, indices: list[int]) -> dict:
    a, b = base['coverage'], other['coverage']
    af, bf = base['frame'], other['frame']
    count = len(indices)
    common = sum(a[i] in HUD_COVERAGE and b[i] in HUD_COVERAGE
                 for i in indices)
    baseline = sum(a[i] in HUD_COVERAGE for i in indices)
    test = sum(b[i] in HUD_COVERAGE for i in indices)
    color_diffs = sum(
        a[i] in HUD_COVERAGE and b[i] in HUD_COVERAGE and af[i] != bf[i]
        for i in indices)
    return {
        'compared_capture': other['name'],
        'roi_pixels': count,
        'baseline_hud_pixels': baseline,
        'compared_hud_pixels': test,
        'common_hud_pixels': common,
        'hud_mask_symmetric_difference': baseline + test - 2*common,
        'hud_mask_identical': baseline + test == 2*common,
        'baseline_only_hud_pixels': baseline-common,
        'compared_only_hud_pixels': test-common,
        'rgb565_color_diffs_on_common_hud_pixels': color_diffs,
        'rgb565_color_diffs_pct_on_common_hud': (
            round(100 * color_diffs / common, 4) if common else None),
    }


def analyze(prefixes: list[Path], roi: str | None = None) -> dict:
    if len(prefixes) < 2:
        raise ValueError('At least two F12 capture prefixes are required')
    if len({str(p) for p in prefixes}) != len(prefixes):
        raise ValueError('Duplicate F12 prefixes are not independent captures')
    caps = [load_capture(p) for p in prefixes]
    width, height = caps[0]['width'], caps[0]['height']
    if any((c['width'],c['height']) != (width,height) for c in caps):
        raise ValueError('F12 quartets must share source dimensions')
    bounds = parse_roi(roi, width, height) if roi else (0,0,width,height)
    x0,y0,x1,y1 = bounds
    indices = [y*width+x for y in range(y0,y1) for x in range(x0,x1)]
    baseline = caps[0]
    return {
        'dimensions': [width, height],
        'roi': list(bounds),
        'baseline': baseline['name'],
        'per_capture': {
            c['name']: region_stats(c, indices) for c in caps
        },
        'comparisons': [compare_one(baseline, c, indices)
                        for c in caps[1:]],
        'interpretation': [
            'HUD means last MSI Z-plane write (coverage class 5/6), not a proof of original hardware correctness.',
            'Different world viewpoints are allowed for HUD ownership; only common last-written HUD pixels are color-compared.',
            'Identical HUD ownership in several captures does NOT rule out a consistently missing element.',
            'Atlas provenance records the last ATTEMPT, so zero-skip over a final HUD write does not mean a missing texel.',
            'One CPU-written pixel without a final MSI write may still be visible; this report cannot prove missing artwork.',
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefixes', nargs='+', type=Path,
                        help='At least two matching native F12 quartet prefixes')
    parser.add_argument('--roi', help='x0,y0,x1,y1 native source coordinates, end-exclusive')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    try:
        data = analyze(args.prefixes, args.roi)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    if args.json:
        print(json.dumps(data, indent=2, sort_keys=True))
    else:
        print(f"F12 {data['dimensions'][0]}x{data['dimensions'][1]}, "
              f"ROI={data['roi']}; baseline={data['baseline']}")
        for capture, report in data['per_capture'].items():
            print(f"  {capture}: final_hud={report['hud_final_write_pixels']} "
                  f"atlas_last_zero_over_written_hud="
                  f"{report['atlas_last_zero_skip_over_hud_write']}")
        for row in data['comparisons']:
            print(f"  vs {row['compared_capture']}: "
                  f"HUD_mask_delta={row['hud_mask_symmetric_difference']} "
                  f"color_diffs_at_common_hud="
                  f"{row['rgb565_color_diffs_on_common_hud_pixels']} / "
                  f"{row['common_hud_pixels']}")
        print('CAUTION:', data['interpretation'][2])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
