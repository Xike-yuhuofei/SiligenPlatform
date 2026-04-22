from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest


THIS_DIR = Path(__file__).resolve().parent
ROOT = THIS_DIR.parents[3]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))

from runtime_gateway_harness import resolve_default_exe


SCRIPT_PATH = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_real_dxf_preview_snapshot.py"
BASELINE_PATH = ROOT / "tests" / "baselines" / "preview" / "rect_diag.preview-snapshot-baseline.json"


def _load_json(path: Path) -> dict | list:
    return json.loads(path.read_text(encoding="utf-8"))


def _assert_close(actual: float, expected: float, tolerance: float, label: str) -> None:
    delta = abs(actual - expected)
    assert delta <= tolerance, f"{label} drifted: actual={actual} expected={expected} tolerance={tolerance}"


def _single_report_dir(report_root: Path) -> Path:
    dirs = [path for path in report_root.iterdir() if path.is_dir()]
    assert len(dirs) == 1, f"expected exactly one report dir in {report_root}, got {len(dirs)}"
    return dirs[0]


def test_real_preview_snapshot_matches_rect_diag_baseline(tmp_path: Path) -> None:
    gateway_exe = resolve_default_exe("siligen_runtime_gateway.exe")
    if not gateway_exe.exists():
        pytest.skip(f"runtime gateway executable missing: {gateway_exe}")

    baseline = _load_json(BASELINE_PATH)
    report_root = tmp_path / "preview-report"
    report_root.mkdir(parents=True, exist_ok=True)

    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT_PATH),
            "--gateway-exe",
            str(gateway_exe),
            "--report-root",
            str(report_root),
        ],
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        timeout=180,
        check=False,
    )
    assert completed.returncode == 0, (
        "preview snapshot script failed\n"
        f"stdout:\n{completed.stdout}\n"
        f"stderr:\n{completed.stderr}"
    )

    report_dir = _single_report_dir(report_root)
    report_json_path = report_dir / "real-dxf-preview-snapshot.json"
    plan_prepare_json_path = report_dir / "plan-prepare.json"
    snapshot_json_path = report_dir / "snapshot.json"
    glue_points_json_path = report_dir / "glue_points.json"
    motion_preview_json_path = report_dir / "motion_preview.json"
    preview_verdict_json_path = report_dir / "preview-verdict.json"
    preview_evidence_md_path = report_dir / "preview-evidence.md"
    hmi_screenshot_path = report_dir / "hmi-preview.png"
    online_smoke_log_path = report_dir / "online-smoke.log"

    assert report_json_path.exists(), f"missing report artifact: {report_json_path}"
    assert plan_prepare_json_path.exists(), f"missing report artifact: {plan_prepare_json_path}"
    assert snapshot_json_path.exists(), f"missing snapshot artifact: {snapshot_json_path}"
    assert glue_points_json_path.exists(), f"missing glue artifact: {glue_points_json_path}"
    assert motion_preview_json_path.exists(), f"missing motion preview artifact: {motion_preview_json_path}"
    assert preview_verdict_json_path.exists(), f"missing report artifact: {preview_verdict_json_path}"
    assert preview_evidence_md_path.exists(), f"missing report artifact: {preview_evidence_md_path}"
    assert hmi_screenshot_path.exists(), f"missing HMI screenshot artifact: {hmi_screenshot_path}"
    assert online_smoke_log_path.exists(), f"missing online smoke log artifact: {online_smoke_log_path}"

    report = _load_json(report_json_path)
    plan_prepare = _load_json(plan_prepare_json_path)
    snapshot = _load_json(snapshot_json_path)
    glue_points = _load_json(glue_points_json_path)
    motion_preview_points = _load_json(motion_preview_json_path)
    preview_verdict = _load_json(preview_verdict_json_path)
    preview_evidence = preview_evidence_md_path.read_text(encoding="utf-8")

    assert report["overall_status"] == "passed"
    assert report["config_mode"] == baseline["config_mode"]
    assert report["preview_source"] == baseline["preview_source"]
    assert report["preview_kind"] == baseline["preview_kind"]
    assert report["baseline_id"]
    assert report["baseline_fingerprint"]
    assert report["production_baseline_source"] == "dxf.plan.prepare"
    assert report["dxf_file"].endswith(str(baseline["dxf_file"]).replace("/", "\\"))
    assert report["plan_id"] == plan_prepare["plan_id"]
    assert report["plan_fingerprint"] == plan_prepare["plan_fingerprint"]
    assert plan_prepare["baseline_id"] == report["baseline_id"]
    assert plan_prepare["baseline_fingerprint"] == report["baseline_fingerprint"]
    assert plan_prepare["production_baseline"]["baseline_id"] == report["baseline_id"]
    assert snapshot["preview_state"] == baseline["snapshot"]["preview_state"]
    assert snapshot["plan_id"] == plan_prepare["plan_id"]
    assert snapshot["snapshot_hash"] == plan_prepare["plan_fingerprint"]
    assert snapshot["segment_count"] == baseline["snapshot"]["segment_count"]
    assert snapshot["preview_kind"] == baseline["preview_kind"]
    assert snapshot["glue_point_count"] == baseline["snapshot"]["glue_point_count"]
    assert snapshot["point_count"] == baseline["snapshot"]["glue_point_count"]
    assert snapshot["glue_point_count"] == len(glue_points)
    assert snapshot["motion_preview"]["point_count"] == baseline["motion_preview"]["point_count"]
    assert snapshot["motion_preview"]["point_count"] == len(motion_preview_points)
    assert snapshot["motion_preview"]["source_point_count"] >= snapshot["motion_preview"]["point_count"]
    assert report["gateway_port"] > 0
    assert preview_verdict["verdict"] == "passed"
    assert preview_verdict["launch_mode"] == "online"
    assert preview_verdict["online_ready"] is True
    assert preview_verdict["baseline_id"] == report["baseline_id"]
    assert preview_verdict["baseline_fingerprint"] == report["baseline_fingerprint"]
    assert preview_verdict["production_baseline_source"] == "dxf.plan.prepare"
    assert preview_verdict["preview_source"] == baseline["preview_source"]
    assert preview_verdict["preview_kind"] == baseline["preview_kind"]
    assert preview_verdict["plan_id"] == plan_prepare["plan_id"]
    assert preview_verdict["plan_fingerprint"] == plan_prepare["plan_fingerprint"]
    assert preview_verdict["snapshot_hash"] == snapshot["snapshot_hash"]
    assert preview_verdict["geometry_semantics_match"] is True
    assert preview_verdict["order_semantics_match"] is True
    assert preview_verdict["dispense_motion_semantics_match"] is True
    assert preview_verdict["glue_point_count"] == baseline["snapshot"]["glue_point_count"]
    assert preview_verdict["motion_preview_point_count"] == baseline["motion_preview"]["point_count"]
    assert (
        preview_verdict["motion_preview_source_point_count"] >= preview_verdict["motion_preview_point_count"]
    )
    assert preview_verdict["corner_duplicate_point_count"] == 0
    assert "preview-verdict.json" in preview_evidence
    assert "plan_fingerprint" in preview_evidence
    assert "glue_points.json" in preview_evidence
    assert "motion_preview.json" in preview_evidence
    assert "hmi-preview.png" in preview_evidence

    motion_preview = snapshot["motion_preview"]
    baseline_motion_preview = baseline["motion_preview"]
    assert motion_preview["source"] == baseline_motion_preview["source"]
    assert motion_preview["kind"] == baseline_motion_preview["kind"]
    assert motion_preview["source_point_count"] >= motion_preview["point_count"]
    assert motion_preview["point_count"] == baseline_motion_preview["point_count"]
    assert motion_preview["is_sampled"] == baseline_motion_preview["is_sampled"]
    assert motion_preview["sampling_strategy"] == baseline_motion_preview["sampling_strategy"]
    assert len(motion_preview["polyline"]) == baseline_motion_preview["point_count"]

    coordinate_tolerance = float(baseline["tolerances"]["coordinate_mm"])
    length_tolerance = float(baseline["tolerances"]["length_mm"])
    time_tolerance = float(baseline["tolerances"]["time_s"])
    spacing_tolerance = float(baseline["tolerances"]["spacing_mm"])

    _assert_close(
        float(snapshot["total_length_mm"]),
        float(baseline["snapshot"]["total_length_mm"]),
        length_tolerance,
        "snapshot.total_length_mm",
    )
    _assert_close(
        float(snapshot["estimated_time_s"]),
        float(baseline["snapshot"]["estimated_time_s"]),
        time_tolerance,
        "snapshot.estimated_time_s",
    )

    glue_summary = report["glue_summary"]
    baseline_glue = baseline["glue_summary"]
    assert glue_summary["point_count"] == baseline_glue["point_count"]
    assert glue_summary["corner_duplicate_point_count"] == baseline_glue["corner_duplicate_point_count"]
    _assert_close(
        float(glue_summary["adjacent_distance_median_mm"]),
        float(baseline_glue["adjacent_distance_median_mm"]),
        spacing_tolerance,
        "glue_summary.adjacent_distance_median_mm",
    )

    for axis_name in ("x_range", "y_range"):
        actual_range = glue_summary[axis_name]
        expected_range = baseline_glue[axis_name]
        _assert_close(float(actual_range[0]), float(expected_range[0]), coordinate_tolerance, f"{axis_name}[0]")
        _assert_close(float(actual_range[1]), float(expected_range[1]), coordinate_tolerance, f"{axis_name}[1]")

    for point_name in ("first_point", "last_point"):
        actual_point = glue_summary[point_name]
        expected_point = baseline_glue[point_name]
        _assert_close(float(actual_point["x"]), float(expected_point["x"]), coordinate_tolerance, f"{point_name}.x")
        _assert_close(float(actual_point["y"]), float(expected_point["y"]), coordinate_tolerance, f"{point_name}.y")

    motion_preview_summary = report["motion_preview_geometry_summary"]
    baseline_motion_summary = baseline["motion_preview_geometry_summary"]
    assert motion_preview_summary["point_count"] == baseline_motion_summary["point_count"]
    assert motion_preview_summary["axis_aligned_segments"] > 0
    assert motion_preview_summary["diagonal_segments"] > 0
    assert (
        motion_preview_summary["axis_aligned_segments"] + motion_preview_summary["diagonal_segments"]
        == motion_preview_summary["point_count"] - 1
    )

    for axis_name in ("x_range", "y_range"):
        actual_range = motion_preview_summary[axis_name]
        expected_range = baseline_motion_summary[axis_name]
        _assert_close(
            float(actual_range[0]),
            float(expected_range[0]),
            coordinate_tolerance,
            f"motion_preview_{axis_name}[0]",
        )
        _assert_close(
            float(actual_range[1]),
            float(expected_range[1]),
            coordinate_tolerance,
            f"motion_preview_{axis_name}[1]",
        )

    for point_name in ("first_point", "last_point"):
        actual_point = motion_preview_summary[point_name]
        expected_point = baseline_motion_summary[point_name]
        _assert_close(
            float(actual_point["x"]),
            float(expected_point["x"]),
            coordinate_tolerance,
            f"motion_preview_{point_name}.x",
        )
        _assert_close(
            float(actual_point["y"]),
            float(expected_point["y"]),
            coordinate_tolerance,
            f"motion_preview_{point_name}.y",
        )

    for expected_point in baseline["glue_sample_points"]:
        index = int(expected_point["index"])
        assert 0 <= index < len(glue_points), f"glue sample index out of range: {index}"
        actual_point = glue_points[index]
        _assert_close(float(actual_point["x"]), float(expected_point["x"]), coordinate_tolerance, f"glue[{index}].x")
        _assert_close(float(actual_point["y"]), float(expected_point["y"]), coordinate_tolerance, f"glue[{index}].y")



