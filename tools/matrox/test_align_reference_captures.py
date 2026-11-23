#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic image registration smoke; no original game assets are bundled."""
import unittest

import cv2
import numpy as np

from align_reference_captures import AlignmentError, analyze, parse_box, select_crop


def fixture() -> np.ndarray:
    rng = np.random.default_rng(190700)
    a = rng.integers(10, 170, size=(400, 640, 3), dtype=np.uint8)
    for i in range(72):
        x = int(rng.integers(28, 590))
        y = int(rng.integers(25, 375))
        c = tuple(int(x) for x in rng.integers(20, 251, size=3))
        cv2.circle(a, (x, y), int(rng.integers(3, 12)), c, -1)
    cv2.rectangle(a,(184,63),(454,335),(20,20,245),5)
    cv2.putText(a, 'F12 1996', (210,200),cv2.FONT_HERSHEY_SIMPLEX,1.6,
                (240,210,60),3)
    return a


class MatroxReferenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.a = fixture()
        cls.matrix = np.float32([[1.095,0,16],[0,1.095,.4]])
        cls.b = cv2.warpAffine(cls.a,cls.matrix,(640,400),
                               flags=cv2.INTER_LINEAR)

    def test_detects_zoom_and_offset_from_synthetic_images(self):
        result, aligned, valid = analyze(self.a,self.b,
                                         panel='184,63,455,336',band=12)
        self.assertAlmostEqual(result['scale'],1.095,places=2)
        self.assertAlmostEqual(result['candidate_to_reference_transform'][0][2],
                               16.,delta=2)
        self.assertGreater(result['ransac_inliers'],100)
        self.assertGreater(result['inlier_fraction'],.85)
        self.assertGreater(result['full_frame']['overlap_pixels'],170000)
        self.assertIn('left',result['panel_border']['sides'])
        self.assertEqual(aligned.shape,self.a.shape)
        self.assertEqual(valid.shape,self.a.shape[:2])

    def test_local_cursor_is_not_a_reason_to_reject_geometry(self):
        b=self.b.copy()
        cv2.circle(b,(300,220),18,(255,255,255),-1)
        result,_,_ = analyze(self.a,b)
        self.assertAlmostEqual(result['scale'],1.095,places=2)
        self.assertGreater(result['ransac_inliers'],100)
        self.assertGreater(result['full_frame']['fraction_above_40'],0)

    def test_reject_unrelated_or_featureless_scene(self):
        with self.assertRaises(AlignmentError):
            analyze(np.zeros_like(self.a),self.b)
        with self.assertRaises(AlignmentError):
            analyze(self.a,np.zeros_like(self.b))
        unrelated=np.random.default_rng(42).integers(0,256,self.a.shape,dtype=np.uint8)
        with self.assertRaises(AlignmentError):
            analyze(self.a,unrelated)

    def test_reject_implausible_scale(self):
        too_large=cv2.warpAffine(self.a,np.float32([[1.40,0,0],[0,1.40,0]]),
                                 (640,400))
        with self.assertRaises(AlignmentError):
            analyze(self.a,too_large)

    def test_reject_invalid_registration_confidence_or_image_channels(self):
        with self.assertRaises(AlignmentError):
            analyze(self.a, self.b, min_matches=0)
        with self.assertRaises(AlignmentError):
            analyze(self.a, self.b, min_inlier_fraction=1.5)
        with self.assertRaises(AlignmentError):
            analyze(self.a[:, :, 0], self.b)
        with self.assertRaises(AlignmentError):
            analyze(self.a, self.b, band=0)

    def test_bounded_crop_and_border(self):
        self.assertEqual(parse_box('1,2,100,80',self.a.shape),(1,2,100,80))
        self.assertEqual(select_crop(self.a,'1,2,100,80').shape,(78,99,3))
        for bad in ('x,0,50,50','1,2,3','0,0,700,400','10,10,1,2'):
            with self.assertRaises(AlignmentError):
                parse_box(bad,self.a.shape)
        with self.assertRaises(AlignmentError):
            analyze(self.a,self.b,panel='100,100,108,105',band=10)


if __name__ == '__main__':
    unittest.main()
