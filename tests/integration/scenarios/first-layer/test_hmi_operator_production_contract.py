from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
from types import SimpleNamespace


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
            "FAIL: Timed out waiting for operator preview becomes ready without extra selection gate",
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


def test_build_report_markdown_explicitly_marks_runtime_log_gap() -> None:
    report = {
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
