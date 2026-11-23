#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline Matrox shadow-once conservation and underlay triage.

The MSI95 x86 DLL logs cumulative *overdraw samples*, NOT distinct pixels.
The opaque underlay map is per-frame and records the most recent MSI write.
An unrecognized underlay is NOT proof of an erroneous black surface.
Only narrowly detectable UINT32 counter wraps are reconstructed.
Usage: python tools/matrox/analyze_shadow_telemetry.py LOG [--json]
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

UINT32 = 1 << 32
FIELDS = re.compile(r"([A-Za-z_][A-Za-z_0-9]*)=\s*(\d+)")
SOURCES = (
    ("SHADOW_BLEND dark_flat_pixels=",
     {"dark_flat_pixels": "candidates", "blended_pixels": "blend_primary"}),
    ("SHADOW_ONCE blended_pixels=",
     {"blended_pixels": "blend_once", "repeated_samples": "repeated"}),
    ("SHADOW_ONCE untextured_base=",
     {"untextured_base": "no_base", "other_surface_resets": "resets"}),
    ("SHADOW_UNDERLAY candidates_with_texture=",
     {"candidates_with_texture": "with_texture", "no_texture": "without_texture"}),
    ("SHADOW_UNDERLAY opaque_texture=",
     {"opaque_texture": "opaque_texture", "transparent_texture": "transparent_texture"}),
)
COUNTERS = frozenset(n for _, aliases in SOURCES for n in aliases.values())


def read_snapshots(path: Path) -> tuple[list[dict], str | None]:
    latest: dict[str, int] = {}
    snapshots: list[dict] = []
    configured = None
    pending = None
    with path.open("r", encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if line.startswith("SHADOW_ONCE enabled ("):
                configured = "enabled"
            elif line.startswith("SHADOW_ONCE disabled ("):
                configured = "disabled"
            for prefix, aliases in SOURCES:
                if line.startswith(prefix):
                    fields = {k: int(v) for k, v in FIELDS.findall(line)}
                    latest.update({alias: fields[k] for k, alias in aliases.items()
                                   if k in fields})
                    break
            if line.startswith("PERF_WALL elapsed_ms="):
                values = {k: int(v) for k,v in FIELDS.findall(line)}
                if "frames" in values and "elapsed_ms" in values:
                    pending = {"frames": values["frames"], "elapsed_ms": values["elapsed_ms"]}
            elif line.startswith("PERF_COST state_ms=") and pending is not None:
                if len(latest) == len(COUNTERS):
                    snapshots.append({**latest, **pending})
                pending = None
    return snapshots, configured


def delta32(previous: int, current: int) -> tuple[int, bool] | None:
    if current >= previous:
        return current - previous, False
    if (0xF0000000 <= previous < UINT32 and
            0 <= current <= 0x0FFFFFFF):
        return current + UINT32 - previous, True
    return None


def analyze(path: Path) -> dict:
    snapshots, configured = read_snapshots(path)
    totals = {key: 0 for key in COUNTERS}
    wraps = {key: 0 for key in COUNTERS}
    matched = 0
    first_disagreement = None
    intervals = 0
    invalid_intervals = []
    max_base_pct = {"percent": -1.0, "from_frame": None, "to_frame": None}
    for previous, current in zip(snapshots, snapshots[1:]):
        intervals += 1
        values = {key: delta32(previous[key], current[key]) for key in COUNTERS}
        if (current["frames"] <= previous["frames"] or
                current["elapsed_ms"] <= previous["elapsed_ms"] or
                any(v is None for v in values.values())):
            invalid_intervals.append([previous["frames"], current["frames"]])
            continue
        d = {key: v[0] for key,v in values.items()}
        for key,v in values.items():
            totals[key] += v[0]
            wraps[key] += int(v[1])
        ok = (d["candidates"] ==
              d["blend_once"] + d["no_base"] + d["repeated"] and
              d["blend_once"] == d["blend_primary"] == d["with_texture"] ==
              d["transparent_texture"] and
              d["no_base"] == d["without_texture"] and
              d["opaque_texture"] == 0)
        if ok:
            matched += 1
        elif first_disagreement is None:
            first_disagreement = {
                "from_frame": previous["frames"],
                "to_frame": current["frames"],
                "deltas": d,
            }
        if d["candidates"] >= 100000:
            pct = 100.0 * d["no_base"] / d["candidates"]
            if pct > max_base_pct["percent"]:
                max_base_pct = {
                    "percent": round(pct, 3),
                    "from_frame": previous["frames"],
                    "to_frame": current["frames"],
                }
    valid = intervals - len(invalid_intervals)
    candidates = totals["candidates"]
    return {
        "log": str(path),
        "shadow_once": configured,
        "snapshots": len(snapshots),
        "last_snapshot_frame": snapshots[-1]["frames"] if snapshots else None,
        "total_checked_intervals": intervals,
        "valid_intervals": valid,
        "conserved_intervals": matched,
        "first_disagreement": first_disagreement,
        "invalid_intervals": invalid_intervals,
        "total_sample_deltas": totals,
        "uint32_rollovers": {k:v for k,v in sorted(wraps.items()) if v},
        "no_base_percent": round(100*totals["no_base"]/candidates, 3)
        if candidates else None,
        "blended_percent": round(100*totals["blend_once"]/candidates, 3)
        if candidates else None,
        "repeated_percent": round(100*totals["repeated"]/candidates, 3)
        if candidates else None,
        "max_interval_no_base_share": max_base_pct
        if max_base_pct["from_frame"] is not None else None,
        "warning": (
            "Pixel writes are overdraw-inclusive and may be repeatedly counted. "
            "The per-frame shadow underlay map is last-MSI-write state, not an "
            "original Mystique hardware oracle. No-base does not prove a missing "
            "texture or wrong shadow. A whole hidden 2**32 rollover between "
            "two snapshots cannot be inferred; invalid deltas are rejected."
        ),
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        report = analyze(args.log)
    except OSError as error:
        parser.error(str(error))
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"{args.log}: shadow_once={report['shadow_once']} "
              f"frames={report['last_snapshot_frame']} "
              f"conserved={report['conserved_intervals']}/"
              f"{report['valid_intervals']} valid intervals")
        print("Overdraw samples:",report["total_sample_deltas"])
        print("32-bit wraps:",report["uint32_rollovers"])
        print(f"no_base={report['no_base_percent']}% "
              f"blended={report['blended_percent']}% "
              f"repeated={report['repeated_percent']}%")
        if report["first_disagreement"]:
            print("WARNING: shadow accounting mismatch:",report["first_disagreement"])
        if report["invalid_intervals"]:
            print("WARNING: rejected intervals:",report["invalid_intervals"])
        print("Interpretation:",report["warning"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
