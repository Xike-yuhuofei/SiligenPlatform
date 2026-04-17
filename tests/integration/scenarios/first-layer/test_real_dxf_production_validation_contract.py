from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile


ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_real_dxf_production_validation.py"
SPEC = importlib.util.spec_from_file_location("run_real_dxf_production_validation", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
production_validation = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(production_validation)


def test_extract_gateway_log_summary_reads_path_trigger_counts() -> None:
    log_text = """
[2026-04-15 18:30:02.200] [info] StartPositionTriggeredDispenser: events=180, coordinate_system=1, start_level=0, tolerance=8, pulse_width=15ms
[2026-04-15 18:30:07.300] [info] PathTriggeredDispenserLoop: 路径触发任务完成, completedCount=180
[2026-04-15 18:30:07.500] [info] StopDispenser: completedCount=180, totalCount=180
""".strip()

    summary = production_validation.extract_gateway_log_summary(log_text)

    assert summary["start_position_triggered"]["events"] == 180
    assert summary["start_position_triggered"]["coordinate_system"] == 1
    assert summary["start_position_triggered"]["start_level"] == 0
    assert summary["path_trigger_completed"]["completed_count"] == 180
    assert summary["stop_dispenser"]["completed_count"] == 180
    assert summary["stop_dispenser"]["total_count"] == 180


def test_default_dxf_file_uses_repo_canonical_sample() -> None:
    assert production_validation.DEFAULT_REPO_DXF == ROOT / "samples" / "dxf" / "rect_diag.dxf"


def test_compute_axis_coverage_reports_expected_ratios() -> None:
    expected_bbox = production_validation.summarize_points_bbox(
        [
            {"x": 10.0, "y": 20.0},
            {"x": 110.0, "y": 120.0},
        ]
    )
    observed_bbox = production_validation.summarize_points_bbox(
        [
            {"x": 12.0, "y": 24.0},
            {"x": 102.0, "y": 108.0},
        ]
    )

    coverage = production_validation.compute_axis_coverage(expected_bbox, observed_bbox)

    assert coverage["ratio_x"] == 0.9
    assert coverage["ratio_y"] == 0.84


def test_build_checklist_uses_job_status_as_single_blocking_authority() -> None:
    checks = production_validation.build_checklist(
        snapshot_result={
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "glue_point_count": 180,
        },
        final_job_status={
            "state": "completed",
            "overall_progress_percent": 100,
            "completed_count": 1,
            "target_count": 1,
        },
        log_summary={
            "path_trigger_completed": {"completed_count": 67},
            "stop_dispenser": {"completed_count": 67, "total_count": 180},
        },
        axis_coverage={"ratio_x": 0.30, "ratio_y": 0.25},
        min_axis_coverage_ratio=0.80,
    )

    assert all(payload["passed"] for payload in checks.values())


def test_build_checklist_fails_when_job_is_not_terminal_completed() -> None:
    checks = production_validation.build_checklist(
        snapshot_result={
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "glue_point_count": 180,
        },
        final_job_status={
            "state": "failed",
            "overall_progress_percent": 100,
            "completed_count": 0,
            "target_count": 1,
            "error_message": "failure_stage=path_trigger_reconcile;failure_code=DISPENSER_TRIGGER_INCOMPLETE",
        },
        log_summary={
            "path_trigger_completed": {"completed_count": 180},
            "stop_dispenser": {"completed_count": 180, "total_count": 180},
        },
        axis_coverage={"ratio_x": 1.0, "ratio_y": 1.0},
        min_axis_coverage_ratio=0.80,
    )

    assert not checks["job_terminal_completed"]["passed"]


def test_read_log_segment_reads_full_file_after_rotation() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        log_path = Path(temp_dir) / "tcp_server.log"
        log_path.write_text("new-log-line\n", encoding="utf-8")

        segment = production_validation.read_log_segment(log_path, start_offset=1024)

    assert segment.strip() == "new-log-line"
