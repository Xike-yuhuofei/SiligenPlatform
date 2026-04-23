from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hmi_operator_production_test.py"
SPEC = importlib.util.spec_from_file_location("run_hmi_operator_production_test", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
operator_runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(operator_runner)


def test_default_max_glue_points_is_5000() -> None:
    assert operator_runner.DEFAULT_MAX_GLUE_POINTS == 5000


def test_finalize_process_log_capture_reads_file_backed_hmi_output(tmp_path: Path) -> None:
    stdout_path = tmp_path / "hmi-stdout.log"
    stderr_path = tmp_path / "hmi-stderr.log"
    stdout_stream = stdout_path.open("w", encoding="utf-8")
    stderr_stream = stderr_path.open("w", encoding="utf-8")
    stdout_stream.write("OPERATOR_CONTEXT stage=preview-ready artifact_id=artifact-1\n")
    stderr_stream.write("SUPERVISOR_DIAG online_ready=true\n")

    stdout_stream, stderr_stream, stdout_text, stderr_text = operator_runner.finalize_process_log_capture(
        stdout_stream=stdout_stream,
        stderr_stream=stderr_stream,
        stdout_path=stdout_path,
        stderr_path=stderr_path,
    )

    assert stdout_stream is None
    assert stderr_stream is None
    assert "stage=preview-ready" in stdout_text
    assert "online_ready=true" in stderr_text


def test_summarize_operator_output_requires_formal_stage_sequence_and_staged_screenshots() -> None:
    output = "\n".join(
        [
            "OPERATOR_CONTEXT stage=preview-ready artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=ready preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=null snapshot_ready=true job_id=null target_count=1 completed_count=0/1 global_progress_percent=0 current_operation=空闲 preview_confirmed=false",
            "OPERATOR_CONTEXT stage=preview-refreshed artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=ready preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=null snapshot_ready=true job_id=null target_count=1 completed_count=0/1 global_progress_percent=0 current_operation=空闲 preview_confirmed=false",
            "OPERATOR_CONTEXT stage=production-started artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=confirmed preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=hash-1 snapshot_ready=true job_id=job-1 target_count=1 completed_count=0/1 global_progress_percent=0 current_operation=生产运行中 preview_confirmed=true",
            "OPERATOR_CONTEXT stage=production-running artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=confirmed preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=hash-1 snapshot_ready=true job_id=job-1 target_count=1 completed_count=0/1 global_progress_percent=12 current_operation=生产运行中 preview_confirmed=true",
            "OPERATOR_CONTEXT stage=production-completed artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=confirmed preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=hash-1 snapshot_ready=true job_id=null target_count=1 completed_count=1/1 global_progress_percent=100 current_operation=完成 preview_confirmed=true",
            "OPERATOR_CONTEXT stage=next-job-ready artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=ready preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=hash-1 snapshot_ready=true job_id=null target_count=1 completed_count=1/1 global_progress_percent=100 current_operation=完成 preview_confirmed=true",
            "HMI_SCREENSHOT stage=production-running path=D:/reports/01-production-running.png",
            "HMI_SCREENSHOT stage=production-completed path=D:/reports/02-production-completed.png",
            "HMI_SCREENSHOT stage=next-job-ready path=D:/reports/03-next-job-ready.png",
        ]
    )

    summary = operator_runner.summarize_operator_output(output)

    assert summary["contract_ok"] is True
    assert tuple(summary["operator_context_stages"]) == operator_runner.REQUIRED_OPERATOR_STAGES
    assert summary["preview_confirmed"] is True
    assert summary["plan_fingerprint"] == "fp-1"
    assert summary["screenshots_by_stage"]["production-running"].endswith("01-production-running.png")


def test_summarize_operator_output_fail_closes_on_partial_preview_evidence() -> None:
    output = "\n".join(
        [
            "FAIL: Timed out waiting for operator preview becomes ready without recipe selection",
            "OPERATOR_CONTEXT stage=preview-ready-failed artifact_id=artifact-1 plan_id=null plan_fingerprint=null preview_source=null preview_kind=null glue_point_count=0 preview_gate_state=failed preview_gate_error=preview_timeout snapshot_hash=null confirmed_snapshot_hash=null snapshot_ready=false job_id=null target_count=1 completed_count=0/1 global_progress_percent=0 current_operation=空闲 preview_confirmed=false",
        ]
    )

    summary = operator_runner.summarize_operator_output(output)

    assert summary["contract_ok"] is False
    assert summary["stage_sequence_ok"] is False
    assert summary["failure_stages"] == ["preview-ready-failed"]
    assert summary["preview_gate_state"] == "failed"
    assert summary["preview_gate_error"] == "preview_timeout"
    assert "required stage sequence" in summary["contract_error"]


def test_evaluate_operator_execution_requires_next_job_ready_stage() -> None:
    summary = {
        "stage_sequence_ok": False,
        "failure_stages": [],
        "preview_source": "planned_glue_snapshot",
        "preview_kind": "glue_points",
        "preview_confirmed": True,
        "contract_error": "missing next-job-ready",
        "operator_context_stages": [
            "preview-ready",
            "preview-refreshed",
            "production-started",
            "production-running",
            "production-completed",
        ],
        "completed_count_label": "1/1",
    }

    result = operator_runner.evaluate_operator_execution(
        operator_summary=summary,
        final_job_status={
            "state": "completed",
            "completed_count": 1,
            "target_count": 1,
        },
        hmi_exit_code=0,
        combined_output="",
    )

    assert result["status"] == "failed"
    assert any(issue["type"] == "恢复类" for issue in result["issues"])


def test_traceability_conclusion_is_insufficient_when_coord_history_is_empty() -> None:
    traceability = operator_runner.evaluate_traceability_correspondence(
        snapshot_result={
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "glue_point_count": 2,
            "glue_points": [
                {"x": 0.0, "y": 0.0},
                {"x": 10.0, "y": 10.0},
            ],
        },
        final_job_status={
            "state": "completed",
            "overall_progress_percent": 100,
            "completed_count": 1,
            "target_count": 1,
        },
        log_summary={
            "execution_mode": "profile_compare",
            "profile_compare_request_summary": {
                "request_count": 1,
                "sum_business_trigger_count": 2,
                "sum_start_boundary_trigger_count": 0,
                "sum_future_compare_count": 2,
            },
            "cmp_pulse_once": {"count": 0},
            "supply_close": {"count": 1},
            "execution_complete": {"count": 1},
        },
        machine_status_history=[
            {
                "position": {"x": 0.0, "y": 0.0},
            },
            {
                "position": {"x": 9.0, "y": 9.0},
            },
        ],
        coord_status_history=[],
        min_axis_coverage_ratio=0.80,
        snapshot_readback_error="",
    )

    assert traceability["summary_alignment_status"] == "passed"
    assert traceability["status"] == "insufficient_evidence"
    assert "coord-status-history" in traceability["reason"]