def test_build_preview_verdict_treats_same_position_with_distinct_reveal_lengths_as_valid() -> None:
    from run_real_dxf_preview_snapshot import build_preview_verdict, summarize_glue_points

    glue_points = [
        {"x": 336.053, "y": 152.501},
        {"x": 338.638, "y": 153.484},
        {"x": 338.638, "y": 153.484},
        {"x": 336.053, "y": 152.501},
    ]
    glue_reveal_lengths_mm = [13818.0380859375, 13820.8037109375, 13848.8486328125, 13851.6142578125]
    glue_summary = summarize_glue_points(glue_points, 1e-4, glue_reveal_lengths_mm)

    verdict = build_preview_verdict(
        launch_mode="online",
        online_ready=True,
        dxf_file=Path("demo.dxf"),
        artifact_id="artifact-1",
        preview_source="planned_glue_snapshot",
        preview_kind="glue_points",
        snapshot_hash="hash-1",
        plan_id="plan-1",
        snapshot_plan_id="plan-1",
        plan_fingerprint="hash-1",
        glue_summary=glue_summary,
        motion_preview_summary={"point_count": 3},
        snapshot_payload={
            "glue_point_count": 4,
            "motion_preview": {
                "point_count": 3,
                "source_point_count": 10,
            },
        },
        confirmed=True,
        error_message="",
    )

    assert glue_summary["corner_duplicate_point_count"] == 1
    assert glue_summary["illegal_corner_duplicate_point_count"] == 0
    assert verdict["geometry_semantics_match"] is True
    assert verdict["verdict"] == "passed"
