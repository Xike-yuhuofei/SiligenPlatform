from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest


THIS_DIR = Path(__file__).resolve().parent
ROOT = THIS_DIR.parents[2]
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

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
    polyline_json_path = report_dir / "trajectory_polyline.json"
    preview_verdict_json_path = report_dir / "preview-verdict.json"
    preview_evidence_md_path = report_dir / "preview-evidence.md"

    assert report_json_path.exists(), f"missing report artifact: {report_json_path}"
    assert plan_prepare_json_path.exists(), f"missing report artifact: {plan_prepare_json_path}"
    assert snapshot_json_path.exists(), f"missing snapshot artifact: {snapshot_json_path}"
    assert polyline_json_path.exists(), f"missing polyline artifact: {polyline_json_path}"
    assert preview_verdict_json_path.exists(), f"missing report artifact: {preview_verdict_json_path}"
    assert preview_evidence_md_path.exists(), f"missing report artifact: {preview_evidence_md_path}"

    report = _load_json(report_json_path)
    plan_prepare = _load_json(plan_prepare_json_path)
    snapshot = _load_json(snapshot_json_path)
    polyline = _load_json(polyline_json_path)
    preview_verdict = _load_json(preview_verdict_json_path)
    preview_evidence = preview_evidence_md_path.read_text(encoding="utf-8")

    assert report["overall_status"] == "passed"
    assert report["config_mode"] == baseline["config_mode"]
    assert report["preview_source"] == baseline["preview_source"]
    assert report["dxf_file"].endswith(str(baseline["dxf_file"]).replace("/", "\\"))
    assert report["plan_id"] == plan_prepare["plan_id"]
    assert report["plan_fingerprint"] == plan_prepare["plan_fingerprint"]
    assert snapshot["preview_state"] == baseline["snapshot"]["preview_state"]
    assert snapshot["plan_id"] == plan_prepare["plan_id"]
    assert snapshot["snapshot_hash"] == plan_prepare["plan_fingerprint"]
    assert snapshot["segment_count"] == baseline["snapshot"]["segment_count"]
    assert snapshot["point_count"] == baseline["snapshot"]["point_count"]
    assert snapshot["polyline_point_count"] == baseline["snapshot"]["polyline_point_count"]
    assert snapshot["polyline_source_point_count"] == baseline["snapshot"]["polyline_source_point_count"]
    assert snapshot["polyline_point_count"] == len(polyline)
    assert preview_verdict["verdict"] == "passed"
    assert preview_verdict["launch_mode"] == "online"
    assert preview_verdict["online_ready"] is True
    assert preview_verdict["preview_source"] == baseline["preview_source"]
    assert preview_verdict["plan_id"] == plan_prepare["plan_id"]
    assert preview_verdict["plan_fingerprint"] == plan_prepare["plan_fingerprint"]
    assert preview_verdict["snapshot_hash"] == snapshot["snapshot_hash"]
    assert preview_verdict["geometry_semantics_match"] is True
    assert preview_verdict["order_semantics_match"] is True
    assert preview_verdict["dispense_motion_semantics_match"] is True
    assert "preview-verdict.json" in preview_evidence
    assert "plan_fingerprint" in preview_evidence

    coordinate_tolerance = float(baseline["tolerances"]["coordinate_mm"])
    length_tolerance = float(baseline["tolerances"]["length_mm"])
    time_tolerance = float(baseline["tolerances"]["time_s"])

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

    geometry_summary = report["geometry_summary"]
    baseline_geometry = baseline["geometry_summary"]
    assert geometry_summary["point_count"] == baseline_geometry["point_count"]
    assert geometry_summary["axis_aligned_segments"] == baseline_geometry["axis_aligned_segments"]
    assert geometry_summary["diagonal_segments"] == baseline_geometry["diagonal_segments"]

    for axis_name in ("x_range", "y_range"):
        actual_range = geometry_summary[axis_name]
        expected_range = baseline_geometry[axis_name]
        _assert_close(float(actual_range[0]), float(expected_range[0]), coordinate_tolerance, f"{axis_name}[0]")
        _assert_close(float(actual_range[1]), float(expected_range[1]), coordinate_tolerance, f"{axis_name}[1]")

    for point_name in ("first_point", "last_point"):
        actual_point = geometry_summary[point_name]
        expected_point = baseline_geometry[point_name]
        _assert_close(float(actual_point["x"]), float(expected_point["x"]), coordinate_tolerance, f"{point_name}.x")
        _assert_close(float(actual_point["y"]), float(expected_point["y"]), coordinate_tolerance, f"{point_name}.y")

    for expected_point in baseline["sample_points"]:
        index = int(expected_point["index"])
        assert 0 <= index < len(polyline), f"sample index out of range: {index}"
        actual_point = polyline[index]
        _assert_close(float(actual_point["x"]), float(expected_point["x"]), coordinate_tolerance, f"polyline[{index}].x")
        _assert_close(float(actual_point["y"]), float(expected_point["y"]), coordinate_tolerance, f"polyline[{index}].y")
