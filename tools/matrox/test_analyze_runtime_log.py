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

    def test_missing_file(self):
        r = analyzer.parse_log(Path("totally_nonexistent_scorched_runtime_log.txt"))
        self.assertEqual(r["status"], "unreadable")

if __name__ == "__main__":
    unittest.main()
