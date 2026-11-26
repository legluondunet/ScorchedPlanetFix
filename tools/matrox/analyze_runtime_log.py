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


OPACITY_BEGIN_RE = re.compile(r"MSI_OPACITY_BEGIN scope=(F12|exit) frame=(\d+)")
OPACITY_ROW_RE = re.compile(r"MSI_OPACITY value=(\d+) triangles=(\d+) zplanes=(\d+)( saturated=1)?")
UINT32_MAX = 0xFFFFFFFF


class OpacityAudit:
    """Track complete snapshots, never infer zero from absent/truncated data."""

    def __init__(self):
        self.current = None
        self.latest = None
        self.init_count = 0
        self.exit_triangles = None
        self.exit_zplanes = None

    def feed(self, line):
        if line.startswith("msiInit width="):
            self.init_count += 1
            self.current = self.latest = None
            self.exit_triangles = self.exit_zplanes = None
        elif line.startswith("msiExit total_frames="):
            self.exit_triangles = fields(line).get("triangles")
            self.exit_zplanes = None
        elif line.startswith("msiExit total_zplanes="):
            self.exit_zplanes = fields(line).get("total_zplanes")
        elif line.startswith("MSI_OPACITY_BEGIN"):
            m = OPACITY_BEGIN_RE.fullmatch(line)
            nested = self.current is not None
            self.current = {
                "scope": m[1] if m else None, "frame": int(m[2]) if m else None,
                "bins": {}, "errors": [] if m else ["invalid header"],
            }
            if nested:
                self.current["errors"].append("nested header")
            self.latest = None
        elif line.startswith("MSI_OPACITY value="):
            if self.current is None:
                self.latest = {"status": "invalid", "errors": ["row without header"]}
                return
            m = OPACITY_ROW_RE.fullmatch(line)
            if not m:
                self.current["errors"].append("invalid row")
                return
            v, t, z = map(int, m.groups()[:3])
            saturated = max(t, z) == UINT32_MAX
            if not (0 <= v <= 255 and 0 <= t <= UINT32_MAX and
                    0 <= z <= UINT32_MAX and t + z > 0):
                self.current["errors"].append("invalid bin or count")
            elif v in self.current["bins"]:
                self.current["errors"].append("duplicate bin")
            elif saturated != bool(m[4]):
                self.current["errors"].append("inconsistent saturation marker")
            else:
                self.current["bins"][v] = {"triangles": t, "zplanes": z}
                if saturated:
                    self.current["errors"].append("saturated counter")
        elif line == "MSI_OPACITY_END":
            if self.current is None:
                self.latest = {"status": "invalid", "errors": ["end without header"]}
                return
            self.latest = self.finish(self.current)
            self.current = None
        elif line.startswith("MSI_OPACITY"):
            if self.current is not None:
                self.current["errors"].append("unrecognized opacity record")
            else:
                self.latest = {"status": "invalid", "errors": ["unrecognized opacity record"]}

    def finish(self, snap):
        bins = snap["bins"]
        totals = {k: sum(b[k] for b in bins.values()) for k in ("triangles", "zplanes")}
        errors = list(snap["errors"])
        if snap["scope"] == "exit" and self.init_count == 1:
            if (totals["triangles"] != self.exit_triangles or
                    totals["zplanes"] != self.exit_zplanes):
                errors.append("exit totals mismatch or missing")
        values = sorted(bins)
        status = "invalid" if errors else (
            "no_calls" if not values else
            "opaque_only" if values == [100] else
            "nonopaque_requested"
        )
        return {
            "status": status, "scope": snap["scope"], "frame": snap["frame"],
            "totals": totals, "bins": bins, "errors": errors,
            "intermediate_requested": None if errors else any(1 <= v <= 99 for v in values),
            "zero_requested": None if errors else 0 in bins,
            "out_of_range_requested": None if errors else any(v > 100 for v in values),
            "exit_totals_checked": snap["scope"] == "exit" and self.init_count == 1,
        }

    def report(self):
        if self.current is not None:
            return {"status": "incomplete", "errors": ["missing end marker"]}
        return self.latest or {"status": "unavailable", "errors": []}



