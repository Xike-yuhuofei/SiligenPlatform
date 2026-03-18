#!/usr/bin/env python3
# -*- coding: ascii -*-
"""
DXF glue-point spacing optimizer (scheme C: geometry-based resampling).

Reads DXF, flattens geometry into polylines, resamples at a target spacing,
optionally enforces a global minimum spacing, and outputs CSV + JSON report.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import sys
from typing import Iterable, List, Tuple

try:
    import ezdxf  # type: ignore
except Exception as exc:  # pragma: no cover - runtime dependency
    print(f"Error: ezdxf not available: {exc}", file=sys.stderr)
    sys.exit(2)

Point = Tuple[float, float]


def distance(a: Point, b: Point) -> float:
    dx = b[0] - a[0]
    dy = b[1] - a[1]
    return math.hypot(dx, dy)


def dedup_consecutive(points: Iterable[Point], eps: float = 1e-9) -> List[Point]:
    out: List[Point] = []
    last: Point | None = None
    for p in points:
        if last is None:
            out.append(p)
            last = p
            continue
        if distance(last, p) > eps:
            out.append(p)
            last = p
    return out


def _flatten_entity(entity, tol: float) -> List[Point]:
    pts: List[Point] = []
    try:
        if hasattr(entity, "flattening"):
            pts = [(float(p[0]), float(p[1])) for p in entity.flattening(tol)]
    except Exception:
        pts = []

    if pts:
        return dedup_consecutive(pts)

    # Fallback for common types
    etype = entity.dxftype()
    if etype == "LINE":
        start = entity.dxf.start
        end = entity.dxf.end
        pts = [(float(start[0]), float(start[1])), (float(end[0]), float(end[1]))]
    elif etype in ("LWPOLYLINE", "POLYLINE"):
        try:
            pts = [(float(p[0]), float(p[1])) for p in entity.get_points("xy")]
        except Exception:
            pts = []
    elif etype == "CIRCLE":
        center = entity.dxf.center
        radius = float(entity.dxf.radius)
        if radius > 0:
            # approximate circle as polygon
            segs = max(12, int(math.ceil((2 * math.pi * radius) / max(tol, 0.01))))
            pts = [
                (
                    float(center[0]) + radius * math.cos(2 * math.pi * i / segs),
                    float(center[1]) + radius * math.sin(2 * math.pi * i / segs),
                )
                for i in range(segs + 1)
            ]
    elif etype == "ARC":
        center = entity.dxf.center
        radius = float(entity.dxf.radius)
        start = math.radians(float(entity.dxf.start_angle))
        end = math.radians(float(entity.dxf.end_angle))
        if radius > 0:
            # normalize CCW span
            span = end - start
            if span <= 0:
                span += 2 * math.pi
            length = radius * span
            segs = max(4, int(math.ceil(length / max(tol, 0.01))))
            pts = [
                (
                    float(center[0]) + radius * math.cos(start + span * i / segs),
                    float(center[1]) + radius * math.sin(start + span * i / segs),
                )
                for i in range(segs + 1)
            ]

    return dedup_consecutive(pts)


def resample_polyline(
    points: List[Point],
    step: float,
    include_start: bool = True,
    include_end: bool = True,
    end_min_ratio: float = 0.5,
) -> List[Point]:
    if not points:
        return []

    out: List[Point] = []
    if include_start:
        out.append(points[0])

    last = points[0]
    accumulated = 0.0
    eps = 1e-9

    for p in points[1:]:
        seg_len = distance(last, p)
        if seg_len < eps:
            last = p
            continue
        while accumulated + seg_len >= step:
            ratio = (step - accumulated) / seg_len
            nx = last[0] + ratio * (p[0] - last[0])
            ny = last[1] + ratio * (p[1] - last[1])
            new_pt = (nx, ny)
            out.append(new_pt)
            last = new_pt
            seg_len = distance(last, p)
            accumulated = 0.0
        accumulated += seg_len
        last = p

    if include_end:
        end = points[-1]
        if not out:
            out.append(end)
        else:
            if distance(out[-1], end) >= step * end_min_ratio:
                out.append(end)

    return out


class SpatialHash:
    def __init__(self, cell_size: float) -> None:
        self.cell = max(cell_size, 1e-6)
        self.grid: dict[Tuple[int, int], List[int]] = {}

    def _cell_of(self, p: Point) -> Tuple[int, int]:
        return (int(math.floor(p[0] / self.cell)), int(math.floor(p[1] / self.cell)))

    def add(self, p: Point, idx: int) -> None:
        key = self._cell_of(p)
        self.grid.setdefault(key, []).append(idx)

    def nearby_indices(self, p: Point) -> Iterable[int]:
        cx, cy = self._cell_of(p)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                key = (cx + dx, cy + dy)
                if key in self.grid:
                    for idx in self.grid[key]:
                        yield idx


def enforce_global_min(points: List[Point], min_dist: float) -> Tuple[List[Point], int]:
    if min_dist <= 0:
        return points, 0
    grid = SpatialHash(min_dist)
    kept: List[Point] = []
    skipped = 0
    for p in points:
        too_close = False
        for idx in grid.nearby_indices(p):
            if distance(p, kept[idx]) < min_dist:
                too_close = True
                break
        if too_close:
            skipped += 1
            continue
        grid.add(p, len(kept))
        kept.append(p)
    return kept, skipped


def analyze_spacing(points: List[Point], thresholds: List[float]) -> dict:
    if not points:
        return {"min_distance": None, "pair_counts": {}}

    thresholds = sorted([t for t in thresholds if t > 0])
    max_th = thresholds[-1] if thresholds else 0.0
    grid = SpatialHash(max_th if max_th > 0 else 1.0)

    min_dist = float("inf")
    counts = {t: 0 for t in thresholds}

    for i, p in enumerate(points):
        for idx in grid.nearby_indices(p):
            d = distance(p, points[idx])
            if d < min_dist:
                min_dist = d
            for t in thresholds:
                if d < t:
                    counts[t] += 1
        grid.add(p, i)

    if min_dist == float("inf"):
        min_dist = None

    return {"min_distance": min_dist, "pair_counts": counts}


def main() -> int:
    parser = argparse.ArgumentParser(description="DXF glue-point spacing optimizer")
    parser.add_argument("--dxf", required=True, help="DXF file path")
    parser.add_argument("--out", default="", help="Output CSV path")
    parser.add_argument("--report", default="", help="Output report JSON path")
    parser.add_argument("--step", type=float, default=2.0, help="Resample spacing (mm)")
    parser.add_argument(
        "--flatten-tol",
        type=float,
        default=0.05,
        help="Flattening tolerance for curves (mm)",
    )
    parser.add_argument(
        "--global-min",
        type=float,
        default=None,
        help="Global minimum spacing (mm), default=step; set 0 to disable",
    )
    parser.add_argument(
        "--thresholds",
        default="1,2",
        help="Comma-separated thresholds for analysis (mm)",
    )
    parser.add_argument("--include-end", action="store_true", help="Include segment endpoints if far enough")
    parser.add_argument(
        "--end-min-ratio",
        type=float,
        default=0.5,
        help="Endpoint inclusion threshold ratio (distance >= step*ratio)",
    )

    args = parser.parse_args()
    dxf_path = args.dxf
    if not os.path.isfile(dxf_path):
        print(f"Error: DXF not found: {dxf_path}", file=sys.stderr)
        return 2

    out_csv = args.out
    if not out_csv:
        base, _ = os.path.splitext(dxf_path)
        out_csv = base + "_glue_points_optimized.csv"

    report_path = args.report
    if not report_path:
        base, _ = os.path.splitext(out_csv)
        report_path = base + "_report.json"

    step = float(args.step)
    if step <= 0:
        print("Error: --step must be > 0", file=sys.stderr)
        return 2

    flatten_tol = float(args.flatten_tol)
    if flatten_tol <= 0:
        flatten_tol = min(0.05, step / 10.0)

    global_min = args.global_min if args.global_min is not None else step
    if global_min < 0:
        global_min = 0.0

    thresholds: List[float] = []
    for part in args.thresholds.split(","):
        part = part.strip()
        if not part:
            continue
        try:
            thresholds.append(float(part))
        except ValueError:
            print(f"Warning: invalid threshold ignored: {part}", file=sys.stderr)
    if not thresholds:
        thresholds = [step]

    doc = ezdxf.readfile(dxf_path)
    msp = doc.modelspace()

    records: List[dict] = []
    all_candidates: List[Point] = []
    meta: List[Tuple[int, str, int]] = []

    segment_index = 0
    for entity in msp:
        etype = entity.dxftype()
        pts = _flatten_entity(entity, flatten_tol)
        if len(pts) < 2:
            continue
        sampled = resample_polyline(
            pts,
            step=step,
            include_start=True,
            include_end=args.include_end,
            end_min_ratio=args.end_min_ratio,
        )
        if not sampled:
            continue
        for idx, p in enumerate(sampled, start=1):
            all_candidates.append(p)
            meta.append((segment_index, etype, idx))
        segment_index += 1

    kept_points, skipped = enforce_global_min(all_candidates, global_min)

    # Rebuild records aligned with kept_points
    if len(kept_points) != len(all_candidates):
        # rebuild by re-filtering to keep meta aligned
        grid = SpatialHash(global_min if global_min > 0 else 1.0)
        records = []
        for i, p in enumerate(all_candidates):
            if global_min > 0:
                too_close = False
                for idx in grid.nearby_indices(p):
                    if distance(p, records[idx]["_pt"]) < global_min:
                        too_close = True
                        break
                if too_close:
                    continue
            seg_idx, etype, trig_idx = meta[i]
            rec = {
                "index": len(records) + 1,
                "segment_index": seg_idx,
                "segment_type": etype,
                "trigger_index": trig_idx,
                "x_mm": p[0],
                "y_mm": p[1],
                "_pt": p,
            }
            grid.add(p, len(records))
            records.append(rec)
        for rec in records:
            rec.pop("_pt", None)
    else:
        for i, p in enumerate(kept_points):
            seg_idx, etype, trig_idx = meta[i]
            records.append(
                {
                    "index": i + 1,
                    "segment_index": seg_idx,
                    "segment_type": etype,
                    "trigger_index": trig_idx,
                    "x_mm": p[0],
                    "y_mm": p[1],
                }
            )

    # Write CSV
    os.makedirs(os.path.dirname(out_csv), exist_ok=True)
    with open(out_csv, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["index", "segment_index", "segment_type", "trigger_index", "x_mm", "y_mm"])
        for rec in records:
            writer.writerow(
                [
                    rec["index"],
                    rec["segment_index"],
                    rec["segment_type"],
                    rec["trigger_index"],
                    f"{rec['x_mm']:.6f}",
                    f"{rec['y_mm']:.6f}",
                ]
            )

    analysis = analyze_spacing([(r["x_mm"], r["y_mm"]) for r in records], thresholds)
    report = {
        "dxf": dxf_path,
        "output_csv": out_csv,
        "point_count": len(records),
        "skipped_due_to_global_min": skipped,
        "step_mm": step,
        "flatten_tol_mm": flatten_tol,
        "global_min_mm": global_min,
        "thresholds_mm": thresholds,
        "analysis": analysis,
    }

    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)

    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
