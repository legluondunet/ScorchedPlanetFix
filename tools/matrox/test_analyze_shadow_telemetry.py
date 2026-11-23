#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic-only conservation and wrapping tests; no game media or logs."""
import tempfile
import unittest
from pathlib import Path

from analyze_shadow_telemetry import analyze, delta32, UINT32


class ShadowTelemetryTests(unittest.TestCase):
    @staticmethod
    def snapshot(frame, elapsed, candidates, blended, no_base, repeated,
                 resets=0, opaque=0):
        return (
            f"SHADOW_BLEND dark_flat_pixels= {candidates} blended_pixels= {blended}\n"
            f"SHADOW_ONCE blended_pixels= {blended} repeated_samples= {repeated}\n"
            f"SHADOW_ONCE untextured_base= {no_base} other_surface_resets= {resets}\n"
            f"SHADOW_UNDERLAY candidates_with_texture= {blended} no_texture= {no_base}\n"
            f"SHADOW_UNDERLAY opaque_texture= {opaque} transparent_texture= {blended-opaque}\n"
            f"PERF_WALL elapsed_ms= {elapsed} frames= {frame}\n"
            f"PERF_COST render_ms= 15 gdi_ms= 2\n"
            f"PERF_COST state_ms= 0 triangles= {frame * 3}\n"
        )

    def check_log(self, body):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/"test.log"
            path.write_text(body, encoding="utf-8")
            return analyze(path)

    def test_long_session_uint32_wrap_conserves_three_categories(self):
        # A real accepted delta crosses UINT32_MAX between two 300-frame snapshots.
        base = UINT32-100
        report=self.check_log(
            "SHADOW_ONCE enabled (experimental 50pct dark-flat)\n"+
            self.snapshot(300,10000,base,base//2,base//2-20,20)+
            self.snapshot(600,20000,250,base//2+120,base//2+130,0,
                          resets=20))
        self.assertEqual(report['shadow_once'],'enabled')
        self.assertEqual(report['valid_intervals'],1)
        self.assertEqual(report['conserved_intervals'],1)
        self.assertEqual(report['uint32_rollovers']['candidates'],1)
        self.assertEqual(report['total_sample_deltas']['candidates'],350)
        self.assertEqual(report['total_sample_deltas']['blend_once'],120)
        self.assertEqual(report['total_sample_deltas']['no_base'],150)
        self.assertEqual(report['total_sample_deltas']['repeated'],80)

    def test_clean_ordinary_samples(self):
        report=self.check_log(
            "SHADOW_ONCE enabled (test)\n"+
            self.snapshot(300,10000,1000,400,580,20)+
            self.snapshot(600,20000,1300,500,750,50))
        self.assertEqual(report['valid_intervals'],1)
        self.assertEqual(report['conserved_intervals'],1)
        self.assertAlmostEqual(report['no_base_percent'],56.6667,places=2)
        self.assertEqual(report['total_sample_deltas']['repeated'],30)
        self.assertEqual(report['first_disagreement'],None)

    def test_shadow_invariant_failure_is_reported_not_ignored(self):
        report=self.check_log(
            self.snapshot(300,10000,1000,400,580,20)+
            self.snapshot(600,20000,1300,500,749,50))
        self.assertEqual(report['valid_intervals'],1)
        self.assertEqual(report['conserved_intervals'],0)
        self.assertEqual(report['first_disagreement']['to_frame'],600)

    def test_ambiguous_counter_reset_rejected(self):
        report=self.check_log(
            self.snapshot(300,10000,1000,400,580,20)+
            self.snapshot(600,20000,50,10,40,0))
        self.assertEqual(report['valid_intervals'],0)
        self.assertEqual(report['invalid_intervals'],[[300,600]])
        self.assertEqual(report['total_sample_deltas']['candidates'],0)

    def test_unfinished_sample_not_accepted(self):
        report=self.check_log(
            self.snapshot(300,10000,1000,400,580,20)+
            "PERF_WALL elapsed_ms=12000 frames=600\n")
        self.assertEqual(report['snapshots'],1)
        self.assertEqual(report['total_checked_intervals'],0)

    def test_no_counter_samples(self):
        report=self.check_log("SHADOW_ONCE disabled (reference)\n"
                              "PERF_WALL elapsed_ms= 100 frames= 2\n"
                              "PERF_COST state_ms= 0 triangles= 2\n")
        self.assertEqual(report['snapshots'],0)
        self.assertEqual(report['shadow_once'],'disabled')
        self.assertIsNone(report['no_base_percent'])

    def test_rollover_only_near_boundary(self):
        self.assertEqual(delta32(UINT32-10,15),(25,True))
        self.assertEqual(delta32(123,100),None)
        self.assertEqual(delta32(0,100),(100,False))


if __name__ == "__main__":
    unittest.main()
