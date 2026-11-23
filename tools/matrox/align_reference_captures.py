#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare image geometry before diagnosing missing Matrox border fragments.

Requires: pip install opencv-python-headless numpy
No game files are modified, and no screenshot is included in this repository.
The estimated similarity maps candidate pixels into reference coordinates.
Registration is not proof of original hardware pixel parity.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import cv2
import numpy as np


class AlignmentError(ValueError):
    pass


def parse_box(text: str, shape: tuple[int, ...]) -> tuple[int, int, int, int]:
    try:
        x0, y0, x1, y1 = (int(value) for value in text.split(','))
    except (ValueError, TypeError):
        raise AlignmentError('ROI must be x0,y0,x1,y1 (end-exclusive)') from None
    height, width = shape[:2]
    if not (0 <= x0 < x1 <= width and 0 <= y0 < y1 <= height):
        raise AlignmentError(f'ROI {text} is outside the {width}x{height} source')
    return (x0, y0, x1, y1)


def select_crop(img: np.ndarray, crop: str | None) -> np.ndarray:
    if crop is None:
        return img
    x0, y0, x1, y1 = parse_box(crop, img.shape)
    return img[y0:y1, x0:x1].copy()


def summarize(candidate: np.ndarray, aligned: np.ndarray,
              valid: np.ndarray, roi: tuple[int, int, int, int]) -> dict:
    x0, y0, x1, y1 = roi
    use = valid[y0:y1, x0:x1]
    if use.sum() < 100:
        raise AlignmentError('ROI has fewer than 100 valid overlapping pixels')
    absolute = np.abs(candidate[y0:y1, x0:x1].astype(np.int16) -
                      aligned[y0:y1, x0:x1].astype(np.int16))
    selection = absolute[use]
    return {
        'overlap_pixels': int(use.sum()),
        'mean_absolute_channel_difference': round(float(selection.mean()), 3),
        'pixels_any_channel_difference_above_40': int((selection.max(axis=1) > 40).sum()),
        'fraction_above_40': round(float((selection.max(axis=1) > 40).mean()), 5),
    }


