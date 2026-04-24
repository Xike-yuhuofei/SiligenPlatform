from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
from types import SimpleNamespace

import pytest


ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hmi_operator_production_test.py"
UI_QTEST_PATH = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "tools" / "ui_qtest.py"
SPEC = importlib.util.spec_from_file_location("run_hmi_operator_production_test", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
operator_runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(operator_runner)


class StubTcpJsonClient:
    def __init__(self, response: dict[str, object]) -> None:
        self._response = response
        self.calls: list[tuple[str, object | None, float | None]] = []

    def send_request(
        self,
        method: str,
        params: object | None = None,
        *,
        timeout_seconds: float | None = None,
    ) -> dict[str, object]:
        self.calls.append((method, params, timeout_seconds))
        return dict(self._response)


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


def test_gateway_launch_spec_payload_uses_single_external_gateway_contract(tmp_path: Path) -> None:
    build_root = tmp_path / "control-apps-build"
    exe_path = build_root / "bin" / "Debug" / "siligen_runtime_gateway.exe"
    lib_dir = build_root / "lib" / "Debug"
    exe_path.parent.mkdir(parents=True, exist_ok=True)
    lib_dir.mkdir(parents=True, exist_ok=True)
    exe_path.write_text("", encoding="utf-8")
    config_path = tmp_path / "machine_config.ini"
    config_path.write_text("[Network]\n", encoding="utf-8")
    args = SimpleNamespace(
        gateway_exe=exe_path,
        config_path=config_path,
        host="127.0.0.1",
    )

    payload = operator_runner.build_gateway_launch_spec_payload(args=args, effective_port=61234)

    assert payload["executable"] == str(exe_path.resolve())
    assert payload["cwd"] == str(operator_runner.ROOT.resolve())
    assert payload["args"] == ["--config", str(config_path.resolve()), "--port", "61234"]
    assert payload["env"]["SILIGEN_TCP_SERVER_HOST"] == "127.0.0.1"
    assert payload["env"]["SILIGEN_TCP_SERVER_PORT"] == "61234"
    assert str(exe_path.parent.resolve()) in payload["pathEntries"]
    assert str(lib_dir.resolve()) in payload["pathEntries"]


def test_build_hmi_process_env_injects_gateway_launch_spec(tmp_path: Path, monkeypatch) -> None:
    spec_path = tmp_path / "gateway-launch.json"
    spec_path.write_text("{}", encoding="utf-8")
    monkeypatch.setenv("PYTHONPATH", "existing-pythonpath")
    monkeypatch.setenv("SILIGEN_GATEWAY_EXE", str(tmp_path / "legacy-gateway.exe"))

    env = operator_runner.build_hmi_process_env(
        gateway_launch_spec_path=spec_path,
        host="127.0.0.1",
        port=61234,
    )

    py_entries = env["PYTHONPATH"].split(os.pathsep)
    assert py_entries[0] == str(operator_runner.HMI_APPLICATION_ROOT)
    assert py_entries[1] == str(operator_runner.HMI_SOURCE_ROOT)
    assert py_entries[-1] == "existing-pythonpath"
    assert env["SILIGEN_GATEWAY_LAUNCH_SPEC"] == str(spec_path.resolve())
    assert env["SILIGEN_GATEWAY_AUTOSTART"] == "1"
    assert env["SILIGEN_TCP_SERVER_HOST"] == "127.0.0.1"
    assert env["SILIGEN_TCP_SERVER_PORT"] == "61234"
    assert "SILIGEN_GATEWAY_EXE" not in env


def test_write_gateway_launch_spec_persists_report_artifact(tmp_path: Path) -> None:
    exe_path = tmp_path / "gateway.exe"
    exe_path.write_text("", encoding="utf-8")
    config_path = tmp_path / "machine_config.ini"
    config_path.write_text("[Network]\n", encoding="utf-8")
    spec_path = tmp_path / "report" / "gateway-launch.json"
    spec_path.parent.mkdir(parents=True, exist_ok=True)
    args = SimpleNamespace(
        gateway_exe=exe_path,
        config_path=config_path,
        host="127.0.0.1",
    )

    payload = operator_runner.write_gateway_launch_spec(
        args=args,
        effective_port=9527,
        spec_path=spec_path,
    )

    persisted = json.loads(spec_path.read_text(encoding="utf-8"))
    assert persisted == payload


def test_runner_queries_traceability_before_shutting_down_gateway() -> None:
    source = MODULE_PATH.read_text(encoding="utf-8")

    traceability_query_index = source.index("observer_client.send_request(")
    gateway_shutdown_index = source.index("if gateway_process is not None:")

    assert traceability_query_index < gateway_shutdown_index


def test_refresh_operator_exclusive_windows_tracks_home_auto_markers(tmp_path: Path) -> None:
    stdout_path = tmp_path / "hmi-stdout.log"
    stdout_path.write_text(
        "\n".join(
            [
                "OPERATOR_EXCLUSIVE_WINDOW kind=home_auto state=started",
                "OPERATOR_EXCLUSIVE_WINDOW kind=home_auto state=completed",
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    offset, active = operator_runner.refresh_operator_exclusive_windows(
        stdout_path=stdout_path,
        read_offset=0,
        active_windows=set(),
    )

    assert offset > 0
    assert active == set()


def test_refresh_production_started_job_id_reads_live_operator_context(tmp_path: Path) -> None:
    stdout_path = tmp_path / "hmi-stdout.log"
    stdout_path.write_text(
        "\n".join(
            [
                "OPERATOR_CONTEXT stage=preview-ready artifact_id=artifact-1",
                "OPERATOR_CONTEXT stage=production-started artifact_id=artifact-1 job_id=job-42 target_count=1 completed_count=0/1",
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    offset, job_id = operator_runner.refresh_production_started_job_id(
        stdout_path=stdout_path,
        read_offset=0,
        current_job_id="",
    )

    assert offset > 0
    assert job_id == "job-42"


def test_refresh_operator_job_tracking_clears_active_job_after_terminal_stage(tmp_path: Path) -> None:
    stdout_path = tmp_path / "hmi-stdout.log"
    stdout_path.write_text(
        "\n".join(
            [
                "OPERATOR_CONTEXT stage=production-started artifact_id=artifact-1 job_id=job-42 target_count=1 completed_count=0/1",
                "OPERATOR_CONTEXT stage=production-completed artifact_id=artifact-1 job_id=null target_count=1 completed_count=1/1",
                "OPERATOR_EXCLUSIVE_WINDOW kind=next_job_ready_recovery state=started",
                "OPERATOR_CONTEXT stage=next-job-ready artifact_id=artifact-1 job_id=null target_count=1 completed_count=1/1",
                "OPERATOR_EXCLUSIVE_WINDOW kind=next_job_ready_recovery state=completed",
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    offset, active_job_id, production_job_id = operator_runner.refresh_operator_job_tracking(
        stdout_path=stdout_path,
        read_offset=0,
        current_job_id="",
        production_job_id="",
    )

    assert offset > 0
    assert active_job_id == ""
    assert production_job_id == "job-42"


def test_terminal_job_states_match_runtime_contract() -> None:
    assert operator_runner.TERMINAL_JOB_STATES == {"completed", "failed", "cancelled"}


def test_ui_qtest_marks_next_job_ready_recovery_as_operator_exclusive() -> None:
    source = UI_QTEST_PATH.read_text(encoding="utf-8")

    assert '_emit_operator_exclusive_window(kind="next_job_ready_recovery", state="started")' in source
    assert '_emit_operator_exclusive_window(kind="next_job_ready_recovery", state="completed")' in source
    assert '_emit_operator_exclusive_window(kind="next_job_ready_recovery", state="failed")' in source


def test_collect_observer_sample_uses_single_job_observation_query() -> None:
    client = StubTcpJsonClient(
        {
            "result": {
                "sampled_at": "2026-04-24T05:00:00Z",
                "machine_status": {
                    "connected": True,
                    "supervision": {
                        "current_state": "Online",
                        "requested_state": "Online",
                        "state_reason": "",
                        "state_change_in_process": False,
                        "failure_stage": "",
                        "failure_code": "",
                    },
                    "job_execution": {"job_id": "job-1", "state": "running"},
                    "io": {},
                    "effective_interlocks": {},
                    "axes": {},
                    "position": {"x": 1.0, "y": 2.0},
                },
                "job_status": {
                    "job_id": "job-1",
                    "state": "running",
                    "completed_count": 0,
                    "target_count": 1,
                    "current_cycle": 1,
                    "current_segment": 3,
                    "total_segments": 10,
                    "cycle_progress_percent": 10,
                    "overall_progress_percent": 10,
                    "elapsed_seconds": 0.5,
                    "error_message": "",
                    "dry_run": False,
                },
                "coord_status": {
                    "coord_sys": 1,
                    "state": 1,
                    "is_moving": True,
                    "remaining_segments": 7,
                    "current_velocity": 12.5,
                    "raw_status_word": 33,
                    "raw_segment": 7,
                    "mc_status_ret": 0,
                    "position": {"x": 1.0, "y": 2.0},
                    "axes": {},
                },
            }
        }
    )

    sample = operator_runner.collect_observer_sample(
        client,
        poll_index=3,
        job_id="job-1",
        timeout_seconds=2.5,
    )

    assert client.calls == [
        (
            "dxf.job.observation",
            {"job_id": "job-1", "coord_sys": operator_runner.DEFAULT_COORD_SYS},
            2.5,
        )
    ]
    assert sample["machine_status"]["job_execution"]["job_id"] == "job-1"
    assert sample["job_status"]["job_id"] == "job-1"
    assert sample["coord_status"]["coord_sys"] == operator_runner.DEFAULT_COORD_SYS


def test_collect_observer_sample_requires_explicit_job_id() -> None:
    client = StubTcpJsonClient({"result": {}})

    with pytest.raises(ValueError, match="non-empty job_id"):
        operator_runner.collect_observer_sample(client, poll_index=0, job_id="")

    assert client.calls == []


def test_collect_observer_sample_fail_closes_on_partial_observation_payload() -> None:
    client = StubTcpJsonClient(
        {
            "result": {
                "sampled_at": "2026-04-24T05:00:00Z",
                "machine_status": {},
                "job_status": {},
            }
        }
    )

    with pytest.raises(RuntimeError, match="partial observation payload"):
        operator_runner.collect_observer_sample(client, poll_index=0, job_id="job-1")


def test_collect_terminal_job_readback_queries_formal_observation_after_hmi_exit() -> None:
    client = StubTcpJsonClient(
        {
            "result": {
                "sampled_at": "2026-04-24T05:00:00Z",
                "machine_status": {
                    "connected": True,
                    "supervision": {
                        "current_state": "Online",
                        "requested_state": "Online",
                        "state_reason": "",
                        "state_change_in_process": False,
                        "failure_stage": "",
                        "failure_code": "",
                    },
                    "job_execution": {"job_id": "job-1", "state": "completed"},
                    "io": {},
                    "effective_interlocks": {},
                    "axes": {},
                    "position": {"x": 1.0, "y": 2.0},
                },
                "job_status": {
                    "job_id": "job-1",
                    "state": "completed",
                    "completed_count": 1,
                    "target_count": 1,
                    "current_cycle": 1,
                    "current_segment": 394,
                    "total_segments": 394,
                    "cycle_progress_percent": 100,
                    "overall_progress_percent": 100,
                    "elapsed_seconds": 3.0,
                    "error_message": "",
                    "dry_run": False,
                },
                "coord_status": {
                    "coord_sys": 1,
                    "state": 1,
                    "is_moving": False,
                    "remaining_segments": 0,
                    "current_velocity": 0.0,
                    "raw_status_word": 48,
                    "raw_segment": 0,
                    "mc_status_ret": 0,
                    "position": {"x": 1.0, "y": 2.0},
                    "axes": {},
                },
            }
        }
    )

    sample = operator_runner.collect_terminal_job_readback(
        client,
        poll_index=17,
        job_id="job-1",
        final_job_status={"state": "running"},
    )

    assert client.calls == [
        (
            "dxf.job.observation",
            {"job_id": "job-1", "coord_sys": operator_runner.DEFAULT_COORD_SYS},
            2.0,
        )
    ]
    assert sample is not None
    assert sample["job_status"]["state"] == "completed"


def test_collect_terminal_job_readback_skips_when_final_state_already_terminal() -> None:
    client = StubTcpJsonClient({"result": {}})

    sample = operator_runner.collect_terminal_job_readback(
        client,
        poll_index=4,
        job_id="job-1",
        final_job_status={"state": "completed"},
    )

    assert sample is None
    assert client.calls == []


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
    assert summary["job_id"] == "job-1"
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


def test_summarize_operator_output_fail_closes_when_production_started_job_id_missing() -> None:
    output = "\n".join(
        [
            "OPERATOR_CONTEXT stage=preview-ready artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=ready preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=null snapshot_ready=true job_id=null target_count=1 completed_count=0/1 global_progress_percent=0 current_operation=空闲 preview_confirmed=false",
            "OPERATOR_CONTEXT stage=preview-refreshed artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=ready preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=null snapshot_ready=true job_id=null target_count=1 completed_count=0/1 global_progress_percent=0 current_operation=空闲 preview_confirmed=false",
            "OPERATOR_CONTEXT stage=production-started artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=confirmed preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=hash-1 snapshot_ready=true job_id=null target_count=1 completed_count=0/1 global_progress_percent=0 current_operation=生产运行中 preview_confirmed=true",
            "OPERATOR_CONTEXT stage=production-running artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=confirmed preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=hash-1 snapshot_ready=true job_id=job-1 target_count=1 completed_count=0/1 global_progress_percent=12 current_operation=生产运行中 preview_confirmed=true",
            "OPERATOR_CONTEXT stage=production-completed artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=confirmed preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=hash-1 snapshot_ready=true job_id=null target_count=1 completed_count=1/1 global_progress_percent=100 current_operation=完成 preview_confirmed=true",
            "OPERATOR_CONTEXT stage=next-job-ready artifact_id=artifact-1 plan_id=plan-1 plan_fingerprint=fp-1 preview_source=planned_glue_snapshot preview_kind=glue_points glue_point_count=2 preview_gate_state=ready preview_gate_error=null snapshot_hash=hash-1 confirmed_snapshot_hash=hash-1 snapshot_ready=true job_id=null target_count=1 completed_count=1/1 global_progress_percent=100 current_operation=完成 preview_confirmed=true",
            "HMI_SCREENSHOT stage=production-running path=D:/reports/01-production-running.png",
            "HMI_SCREENSHOT stage=production-completed path=D:/reports/02-production-completed.png",
            "HMI_SCREENSHOT stage=next-job-ready path=D:/reports/03-next-job-ready.png",
        ]
    )

    summary = operator_runner.summarize_operator_output(output)

    assert summary["job_id"] == "null"
    assert summary["contract_ok"] is False


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


def test_traceability_conclusion_reads_runtime_job_traceability_verdict() -> None:
    traceability = operator_runner.evaluate_traceability_correspondence(
        job_id="job-1",
        job_traceability={
            "job_id": "job-1",
            "terminal_state": "completed",
            "expected_trace": [{"trigger_sequence_id": 1}, {"trigger_sequence_id": 2}],
            "actual_trace": [{"trigger_sequence_id": 1}, {"trigger_sequence_id": 2}],
            "mismatches": [],
            "verdict": "passed",
            "verdict_reason": "",
            "strict_one_to_one_proven": True,
        },
    )

    assert traceability["status"] == "passed"
    assert traceability["strict_one_to_one_proven"] is True
    assert traceability["expected_trace_count"] == 2
    assert traceability["actual_trace_count"] == 2
    assert traceability["mismatch_count"] == 0


def test_traceability_conclusion_fail_closes_on_invalid_passed_without_strict_proof() -> None:
    traceability = operator_runner.evaluate_traceability_correspondence(
        job_id="job-1",
        job_traceability={
            "job_id": "job-1",
            "terminal_state": "completed",
            "expected_trace": [],
            "actual_trace": [],
            "mismatches": [],
            "verdict": "passed",
            "verdict_reason": "",
            "strict_one_to_one_proven": False,
        },
    )

    assert traceability["status"] == "failed"
    assert "violated contract" in traceability["reason"]


def test_traceability_conclusion_preserves_insufficient_evidence_from_runtime() -> None:
    traceability = operator_runner.evaluate_traceability_correspondence(
        job_id="job-2",
        job_traceability={
            "job_id": "job-2",
            "terminal_state": "completed",
            "expected_trace": [{"trigger_sequence_id": 1}],
            "actual_trace": [],
            "mismatches": [{"code": "terminal_trigger_count_mismatch"}],
            "verdict": "insufficient_evidence",
            "verdict_reason": "actual trace count does not match expected trace count at terminal state",
            "strict_one_to_one_proven": False,
        },
    )

    assert traceability["status"] == "insufficient_evidence"
    assert traceability["strict_one_to_one_proven"] is False
    assert traceability["mismatch_count"] == 1


def test_observation_alignment_uses_summary_level_checks_without_rewriting_strict_truth() -> None:
    alignment = operator_runner.evaluate_observation_alignment(
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
                "sum_start_boundary_trigger_count": 1,
            },
            "cmp_pulse_once": {"count": 1},
            "supply_close": {"count": 1},
            "execution_complete": {"count": 1},
        },
        machine_status_history=[
            {"position": {"x": 0.0, "y": 0.0}},
            {"position": {"x": 10.0, "y": 10.0}},
        ],
        coord_status_history=[{"position": {"x": 0.0, "y": 0.0}}],
        min_axis_coverage_ratio=0.8,
    )

    assert alignment["status"] == "passed"
    assert alignment["summary_checks"]["profile_compare_business_trigger_matches_glue_count"]["passed"] is True
    assert alignment["coord_status_sample_count"] == 1


def test_observation_integrity_warns_on_recovered_supervisor_drop() -> None:
    integrity = operator_runner.evaluate_observation_integrity(
        combined_output="\n".join(
            [
                "SUPERVISOR_EVENT type=stage_failed stage=hardware_ready recoverable=true",
                "SUPERVISOR_EVENT type=stage_succeeded stage=online_ready recoverable=true",
            ]
        ),
        observer_poll_errors=["tcp response timeout method=status"],
        observer_poll_failures=[
            {
                "phase": "polling",
                "poll_index": 4,
                "job_id": "job-1",
                "message": "tcp response timeout method=status",
            }
        ],
        job_status_history=[{"state": "completed"}],
        machine_status_history=[{"position": {"x": 0.0, "y": 0.0}}],
        coord_status_history=[],
        snapshot_result={"plan_id": "plan-1"},
        job_traceability={"job_id": "job-1", "verdict": "passed", "strict_one_to_one_proven": True},
    )

    assert integrity["status"] == "warning"
    assert integrity["recoverable_stage_failure_count"] == 1
    assert integrity["recovered_online_ready"] is True
    assert integrity["observer_poll_error_count"] == 1


def test_build_observer_poll_failure_captures_timeout_metadata() -> None:
    exc = TimeoutError("tcp response timeout method=dxf.job.observation")
    setattr(exc, "method", "dxf.job.observation")
    setattr(exc, "request_id", "17")
    setattr(exc, "elapsed_ms", 2034.5)

    failure = operator_runner.build_observer_poll_failure(
        phase="polling",
        poll_index=8,
        job_id="job-42",
        exc=exc,
    )

    assert failure == {
        "phase": "polling",
        "poll_index": 8,
        "job_id": "job-42",
        "message": "tcp response timeout method=dxf.job.observation",
        "method": "dxf.job.observation",
        "request_id": "17",
        "elapsed_ms": 2034.5,
    }


def test_determine_overall_status_requires_strict_traceability_pass() -> None:
    status = operator_runner.determine_overall_status(
        operator_execution={"status": "passed"},
        traceability={"status": "insufficient_evidence"},
        observation_integrity={"status": "passed"},
    )

    assert status == "failed"


def test_determine_overall_status_rejects_observation_integrity_warning() -> None:
    status = operator_runner.determine_overall_status(
        operator_execution={"status": "passed"},
        traceability={"status": "passed"},
        observation_integrity={"status": "warning"},
    )

    assert status == "failed"


def test_build_report_emits_canonical_schema_and_timing_metadata() -> None:
    report = operator_runner.build_report(
        args=SimpleNamespace(
            dxf_file=Path("D:/Projects/SiligenSuite/samples/dxf/Demo-1.dxf"),
            gateway_exe=Path("D:/build/ca/bin/Debug/siligen_runtime_gateway.exe"),
            config_path=Path("D:/Projects/SiligenSuite/config/machine/machine_config.ini"),
            host="127.0.0.1",
        ),
        effective_port=61234,
        started_at="2026-04-24T08:40:07+00:00",
        finished_at="2026-04-24T08:47:55+00:00",
        duration_seconds=468.0,
        overall_status="passed",
        steps=[
            {
                "step": "input-check",
                "status": "passed",
                "detail": "{}",
                "timestamp": "2026-04-24T08:40:07+00:00",
            }
        ],
        operator_execution={
            "status": "passed",
            "issues": [],
            "operator_context": {
                "operator_context_stages": list(operator_runner.REQUIRED_OPERATOR_STAGES),
            },
        },
        traceability={"status": "passed"},
        observation_alignment={"status": "passed"},
        observation_integrity={"status": "passed"},
        artifacts={"report_json": "D:/reports/report.json"},
        snapshot_result={"plan_id": "plan-1"},
        log_summary={"execution_complete_count": 1},
        observer_poll_errors=[],
        observer_poll_failures=[],
        error_message="",
        hmi_command=["python", "ui_qtest.py"],
    )

    assert set(report) == {
        "schema_version",
        "started_at",
        "finished_at",
        "duration_seconds",
        "overall_status",
        "test_goal",
        "scope",
        "operator_execution",
        "traceability_correspondence",
        "observation_alignment",
        "observation_integrity",
        "control_script_capability",
        "steps",
        "artifacts",
        "snapshot_result",
        "log_summary",
        "observer_poll_errors",
        "observer_poll_failures",
        "error",
    }
    assert report["schema_version"] == operator_runner.REPORT_SCHEMA_VERSION
    assert report["started_at"] == "2026-04-24T08:40:07+00:00"
    assert report["finished_at"] == "2026-04-24T08:47:55+00:00"
    assert report["duration_seconds"] == 468.0
    assert report["scope"]["dxf_file"].endswith("Demo-1.dxf")
    assert report["scope"]["bypass_hmi_allowed"] is False
    assert report["control_script_capability"]["entry_script"].endswith("ui_qtest.py")
    assert report["control_script_capability"]["covers_production_start"] is True
    assert report["control_script_capability"]["status"] == "allow"
    assert report["control_script_capability"]["gaps"] == []
    assert all(set(step) == {"step", "status", "detail", "timestamp"} for step in report["steps"])
    assert "generated_at" not in report
    assert "input" not in report
    assert "operation_issues" not in report


def test_build_report_markdown_explicitly_marks_runtime_log_gap() -> None:
    report = {
        "schema_version": operator_runner.REPORT_SCHEMA_VERSION,
        "started_at": "2026-04-24T08:40:07+00:00",
        "finished_at": "2026-04-24T08:47:55+00:00",
        "duration_seconds": 468.0,
        "test_goal": ["goal"],
        "scope": {
            "dxf_file": "Demo-1.dxf",
            "hmi_entry": "ui_qtest.py",
        },
        "operator_execution": {
            "status": "passed",
            "issues": [],
        },
        "traceability_correspondence": {
            "status": "failed",
            "reason": "runtime dxf.job.traceability returned failed",
            "expected_trace_count": 2,
            "actual_trace_count": 1,
            "mismatch_count": 1,
            "terminal_state": "completed",
        },
        "observation_alignment": {
            "status": "passed",
            "reason": "summary-level alignment checks passed",
        },
        "observation_integrity": {
            "status": "warning",
            "reason": "observer polling reported transient errors",
        },
        "control_script_capability": {
            "entry_script": "ui_qtest.py",
            "covers_production_start": True,
            "status": "allow",
            "gaps": [],
        },
        "artifacts": {
            "hmi_stdout_log": "D:/reports/hmi-stdout.log",
            "runtime_log_note": "当前 runner 直接启动 runtime gateway，无独立 runtime 进程日志；请以 gateway stdout/stderr 与 tcp_server.log 为准。",
            "gateway_stdout_log": "D:/reports/gateway-stdout.log",
            "gateway_stderr_log": "D:/reports/gateway-stderr.log",
            "gateway_log_copy": "D:/reports/tcp_server.log",
            "gateway_launch_spec": "D:/reports/gateway-launch.json",
            "job_traceability": "D:/reports/job-traceability.json",
            "report_json": "D:/reports/report.json",
            "report_markdown": "D:/reports/report.md",
            "coord_status_history": "D:/reports/coord-status-history.json",
            "hmi_screenshot_dir": "D:/reports/screenshots",
        },
        "observer_poll_errors": [],
    }

    markdown = operator_runner.build_report_markdown(report)

    assert "当前 runner 直接启动 runtime gateway，无独立 runtime 进程日志" in markdown
    assert "- gateway stdout：`D:/reports/gateway-stdout.log`" in markdown
    assert "- gateway stderr：`D:/reports/gateway-stderr.log`" in markdown
    assert "- gateway tcp log：`D:/reports/tcp_server.log`" in markdown
    assert "- dxf.job.traceability：`D:/reports/job-traceability.json`" in markdown
    assert "runtime strict traceability 已明确失败" in markdown
    assert "summary-level alignment" in markdown
    assert "observation integrity 有告警" in markdown
    assert "- 测试开始：`2026-04-24T08:40:07+00:00`" in markdown
    assert "- 测试结束：`2026-04-24T08:47:55+00:00`" in markdown
    assert "- 总耗时（秒）：`468.0`" in markdown
    assert "- runtime：`D:/reports/gateway-stdout.log`" not in markdown


def test_format_runtime_evidence_prefers_explicit_runtime_logs() -> None:
    rendered = operator_runner.format_runtime_evidence(
        {
            "runtime_stdout_log": "D:/reports/runtime-stdout.log",
            "runtime_stderr_log": "D:/reports/runtime-stderr.log",
            "runtime_log_note": "should not be used",
        }
    )

    assert rendered == "`D:/reports/runtime-stdout.log` / `D:/reports/runtime-stderr.log`"
