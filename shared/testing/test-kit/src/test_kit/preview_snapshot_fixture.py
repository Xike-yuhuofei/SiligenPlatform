from __future__ import annotations

import copy
import json
import math
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import Any


PointLike = Mapping[str, object] | Sequence[float]
FIXTURE_RELATIVE_PATH = Path(
    "shared/contracts/application/fixtures/responses/dxf.preview.snapshot.success.json"
)


def _workspace_root() -> Path:
    current = Path(__file__).resolve()
    for parent in current.parents:
        if (parent / FIXTURE_RELATIVE_PATH).exists():
            return parent
    raise FileNotFoundError(f"workspace root not found for fixture {FIXTURE_RELATIVE_PATH}")


def _fixture_path() -> Path:
    return _workspace_root() / FIXTURE_RELATIVE_PATH


def _round_mm(value: float) -> float:
    return round(float(value), 6)


def _normalize_points(points: Sequence[PointLike]) -> list[dict[str, float]]:
    normalized: list[dict[str, float]] = []
    for point in points:
        if isinstance(point, Mapping):
            x = float(point["x"])
            y = float(point["y"])
        else:
            if len(point) != 2:
                raise ValueError("preview point must contain exactly x/y")
            x = float(point[0])
            y = float(point[1])
        if not math.isfinite(x) or not math.isfinite(y):
            raise ValueError("preview point contains non-finite coordinates")
        normalized.append({"x": x, "y": y})
    return normalized


def _point_pairs(points: Sequence[dict[str, float]]) -> list[tuple[float, float]]:
    return [(float(point["x"]), float(point["y"])) for point in points]


def _polyline_length(points: Sequence[tuple[float, float]]) -> float:
    if len(points) < 2:
        return 0.0
    total = 0.0
    for index in range(1, len(points)):
        prev_x, prev_y = points[index - 1]
        curr_x, curr_y = points[index]
        total += math.hypot(curr_x - prev_x, curr_y - prev_y)
    return total


def build_glue_reveal_lengths(points: Sequence[PointLike]) -> list[float]:
    normalized_points = _point_pairs(_normalize_points(points))
    reveal_lengths: list[float] = []
    cumulative_length = 0.0
    previous_point: tuple[float, float] | None = None
    for point_x, point_y in normalized_points:
        if previous_point is not None:
            cumulative_length += math.hypot(point_x - previous_point[0], point_y - previous_point[1])
        reveal_lengths.append(_round_mm(cumulative_length))
        previous_point = (point_x, point_y)
    return reveal_lengths


def _project_display_reveal_lengths(
    glue_points: Sequence[tuple[float, float]],
    motion_preview: Sequence[tuple[float, float]],
) -> list[float]:
    if not glue_points:
        return []
    if len(motion_preview) < 2:
        return [0.0 for _ in glue_points]

    cumulative_lengths = [0.0]
    for index in range(1, len(motion_preview)):
        prev_x, prev_y = motion_preview[index - 1]
        curr_x, curr_y = motion_preview[index]
        cumulative_lengths.append(
            cumulative_lengths[-1] + math.hypot(curr_x - prev_x, curr_y - prev_y)
        )

    reveal_lengths: list[float] = []
    search_segment_index = 0
    previous_length = 0.0
    for glue_x, glue_y in glue_points:
        best_distance_sq: float | None = None
        best_length = previous_length
        best_segment_index = search_segment_index
        for segment_index in range(search_segment_index, len(motion_preview) - 1):
            start_x, start_y = motion_preview[segment_index]
            end_x, end_y = motion_preview[segment_index + 1]
            dx = end_x - start_x
            dy = end_y - start_y
            segment_length_sq = (dx * dx) + (dy * dy)
            if segment_length_sq <= 1e-12:
                continue
            point_dx = glue_x - start_x
            point_dy = glue_y - start_y
            ratio = max(0.0, min(1.0, ((point_dx * dx) + (point_dy * dy)) / segment_length_sq))
            projected_x = start_x + (dx * ratio)
            projected_y = start_y + (dy * ratio)
            distance_sq = ((glue_x - projected_x) ** 2) + ((glue_y - projected_y) ** 2)
            segment_length = math.sqrt(segment_length_sq)
            candidate_length = max(
                previous_length,
                cumulative_lengths[segment_index] + (segment_length * ratio),
            )
            if best_distance_sq is None or distance_sq < best_distance_sq:
                best_distance_sq = distance_sq
                best_length = candidate_length
                best_segment_index = segment_index
        reveal_lengths.append(_round_mm(best_length))
        previous_length = best_length
        search_segment_index = best_segment_index
    return reveal_lengths


def load_preview_snapshot_success_fixture() -> dict[str, Any]:
    return json.loads(_fixture_path().read_text(encoding="utf-8"))


def load_preview_snapshot_success_result() -> dict[str, Any]:
    payload = load_preview_snapshot_success_fixture()
    result = payload.get("result")
    if not isinstance(result, dict):
        raise ValueError("preview success fixture missing result block")
    return copy.deepcopy(result)