def analyze(candidate: np.ndarray, reference: np.ndarray,
            panel: str | None = None, band: int = 10,
            min_matches: int = 25, min_inlier_fraction: float = .65
            ) -> tuple[dict, np.ndarray, np.ndarray]:
    if candidate.ndim != 3 or reference.ndim != 3 or (
            candidate.shape[2] != 3 or reference.shape[2] != 3):
        raise AlignmentError('Expected three-channel BGR images')
    if band < 1 or min_matches < 6 or not 0.0 < min_inlier_fraction <= 1.0:
        raise AlignmentError('Invalid band or registration confidence threshold')
    detector = cv2.SIFT_create(nfeatures=2500)
    a_kp, a_desc = detector.detectAndCompute(
        cv2.cvtColor(candidate, cv2.COLOR_BGR2GRAY), None)
    b_kp, b_desc = detector.detectAndCompute(
        cv2.cvtColor(reference, cv2.COLOR_BGR2GRAY), None)
    if a_desc is None or b_desc is None:
        raise AlignmentError('Insufficient distinctive image features')
    pairs = cv2.BFMatcher(cv2.NORM_L2).knnMatch(a_desc, b_desc, k=2)
    good = [m for pair in pairs if len(pair) == 2
            for m, n in [pair] if m.distance < .70 * n.distance]
    if len(good) < min_matches:
        raise AlignmentError(f'Only {len(good)} reliable feature matches')
    src = np.float32([a_kp[m.queryIdx].pt for m in good])
    dst = np.float32([b_kp[m.trainIdx].pt for m in good])
    matrix, inliers = cv2.estimateAffinePartial2D(
        src, dst, method=cv2.RANSAC, ransacReprojThreshold=2.0,
        confidence=.995, maxIters=5000, refineIters=15)
    if matrix is None or inliers is None:
        raise AlignmentError('No supported similarity transform')
    accepted = int(inliers.sum())
    fraction = accepted / len(good)
    if accepted < min_matches or fraction < min_inlier_fraction:
        raise AlignmentError('Images may be different scenes; registration rejected')
    scale = math.hypot(float(matrix[0, 0]), float(matrix[1, 0]))
    rotation = math.degrees(math.atan2(float(matrix[1, 0]),
                                       float(matrix[0, 0])))
    if not (.75 <= scale <= 1.25 and abs(rotation) <= 3.0):
        raise AlignmentError('Implausible zoom/rotation for a Matrox screenshot comparison')
    inv = cv2.invertAffineTransform(matrix)
    h, w = candidate.shape[:2]
    aligned = cv2.warpAffine(reference, inv, (w, h),
                             flags=cv2.INTER_LINEAR)
    valid = cv2.warpAffine(
        np.ones(reference.shape[:2], dtype=np.uint8), inv, (w, h),
        flags=cv2.INTER_NEAREST).astype(bool)
    full = summarize(candidate, aligned, valid, (0, 0, w, h))
    report = {
        'candidate_size': [w, h],
        'reference_size': [int(reference.shape[1]), int(reference.shape[0])],
        'candidate_to_reference_transform': [
            [round(float(v), 6) for v in row] for row in matrix],
        'scale': round(scale, 6),
        'rotation_degrees': round(rotation, 5),
        'good_feature_matches': len(good),
        'ransac_inliers': accepted,
        'inlier_fraction': round(fraction, 5),
        'full_frame': full,
        'interpretation': (
            'Images were normalized geometrically. Differences include '
            'image resampling, movement and compression; similarity does not '
            'establish original hardware raster parity or absent triangles.'
        ),
    }
    if panel is not None:
        x0, y0, x1, y1 = parse_box(panel, candidate.shape)
        if (x1 - x0) < 2*band or (y1-y0) < 2*band:
            raise AlignmentError('Panel too small for the requested border band')
        regions = {
            'top': (x0, y0, x1, y0 + band),
            'bottom': (x0, y1 - band, x1, y1),
            'left': (x0, y0 + band, x0 + band, y1 - band),
            'right': (x1-band, y0 + band, x1, y1 - band),
        }
        report['panel_border'] = {
            'roi': [x0,y0,x1,y1],
            'band_pixels': band,
            'sides': {name: summarize(candidate, aligned, valid, box)
                      for name,box in regions.items()},
        }
    return report, aligned, valid


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--candidate-crop', help='x0,y0,x1,y1 source rectangle')
    parser.add_argument('--reference-crop', help='x0,y0,x1,y1 source rectangle')
    parser.add_argument('--panel', help='Border rectangle in candidate coordinates')
    parser.add_argument('--border-width', type=int, default=10)
    parser.add_argument('--overlay', type=Path,
                        help='Optional three-column image for local human review')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    try:
        a = cv2.imread(str(args.candidate), cv2.IMREAD_COLOR)
        b = cv2.imread(str(args.reference), cv2.IMREAD_COLOR)
        if a is None or b is None:
            raise AlignmentError('Cannot read candidate/reference images')
        a = select_crop(a, args.candidate_crop)
        b = select_crop(b, args.reference_crop)
        report, aligned, valid = analyze(a, b, args.panel, args.border_width)
        if args.overlay:
            h,w = a.shape[:2]
            combined = np.zeros((h+35, w*3, 3), dtype=np.uint8)
            for i,(img,label) in enumerate((
                (a,'Candidate'),(aligned,'Reference aligned'),
                (cv2.addWeighted(a,.5,aligned,.5,0),'50/50 overlay'))):
                combined[35:,i*w:(i+1)*w] = img
                cv2.putText(combined,label,(i*w+8,24),
                            cv2.FONT_HERSHEY_SIMPLEX,.55,(255,255,255),1)
            if not cv2.imwrite(str(args.overlay), combined):
                raise AlignmentError('Cannot write overlay')
    except AlignmentError as error:
        parser.error(str(error))
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"Scale={report['scale']:.4f}, rotation={report['rotation_degrees']:.3f}°; "
              f"inliers={report['ransac_inliers']}/{report['good_feature_matches']}")
        print('Full-frame normalized comparison:',report['full_frame'])
        if 'panel_border' in report:
            print('Panel border:', report['panel_border']['sides'])
        print('CAUTION:',report['interpretation'])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
