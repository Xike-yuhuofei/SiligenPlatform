from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import tempfile


ROOT = Path(__file__).resolve().parents[4]
HIL_SUBDIR = "hardware" + "-in" + "-loop"
MODULE_PATH = ROOT / "tests" / "e2e" / HIL_SUBDIR / "run_real_dxf_production_validation.py"
SPEC = importlib.util.spec_from_file_location("run_real_dxf_production_validation", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
production_validation = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(production_validation)


def test_default_dxf_file_uses_repo_canonical_sample() -> None:
    assert production_validation.DEFAULT_REPO_DXF == ROOT / "samples" / "dxf" / "rect_diag.dxf"


def test_extract_gateway_log_summary_reads_path_trigger_counts() -> None:
    log_text = """
[2026-04-15 18:30:02.200] [info] StartPositionTriggeredDispenser: events=180, coordinate_system=1, start_level=0, tolerance=8, pulse_width=15ms
[2026-04-15 18:30:07.300] [info] PathTriggeredDispenserLoop: 路径触发任务完成, completedCount=180
[2026-04-15 18:30:07.500] [info] StopDispenser: completedCount=180, totalCount=180
""".strip()

    summary = production_validation.extract_gateway_log_summary(log_text)

    assert summary["execution_mode"] == "path_trigger"
    assert summary["start_position_triggered"]["events"] == 180
    assert summary["start_position_triggered"]["coordinate_system"] == 1
    assert summary["start_position_triggered"]["start_level"] == 0
    assert summary["path_trigger_completed"]["completed_count"] == 180
    assert summary["stop_dispenser"]["completed_count"] == 180
    assert summary["stop_dispenser"]["total_count"] == 180


def test_extract_gateway_log_summary_reads_profile_compare_counts() -> None:
    cmp_token = "MC" + "_CmpPluse"
    log_text = f"""
[2026-04-20 20:08:21.069] [info] [siligen] [DispensingProcessService] profile_compare_request_summary span_index=0 compare_source_axis=2 business_trigger_count=87 start_boundary_trigger_count=0 future_compare_count=87 pulse_width_us=2000
[2026-04-20 20:09:02.982] [info] [siligen] [DispensingProcessService] profile_compare_request_summary span_index=1 compare_source_axis=1 business_trigger_count=93 start_boundary_trigger_count=5 future_compare_count=88 pulse_width_us=2000
[2026-04-20 20:09:03.100] [info] [siligen] [ValveAdapter] CallCmpPulseOnce: 调用 {cmp_token} 单次脉冲: Channel=1, PulseWidth=2000us, StartLevel=0
[2026-04-20 20:09:03.101] [info] [siligen] [ValveAdapter] CallCmpPulseOnce: 调用 {cmp_token} 单次脉冲: Channel=1, PulseWidth=2000us, StartLevel=0
[2026-04-20 20:09:03.102] [info] [siligen] [ValveAdapter] CallCmpPulseOnce: 调用 {cmp_token} 单次脉冲: Channel=1, PulseWidth=2000us, StartLevel=0
[2026-04-20 20:09:03.103] [info] [siligen] [ValveAdapter] CallCmpPulseOnce: 调用 {cmp_token} 单次脉冲: Channel=1, PulseWidth=2000us, StartLevel=0
[2026-04-20 20:09:03.104] [info] [siligen] [ValveAdapter] CallCmpPulseOnce: 调用 {cmp_token} 单次脉冲: Channel=1, PulseWidth=2000us, StartLevel=0
[2026-04-20 20:18:23.800] [info] [siligen] [ValveAdapter] OpenSupply: 供胶阀 Y2 打开成功
[2026-04-20 20:18:23.868] [info] [siligen] [ValveAdapter] CloseSupply: 供胶阀 Y2 关闭成功
[2026-04-20 20:18:23.870] [info] [siligen] [DispensingExecutionUseCase] 点胶执行完成: segments=394, distance=3400.968mm, time=607.58s
""".strip()

    summary = production_validation.extract_gateway_log_summary(log_text)

    assert summary["execution_mode"] == "profile_compare"
    assert summary["profile_compare_request_summary"]["request_count"] == 2
    assert summary["profile_compare_request_summary"]["sum_business_trigger_count"] == 180
    assert summary["profile_compare_request_summary"]["sum_start_boundary_trigger_count"] == 5
    assert summary["profile_compare_request_summary"]["sum_future_compare_count"] == 175
    assert summary["cmp_pulse_once"]["count"] == 5
    assert summary["supply_open"]["count"] == 1
    assert summary["supply_close"]["count"] == 1
    assert summary["execution_complete"]["count"] == 1
    assert summary["execution_complete"]["segments"] == 394


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


def test_build_checklist_passes_when_counts_and_coverage_match() -> None:
    checks = production_validation.build_checklist(
        validation_mode="production_execution",
        plan_result={
            "input_quality": {
                "report_id": "report-ok-1",
                "report_path": "D:/runtime/uploads/sample.validation.json",
                "schema_version": "DXFValidationReport.v1",
                "dxf_hash": "sha256-sample",
                "source_drawing_ref": "sha256:sha256-sample",
                "gate_result": "PASS",
                "classification": "success",
                "preview_ready": True,
                "production_ready": True,
                "summary": "DXF import succeeded and is ready for production.",
                "primary_code": "",
                "warning_codes": [],
                "error_codes": [],
                "resolved_units": "mm",
                "resolved_unit_scale": 1.0,
            }
        },
        snapshot_result={
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "glue_point_count": 180,
        },
        job_start_response={},
        blocked_motion_observation={},
        final_job_status={
            "state": "completed",
            "overall_progress_percent": 100,
            "completed_count": 1,
            "target_count": 1,
        },
        log_summary={
            "path_trigger_completed": {"completed_count": 180},
            "stop_dispenser": {"completed_count": 180, "total_count": 180},
        },
        axis_coverage={"ratio_x": 0.91, "ratio_y": 0.89},
        observed_bbox={"count": 32},
        min_axis_coverage_ratio=0.80,
    )

    assert all(payload["passed"] for payload in checks.values())


def test_build_checklist_passes_when_profile_compare_counts_and_completion_match() -> None:
    checks = production_validation.build_checklist(
        validation_mode="production_execution",
        plan_result={
            "input_quality": {
                "report_id": "report-ok-2",
                "report_path": "D:/runtime/uploads/sample.validation.json",
                "schema_version": "DXFValidationReport.v1",
                "dxf_hash": "sha256-sample",
                "source_drawing_ref": "sha256:sha256-sample",
                "gate_result": "PASS",
                "classification": "success",
                "preview_ready": True,
                "production_ready": True,
                "summary": "DXF import succeeded and is ready for production.",
                "primary_code": "",
                "warning_codes": [],
                "error_codes": [],
                "resolved_units": "mm",
                "resolved_unit_scale": 1.0,
            }
        },
        snapshot_result={
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "glue_point_count": 180,
        },
        job_start_response={},
        blocked_motion_observation={},
        final_job_status={
            "state": "completed",
            "overall_progress_percent": 100,
            "completed_count": 1,
            "target_count": 1,
        },
        log_summary={
            "execution_mode": "profile_compare",
            "profile_compare_request_summary": {
                "request_count": 2,
                "sum_business_trigger_count": 180,
                "sum_start_boundary_trigger_count": 5,
                "sum_future_compare_count": 175,
            },
            "cmp_pulse_once": {"count": 5},
            "supply_close": {"count": 1},
            "execution_complete": {"count": 1},
        },
        axis_coverage={"ratio_x": 0.91, "ratio_y": 0.89},
        observed_bbox={"count": 32},
        min_axis_coverage_ratio=0.80,
    )

    assert all(payload["passed"] for payload in checks.values())


def test_build_checklist_treats_preview_only_production_block_as_known_failure_contract() -> None:
    plan_result = {
        "input_quality": {
            "report_id": "report-1",
            "report_path": "D:/runtime/uploads/sample.validation.json",
            "schema_version": "DXFValidationReport.v1",
            "dxf_hash": "sha256-sample",
            "source_drawing_ref": "sha256:sha256-sample",
            "gate_result": "FAIL",
            "classification": "preview_only",
            "preview_ready": True,
            "production_ready": False,
            "summary": "owner execution package 不满足 formal runtime compare contract",
            "primary_code": "FORMAL_COMPARE_BLOCKED",
            "warning_codes": [],
            "error_codes": ["FORMAL_COMPARE_BLOCKED"],
            "resolved_units": "mm",
            "resolved_unit_scale": 1.0,
        },
        "formal_compare_gate": {
            "status": "production_blocked",
            "reason_code": "descending_or_returning_compare_geometry",
            "authority_span_ref": "span-1",
            "trigger_begin_index": 222,
        },
    }
    job_start_response = {
        "error": {
            "code": 2901,
            "message": "owner execution package 不满足 formal runtime compare contract",
        }
    }

    checks = production_validation.build_checklist(
        validation_mode="production_blocked",
        plan_result=plan_result,
        snapshot_result={
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "glue_point_count": 180,
        },
        job_start_response=job_start_response,
        blocked_motion_observation={
            "sample_count": 5,
            "motion_detected": False,
            "window_seconds": 0.4,
        },
        final_job_status={},
        log_summary={},
        axis_coverage={"ratio_x": 0.0, "ratio_y": 0.0},
        observed_bbox={"count": 0},
        min_axis_coverage_ratio=0.80,
    )

    assert all(payload["passed"] for payload in checks.values())
    overall_status, error = production_validation.determine_overall_status(checks, "production_blocked")
    assert overall_status == "known_failure"
    assert error == ""


def test_build_checklist_fails_production_block_without_observation_samples() -> None:
    checks = production_validation.build_checklist(
        validation_mode="production_blocked",
        plan_result={
            "input_quality": {
                "report_id": "report-1",
                "report_path": "D:/runtime/uploads/sample.validation.json",
                "schema_version": "DXFValidationReport.v1",
                "dxf_hash": "sha256-sample",
                "source_drawing_ref": "sha256:sha256-sample",
                "gate_result": "FAIL",
                "classification": "preview_only",
                "preview_ready": True,
                "production_ready": False,
                "summary": "blocked",
                "primary_code": "FORMAL_COMPARE_BLOCKED",
                "warning_codes": [],
                "error_codes": ["FORMAL_COMPARE_BLOCKED"],
                "resolved_units": "mm",
                "resolved_unit_scale": 1.0,
            },
            "formal_compare_gate": {"status": "production_blocked"},
        },
        snapshot_result={
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "glue_point_count": 180,
        },
        job_start_response={"error": {"message": "blocked"}},
        blocked_motion_observation={
            "sample_count": 0,
            "motion_detected": False,
        },
        final_job_status={},
        log_summary={},
        axis_coverage={"ratio_x": 0.0, "ratio_y": 0.0},
        observed_bbox={"count": 0},
        min_axis_coverage_ratio=0.80,
    )

    overall_status, error = production_validation.determine_overall_status(checks, "production_blocked")

    assert overall_status == "failed"
    assert "post_block_observation_samples_collected" in error


def test_summarize_blocked_motion_observation_detects_motion_from_coord_or_position() -> None:
    summary = production_validation.summarize_blocked_motion_observation(
        machine_status_history=[
            {
                "sampled_at": "2026-04-20T00:00:00Z",
                "position": {"x": 0.0, "y": 0.0},
            },
            {
                "sampled_at": "2026-04-20T00:00:00.100000Z",
                "position": {"x": 0.01, "y": 0.0},
            },
        ],
        coord_status_history=[
            {
                "sampled_at": "2026-04-20T00:00:00Z",
                "is_moving": False,
                "remaining_segments": 0,
                "current_velocity": 0.0,
                "raw_status_word": 0,
                "raw_segment": 0,
                "position": {"x": 0.0, "y": 0.0},
                "axes": {"X": {"position": 0.0, "velocity": 0.0}, "Y": {"position": 0.0, "velocity": 0.0}},
            },
            {
                "sampled_at": "2026-04-20T00:00:00.100000Z",
                "is_moving": True,
                "remaining_segments": 1,
                "current_velocity": 1.0,
                "raw_status_word": 1,
                "raw_segment": 1,
                "position": {"x": 0.01, "y": 0.0},
                "axes": {"X": {"position": 0.01, "velocity": 1.0}, "Y": {"position": 0.0, "velocity": 0.0}},
            },
        ],
    )

    assert summary["sample_count"] == 2
    assert summary["motion_detected"] is True
    assert summary["coord_motion_detected"] is True
    assert summary["machine_position_motion_detected"] is True


def test_resolve_validation_mode_returns_production_blocked_for_formal_gate_negative_case() -> None:
    assert production_validation.resolve_validation_mode(
        {
            "input_quality": {
                "report_id": "report-1",
                "report_path": "D:/runtime/uploads/sample.validation.json",
                "schema_version": "DXFValidationReport.v1",
                "dxf_hash": "sha256-sample",
                "source_drawing_ref": "sha256:sha256-sample",
                "gate_result": "FAIL",
                "classification": "preview_only",
                "preview_ready": True,
                "production_ready": False,
                "summary": "blocked",
                "primary_code": "FORMAL_COMPARE_BLOCKED",
                "warning_codes": [],
                "error_codes": ["FORMAL_COMPARE_BLOCKED"],
                "resolved_units": "mm",
                "resolved_unit_scale": 1.0,
            },
            "formal_compare_gate": {"status": "production_blocked"},
        }
    ) == "production_blocked"
    assert production_validation.resolve_validation_mode(
        {
            "input_quality": {
                "report_id": "report-2",
                "report_path": "D:/runtime/uploads/sample.validation.json",
                "schema_version": "DXFValidationReport.v1",
                "dxf_hash": "sha256-sample",
                "source_drawing_ref": "sha256:sha256-sample",
                "gate_result": "PASS",
                "classification": "success",
                "preview_ready": True,
                "production_ready": True,
                "summary": "ready",
                "primary_code": "",
                "warning_codes": [],
                "error_codes": [],
                "resolved_units": "mm",
                "resolved_unit_scale": 1.0,
            },
            "formal_compare_gate": None,
        }
    ) == "production_execution"


def test_build_report_freezes_execution_timing_summary_contract() -> None:
    args = argparse.Namespace(
        gateway_exe=Path("D:/fake/siligen_runtime_gateway.exe"),
        config_path=Path("D:/fake/machine_config.ini"),
        dxf_file=Path("D:/fake/rect_diag.dxf"),
        host="127.0.0.1",
    )
    breakdown = {
        "execution_nominal_time_s": 340.0968,
        "motion_completion_grace_s": 6.0,
        "owner_span_count": 265,
        "owner_span_overhead_s": 265.0,
        "cycle_budget_s": 611.0968,
        "target_count": 1,
        "total_budget_s": 611.0968,
    }

    report = production_validation.build_report(
        args=args,
        effective_port=49397,
        overall_status="passed",
        validation_mode="production_execution",
        job_id="job-1",
        plan_id="plan-1",
        plan_fingerprint="fp-1",
        snapshot_hash="snap-1",
        steps=[],
        checks={},
        axis_coverage={},
        expected_bbox={},
        observed_bbox={},
        blocked_motion_observation={},
        snapshot_result={},
        final_job_status={},
        log_summary={"execution_complete": {"time_s": "607.31"}},
        artifacts={
            "job_timeout_budget": {
                "execution_budget_seconds": 611.0968,
                "cycle_budget_seconds": 611.0968,
                "execution_budget_breakdown": breakdown,
                "effective_timeout_seconds": 611.0968,
            }
        },
        error_message="",
        plan_result={"execution_nominal_time_s": 340.0968},
        production_baseline={
            "baseline_id": "baseline-production-default",
            "baseline_fingerprint": "baseline-fp-20260421",
            "production_baseline_source": "dxf.plan.prepare",
        },
    )

    assert report["timing_summary"] == {
        "validation_mode": "production_execution",
        "execution_nominal_time_s": 340.0968,
        "execution_budget_s": 611.0968,
        "execution_budget_breakdown": breakdown,
        "observed_execution_time_s": 607.31,
    }


def test_build_report_nulls_execution_timing_summary_for_production_blocked() -> None:
    args = argparse.Namespace(
        gateway_exe=Path("D:/fake/siligen_runtime_gateway.exe"),
        config_path=Path("D:/fake/machine_config.ini"),
        dxf_file=Path("D:/fake/rect_diag.dxf"),
        host="127.0.0.1",
    )

    report = production_validation.build_report(
        args=args,
        effective_port=49397,
        overall_status="known_failure",
        validation_mode="production_blocked",
        job_id="",
        plan_id="plan-1",
        plan_fingerprint="fp-1",
        snapshot_hash="snap-1",
        steps=[],
        checks={},
        axis_coverage={},
        expected_bbox={},
        observed_bbox={},
        blocked_motion_observation={"sample_count": 3, "motion_detected": False},
        snapshot_result={},
        final_job_status={},
        log_summary={},
        artifacts={},
        error_message="blocked",
        plan_result={"execution_nominal_time_s": 340.0968},
        production_baseline={
            "baseline_id": "baseline-production-default",
            "baseline_fingerprint": "baseline-fp-20260421",
            "production_baseline_source": "dxf.plan.prepare",
        },
    )

    assert report["timing_summary"] == {
        "validation_mode": "production_blocked",
        "execution_nominal_time_s": None,
        "execution_budget_s": None,
        "execution_budget_breakdown": None,
        "observed_execution_time_s": None,
    }


def test_read_log_segment_reads_full_file_after_rotation() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        log_path = Path(temp_dir) / "tcp_server.log"
        log_path.write_text("new-log-line\n", encoding="utf-8")

        segment = production_validation.read_log_segment(log_path, start_offset=1024)

    assert segment.strip() == "new-log-line"


def test_read_current_run_gateway_log_prefers_boot_marker_for_current_port() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        log_path = Path(temp_dir) / "tcp_server.log"
        log_text = "\n".join(
            [
                "[2026-04-20 19:55:00.000] [info] [siligen] [TcpServerMain] ###BOOT_MARK:TCP_SERVER_DIAG_20260203### exe=D:\\Projects\\SiligenSuite\\build\\ca\\bin\\Debug\\siligen_runtime_gateway.exe config=D:\\Projects\\SiligenSuite\\config\\machine\\machine_config.ini port=49111",
                "[2026-04-20 19:55:10.000] [info] [siligen] [DispensingExecutionUseCase] 点胶执行完成: segments=5, distance=10.000mm, time=1.00s",
                "[2026-04-20 20:07:38.476] [info] [siligen] [TcpServerMain] ###BOOT_MARK:TCP_SERVER_DIAG_20260203### exe=D:\\Projects\\SiligenSuite\\build\\ca\\bin\\Debug\\siligen_runtime_gateway.exe config=D:\\Projects\\SiligenSuite\\config\\machine\\machine_config.ini port=49397",
                "[2026-04-20 20:08:21.069] [info] [siligen] [DispensingProcessService] profile_compare_request_summary span_index=0 compare_source_axis=2 business_trigger_count=87 start_boundary_trigger_count=0 future_compare_count=87 pulse_width_us=2000",
                "[2026-04-20 20:18:23.870] [info] [siligen] [DispensingExecutionUseCase] 点胶执行完成: segments=394, distance=3400.968mm, time=607.58s",
                "",
            ]
        )
        log_path.write_text(log_text, encoding="utf-8")
        late_only_text = (
            "[2026-04-20 20:18:23.870] [info] [siligen] [DispensingExecutionUseCase] 点胶执行完成: segments=394, "
            "distance=3400.968mm, time=607.58s\n"
        )

        segment = production_validation.read_current_run_gateway_log(
            log_path,
            start_offset=len(log_text.encode("utf-8")) - len(late_only_text.encode("utf-8")),
            effective_port=49397,
        )

    assert segment.startswith("[2026-04-20 20:07:38.476]")
    assert "port=49397" in segment
    assert "span_index=0" in segment
    assert "port=49111" not in segment
