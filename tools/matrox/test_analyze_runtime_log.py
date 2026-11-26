#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic test cases only: no game content or user's real logs in repo."""
import importlib.util
import tempfile
import unittest
from pathlib import Path

SOURCE = Path(__file__).with_name("analyze_runtime_log.py")
spec = importlib.util.spec_from_file_location("analyze_runtime_log", SOURCE)
analyzer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyzer)

class RuntimeLogTests(unittest.TestCase):
    def parse(self, body):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "ScorchedPlanetFix.log")
            path.write_text(body, encoding="utf-8")
            return analyzer.parse_log(path)

    GPU_SESSION = """msiInit width=640
PREVIEW_RENDERER requested=vulkan (experimental)
PREVIEW_RENDERER active=vulkan (CPU raster)
GPU_SHADOW_AUTO enabled sample=1/120 max_passes=1
GPU_BYPASS pilot max_triangles=1
GPU_CPU_REPLAY PASS frame=120 draws=8 exact=256000 mask_diff=0 written=100
GPU_SHADOW PASS frame=120 draws=8 active=90 exact=88 one_lsb=1 edge=1 unexplained=0 outside_bounds=0 scope=auto
GPU_TAGGED_BATCH PASS frame=120 draws=8 written=99 untouched=255901 color_diff=0 coverage_diff=0 invalid=0 homogeneous=1 provenance_checked=99 provenance_diff=0
GPU_RETIREMENT PASS frame=120 draws=8 gpu_tested=1 gpu_written=99 cpu_written=100 errors=0
GPU_COVERAGE PASS frame=120 cpu_written=100 gpu_written=99 unexplained=0 outside=0 invalid=0
GPU_BYPASS OK frame=240 written=5
GPU_BYPASS changed= 4 elapsed_ms= 1
GPU_DEFER_AUDIT eligible= 10 mapped= 2
GPU_DEFER_AUDIT invalidated= 8 unknown= 0
GPU_DEFER_AUDIT groups= 2 group_draws= 8
GPU_DEFER_AUDIT multi_groups= 2 multi_draws= 8
GPU_DEFER_AUDIT max_group= 4 counts_ok= 1
GPU_DEFER_STRICT eligible= 9 mapped= 2
GPU_DEFER_STRICT invalidated= 7 unknown= 0
GPU_DEFER_STRICT groups= 3 group_draws= 7
GPU_DEFER_STRICT multi_groups= 2 multi_draws= 6
GPU_DEFER_STRICT max_group= 3 counts_ok= 1
GPU_DUAL_VIEW mapped=1 guard=0
GPU_DUAL_VIEW checks= 4 mismatch= 0
GPU_DUAL_VIEW thread_checks= 55 foreign= 0
GPU_DUAL_VIEW_RELEASE success= 1
GPU_SHADOW_AUTO sampled= 2 empty= 1
GPU_SHADOW_AUTO passed= 1 differences= 0
GPU_SHADOW_AUTO errors= 0 skipped= 0
GPU_SHADOW_AUTO disabled= 0 counts_ok= 1
GPU_BYPASS sampled_frames= 1 empty= 0
GPU_BYPASS attempted= 1 passed= 1
GPU_BYPASS failed= 0 disabled= 0
GPU_BYPASS written= 5 changed= 4
SESSION_VULKAN frames= 300 skipped= 0
SESSION_VULKAN present_ms= 2 recreates= 0
msiExit total_frames= 300 triangles= 1200
MSI_HEAP_TRACKER exit_live= 0 peak_live= 5
SHUTDOWN_COMPLETE elapsed_ms= 2
"""
    def test_gpu_full_diagnostic_gate(self):
        r = self.parse(self.GPU_SESSION)["gpu_diagnostics"]
        self.assertEqual(r["status"], "validated", r)
        self.assertEqual(r["totals"]["GPU_CPU_REPLAY"]["exact_pixels"], 256000)
        self.assertEqual(r["totals"]["GPU_RETIREMENT"]["gpu_tested"], 1)

    def test_gpu_gate_rejects_contradictory_and_missing_evidence(self):
        for before, after, expected in (
            ("mask_diff=0", "mask_diff=1", "failed"),
            ("cpu_written=100 errors=0", "cpu_written=101 errors=0", "failed"),
            ("GPU_RETIREMENT PASS", "GPU_RETIREMENT ERROR", "failed"),
            ("GPU_DUAL_VIEW_RELEASE success= 1", "", "incomplete"),
            ("foreign= 0", "foreign= 1", "incomplete"),
            ("max_passes=1", "max_passes=24", "incomplete"),
            ("GPU_DUAL_VIEW mapped=1", "GPU_DUAL_VIEW mapped=0", "incomplete"),
            ("GROUP_NOT_PRESENT", "GROUP_NOT_PRESENT", "validated"),
        ):
            with self.subTest(before=before):
                r = self.parse(self.GPU_SESSION.replace(before, after))["gpu_diagnostics"]
                self.assertEqual(r["status"], expected, r)

    def test_gpu_roi_gate(self):
        line = "GPU_TAGGED_ROI PASS frame=120 draws=8 exact=256000 bytes=23760 full_bytes=1024000\n"
        single = "GPU_BYPASS_ROI PASS frame=240 draws=1 exact=256000 bytes=80000 full_bytes=1024000\n"
        body = self.GPU_SESSION.replace("GPU_RETIREMENT PASS", line + "GPU_RETIREMENT PASS").replace("GPU_BYPASS OK", single + "GPU_BYPASS OK")
        r = self.parse(body)["gpu_diagnostics"]["roi"]
        self.assertEqual(r["status"], "validated", r)
        self.assertEqual(r["passes"], 1)
        self.assertEqual(r["bytes"], 23760)
        self.assertEqual(r["reduction_percent"], 97.68)
        self.assertEqual(r["single_triangle_passes"], 1)
        self.assertEqual(r["single_triangle_bytes"], 80000)
        for before, after, expected in (
            ("exact=256000 bytes=", "exact=255999 bytes=", "failed"),
            ("bytes=23760", "bytes=23761", "failed"),
            ("GPU_TAGGED_ROI PASS", "GPU_TAGGED_ROI ERROR", "failed"),
            ("GPU_TAGGED_ROI PASS frame=120", "GPU_TAGGED_ROI PASS frame=121", "incomplete"),
            (line, "", "incomplete"),
            (single, "", "incomplete"),
            ("GPU_BYPASS_ROI PASS frame=240", "GPU_BYPASS_ROI PASS frame=241", "incomplete"),
        ):
            with self.subTest(before=before):
                r = self.parse(body.replace(before, after))["gpu_diagnostics"]["roi"]
                self.assertEqual(r["status"], expected, r)

    def test_gpu_roi_publication_gate(self):
        batch = "GPU_TAGGED_ROI PASS frame=120 draws=8 exact=256000 bytes=23760 full_bytes=1024000\n"
        single = "GPU_BYPASS_ROI_PUBLISH PASS frame=240 draws=1 written=5 bytes=80000 full_bytes=1024000\n"
        body = self.GPU_SESSION.replace("GPU_RETIREMENT PASS", batch + "GPU_RETIREMENT PASS").replace("GPU_BYPASS OK", single + "GPU_BYPASS OK")
        r = self.parse(body)["gpu_diagnostics"]["roi"]
        self.assertEqual(r["status"], "validated", r)
        self.assertEqual(r["single_triangle_mode"], "publication")
        self.assertEqual(r["single_triangle_passes"], 0)
        self.assertEqual(r["single_triangle_publications"], 1)
        self.assertEqual(r["single_triangle_publication_written"], 5)
        self.assertEqual(r["single_triangle_bytes"], 80000)
        for before, after, expected in (
            ("written=5 bytes=", "written=20001 bytes=", "failed"),
            ("bytes=80000", "bytes=80001", "failed"),
            ("draws=1 written=", "draws=2 written=", "failed"),
            ("written=5 bytes=", "written=4 bytes=", "incomplete"),
            ("frame=240 draws=1", "frame=241 draws=1", "incomplete"),
            ("GPU_BYPASS_ROI_PUBLISH PASS", "GPU_BYPASS_ROI_PUBLISH ERROR", "failed"),
            (single, "", "incomplete"),
            (single, single + "GPU_BYPASS_ROI PASS frame=240 draws=1 exact=256000 bytes=80000 full_bytes=1024000\n", "failed"),
        ):
            with self.subTest(before=before):
                r = self.parse(body.replace(before, after))["gpu_diagnostics"]["roi"]
                self.assertEqual(r["status"], expected, r)

    def test_gpu_pilot_metrics(self):
        batch = "GPU_TAGGED_ROI PASS frame=120 draws=8 exact=256000 bytes=23760 full_bytes=1024000\n"
        single = "GPU_BYPASS_ROI_PUBLISH PASS frame=240 draws=1 written=5 bytes=80000 full_bytes=1024000\n"
        metric = "GPU_BYPASS_METRICS frame=240 mode=1 decoded_bytes=65536 upload_bytes=0 qpc_hz=10000000 dispatch_ticks=25000 publish_ticks=1500\n"
        body = self.GPU_SESSION.replace("GPU_RETIREMENT PASS", batch + "GPU_RETIREMENT PASS").replace("GPU_BYPASS OK", single + metric + "GPU_BYPASS OK")
        r = self.parse(body)["gpu_diagnostics"]["metrics"]
        self.assertEqual(r["status"], "validated", r)
        self.assertEqual((r["hits"], r["misses"], r["upload_bytes"], r["timed_samples"]), (1, 0, 0, 1))
        self.assertEqual((r["dispatch_ms"], r["publish_ms"]), (2.5, 0.15))
        for mode in (0, 2):
            r = self.parse(body.replace("mode=1 decoded_bytes=65536 upload_bytes=0", f"mode={mode} decoded_bytes=65536 upload_bytes=65536"))["gpu_diagnostics"]["metrics"]
            self.assertEqual(r["status"], "validated", r)
            self.assertEqual(r["upload_bytes"], 65536)
            self.assertEqual(r["misses" if mode == 2 else "uncached"], 1)
        for before, after, expected in (
            ("mode=1", "mode=3", "failed"),
            ("upload_bytes=0", "upload_bytes=4", "failed"),
            ("decoded_bytes=65536", "decoded_bytes=65540", "failed"),
            ("qpc_hz=10000000", "qpc_hz=0", "failed"),
            ("publish_ticks=1500", "", "failed"),
            ("METRICS frame=240", "METRICS frame=241", "incomplete"),
            (metric, metric + metric, "incomplete"),
            (metric, "", "unavailable"),
        ):
            with self.subTest(before=before):
                r = self.parse(body.replace(before, after))["gpu_diagnostics"]["metrics"]
                self.assertEqual(r["status"], expected, r)
        r = self.parse(body.replace("qpc_hz=10000000 dispatch_ticks=25000 publish_ticks=1500",
                                    "qpc_hz=0 dispatch_ticks=0 publish_ticks=0"))["gpu_diagnostics"]["metrics"]
        self.assertEqual(r["status"], "validated", r)
        self.assertEqual(r["timed_samples"], 0)
        self.assertIsNone(r["dispatch_ms"])
        # Optional newer fields do not invalidate historical logs.
        for suffix, expected in (
            (" fence_wait_valid=1 fence_wait_ticks=20000", "validated"),
            (" fence_wait_valid=0 fence_wait_ticks=0", "validated"),
            (" fence_wait_valid=1 fence_wait_ticks=25001", "failed"),
            (" fence_wait_valid=1", "failed"),
            (" fence_wait_ticks=1", "failed"),
            (" fence_wait_valid=2 fence_wait_ticks=0", "failed"),
            (" fence_wait_valid=0 fence_wait_ticks=1", "failed"),
        ):
            with self.subTest(suffix=suffix):
                r = self.parse(body.replace(metric, metric.rstrip("\n") + suffix + "\n"))["gpu_diagnostics"]["metrics"]
                self.assertEqual(r["status"], expected, r)
                if suffix.endswith("ticks=20000"):
                    self.assertEqual((r["fence_timed_samples"], r["fence_wait_ms"], r["dispatch_excluding_fence_ms"]), (1, 2.0, 0.5))

    def test_gpu_pilot_oracle(self):
        batch = "GPU_TAGGED_ROI PASS frame=120 draws=8 exact=256000 bytes=23760 full_bytes=1024000\n"
        single = "GPU_BYPASS_ROI_PUBLISH PASS frame=240 draws=1 written=5 bytes=80000 full_bytes=1024000\n"
        oracle = "GPU_BYPASS_ORACLE PASS frame=240 cpu_written=6 gpu_written=5 both=5 exact=4 one_lsb=1 edge_color=0 cpu_only=1 gpu_only=0 edge_coverage=1 unexplained=0 outside=0 invalid=0 qpc_hz=10000000 cpu_ticks=45000\n"
        body = self.GPU_SESSION.replace("GPU_RETIREMENT PASS", batch + "GPU_RETIREMENT PASS").replace("GPU_BYPASS OK", oracle + single + "GPU_BYPASS OK")
        r = self.parse(body)["gpu_diagnostics"]["oracle"]
        self.assertEqual(r["status"], "validated", r)
        self.assertEqual((r["exact"], r["edge_coverage"], r["cpu_replay_ms"]), (4, 1, 4.5))
        for before, after, expected in (
            ("ORACLE PASS", "ORACLE DIFFERENCES", "failed"),
            ("unexplained=0 outside=0 invalid=0", "unexplained=1 outside=0 invalid=0", "failed"),
            ("both=5 exact=4", "both=5 exact=3", "failed"),
            ("edge_coverage=1", "edge_coverage=0", "failed"),
            ("qpc_hz=10000000 cpu_ticks=45000", "qpc_hz=0 cpu_ticks=45000", "failed"),
            ("ORACLE PASS frame=240", "ORACLE PASS frame=241", "incomplete"),
            (oracle, "", "unavailable"),
        ):
            with self.subTest(before=before):
                r = self.parse(body.replace(before, after))["gpu_diagnostics"]["oracle"]
                self.assertEqual(r["status"], expected, r)

    def test_gpu_paired_timing(self):
        batch = "GPU_TAGGED_ROI PASS frame=120 draws=8 exact=256000 bytes=23760 full_bytes=1024000\n"
        single = "GPU_BYPASS_ROI_PUBLISH PASS frame=240 draws=1 written=5 bytes=80000 full_bytes=1024000\n"
        oracle = "GPU_BYPASS_ORACLE PASS frame=240 cpu_written=6 gpu_written=5 both=5 exact=4 one_lsb=1 edge_color=0 cpu_only=1 gpu_only=0 edge_coverage=1 unexplained=0 outside=0 invalid=0 qpc_hz=10000000 cpu_ticks=45000\n"
        metric = "GPU_BYPASS_METRICS frame=240 mode=1 decoded_bytes=65536 upload_bytes=0 qpc_hz=10000000 dispatch_ticks=25000 publish_ticks=5000\n"
        body = self.GPU_SESSION.replace("GPU_RETIREMENT PASS", batch + "GPU_RETIREMENT PASS").replace("GPU_BYPASS OK", oracle + metric + single + "GPU_BYPASS OK")
        r = self.parse(body)["gpu_diagnostics"]["paired_timing"]
        self.assertEqual(r["status"], "validated", r)
        self.assertEqual((r["samples"], r["untimed_samples"], r["cpu_replay_ms"], r["gpu_plus_publication_ms"], r["replay_over_gpu_plus_publication"]), (1, 0, 4.5, 3.0, 1.5))
        self.assertEqual(r["by_texture_mode"]["1"]["samples"], 1)
        self.assertIsNone(r["by_texture_mode"]["0"]["replay_over_gpu_plus_publication"])
        r = self.parse(body.replace("qpc_hz=10000000 cpu_ticks=45000", "qpc_hz=20000000 cpu_ticks=45000"))["gpu_diagnostics"]["paired_timing"]
        self.assertEqual(r["status"], "failed")
        r = self.parse(body.replace("qpc_hz=10000000 cpu_ticks=45000", "qpc_hz=0 cpu_ticks=0"))["gpu_diagnostics"]["paired_timing"]
        self.assertEqual(r["status"], "unavailable")
        self.assertEqual(r["samples"], 0)

    def test_gpu_malformed_bypass_record(self):
        r = self.parse(self.GPU_SESSION.replace("GPU_BYPASS OK frame=240 written=5",
                                               "GPU_BYPASS OK written=5"))["gpu_diagnostics"]
        self.assertEqual(r["status"], "failed", r)
        self.assertEqual(r["roi"]["status"], "incomplete")
        self.assertTrue(any("incomplete" in e for e in r["errors"]))

    def test_gpu_legacy_and_truncation(self):
        self.assertEqual(self.parse("msiInit width=640\n")["gpu_diagnostics"]["status"], "unavailable")
        r = self.parse(self.GPU_SESSION.split("GPU_DEFER_AUDIT")[0])["gpu_diagnostics"]
        self.assertNotEqual(r["status"], "validated")
        old = "\n".join(line for line in self.GPU_SESSION.splitlines() if not line.startswith(("GPU_DUAL_VIEW", "GPU_DEFER_STRICT")))
        self.assertEqual(self.parse(old)["gpu_diagnostics"]["status"], "incomplete")

    def test_clean_exit_with_normal_null_global(self):
        r = self.parse("""msiInit width=640 height=400 planes=16 zbuffer=0 debug=0
GAME_GLOBAL_50DCD0 phase=pre_callback frame=0 ptr=0x00000000 prior_ptr=0xFFFFFFFF counter_50DCC0=0x00000000 callback_depth=0
GAME_GLOBAL_50DCD0 phase=pre_frame_present frame=7 ptr=0x00DE0540 prior_ptr=0x00000000 counter_50DCC0=0x00000001 callback_depth=0
GAME_GLOBAL_50DCD0 phase=texture_alloc frame=148 ptr=0x00000000 prior_ptr=0x00DE0540 counter_50DCC0=0x00000007 callback_depth=1
INPUT_PUMP_STATS nested_deferred= 12 frame= 360
GAME_WINDOW_EVENTS forwarded= 300 input_msgs= 28
msiExit total_frames= 901 triangles= 10000
MSI_HEAP_TRACKER exit_live= 0 peak_live= 97
SHUTDOWN_COMPLETE elapsed_ms= 9
SESSION_TIMING elapsed_ms= 19432 render_ms= 4012
""")
        self.assertEqual(r["status"], "clean_exit")
        self.assertEqual(r["frames"], 901)
        self.assertEqual(r["heap_peak_live"], 97)
        self.assertEqual(r["game_global_null_changes_observed"], 1)
        self.assertEqual(r["game_global_nonnull_changes_observed"], 1)
        self.assertEqual(r["nested_deferred_last_sample"], 12)

    def test_null_dereference_with_stack(self):
        r = self.parse("""msiInit width=640 height=400 planes=16 zbuffer=0 debug=0
FIRST_CHANCE_AV #1 code=0xC0000005 ip=0x0046D76D operation=0 address=0x0000000C module=game.exe
AV_REGISTERS eax=0x00000007 ebx=0x00000000 edx=0x00000000 elapsed_ms=10622
AV_INPUT_CONTEXT endframe_depth=0 guest_callback_depth=0 last_msg=0x00000020 last_frame=524 last_msg_age_ms=66
AV_HEAP_TRACKER live=80 capacity=256 peak_live=81
FIRST_CHANCE_AV #2 code=0xC0000005 ip=0x0046D76D operation=0 address=0x0000000C module=game.exe
""")
        self.assertEqual(r["status"], "crash_observed")
        self.assertEqual(r["first_chance_av_count"], 2)
        self.assertEqual(r["av_sites"], [{"ip":"0x0046d76d", "address":"0x0000000c"}])
        self.assertEqual(r["heap_av_live"], 80)
        self.assertEqual(r["last_av_context"]["guest_callback_depth"], 0)

    def test_av_is_not_erased_by_later_teardown(self):
        r = self.parse("""FIRST_CHANCE_AV #1 code=0xC0000005 ip=0x0048585B operation=0 address=0x00000044
msiExit total_frames= 22 triangles= 12
MSI_HEAP_TRACKER exit_live= 0 peak_live= 0
SHUTDOWN_COMPLETE elapsed_ms= 2
""")
        self.assertEqual(r["status"], "crash_observed")

    def test_truncated_log_is_not_clean(self):
        r = self.parse("msiExit total_frames= 512 triangles= 123\nMSI_HEAP_TRACKER exit_live= 0 peak_live= 97\n")
        self.assertEqual(r["status"], "incomplete")

    def test_exit_with_live_texture_heaps(self):
        r = self.parse("msiExit total_frames= 512 triangles= 123\nMSI_HEAP_TRACKER exit_live= 3 peak_live= 97\nSHUTDOWN_COMPLETE elapsed_ms= 5\n")
        self.assertEqual(r["status"], "exit_with_live_heaps")

    def test_exit_without_heap_info(self):
        r = self.parse("msiExit total_frames= 512 triangles= 123\nSHUTDOWN_COMPLETE elapsed_ms= 5\n")
        self.assertEqual(r["status"], "clean_exit_heap_unknown")

    def test_space_guard_decision_samples_preserve_crash_classification(self):
        r = self.parse("""msiInit width=640 height=400 planes=16 zbuffer=0 debug=0
TRANSITION_SPACE_CHAR_EVAL frame=40 heap_live=12 heap_age_ms=95 scene_global=0x00000000 signature_ok=1 callback_depth=0 suppress=1
TRANSITION_SPACE_CHAR_IGNORED count=1 frame=40
TRANSITION_SPACE_CHAR_EVAL frame=75 heap_live=12 heap_age_ms=1600 scene_global=0x00F40920 signature_ok=1 callback_depth=0 suppress=0
msiExit total_frames=100 triangles=150
MSI_HEAP_TRACKER exit_live=0 peak_live=12
SHUTDOWN_COMPLETE elapsed_ms=8
""")
        self.assertEqual(r["status"], "clean_exit")
        self.assertEqual(r["space_char_evaluations"], 2)
        self.assertEqual(r["space_char_suppressed"], 1)
        self.assertEqual(r["last_space_char_evaluation"]["suppress"], 0)
        self.assertEqual(r["last_space_char_evaluation"]["heap_age_ms"], 1600)
        broken = self.parse("""TRANSITION_SPACE_CHAR_EVAL frame=58 heap_live=18 heap_age_ms=78 scene_global=0x00000000 signature_ok=1 callback_depth=0 suppress=0
FIRST_CHANCE_AV #1 code=0xC0000005 ip=0x0046D76D operation=0 address=0x0000000C
""")
        self.assertEqual(broken["status"], "crash_observed")
        self.assertEqual(broken["space_char_suppressed"], 0)

    def test_runtime_host_metadata_from_standalone_log(self):
        native = self.parse("""SYS-OS: runtime=Windows native
SYS-WINE: not detected
SYS-HOST: OS=Windows
SYS-PROTON: not detected (native Windows)
msiInit width=640 height=400 planes=16 zbuffer=0 debug=0
msiExit total_frames=40 triangles=80
MSI_HEAP_TRACKER exit_live=0 peak_live=0
SHUTDOWN_COMPLETE elapsed_ms=2
""")
        self.assertEqual(native["runtime_platform"], "Windows native")
        self.assertIsNone(native["runtime_wine_version"])
        self.assertEqual(native["runtime_host_os"], "Windows")
        self.assertEqual(native["runtime_proton_hint"], "not detected (native Windows)")
        self.assertEqual(native["status"], "clean_exit")

        wine = self.parse("""SYS-OS: runtime=Wine (Win32 compatibility layer)
SYS-WINE: version=11.19
SYS-HOST: OS=Linux
SYS-PROTON: Steam compatibility environment=yes (environment hint, unverified)
msiInit width=640 height=400 planes=16 zbuffer=0 debug=0
FIRST_CHANCE_AV #1 code=0xC0000005 ip=0x0048302D operation=0 address=0x0000000C
""")
        self.assertEqual(wine["runtime_platform"], "Wine (Win32 compatibility layer)")
        self.assertEqual(wine["runtime_wine_version"], "11.19")
        self.assertEqual(wine["runtime_host_os"], "Linux")
        self.assertTrue(wine["runtime_proton_hint"].startswith("Steam compatibility environment=yes"))
        self.assertEqual(wine["status"], "crash_observed")

        old = self.parse("msiInit width=640 height=400 planes=16 zbuffer=0 debug=0\n")
        self.assertIsNone(old["runtime_platform"])
        self.assertIsNone(old["runtime_proton_hint"])

    def test_missing_file(self):
        r = analyzer.parse_log(Path("totally_nonexistent_scorched_runtime_log.txt"))
        self.assertEqual(r["status"], "unreadable")


    def test_opacity_legacy_log_unavailable(self):
        self.assertEqual(self.parse("msiInit width=640\n")["opacity_audit"]["status"], "unavailable")

    def test_opacity_opaque_exit(self):
        r = self.parse("""msiInit width=640
msiExit total_frames= 20 triangles= 500
msiExit total_zplanes= 10 parameter_updates= 50
MSI_OPACITY_BEGIN scope=exit frame=20
MSI_OPACITY value=100 triangles=500 zplanes=10
MSI_OPACITY_END
""")["opacity_audit"]
        self.assertEqual(r["status"], "opaque_only")
        self.assertFalse(r["intermediate_requested"])
        self.assertTrue(r["exit_totals_checked"])

    def test_opacity_mixed_values(self):
        r = self.parse("""MSI_OPACITY_BEGIN scope=F12 frame=20
MSI_OPACITY value=0 triangles=1 zplanes=0
MSI_OPACITY value=50 triangles=3 zplanes=2
MSI_OPACITY value=100 triangles=5 zplanes=9
MSI_OPACITY value=255 triangles=0 zplanes=1
MSI_OPACITY_END
""")["opacity_audit"]
        self.assertEqual(r["status"], "nonopaque_requested")
        self.assertTrue(r["intermediate_requested"])
        self.assertTrue(r["zero_requested"])
        self.assertTrue(r["out_of_range_requested"])
        self.assertEqual(r["totals"], {"triangles": 9, "zplanes": 12})

    def test_opacity_no_calls(self):
        r = self.parse("MSI_OPACITY_BEGIN scope=F12 frame=1\nMSI_OPACITY_END\n")["opacity_audit"]
        self.assertEqual(r["status"], "no_calls")

    def test_opacity_truncated_snapshot_overrides_older(self):
        r = self.parse("""MSI_OPACITY_BEGIN scope=F12 frame=20
MSI_OPACITY value=100 triangles=500 zplanes=10
MSI_OPACITY_END
MSI_OPACITY_BEGIN scope=F12 frame=30
MSI_OPACITY value=100 triangles=600 zplanes=20
""")["opacity_audit"]
        self.assertEqual(r["status"], "incomplete")

    def test_opacity_invalid_bins(self):
        for rows in (
            "MSI_OPACITY value=256 triangles=1 zplanes=0",
            "MSI_OPACITY value=100 triangles=0 zplanes=0",
            "MSI_OPACITY value=100 triangles=1 zplanes=0\nMSI_OPACITY value=100 triangles=1 zplanes=0",
            "MSI_OPACITY value=100 triangles=4294967295 zplanes=0 saturated=1",
            "MSI_OPACITY value=100 triangles=1 zplanes=0 saturated=1",
        ):
            with self.subTest(rows=rows):
                r = self.parse("MSI_OPACITY_BEGIN scope=F12 frame=20\n" + rows + "\nMSI_OPACITY_END\n")["opacity_audit"]
                self.assertEqual(r["status"], "invalid")
                self.assertIsNone(r["intermediate_requested"])

    def test_opacity_exit_totals_mismatch(self):
        r = self.parse("""msiInit width=640
msiExit total_frames= 20 triangles= 500
msiExit total_zplanes= 10 parameter_updates= 50
MSI_OPACITY_BEGIN scope=exit frame=20
MSI_OPACITY value=100 triangles=499 zplanes=10
MSI_OPACITY_END
""")["opacity_audit"]
        self.assertEqual(r["status"], "invalid")

    def test_opacity_malformed_records_and_nested_headers(self):
        for body in (
            "MSI_OPACITY_BEGIN scope=F12 frame=20\nMSI_OPACITY bogus\nMSI_OPACITY_END\n",
            "MSI_OPACITY_BEGIN scope=F12 frame=20\nMSI_OPACITY_BEGIN scope=F12 frame=21\nMSI_OPACITY_END\n",
            "MSI_OPACITY_END\n",
        ):
            with self.subTest(body=body):
                self.assertEqual(self.parse(body)["opacity_audit"]["status"], "invalid")

    def test_opacity_reset_on_reinit(self):
        r = self.parse("""msiInit width=640
MSI_OPACITY_BEGIN scope=F12 frame=20
MSI_OPACITY value=50 triangles=2 zplanes=0
MSI_OPACITY_END
msiInit width=640
""")["opacity_audit"]
        self.assertEqual(r["status"], "unavailable")

if __name__ == "__main__":
    unittest.main()
