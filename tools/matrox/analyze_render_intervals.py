#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only Matrox MSI95 periodic render/atlas telemetry, grouped by frame.

Counters measure cumulative OVERDRAW, not unique output pixels. Wall FPS may
include loading, input and video waits. This is not a controlled A/B benchmark.
Usage: python tools/matrox/analyze_render_intervals.py [--json] LOG
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

KV_RE = re.compile(r"\b([a-zA-Z][a-zA-Z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)")
FAMILIES = {
    "PERF_COST render_ms=": ("render_ms", "gdi_ms"),
    "PERF_COST state_ms=": ("state_ms",),
    "FPS_CAP late_frames=": ("late_frames", "fallback_frames"),
    "WORLD_BILINEAR filtered_samples=": ("filtered_samples", "changed_pixels"),
    "WORLD_BILINEAR keyed_neighbor_bypass=": ("keyed_neighbor_bypass",),
    "WORLD_BILINEAR uniform_neighborhood_samples=": ("uniform_neighborhood_samples",),
    "WORLD_BILINEAR palette_cached_triangles=": ("palette_cached_triangles", "palette_cached_samples"),
    "LARGE_Z_ATLAS missing_samples=": ("missing_samples", "direct_nonzero_samples"),
    "LARGE_Z_ATLAS triangles=": ("drawn_pixels",),
    "RENDER_ZPLANES pixels=": ("pixels",),
    "MSI_MODULATE triangles=": ("pixels",),
    "MSI_DITHER candidates=": ("candidates", "changed_pixels"),
    "PERSPECTIVE_UV adjusted_pixels=": ("adjusted_pixels",),
    "VERTEX_REGION_CHECK reused_queries=": ("reused_queries", "fallback_queries"),
    "LARGE_Z_ATLAS zero_skips_color_keyed=": ("zero_skips_color_keyed", "zero_skips_not_keyed"),
    "ZPLANE_BLUE_KEY rgb565=": ("skipped_pixels",),
    "RENDER_PROGRESS pixels=": ("pixels", "missing_textures"),
}
ALIASES = {
    ("ZPLANE_BLUE_KEY rgb565=", "skipped_pixels"): "z_blue_skips",
    ("RENDER_PROGRESS pixels=", "pixels"): "raster_pixels",
    ("LARGE_Z_ATLAS triangles=", "drawn_pixels"): "atlas_drawn_pixels",
    ("RENDER_ZPLANES pixels=", "pixels"): "zplane_pixels",
    ("MSI_MODULATE triangles=", "pixels"): "modulate_pixels",
    ("MSI_DITHER candidates=", "candidates"): "dither_candidates",
    ("MSI_DITHER candidates=", "changed_pixels"): "dither_changed_pixels",
    ("PERSPECTIVE_UV adjusted_pixels=", "adjusted_pixels"): "perspective_pixels",
}
# The 32-bit DLL declares these values 'unsigned', including samples, RGB
# dithering, and raw raster pixel counters. A long original game session
# exceeds 2**32 output samples even though no memory errors occur.
UINT32 = 1 << 32
PIXEL_WRAP_KEYS = frozenset({
    "raster_pixels", "filtered_samples", "changed_pixels",
    "uniform_neighborhood_samples", "palette_cached_samples",
    "missing_samples", "direct_nonzero_samples", "atlas_drawn_pixels",
    "zero_skips_color_keyed", "zero_skips_not_keyed", "z_blue_skips",
    "modulate_pixels", "dither_candidates", "dither_changed_pixels",
    "zplane_pixels", "perspective_pixels",
})
# Explicit near-boundary criterion. Do not misdiagnose a sudden reset in a
# real cumulative counter as a wrap; samples are at most 300 frames apart
# in normal MSI95 logs. Multiple whole wraps *between* two samples cannot
# be inferred safely from the logfile.
WRAP_NEAR_MAX = 0xF0000000
WRAP_NEAR_ZERO = 0x0FFFFFFF


def counter_delta(key: str, previous: int, current: int) -> tuple[int, bool] | None:
    if current >= previous:
        return current - previous, False
    if (key in PIXEL_WRAP_KEYS and
            WRAP_NEAR_MAX <= previous < UINT32 and
            0 <= current <= WRAP_NEAR_ZERO):
        return UINT32 - previous + current, True
    return None


def fields(line: str) -> dict[str, int]:
    return {key: int(value, 16 if value.lower().startswith("0x") else 10)
            for key, value in KV_RE.findall(line)}