def build_preview_snapshot_success_result(
    *,
    snapshot_id: str | None = None,
    snapshot_hash: str | None = None,
    plan_id: str | None = None,
    preview_source: str | None = None,
    preview_kind: str | None = None,
    preview_state: str | None = None,
    confirmed_at: str | None = None,
    preview_validation_classification: str | None = None,
    preview_exception_reason: str | None = None,
    preview_failure_reason: str | None = None,
    preview_diagnostic_code: str | None = None,
    segment_count: int | None = None,
    glue_points: Sequence[PointLike] | None = None,
    glue_reveal_lengths_mm: Sequence[float] | None = None,
    motion_preview: Sequence[PointLike] | None = None,
    motion_preview_source_point_count: int | None = None,
    execution_point_count: int | None = None,
    total_length_mm: float | None = None,
    estimated_time_s: float | None = None,
    generated_at: str | None = None,
    dry_run: bool | None = None,
    preview_binding_layout_id: str | None = None,
    preview_binding_diagnostic_code: str | None = None,
    preview_binding_failure_reason: str | None = None,
) -> dict[str, Any]:
    result = load_preview_snapshot_success_result()
    binding_block = result["preview_binding"]
    motion_preview_block = result["motion_preview"]

    normalized_glue_points = (
        _normalize_points(glue_points)
        if glue_points is not None
        else _normalize_points(result["glue_points"])
    )
    normalized_motion_preview = (
        _normalize_points(motion_preview)
        if motion_preview is not None
        else _normalize_points(motion_preview_block["polyline"])
    )
    glue_pairs = _point_pairs(normalized_glue_points)
    motion_pairs = _point_pairs(normalized_motion_preview)
    resolved_glue_reveal_lengths = (
        [_round_mm(value) for value in glue_reveal_lengths_mm]
        if glue_reveal_lengths_mm is not None
        else build_glue_reveal_lengths(normalized_glue_points)
    )
    resolved_motion_source_point_count = int(
        motion_preview_source_point_count
        if motion_preview_source_point_count is not None
        else motion_preview_block.get("source_point_count", len(normalized_motion_preview))
    )
    resolved_motion_source_point_count = max(
        resolved_motion_source_point_count,
        len(normalized_motion_preview),
    )
    resolved_execution_point_count = int(
        execution_point_count
        if execution_point_count is not None
        else result.get("execution_point_count", resolved_motion_source_point_count)
    )
    resolved_execution_point_count = max(
        resolved_execution_point_count,
        resolved_motion_source_point_count,
    )
    is_sampled = len(normalized_motion_preview) < resolved_motion_source_point_count
    sampling_strategy = (
        "execution_trajectory_geometry_preserving_clamp"
        if is_sampled
        else "execution_trajectory_geometry_preserving"
    )
    resolved_total_length_mm = (
        float(total_length_mm)
        if total_length_mm is not None
        else (resolved_glue_reveal_lengths[-1] if resolved_glue_reveal_lengths else 0.0)
    )

    result["snapshot_id"] = snapshot_id if snapshot_id is not None else result["snapshot_id"]
    result["snapshot_hash"] = snapshot_hash if snapshot_hash is not None else result["snapshot_hash"]
    result["plan_id"] = plan_id if plan_id is not None else result["plan_id"]
    result["preview_source"] = preview_source if preview_source is not None else result["preview_source"]
    result["preview_kind"] = preview_kind if preview_kind is not None else result["preview_kind"]
    result["preview_state"] = preview_state if preview_state is not None else result["preview_state"]
    result["confirmed_at"] = confirmed_at if confirmed_at is not None else result["confirmed_at"]
    result["preview_validation_classification"] = (
        preview_validation_classification
        if preview_validation_classification is not None
        else "pass"
    )
    result["preview_exception_reason"] = (
        preview_exception_reason if preview_exception_reason is not None else ""
    )
    result["preview_failure_reason"] = (
        preview_failure_reason if preview_failure_reason is not None else ""
    )
    result["preview_diagnostic_code"] = (
        preview_diagnostic_code if preview_diagnostic_code is not None else ""
    )
    result["segment_count"] = int(segment_count if segment_count is not None else result["segment_count"])
    result["glue_points"] = normalized_glue_points
    result["point_count"] = len(normalized_glue_points)
    result["glue_point_count"] = len(normalized_glue_points)
    result["glue_reveal_lengths_mm"] = resolved_glue_reveal_lengths
    result["execution_point_count"] = resolved_execution_point_count
    result["total_length_mm"] = float(resolved_total_length_mm)
    if estimated_time_s is not None:
        result["estimated_time_s"] = float(estimated_time_s)
    if generated_at is not None:
        result["generated_at"] = generated_at
    if dry_run is not None:
        result["dry_run"] = bool(dry_run)

    motion_preview_block["polyline"] = normalized_motion_preview
    motion_preview_block["point_count"] = len(normalized_motion_preview)
    motion_preview_block["source_point_count"] = resolved_motion_source_point_count
    motion_preview_block["is_sampled"] = is_sampled
    motion_preview_block["sampling_strategy"] = sampling_strategy

    binding_block["layout_id"] = (
        preview_binding_layout_id
        if preview_binding_layout_id is not None
        else binding_block.get("layout_id", "")
    )
    binding_block["glue_point_count"] = len(normalized_glue_points)
    binding_block["source_trigger_indices"] = list(range(len(normalized_glue_points)))
    binding_block["display_path_length_mm"] = _round_mm(_polyline_length(motion_pairs))
    binding_block["display_reveal_lengths_mm"] = _project_display_reveal_lengths(
        glue_pairs,
        motion_pairs,
    )
    binding_block["diagnostic_code"] = (
        preview_binding_diagnostic_code
        if preview_binding_diagnostic_code is not None
        else result["preview_diagnostic_code"]
    )
    binding_block["failure_reason"] = (
        preview_binding_failure_reason
        if preview_binding_failure_reason is not None
        else result["preview_failure_reason"]
    )

    return result


__all__ = [
    "build_glue_reveal_lengths",
    "build_preview_snapshot_success_result",
    "load_preview_snapshot_success_fixture",
    "load_preview_snapshot_success_result",
]
