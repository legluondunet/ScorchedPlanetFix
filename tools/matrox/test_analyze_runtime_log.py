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
