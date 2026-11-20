#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Summarize real ScorchedPlanetFix Matrox MSI95 logs, without changing runtime.

No proprietary game bytes are needed. A NULL game global may be normal.
Usage: python tools/matrox/analyze_runtime_log.py [--json] [--assert-clean] LOG...
"""
from __future__ import annotations
import argparse
import json
import re
import sys
from pathlib import Path

AV_RE = re.compile(r"^FIRST_CHANCE_AV\s+#\d+.*?\bip=(0x[0-9A-Fa-f]+).*?\baddress=(0x[0-9A-Fa-f]+)")
GLOBAL_RE = re.compile(r"^GAME_GLOBAL_50DCD0\s+phase=(\S+)\s+frame=(\d+)\s+ptr=(0x[0-9A-Fa-f]+)\s+prior_ptr=(0x[0-9A-Fa-f]+)\s+counter_50DCC0=(0x[0-9A-Fa-f]+)\s+callback_depth=(\d+)")
KV_RE = re.compile(r"\b([a-zA-Z][a-zA-Z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)")

def fields(line: str) -> dict[str, int]:
    return {k: int(v, 16 if v.lower().startswith("0x") else 10) for k, v in KV_RE.findall(line)}

def parse_log(path: Path) -> dict:
    result = {
        "file": str(path), "status": "incomplete", "errors": [],
        "runtime_platform": None, "runtime_wine_version": None,
        "runtime_host_os": None, "runtime_proton_hint": None,
        "init_seen": False, "msi_exit_seen": False, "shutdown_complete": False,
        "session_ms": None, "frames": None, "heap_exit_live": None,
        "heap_peak_live": None, "heap_av_live": None,
        "first_chance_av_count": 0, "av_sites": [],
        "last_av_context": None, "last_av_registers": None,
        "nested_deferred_last_sample": None, "nested_deferred_sample_frame": None,
        "input_messages_last_sample": None, "game_global_samples": 0,
        "space_char_evaluations": 0, "space_char_suppressed": 0,
        "last_space_char_evaluation": None,
        "game_global_pointer_changes_observed": 0,
        "game_global_null_changes_observed": 0,
        "game_global_nonnull_changes_observed": 0, "last_game_global": None,
    }
    try:
        with path.open("r", encoding="utf-8", errors="replace") as log:
            for raw in log:
                line = raw.strip()
                if line.startswith("SYS-OS: runtime="):
                    result["runtime_platform"] = line.partition("runtime=")[2]
                elif line.startswith("SYS-WINE: version="):
                    result["runtime_wine_version"] = line.partition("version=")[2]
                elif line.startswith("SYS-HOST: OS="):
                    result["runtime_host_os"] = line.partition("OS=")[2]
                elif line.startswith("SYS-PROTON: "):
                    result["runtime_proton_hint"] = line.partition(": ")[2]
                elif line.startswith("msiInit width="):
                    result["init_seen"] = True
                elif line.startswith("msiExit total_frames="):
                    result["msi_exit_seen"] = True
                    result["frames"] = fields(line).get("total_frames")
                elif line.startswith("SHUTDOWN_COMPLETE"):
                    result["shutdown_complete"] = True
                elif line.startswith("SESSION_TIMING elapsed_ms="):
                    result["session_ms"] = fields(line).get("elapsed_ms")
                elif line.startswith("MSI_HEAP_TRACKER exit_live="):
                    f = fields(line)
                    result["heap_exit_live"] = f.get("exit_live")
                    result["heap_peak_live"] = f.get("peak_live")
                elif line.startswith("AV_HEAP_TRACKER"):
                    result["heap_av_live"] = fields(line).get("live")
                elif line.startswith("AV_INPUT_CONTEXT"):
                    result["last_av_context"] = fields(line)
                elif line.startswith("AV_REGISTERS"):
                    f = fields(line)
                    result["last_av_registers"] = {
                        k: f[k] for k in ("eax","ebx","ecx","edx","esi","edi","esp","elapsed_ms")
                        if k in f
                    }
                elif line.startswith("INPUT_PUMP_STATS"):
                    f = fields(line)
                    result["nested_deferred_last_sample"] = f.get("nested_deferred")
                    result["nested_deferred_sample_frame"] = f.get("frame")
                elif line.startswith("GAME_WINDOW_EVENTS"):
                    result["input_messages_last_sample"] = fields(line).get("input_msgs")
                elif line.startswith("TRANSITION_SPACE_CHAR_EVAL"):
                    result["space_char_evaluations"] += 1
                    result["last_space_char_evaluation"] = fields(line)
                elif line.startswith("TRANSITION_SPACE_CHAR_IGNORED"):
                    # The existing count is cumulative for this runtime session.
                    result["space_char_suppressed"] = max(
                        result["space_char_suppressed"], fields(line).get("count", 0))
                elif line.startswith("GAME_GLOBAL_50DCD0"):
                    m = GLOBAL_RE.match(line)
                    if m:
                        phase, frame, curr, prev, counter, depth = m.groups()
                        now, before = int(curr, 16), int(prev, 16)
                        result["game_global_samples"] += 1
                        if before != 0xFFFFFFFF and now != before:
                            result["game_global_pointer_changes_observed"] += 1
                            key = ("game_global_null_changes_observed" if now == 0
                                   else "game_global_nonnull_changes_observed")
                            result[key] += 1
                        result["last_game_global"] = {
                            "phase": phase, "frame": int(frame), "is_null": now == 0,
                            "counter": int(counter, 16), "callback_depth": int(depth),
                        }
                elif line.startswith("FIRST_CHANCE_AV"):
                    result["first_chance_av_count"] += 1
                    m = AV_RE.match(line)
                    if m:
                        site = {"ip": m[1].lower(), "address": m[2].lower()}
                        if site not in result["av_sites"]:
                            result["av_sites"].append(site)
    except (OSError, UnicodeError) as e:
        result["status"] = "unreadable"
        result["errors"].append(str(e))
        return result

    if result["first_chance_av_count"]:
        result["status"] = "crash_observed"
    elif result["msi_exit_seen"] and result["shutdown_complete"]:
        if result["heap_exit_live"] is None:
            result["status"] = "clean_exit_heap_unknown"
        elif result["heap_exit_live"] == 0:
            result["status"] = "clean_exit"
        else:
            result["status"] = "exit_with_live_heaps"
    return result

def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", type=Path, nargs="+")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--assert-clean", action="store_true")
    args = parser.parse_args(argv)
    reports = [parse_log(p) for p in args.logs]
    if args.json:
        print(json.dumps(reports, ensure_ascii=False, indent=2))
    else:
        for r in reports:
            print(f"{r['file']}: {r['status']} | platform={r['runtime_platform'] or 'unreported'} "
                  f"wine={r['runtime_wine_version'] or 'unreported'} | frames={r['frames']} "
                  f"session_ms={r['session_ms']} heap_peak={r['heap_peak_live']} "
                  f"heap_exit_live={r['heap_exit_live']} AV={r['first_chance_av_count']} "
                  f"sites={r['av_sites']} "
                  f"nested_deferred(last_sample)={r['nested_deferred_last_sample']} "
                  f"global_null_changes(observed)={r['game_global_null_changes_observed']} "
                f"space_evaluations={r['space_char_evaluations']} "
                f"space_suppressed={r['space_char_suppressed']}")
            for error in r["errors"]:
                print(f"  ERROR: {error}", file=sys.stderr)
    if args.assert_clean and any(r["status"] != "clean_exit" for r in reports):
        return 1
    return 2 if any(r["status"] == "unreadable" for r in reports) else 0

if __name__ == "__main__":
    sys.exit(main())