class GpuAudit:
    """Check diagnostic evidence only; never infer exclusive guest ownership."""
    STAGES = ("GPU_CPU_REPLAY", "GPU_SHADOW", "GPU_TAGGED_BATCH", "GPU_RETIREMENT", "GPU_COVERAGE")
    SUMMARY = ("GPU_SHADOW_AUTO", "GPU_BYPASS", "GPU_DEFER_AUDIT", "GPU_DEFER_STRICT",
               "GPU_DUAL_VIEW", "GPU_DUAL_VIEW_RELEASE", "SESSION_VULKAN")
    REQUIRED = {
        "GPU_CPU_REPLAY": ("frame", "draws", "exact", "mask_diff", "written"),
        "GPU_SHADOW": ("frame", "draws", "active", "exact", "one_lsb", "edge", "unexplained", "outside_bounds"),
        "GPU_TAGGED_BATCH": ("frame", "draws", "written", "untouched", "color_diff", "coverage_diff", "invalid",
                             "homogeneous", "provenance_checked", "provenance_diff"),
        "GPU_RETIREMENT": ("frame", "draws", "gpu_tested", "gpu_written", "cpu_written", "errors"),
        "GPU_COVERAGE": ("frame", "cpu_written", "gpu_written", "unexplained", "outside", "invalid"),
    }

    def __init__(self):
        self.init_count = 0
        self.requested = None
        self.active = None
        self.limits = {}
        self.summaries = {name: {} for name in self.SUMMARY}
        self.records = {name: [] for name in self.STAGES}
        self.bypass = []
        self.roi = []
        self.bypass_roi = []
        self.bypass_roi_publish = []
        self.metrics = []
        self.metrics_errors = []
        self.oracle = []
        self.oracle_errors = []
        self.roi_errors = []
        self.errors = []

    def feed(self, line):
        if line.startswith("msiInit width="):
            self.init_count += 1
        elif line.startswith("PREVIEW_RENDERER requested="):
            self.requested = line.partition("requested=")[2].split()[0]
        elif line.startswith("PREVIEW_RENDERER active="):
            self.active = line.partition("active=")[2].split()[0]
        elif line.startswith("GPU_SHADOW_AUTO enabled "):
            self.limits["passes"] = fields(line).get("max_passes")
        elif line.startswith("GPU_BYPASS pilot "):
            self.limits["bypass"] = fields(line).get("max_triangles")
        elif line.startswith("GPU_BYPASS_ORACLE "):
            f = fields(line)
            required = ("frame", "cpu_written", "gpu_written", "both", "exact", "one_lsb", "edge_color",
                        "cpu_only", "gpu_only", "edge_coverage", "unexplained", "outside", "invalid", "qpc_hz", "cpu_ticks")
            if line.split(" ", 2)[1] != "PASS" or any(k not in f for k in required):
                self.oracle_errors.append(line[:240])
            else:
                self.oracle.append(f)
        elif line.startswith("GPU_BYPASS_METRICS "):
            f = fields(line)
            if any(k not in f for k in ("frame", "mode", "decoded_bytes", "upload_bytes", "qpc_hz", "dispatch_ticks", "publish_ticks")):
                self.metrics_errors.append("incomplete GPU_BYPASS_METRICS")
            else:
                self.metrics.append(f)
        elif line.startswith("GPU_BYPASS_ROI_PUBLISH "):
            f = fields(line)
            if line.split(" ", 2)[1] != "PASS" or any(k not in f for k in ("frame", "draws", "written", "bytes", "full_bytes")):
                self.roi_errors.append(line[:200])
            else:
                self.bypass_roi_publish.append(f)
        elif line.startswith(("GPU_TAGGED_ROI ", "GPU_BYPASS_ROI ")):
            outcome = line.split(" ", 2)[1]
            f = fields(line)
            if outcome != "PASS" or any(k not in f for k in ("frame", "draws", "exact", "bytes", "full_bytes")):
                self.roi_errors.append(line[:200])
            else:
                (self.bypass_roi if line.startswith("GPU_BYPASS_ROI ") else self.roi).append(f)
        elif line.startswith("GPU_BYPASS OK "):
            f = fields(line)
            if not all(k in f for k in ("frame", "written")) or not f.get("written"):
                self.errors.append("GPU_BYPASS: incomplete or empty OK record")
            else:
                self.bypass.append(f)
        elif line.startswith(("GPU_BYPASS FALLBACK ", "GPU_DUAL_VIEW MISMATCH ")):
            self.errors.append(line[:200])
        else:
            prefix, _, tail = line.partition(" ")
            if prefix in self.STAGES:
                outcome = tail.split(" ", 1)[0]
                if outcome != "PASS":
                    self.errors.append(line[:200])
                    return
                f = fields(line)
                if any(k not in f for k in self.REQUIRED[prefix]):
                    self.errors.append(f"{prefix}: incomplete PASS")
                    return
                f["scope"] = tail.partition("scope=")[2].split(" ", 1)[0] or None
                self.records[prefix].append(f)
            elif prefix in self.SUMMARY and "=" in tail:
                f = fields(line)
                # Per-triangle timing has its own changed= record; only
                # the cumulative written=/changed= line is a summary.
                if prefix == "GPU_BYPASS" and "elapsed_ms" in f:
                    return
                if self.summaries[prefix].keys() & f.keys():
                    self.errors.append(f"{prefix}: duplicate summary keys")
                self.summaries[prefix].update(f)

    def roi_report(self):
        errors = list(self.roi_errors)
        missing = []
        for r in self.roi + self.bypass_roi:
            if not (r["exact"] == 256000 and r["full_bytes"] == 1024000 and
                    0 < r["bytes"] <= r["full_bytes"] and r["bytes"] % 4 == 0 and
                    1 <= r["draws"] <= 32):
                errors.append(f"invalid ROI evidence at {r['frame']}")
        pairs = sorted((r["frame"], r["draws"]) for r in self.roi)
        reference = sorted((r["frame"], r["draws"]) for r in self.records["GPU_TAGGED_BATCH"])
        if pairs != reference:
            missing.append("unmatched full-vs-cropped tagged records")
        if not pairs:
            missing.append("no ROI comparison")
        if self.bypass_roi and self.bypass_roi_publish:
            errors.append("mixed single-triangle ROI comparison and publication modes")
        for r in self.bypass_roi_publish:
            if not (r["draws"] == 1 and r["full_bytes"] == 1024000 and
                    0 < r["bytes"] <= r["full_bytes"] and r["bytes"] % 4 == 0 and
                    0 < r["written"] <= r["bytes"] // 4):
                errors.append(f"invalid ROI publication at {r['frame']}")
        if self.bypass_roi_publish:
            if sorted((r["frame"], r["written"]) for r in self.bypass_roi_publish) != sorted((r["frame"], r["written"]) for r in self.bypass):
                missing.append("unmatched single-triangle ROI publication records")
        elif sorted((r["frame"], r["draws"]) for r in self.bypass_roi) != sorted((r["frame"], 1) for r in self.bypass):
            missing.append("unmatched single-triangle ROI records")
        if not self.bypass_roi and not self.bypass_roi_publish:
            missing.append("no single-triangle ROI evidence")
        byte_count = sum(r["bytes"] for r in self.roi)
        full_count = sum(r["full_bytes"] for r in self.roi)
        return {"status": "failed" if errors else "incomplete" if missing else "validated",
                "passes": len(self.roi), "exact_pixels": sum(r["exact"] for r in self.roi),
                "single_triangle_passes": len(self.bypass_roi),
                "single_triangle_publications": len(self.bypass_roi_publish),
                "single_triangle_publication_written": sum(r["written"] for r in self.bypass_roi_publish),
                "single_triangle_mode": "publication" if self.bypass_roi_publish else "comparison" if self.bypass_roi else None,
                "single_triangle_bytes": sum(r["bytes"] for r in self.bypass_roi + self.bypass_roi_publish),
                "single_triangle_full_bytes": sum(r["full_bytes"] for r in self.bypass_roi + self.bypass_roi_publish),
                "bytes": byte_count, "full_bytes": full_count,
                "reduction_percent": round(100 * (1 - byte_count / full_count), 3)
                    if full_count and not errors else None,
                "errors": errors, "missing": missing}

    def metrics_report(self):
        errors = list(self.metrics_errors)
        missing = []
        modes = {0: 0, 1: 0, 2: 0}
        timed = []
        fence_timed = []
        for r in self.metrics:
            mode, size, upload = r["mode"], r["decoded_bytes"], r["upload_bytes"]
            hz, dispatch, publish = r["qpc_hz"], r["dispatch_ticks"], r["publish_ticks"]
            if not (mode in modes and 0 < size <= 8388608 and size % 4 == 0 and
                    (mode == 0 or size <= 65536) and upload == (0 if mode == 1 else size)):
                errors.append(f"invalid texture metrics at {r['frame']}")
            else:
                modes[mode] += 1
            if (hz == 0 and (dispatch or publish)) or (hz != 0 and not (
                    100000 <= hz <= 1000000000 and dispatch <= 4294967295 and publish <= 4294967295)):
                errors.append(f"invalid QPC metrics at {r['frame']}")
            elif hz:
                timed.append(r)
            if "fence_wait_valid" in r or "fence_wait_ticks" in r:
                valid, wait = r.get("fence_wait_valid"), r.get("fence_wait_ticks")
                if not ((valid == 0 and wait == 0) or
                        (valid == 1 and hz and wait is not None and 0 <= wait <= dispatch)):
                    errors.append(f"invalid fence wait metrics at {r['frame']}")
                elif valid:
                    fence_timed.append(r)
        if sorted(r["frame"] for r in self.metrics) != sorted(r["frame"] for r in self.bypass_roi_publish):
            missing.append("unmatched pilot publication metrics")
        status = "failed" if errors else "unavailable" if not self.metrics else "incomplete" if missing else "validated"
        return {"status": status, "records": len(self.metrics), "hits": modes[1], "misses": modes[2],
                "uncached": modes[0], "decoded_bytes": sum(r["decoded_bytes"] for r in self.metrics),
                "upload_bytes": sum(r["upload_bytes"] for r in self.metrics),
                "timed_samples": len(timed),
                "dispatch_ms": round(sum(r["dispatch_ticks"] * 1000 / r["qpc_hz"] for r in timed), 3) if timed else None,
                "publish_ms": round(sum(r["publish_ticks"] * 1000 / r["qpc_hz"] for r in timed), 3) if timed else None,
                "fence_timed_samples": len(fence_timed),
                "fence_wait_ms": round(sum(r["fence_wait_ticks"] * 1000 / r["qpc_hz"] for r in fence_timed), 3) if fence_timed else None,
                "dispatch_excluding_fence_ms": round(sum((r["dispatch_ticks"] - r["fence_wait_ticks"]) * 1000 / r["qpc_hz"] for r in fence_timed), 3) if fence_timed else None,
                "errors": errors, "missing": missing}

    def oracle_report(self):
        errors = list(self.oracle_errors)
        missing = []
        for r in self.oracle:
            if not (0 < r["cpu_written"] <= 256000 and 0 < r["gpu_written"] <= 256000 and
                    r["cpu_written"] == r["both"] + r["cpu_only"] and
                    r["gpu_written"] == r["both"] + r["gpu_only"] and
                    r["both"] == r["exact"] + r["one_lsb"] + r["edge_color"] and
                    r["cpu_only"] + r["gpu_only"] == r["edge_coverage"] and
                    r["both"] + r["cpu_only"] + r["gpu_only"] <= 256000 and
                    not (r["unexplained"] or r["outside"] or r["invalid"])):
                errors.append(f"invalid pilot oracle counts at {r['frame']}")
            if not ((r["qpc_hz"] == 0 and r["cpu_ticks"] == 0) or
                    (100000 <= r["qpc_hz"] <= 1000000000 and r["cpu_ticks"] <= 4294967295)):
                errors.append(f"invalid pilot oracle timing at {r['frame']}")
        if sorted((r["frame"], r["gpu_written"]) for r in self.oracle) != sorted(
                (r["frame"], r["written"]) for r in self.bypass_roi_publish):
            missing.append("unmatched pilot oracle/publication evidence")
        timed = [r for r in self.oracle if r["qpc_hz"]]
        status = "failed" if errors else "unavailable" if not self.oracle else "incomplete" if missing else "validated"
        return {"status": status, "records": len(self.oracle), "timed_samples": len(timed),
                "cpu_replay_ms": round(sum(r["cpu_ticks"] * 1000 / r["qpc_hz"] for r in timed), 3) if timed else None,
                **{k: sum(r[k] for r in self.oracle) for k in ("cpu_written", "gpu_written", "both", "exact", "one_lsb", "edge_color", "edge_coverage")},
                "errors": errors, "missing": missing}

    def paired_timing_report(self):
        # Only compare the same publication, never aggregate different
        # frames or silently include untimed samples on one side.
        oracle, metrics = self.oracle_report(), self.metrics_report()
        if oracle["status"] != "validated" or metrics["status"] != "validated":
            return {"status": "unavailable", "samples": 0}
        a = {r["frame"]: r for r in self.oracle}
        b = {r["frame"]: r for r in self.metrics}
        if len(a) != len(self.oracle) or len(b) != len(self.metrics) or a.keys() != b.keys():
            return {"status": "failed", "samples": 0, "reason": "duplicate or unmatched frames"}
        pairs = []
        for frame, r in a.items():
            m = b[frame]
            if not r["qpc_hz"] or not m["qpc_hz"]:
                continue
            if r["qpc_hz"] != m["qpc_hz"]:
                return {"status": "failed", "samples": 0, "reason": "mismatched QPC frequency"}
            hz = r["qpc_hz"]
            pairs.append((r["cpu_ticks"] * 1000 / hz, m["dispatch_ticks"] * 1000 / hz,
                          m["publish_ticks"] * 1000 / hz, m["mode"]))
        def aggregate(rows):
            cpu = sum(r[0] for r in rows)
            dispatch = sum(r[1] for r in rows)
            publish = sum(r[2] for r in rows)
            return {"samples": len(rows), "cpu_replay_ms": round(cpu, 3),
                    "gpu_dispatch_ms": round(dispatch, 3), "cpu_publication_ms": round(publish, 3),
                    "gpu_plus_publication_ms": round(dispatch + publish, 3),
                    "replay_over_gpu_plus_publication": round(cpu / (dispatch + publish), 3)
                        if rows and cpu > 0 and dispatch + publish > 0 else None}
        return {"status": "validated" if pairs else "unavailable",
                **aggregate(pairs), "untimed_samples": len(a) - len(pairs),
                "by_texture_mode": {str(mode): aggregate([r for r in pairs if r[3] == mode])
                                    for mode in (0, 1, 2)}}

    def report(self, clean):
        errors = list(self.errors)
        missing = []
        totals = {}
        def require(name, keys):
            f = self.summaries[name]
            absent = [key for key in keys if key not in f]
            if absent:
                missing.append(f"{name}: missing {','.join(absent)}")
                return None
            return f

        def check(ok, message):
            if not ok:
                errors.append(message)

        for name, rows in self.records.items():
            totals[name] = {"passes": len(rows), "draws": sum(r.get("draws", 0) for r in rows)}
        totals["GPU_CPU_REPLAY"]["exact_pixels"] = sum(r["exact"] for r in self.records["GPU_CPU_REPLAY"])
        totals["GPU_RETIREMENT"]["gpu_tested"] = sum(r["gpu_tested"] for r in self.records["GPU_RETIREMENT"])
        totals["GPU_RETIREMENT"]["cpu_written"] = sum(r["cpu_written"] for r in self.records["GPU_RETIREMENT"])
        result = {"status": "unavailable", "requested": self.requested, "active": self.active,
                  "limits": self.limits, "summaries": self.summaries, "totals": totals,
                  "errors": errors, "missing": missing, "roi": self.roi_report(), "metrics": self.metrics_report(), "oracle": self.oracle_report(), "paired_timing": self.paired_timing_report()}
        if self.requested != "vulkan" and not self.limits and not any(self.records.values()):
            return result
        if self.init_count != 1:
            missing.append("one complete MSI init required")
        if not clean:
            missing.append("clean session required")
        if self.active != "vulkan":
            missing.append("active Vulkan presentation required")

        auto = require("GPU_SHADOW_AUTO", ("sampled", "empty", "passed", "differences", "errors",
                                           "skipped", "disabled", "counts_ok"))
        bypass = require("GPU_BYPASS", ("sampled_frames", "empty", "attempted", "passed",
                                         "failed", "disabled", "written", "changed"))
        vk = require("SESSION_VULKAN", ("frames", "skipped", "recreates"))
        if auto:
            check(auto["sampled"] == sum(auto[k] for k in ("empty", "passed", "differences", "errors", "skipped"))
                  and auto["counts_ok"] == 1, "automatic sampler accounting mismatch")
            check(not any(auto[k] for k in ("differences", "errors", "disabled")), "automatic sampler failed")
            if not self.limits.get("passes") or auto["passed"] != self.limits["passes"]:
                missing.append("automatic pass limit not reached")
        if bypass:
            check(bypass["attempted"] == bypass["passed"] + bypass["failed"] and
                  bypass["sampled_frames"] == bypass["empty"] + bypass["attempted"],
                  "single-triangle pilot accounting mismatch")
            check(not bypass["failed"] and not bypass["disabled"], "single-triangle pilot failed")
            check(bypass["changed"] <= bypass["written"] and
                  len(self.bypass) == bypass["passed"] and
                  sum(r.get("written", 0) for r in self.bypass) == bypass["written"],
                  "single-triangle pilot record mismatch")
            if not self.limits.get("bypass") or bypass["passed"] != self.limits["bypass"]:
                missing.append("single-triangle pilot limit not reached")
        if vk:
            if not vk["frames"]:
                missing.append("no Vulkan presentation")
            # Recreates/skips are informational; no assertion that every
            # window transition must be free of legitimate recreates.

        # Require matching per-frame stage records, not just total counts.
        # One manual F12 comparison can share neither the auto nor pilot scope.
        shadow = self.records["GPU_SHADOW"]
        frames = sorted(r["frame"] for r in shadow)
        for name in self.STAGES:
            if sorted(r["frame"] for r in self.records[name]) != frames:
                missing.append(f"{name}: unmatched stage records")
        if auto:
            check(sum(r["scope"] in ("auto", "pilot") for r in shadow) == auto["passed"],
                  "automatic sample count differs from shadow records")
        if not shadow:
            missing.append("no full shadow comparison")
        for r in self.records["GPU_CPU_REPLAY"]:
            check(r["exact"] == 256000 and r["mask_diff"] == 0 and r["written"] > 0,
                  f"CPU replay mismatch at {r['frame']}")
        for r in shadow:
            check(r["active"] == r["exact"] + r["one_lsb"] + r["edge"] and
                  not r["unexplained"] and not r["outside_bounds"],
                  f"shadow mismatch at {r['frame']}")
        for r in self.records["GPU_TAGGED_BATCH"]:
            check(r["written"] + r["untouched"] == 256000 and r["written"] > 0 and
                  not any(r[k] for k in ("color_diff", "coverage_diff", "invalid", "provenance_diff")),
                  f"tagged batch mismatch at {r['frame']}")
        for r in self.records["GPU_RETIREMENT"]:
            check(r["gpu_tested"] in (0, 1) and not r["errors"] and r["cpu_written"] > 0,
                  f"retirement mismatch at {r['frame']}")
        for r in self.records["GPU_COVERAGE"]:
            check(not any(r[k] for k in ("unexplained", "outside", "invalid")),
                  f"coverage mismatch at {r['frame']}")
        if not totals["GPU_RETIREMENT"]["gpu_tested"]:
            missing.append("no homogeneous GPU retirement exercise")
        # Cross-check matching draw counts and write counts for full samples.
        if all(sorted(r["frame"] for r in self.records[n]) == frames for n in self.STAGES):
            ordered = [sorted(self.records[n], key=lambda r: r["frame"]) for n in self.STAGES]
            for cpu, sh, tag, ret, cov in zip(*ordered):
                check(cpu["draws"] == sh["draws"] == tag["draws"] == ret["draws"] and
                      cpu["written"] == ret["cpu_written"] == cov["cpu_written"] and
                      tag["written"] == cov["gpu_written"] and
                      (not ret["gpu_tested"] or ret["gpu_written"] == tag["written"]),
                      f"cross-stage counts mismatch at {cpu['frame']}")

        for name in ("GPU_DEFER_AUDIT", "GPU_DEFER_STRICT"):
            audit = require(name, ("eligible", "mapped", "invalidated", "unknown", "groups",
                                   "group_draws", "multi_groups", "multi_draws", "max_group", "counts_ok"))
            if audit:
                check(audit["counts_ok"] == 1 and
                      audit["eligible"] == audit["mapped"] + audit["invalidated"] + audit["unknown"] and
                      audit["invalidated"] == audit["group_draws"] and
                      audit["multi_groups"] <= audit["groups"] <= audit["group_draws"] and
                      audit["multi_draws"] <= audit["group_draws"] and audit["max_group"] <= 32,
                      f"{name}: accounting mismatch")
        dual = require("GPU_DUAL_VIEW", ("mapped", "guard", "checks", "mismatch", "thread_checks", "foreign"))
        release = require("GPU_DUAL_VIEW_RELEASE", ("success",))
        if dual:
            check(dual["guard"] == 0 and dual["mismatch"] == 0, "shared mapping mismatch or unexpected protection")
            if dual["mapped"] != 1 or not dual["checks"] or not dual["thread_checks"]:
                missing.append("shared mapping not exercised")
            # Foreign API calls invalidate a same-thread-only pilot assumption,
            # not the correctness of the current immediate renderer.
            if dual["foreign"]:
                missing.append("foreign API calls observed")
        if release:
            check(release["success"] == 1, "shared mapping release failed")
        result["status"] = "failed" if errors else "incomplete" if missing else "validated"
        return result


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
    opacity = OpacityAudit()
    gpu = GpuAudit()
    try:
        with path.open("r", encoding="utf-8", errors="replace") as log:
            for raw in log:
                line = raw.strip()
                opacity.feed(line)
                gpu.feed(line)
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
        result["opacity_audit"] = {"status": "unavailable", "errors": []}
        result["gpu_diagnostics"] = gpu.report(False)
        result["status"] = "unreadable"
        result["errors"].append(str(e))
        return result

    result["opacity_audit"] = opacity.report()
    if result["first_chance_av_count"]:
        result["status"] = "crash_observed"
    elif result["msi_exit_seen"] and result["shutdown_complete"]:
        if result["heap_exit_live"] is None:
            result["status"] = "clean_exit_heap_unknown"
        elif result["heap_exit_live"] == 0:
            result["status"] = "clean_exit"
        else:
            result["status"] = "exit_with_live_heaps"
    result["gpu_diagnostics"] = gpu.report(result["status"] == "clean_exit")
    return result

def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", type=Path, nargs="+")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--assert-clean", action="store_true")
    parser.add_argument("--assert-gpu-roi", action="store_true",
                        help="Require complete current GPU diagnostics and exact full-vs-ROI comparison")
    parser.add_argument("--assert-gpu-metrics", action="store_true",
                        help="Require validated GPU diagnostics, ROI publication and matching pilot metrics")
    parser.add_argument("--assert-gpu-oracle", action="store_true",
                        help="Require validated GPU diagnostics, ROI, metrics and single-triangle CPU oracle")
    parser.add_argument("--assert-gpu-timing", action="store_true",
                        help="Require all pilot gates and timed CPU/GPU pairs for every publication")
    parser.add_argument("--assert-gpu-diagnostics", action="store_true",
                        help="Require complete current GPU diagnostic evidence (not ownership proof)")
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
                f"space_suppressed={r['space_char_suppressed']} "
                f"opacity={r['opacity_audit']['status']} "
                f"gpu_diagnostics={r['gpu_diagnostics']['status']} "
                f"roi={r['gpu_diagnostics']['roi']['status']} "
                f"metrics={r['gpu_diagnostics']['metrics']['status']}")
            for error in r["errors"]:
                print(f"  ERROR: {error}", file=sys.stderr)
    if args.assert_gpu_roi and any(r["gpu_diagnostics"]["status"] != "validated" or
                                   r["gpu_diagnostics"]["roi"]["status"] != "validated" for r in reports):
        return 1
    if args.assert_gpu_metrics and any(r["gpu_diagnostics"]["status"] != "validated" or
                                       r["gpu_diagnostics"]["roi"]["status"] != "validated" or
                                       r["gpu_diagnostics"]["metrics"]["status"] != "validated" for r in reports):
        return 1
    if args.assert_gpu_oracle and any(r["gpu_diagnostics"]["status"] != "validated" or
                                      any(r["gpu_diagnostics"][k]["status"] != "validated" for k in ("roi", "metrics", "oracle")) for r in reports):
        return 1
    if args.assert_gpu_timing and any(r["gpu_diagnostics"]["status"] != "validated" or
                                      any(r["gpu_diagnostics"][k]["status"] != "validated" for k in ("roi", "metrics", "oracle", "paired_timing")) or
                                      r["gpu_diagnostics"]["paired_timing"].get("untimed_samples") != 0 for r in reports):
        return 1
    if args.assert_gpu_diagnostics and any(r["gpu_diagnostics"]["status"] != "validated" for r in reports):
        return 1
    if args.assert_clean and any(r["status"] != "clean_exit" for r in reports):
        return 1
    return 2 if any(r["status"] == "unreadable" for r in reports) else 0

if __name__ == "__main__":
    sys.exit(main())