def difference(previous: dict, current: dict) -> dict | None:
    frames = current["frames"] - previous["frames"]
    elapsed = current["elapsed_ms"] - previous["elapsed_ms"]
    if frames <= 0 or elapsed <= 0:
        return None
    diffs = {}
    wraps = []
    for prefix, names in FAMILIES.items():
        for name in names:
            key = ALIASES.get((prefix, name), name)
            if key not in previous or key not in current:
                continue
            measured = counter_delta(key, previous[key], current[key])
            if measured is None:
                return None  # Counter reset or ambiguous wrap; never silently infer one.
            delta, wrapped = measured
            if wrapped:
                wraps.append(key)
            diffs[key] = delta
    return {
        "from_frame": previous["frames"], "to_frame": current["frames"],
        "frames": frames, "elapsed_ms": elapsed,
        "observed_fps": round(1000.0 * frames / elapsed, 2),
        "render_ms_per_frame": (round(diffs["render_ms"] / frames, 2)
                                if "render_ms" in diffs else None),
        "bilinear_samples_per_frame": (round(diffs["filtered_samples"] / frames, 1)
                                      if "filtered_samples" in diffs else None),
        "late_frames_pct": (round(100.0 * diffs["late_frames"] / frames, 1)
                            if "late_frames" in diffs else None),
        "deltas": diffs,
        "counter_wraps": wraps,
    }

def analyze(path: Path, min_frames: int = 60) -> dict:
    if min_frames < 1:
        raise ValueError("min_frames must be positive")
    latest: dict[str, int] = {}
    snapshots: list[dict] = []
    # MSI95 writes PERF_WALL, then PERF_COST(render), then PERF_COST(state)
    # for the SAME frame. Finish the snapshot only after those costs arrive.
    pending_wall: dict[str, int] | None = None
    exit_frames = None
    session_ms = None
    with path.open("r", encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if line.startswith("PERF_WALL elapsed_ms="):
                sample = fields(line)
                if "elapsed_ms" in sample and "frames" in sample:
                    pending_wall = {"elapsed_ms": sample["elapsed_ms"],
                                    "frames": sample["frames"]}
            elif line.startswith("msiExit total_frames="):
                exit_frames = fields(line).get("total_frames")
            elif line.startswith("SESSION_TIMING elapsed_ms="):
                session_ms = fields(line).get("elapsed_ms")
            else:
                for prefix, names in FAMILIES.items():
                    if line.startswith(prefix):
                        data = fields(line)
                        for name in names:
                            if name in data:
                                latest[ALIASES.get((prefix, name), name)] = data[name]
                        if prefix == "PERF_COST state_ms=" and pending_wall is not None:
                            snapshots.append({**latest, **pending_wall})
                            pending_wall = None
                        break
    intervals = []
    valid_steps = []
    for before, after in zip(snapshots, snapshots[1:]):
        interval = difference(before, after)
        if interval is not None:
            valid_steps.append(interval)
            if interval["frames"] >= min_frames:
                intervals.append(interval)
    wrap_totals: dict[str, int] = {}
    for row in valid_steps:
        for key in row["counter_wraps"]:
            wrap_totals[key] = wrap_totals.get(key, 0) + 1
    # All accepted intervals must connect to report reliable cumulative
    # totals; otherwise a reset/ambiguous wrap might have been skipped.
    fully_connected = (len(valid_steps) == max(0, len(snapshots) - 1))
    extended_totals = ({
        key: value + UINT32 * wrap_totals.get(key, 0)
        for key, value in snapshots[-1].items()
        if key in PIXEL_WRAP_KEYS
    } if fully_connected and snapshots else {})
    return {
        "file": str(path), "snapshots": len(snapshots),
        "counter_wraps": wrap_totals,
        "last_snapshot_extended": extended_totals,
        "snapshot_last_frame": snapshots[-1]["frames"] if snapshots else None,
        "exit_frames": exit_frames, "session_ms": session_ms,
        "intervals": intervals,
        "note": (
            "Overdraw-inclusive samples; wall FPS includes waits. "
            "Only near-boundary 32-bit pixel counter rollovers are recovered; "
            "ambiguous resets are rejected. At least one full 2**32 cycle "
            "between snapshots cannot be identified. Not controlled A/B evidence."
        ),
    }

def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--min-frames", type=int, default=60)
    args = parser.parse_args(argv)
    if args.min_frames < 1:
        parser.error("--min-frames must be >=1")
    try:
        report = analyze(args.log, args.min_frames)
    except OSError as error:
        parser.error(str(error))
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"{args.log}: {report['snapshots']} snapshots; "
              f"last={report['snapshot_last_frame']} exit={report['exit_frames']}")
        print("Frames        Wall FPS  Render ms/frame  Bilinear samples/frame  Late %  Atlas zero skips (keyed/non-keyed)")
        for row in report["intervals"]:
            d = row["deltas"]
            fmt = lambda x: "n/a" if x is None else str(x)
            print(f"{row['from_frame']:>5}-{row['to_frame']:<5} "
                  f"{row['observed_fps']:>8.2f}  {fmt(row['render_ms_per_frame']):>15}  "
                  f"{fmt(row['bilinear_samples_per_frame']):>22}  "
                  f"{fmt(row['late_frames_pct']):>6}  "
                  f"{d.get('zero_skips_color_keyed', 'n/a')}/{d.get('zero_skips_not_keyed', 'n/a')}")
        if report["counter_wraps"]:
            print("Recovered UINT32 pixel counter rollovers:", report["counter_wraps"])
            if report["last_snapshot_extended"]:
                print("Last snapshot reconstructed totals:", report["last_snapshot_extended"])
        print(report["note"])
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
