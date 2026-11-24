#!/usr/bin/env python3
"""Correlate four same-frame Matrox MSI95 F12 debug BMPs; never alters game assets.

The provenance layers are the LAST *attempted* sample; the coverage layer is
last MSI pixel ownership. Neither can prove that an original asset is absent.
Only RGB565 BI_BITFIELDS BMPs from the native F12 diagnostic are supported.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
from collections import Counter
from pathlib import Path

# Exact RGB565 debug palette constants from msi95_probe.c, not 24-bit RGB values.
COVERAGE = {
    0: 'no_msi_write', 1: 'world_texture_colored', 2: 'world_texture_black',
    3: 'world_flat_colored', 4: 'world_flat_black',
    5: 'zplane_colored', 6: 'zplane_black',
    7: 'world_key_discard_only', 8: 'zplane_key_discard_only',
    9: 'zplane_unmarked_zero_only',
}
COVERAGE_RGB565 = [0x0000, 0x07E0, 0xF800, 0x07FF, 0xF81F,
                   0xFFE0, 0xFC00, 0x001F, 0x07DF, 0x8410]
WORLD = {
    0: 'no_world_sample', 1: 'rgb16_draw', 2: 'indexed8_nonzero_draw',
    3: 'indexed8_zero_draw', 4: 'rgb16_key_discard',
    5: 'indexed_key_discard', 6: 'blue_fallback_discard',
    7: 'indexed4_draw', 8: 'indexed8_index0_rgbkey_discard',
    9: 'indexed8_index1_rgbkey_discard',
    10: 'indexed8_index241_rgbkey_discard',
    11: 'indexed8_other_rgbkey_discard',
    12: 'indexed8_index1_opaque_preview',
    13: 'indexed8_icon01_index241_opaque',
}
WORLD_RGB565 = [0x0000, 0x07E0, 0xAFE5, 0xFD20, 0x8010,
                0xF800, 0x001F, 0x07FF, 0xF81F, 0xFFE0,
                0xA01F, 0xFFFF, 0xFC00, 0x83E0]
ATLAS = {
    0: 'not_sampled', 1: 'uploaded_nonzero', 2: 'uploaded_zero',
    3: 'unmarked_nonzero', 4: 'unmarked_zero_skip', 5: 'key_discard',
}
ATLAS_RGB565 = [0x0000, 0x07E0, 0x07FF, 0xF81F, 0xF800, 0xFFE0]


def read_rgb565_bmp(path: Path) -> tuple[int, int, list[int]]:
    data = path.read_bytes()
    if len(data) < 66 or data[:2] != b'BM':
        raise ValueError(f'{path}: not a 66-byte-header RGB565 BMP')
    total_size = struct.unpack_from('<I', data, 2)[0]
    offset, dibsize = struct.unpack_from('<II', data, 10)
    width, height = struct.unpack_from('<ii', data, 18)
    planes, bits, compression = struct.unpack_from('<HHI', data, 26)
    masks = struct.unpack_from('<III', data, 54)
    if (dibsize != 40 or offset != 66 or planes != 1 or bits != 16 or
            compression != 3 or masks != (0xf800, 0x07e0, 0x001f) or
            width <= 0 or width > 4096 or height == 0 or abs(height) > 4096):
        raise ValueError(f'{path}: unsupported BMP encoding/dimensions/masks')
    h = abs(height)
    stride = ((width * 2 + 3) // 4) * 4
    if total_size != len(data) or len(data) != offset + stride * h:
        raise ValueError(f'{path}: BMP length/header mismatch')
    pixels = [0] * (width * h)
    for row in range(h):
        y = row if height < 0 else h - 1 - row
        start = offset + row * stride
        pixels[y * width:(y + 1) * width] = struct.unpack_from(f'<{width}H', data, start)
    return width, h, pixels


def decode_debug_pixels(pixels: list[int], palette: list[int], name: str) -> list[int]:
    by_rgb = {v: k for k, v in enumerate(palette)}
    if len(by_rgb) != len(palette):
        raise ValueError('internal diagnostic palette contains duplicate colors')
    unknown = set(pixels) - by_rgb.keys()
    if unknown:
        raise ValueError(f'{name}: unknown RGB565 diagnostic color: ' +
                         ', '.join(hex(x) for x in sorted(unknown)[:8]))
    return [by_rgb[v] for v in pixels]


def resolve_inputs(prefix: Path) -> dict[str, Path]:
    """Accept user-downloaded (1) filename suffixes; never guess across serials."""
    folder, stem = prefix.parent, prefix.name
    paths = {}
    for kind in ('frame', 'coverage', 'atlas', 'world'):
        matches = list(folder.glob(f'{stem}_{kind}.bmp')) + list(folder.glob(f'{stem}_{kind}(?).bmp'))
        if len(matches) != 1:
            raise ValueError(f'{stem}_{kind}: expected exactly one BMP, found {len(matches)}')
        paths[kind] = matches[0]
    return paths


def parse_roi(raw: str, width: int, height: int) -> tuple[int, int, int, int]:
    if not re.fullmatch(r'\d+,\d+,\d+,\d+', raw):
        raise ValueError('--roi must be x0,y0,x1,y1 (end-exclusive)')
    x0, y0, x1, y1 = (int(part) for part in raw.split(','))
    if not (0 <= x0 < x1 <= width and 0 <= y0 < y1 <= height):
        raise ValueError('--roi must be contained in the BMP dimensions')
    return x0, y0, x1, y1


def connected_regions(mask: bytearray, width: int,
                      box: tuple[int, int, int, int],
                      minimum: int = 8, maximum: int = 8) -> dict:
    """Four-connected suspect regions, in native 640x400 coordinates.

    A cluster of a *diagnostic category* is not proof of a missing asset:
    provenance is last ATTEMPT and MSI coverage is last WRITE only.
    """
    x0, y0, x1, y1 = box
    visited = bytearray(len(mask))
    regions = []
    total = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            root = y * width + x
            if not mask[root] or visited[root]:
                continue
            queue = [root]
            visited[root] = 1
            count = 0
            left = right = x
            top = bottom = y
            while queue:
                index = queue.pop()
                py, px = divmod(index, width)
                count += 1
                left = min(left, px)
                right = max(right, px)
                top = min(top, py)
                bottom = max(bottom, py)
                for nx, ny in ((px - 1, py), (px + 1, py),
                               (px, py - 1), (px, py + 1)):
                    if nx < x0 or nx >= x1 or ny < y0 or ny >= y1:
                        continue
                    neighbor = ny * width + nx
                    if mask[neighbor] and not visited[neighbor]:
                        visited[neighbor] = 1
                        queue.append(neighbor)
            total += 1
            if count >= minimum:
                regions.append({
                    'pixels': count,
                    'bbox': [left, top, right + 1, bottom + 1],
                    'touches_roi_boundary': (
                        left == x0 or top == y0 or right + 1 == x1 or
                        bottom + 1 == y1),
                })
    regions.sort(key=lambda item: (-item['pixels'], item['bbox'][1],
                                   item['bbox'][0]))
    return {
        'total_components_all_sizes': total,
        'components_above_minimum': len(regions),
        'min_pixels': minimum,
        'reported': regions[:maximum],
        'unreported_above_minimum': max(0, len(regions) - maximum),
    }


# Last-attempt provenance and last-write coverage may describe DIFFERENT
# polygons. Each intersection below is a location of interest, NOT a verdict.
REGION_CATEGORIES = {
    'visible_world_index1_black_preview': (
        'RGB565 black final pixel, last world sample index1-opaque, '
        'last MSI write textured black'),
    'world_index241_keyed_only': (
        'last index241 world sample was keyed, no later MSI write '
        '(other game-side framebuffer writes are possible)'),
    'world_index1_keyed_only': (
        'last index1 world sample was keyed, no later MSI write'),
    'world_other_rgb_keyed_only': (
        'other indexed world sample keyed, no later MSI write'),
    'zplane_unmarked_zero_black_unowned': (
        'last sparse atlas sample was unmarked zero, final black pixel '
        'without a later MSI write; may be intentional transparent space'),
    'black_without_final_msi_write': (
        'black framebuffer with no final MSI write: often intentional '
        'background, never proof of missing art'),
}


def analyze(files: dict[str, Path], roi: str | None = None,
            regions: bool = False, min_region_pixels: int = 8,
            max_regions: int = 8) -> dict:
    if not 1 <= min_region_pixels <= 256000:
        raise ValueError('min_region_pixels must be between 1 and 256000')
    if not 1 <= max_regions <= 32:
        raise ValueError('max_regions must be between 1 and 32')
    result = {kind: read_rgb565_bmp(files[kind])
              for kind in ('frame', 'coverage', 'atlas', 'world')}
    w, h, frame = result['frame']
    if any((x[0], x[1]) != (w, h) for x in result.values()):
        raise ValueError('F12 quartet dimension mismatch; combine the same captured frame')
    coverage = decode_debug_pixels(result['coverage'][2], COVERAGE_RGB565, 'coverage')
    atlas = decode_debug_pixels(result['atlas'][2], ATLAS_RGB565, 'atlas')
    world = decode_debug_pixels(result['world'][2], WORLD_RGB565, 'world')
    box = (0, 0, w, h) if roi is None else parse_roi(roi, w, h)
    x0, y0, x1, y1 = box
    indices = (y*w+x for y in range(y0, y1) for x in range(x0, x1))
    cc, ac, wc = Counter(), Counter(), Counter()
    spatial_masks = ({key: bytearray(w * h) for key in REGION_CATEGORIES}
                     if regions else None)
    black = 0
    suspect_black_unowned = 0
    atlas_zero_on_final_colored = 0
    atlas_zero_on_final_black = 0
    atlas_zero_covered = Counter()
    for i in indices:
        c, a, src = coverage[i], atlas[i], world[i]
        if spatial_masks is not None:
            is_black = frame[i] == 0
            unowned = c in (0, 7, 8, 9)
            spatial_masks['visible_world_index1_black_preview'][i] = (
                is_black and c == 2 and src == 12)
            spatial_masks['world_index241_keyed_only'][i] = (
                c == 7 and src == 10)
            spatial_masks['world_index1_keyed_only'][i] = (
                c == 7 and src == 9)
            spatial_masks['world_other_rgb_keyed_only'][i] = (
                c == 7 and src == 11)
            spatial_masks['zplane_unmarked_zero_black_unowned'][i] = (
                is_black and unowned and a == 4)
            spatial_masks['black_without_final_msi_write'][i] = (
                is_black and unowned)
        cc[COVERAGE[c]] += 1
        ac[ATLAS[a]] += 1
        wc[WORLD[src]] += 1
        if frame[i] == 0:
            black += 1
            if c in (0, 7, 8, 9):
                suspect_black_unowned += 1
        if a == 4:  # Last *attempted* atlas sample skipped, not necessarily a hole.
            atlas_zero_covered[COVERAGE[c]] += 1
            if frame[i]:
                atlas_zero_on_final_colored += 1
            else:
                atlas_zero_on_final_black += 1
    count = (x1 - x0) * (y1 - y0)
    spatial_report = None
    if spatial_masks is not None:
        spatial_report = {
            key: {
                'meaning': REGION_CATEGORIES[key],
                'candidate_pixels_in_roi': sum(mask),
                **connected_regions(mask, w, box, min_region_pixels,
                                    max_regions),
            }
            for key, mask in spatial_masks.items()
        }
    report = {
        'dimensions': [w, h], 'roi': list(box), 'pixels': count,
        'frame': {'black': black, 'colored': count - black,
                  'black_without_final_msi_write': suspect_black_unowned},
        'coverage': dict(sorted(cc.items())),
        'atlas_last_attempt': dict(sorted(ac.items())),
        'world_last_attempt': dict(sorted(wc.items())),
        'atlas_unmarked_zero_crosscheck': {
            'last_attempt_skips': ac['unmarked_zero_skip'],
            'final_frame_colored': atlas_zero_on_final_colored,
            'final_frame_black': atlas_zero_on_final_black,
            'final_msi_ownership': dict(sorted(atlas_zero_covered.items()))
        },
        'interpretation': [
            'World/atlas provenance classify the LAST ATTEMPTED sample, not final pixel ownership.',
            'Coverage shows last MSI write; no MSI write does not prove framebuffer unchanged (CPU writes exist).',
            'Black-without-final-MSI-write is a suspect location, NOT proof of missing graphics.',
            'An atlas unmarked-zero skip may be color-keyed and a later triangle may cover its pixel.',
            'F12 BMPs alone do not establish original Matrox hardware equivalence.',
        ],
    }
    if spatial_report is not None:
        report['spatial_regions'] = spatial_report
    return report


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('prefix', type=Path, help='Path through serial, e.g. captures/matrox_capture_0003')
    p.add_argument('--roi', help='End-exclusive x0,y0,x1,y1 to inspect panel or portrait region')
    p.add_argument('--json', action='store_true', help='Print machine-readable JSON')
    p.add_argument('--regions', action='store_true',
                   help='Rank bounded four-connected suspect pixel regions')
    p.add_argument('--min-region-pixels', type=int, default=8,
                   help='Minimum pixel count per reported region (default 8)')
    p.add_argument('--max-regions', type=int, default=8,
                   help='Maximum listed regions per category (default 8, max 32)')
    args = p.parse_args()
    try:
        report = analyze(resolve_inputs(args.prefix), args.roi,
                         regions=args.regions,
                         min_region_pixels=args.min_region_pixels,
                         max_regions=args.max_regions)
    except (ValueError, OSError, struct.error) as exc:
        p.error(str(exc))
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0
    print(f"F12 {args.prefix.name}: {report['dimensions'][0]}x{report['dimensions'][1]}, "
          f"ROI={report['roi']}, analyzed={report['pixels']}")
    print('Actual framebuffer:', report['frame'])
    print('Last MSI draw ownership:', report['coverage'])
    print('Last sampled world texel:', report['world_last_attempt'])
    print('Last sampled Z-plane atlas:', report['atlas_last_attempt'])
    print('Skipped atlas zero vs actual framebuffer / ownership:',
          report['atlas_unmarked_zero_crosscheck'])
    if args.regions:
        print('Candidate connected regions (end-exclusive RGB565 coordinates):')
        for category, detail in report['spatial_regions'].items():
            print(f"  {category}: candidate_pixels={detail['candidate_pixels_in_roi']}"
                  f" components={detail['total_components_all_sizes']}")
            for region in detail['reported']:
                print(f"    pixels={region['pixels']} bbox={region['bbox']}"
                      f" roi_edge={region['touches_roi_boundary']}")
        print('CAUTION: connected candidate regions are NOT confirmed missing art.')
    print('CAUTION:', report['interpretation'][0])
    print('CAUTION:', report['interpretation'][2])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
