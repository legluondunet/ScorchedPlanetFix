#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic-only interval parser regression; no real game media."""
import importlib.util
import tempfile
import unittest
from pathlib import Path

SOURCE = Path(__file__).with_name("analyze_render_intervals.py")
spec = importlib.util.spec_from_file_location("analyze_render_intervals", SOURCE)
analyzer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyzer)

class RenderIntervalTests(unittest.TestCase):
    def run_parser(self, body, minimum=60):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder, "synthetic_msi95.log")
            path.write_text(body, encoding="utf-8")
            return analyzer.analyze(path, minimum)

    @staticmethod
    def sample(frame, milliseconds, render, bilinear, late, keyed, unkeyed=0):
        return (f"RENDER_PROGRESS pixels= {frame * 100} missing_textures= 0\n"
                f"FPS_CAP late_frames= {late} fallback_frames= 0\n"
                f"WORLD_BILINEAR filtered_samples= {bilinear} changed_pixels= {bilinear // 2}\n"
                f"LARGE_Z_ATLAS zero_skips_color_keyed= {keyed} zero_skips_not_keyed= {unkeyed}\n"
                f"PERF_WALL elapsed_ms= {milliseconds} frames= {frame}\n"
                f"PERF_COST render_ms= {render} gdi_ms= 2\n"
                f"PERF_COST state_ms= 0 triangles= {frame * 10}\n")

    def test_consecutive_intervals_use_deltas_not_totals(self):
        log = (self.sample(300,10000,1000,100000,20,400)
               + self.sample(600,20000,4000,700000,120,3000)
               + self.sample(900,35000,7000,1900000,380,9000)
               + "msiExit total_frames= 1000 triangles= 8\n"
               + "SESSION_TIMING elapsed_ms= 36000 render_ms= 7500\n")
        report = self.run_parser(log)
        self.assertEqual(report["snapshot_last_frame"], 900)
        self.assertEqual(report["exit_frames"], 1000)
        self.assertEqual(report["session_ms"], 36000)
        a, b = report["intervals"]
        self.assertEqual(a["frames"], 300)
        self.assertEqual(a["observed_fps"], 30.0)
        self.assertEqual(a["render_ms_per_frame"], 10.0)
        self.assertEqual(a["bilinear_samples_per_frame"], 2000.0)
        self.assertEqual(a["late_frames_pct"], 33.3)
        self.assertEqual(a["deltas"]["zero_skips_color_keyed"], 2600)
        self.assertEqual(b["observed_fps"], 20.0)
        self.assertEqual(b["deltas"]["filtered_samples"], 1200000)
        self.assertEqual(b["deltas"]["zero_skips_not_keyed"], 0)

    def test_perf_cost_after_wall_marker_is_not_one_snapshot_late(self):
        # This is the true native MSI95 log order: wall first, render/state
        # counters follow it, then the next frame starts.
        report = self.run_parser(self.sample(300,10000,1000,30000,20,500)
                                 + self.sample(600,20000,4000,80000,120,2000))
        self.assertEqual(len(report["intervals"]), 1)
        self.assertEqual(report["intervals"][0]["render_ms_per_frame"], 10.0)
        self.assertEqual(report["intervals"][0]["deltas"]["render_ms"], 3000)

    def test_unfinished_wall_sample_does_not_reuse_previous_frame_cost(self):
        report = self.run_parser(self.sample(300,10000,1000,30000,20,500)
                                 + "PERF_WALL elapsed_ms=20000 frames=600\n")
        self.assertEqual(report["snapshots"], 1)
        self.assertEqual(report["snapshot_last_frame"], 300)

    def test_short_intervals_are_ignored(self):
        report = self.run_parser(self.sample(1,100,0,0,0,0) +
                                 self.sample(5,200,2,0,0,0))
        self.assertEqual(report["snapshots"], 2)
        self.assertEqual(report["intervals"], [])

    def test_counter_reset_not_a_valid_interval(self):
        report = self.run_parser(self.sample(300,1000,500,10000,10,1000) +
                                 self.sample(600,2000,100,20000,20,2000))
        self.assertEqual(report["intervals"], [])


    def test_uint32_raster_and_dither_wrap_with_distinct_bilinear_names(self):
        def event(frame, elapsed, render, raster, dither, diff, zpixels):
            return (
                f"RENDER_PROGRESS pixels= {raster} missing_textures= 0\\n"
                f"MSI_DITHER candidates= {dither} changed_pixels= {diff}\\n"
                f"MSI_MODULATE triangles= 300 pixels= {dither}\\n"
                f"RENDER_ZPLANES pixels= {zpixels} rejected= 0\\n"
                f"WORLD_BILINEAR filtered_samples= 400 changed_pixels= 100\\n"
                f"FPS_CAP late_frames= {frame // 2} fallback_frames= 0\\n"
                f"PERF_WALL elapsed_ms= {elapsed} frames= {frame}\\n"
                f"PERF_COST render_ms= {render} gdi_ms= 40\\n"
                f"PERF_COST state_ms= 30 triangles= 1000\\n"
            ).replace('\\\\n', '\\n')
        start = (1 << 32) - 80
        report = self.run_parser(
            event(300, 10000, 1000, start, start, start-100, start) +
            event(600, 20000, 2000, 120, 50, 100, 90))
        self.assertEqual(len(report['intervals']), 1)
        row = report['intervals'][0]
        self.assertEqual(row['deltas']['raster_pixels'], 200)
        self.assertEqual(row['deltas']['dither_candidates'], 130)
        self.assertEqual(row['deltas']['dither_changed_pixels'], 280)
        self.assertEqual(row['deltas']['zplane_pixels'], 170)
        self.assertEqual(row['deltas']['changed_pixels'], 0)
        self.assertEqual(set(row['counter_wraps']), {
            'raster_pixels', 'dither_candidates',
            'dither_changed_pixels', 'modulate_pixels',
            'zplane_pixels',
        })
        self.assertEqual(report['last_snapshot_extended']['raster_pixels'],
                         (1 << 32) + 120)
        self.assertEqual(report['counter_wraps']['raster_pixels'], 1)

    def test_ambiguous_uint32_drop_is_not_a_wrap(self):
        # The source did not approach UINT32_MAX, so a decrease is likely
        # a reset or an incomplete log. Do not silently add 2**32.
        report = self.run_parser(
            self.sample(300, 1000, 500, 700000, 10, 1000) +
            self.sample(600, 2000, 900, 300, 20, 2000))
        self.assertEqual(report['intervals'], [])
        self.assertEqual(report['last_snapshot_extended'], {})

    def test_short_initial_frames_still_count_in_extended_totals(self):
        log = ("RENDER_PROGRESS pixels= 5 missing_textures= 0\\n"
               "PERF_WALL elapsed_ms= 50 frames= 1\\n"
               "PERF_COST state_ms= 0 triangles= 1\\n"
               "RENDER_PROGRESS pixels= 1000 missing_textures= 0\\n"
               "PERF_WALL elapsed_ms= 60 frames= 2\\n"
               "PERF_COST state_ms= 0 triangles= 2\\n"
               "RENDER_PROGRESS pixels= 2000 missing_textures= 0\\n"
               "PERF_WALL elapsed_ms= 10000 frames= 300\\n"
               "PERF_COST state_ms= 0 triangles= 300\\n").replace('\\\\n', '\\n')
        report = self.run_parser(log)
        self.assertEqual(len(report['intervals']), 1)
        self.assertEqual(report['last_snapshot_extended']['raster_pixels'], 2000)

    def test_duplicate_wall_marker_skipped(self):
        report = self.run_parser(self.sample(300,1000,500,10000,10,1000) +
                                 self.sample(300,1000,500,10000,10,1000) +
                                 self.sample(600,2000,900,30000,20,2000))
        self.assertEqual(len(report["intervals"]), 1)

    def test_missing_counter_and_zero_wall_time(self):
        report = self.run_parser("PERF_WALL elapsed_ms=1000 frames=300\n"
                                 "PERF_COST state_ms=0 triangles=3000\n"
                                 "PERF_WALL elapsed_ms=1000 frames=600\n"
                                 "PERF_COST state_ms=0 triangles=6000\n"
                                 "PERF_WALL elapsed_ms=5000 frames=900\n"
                                 "PERF_COST state_ms=0 triangles=9000\n")
        self.assertEqual(len(report["intervals"]), 1)
        self.assertEqual(report["intervals"][0]["observed_fps"], 75.0)
        self.assertIsNone(report["intervals"][0]["render_ms_per_frame"])

    def test_error_on_invalid_minimum(self):
        with self.assertRaises(ValueError):
            self.run_parser("", minimum=0)

    def test_zplane_blue_skip_counter_does_not_collide(self):
        report = self.run_parser(self.sample(300,10000,1000,0,20,400) +
                                 "ZPLANE_BLUE_KEY rgb565=0x00FF skipped_pixels=100\n"
                                 "PERF_WALL elapsed_ms=12000 frames=400\n"
                                 "PERF_COST state_ms=0 triangles=4000\n" +
                                 "ZPLANE_BLUE_KEY rgb565=0x00FF skipped_pixels=250\n"
                                 "PERF_WALL elapsed_ms=14000 frames=500\n"
                                 "PERF_COST state_ms=0 triangles=5000\n", minimum=50)
        self.assertEqual(report["intervals"][1]["deltas"]["z_blue_skips"], 150)

if __name__ == "__main__":
    unittest.main()
