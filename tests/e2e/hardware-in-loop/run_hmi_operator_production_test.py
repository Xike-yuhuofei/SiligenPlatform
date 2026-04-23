from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[3]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))

from dxf_hil_observation import DEFAULT_COORD_SYS, normalize_coord_status_sample, normalize_job_status_sample, normalize_machine_status_sample  # noqa: E402
from run_real_dxf_machine_dryrun import parse_int, status_result  # noqa: E402
from run_real_dxf_production_validation import (  # noqa: E402
    add_step,
    build_checklist,
    compute_axis_coverage,
    determine_overall_status,
    extract_gateway_log_summary,
    read_current_run_gateway_log,
    summarize_machine_bbox,
    summarize_points_bbox,
    write_json,
)
from runtime_gateway_harness import TcpJsonClient, build_process_env, resolve_default_exe, wait_gateway_ready  # noqa: E402


DEFAULT_GATEWAY_EXE = resolve_default_exe("siligen_runtime_gateway.exe")
DEFAULT_CONFIG_PATH = ROOT / "config" / "machine" / "machine_config.ini"
DEFAULT_DEMO1_DXF = ROOT / "samples" / "dxf" / "Demo-1.dxf"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "online-validation" / "hmi-production-operator-test"
DEFAULT_STATUS_POLL_INTERVAL_SECONDS = 0.10
DEFAULT_GATEWAY_READY_TIMEOUT = 8.0
DEFAULT_CONNECT_TIMEOUT = 15.0
DEFAULT_UI_TIMEOUT_MS = 900000
DEFAULT_MAX_POLYLINE_POINTS = 4000
DEFAULT_MAX_GLUE_POINTS = 5000
DEFAULT_MIN_AXIS_COVERAGE_RATIO = 0.80
UI_QTEST = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "tools" / "ui_qtest.py"
HMI_SOURCE_ROOT = ROOT / "apps" / "hmi-app" / "src"
HMI_APPLICATION_ROOT = ROOT / "modules" / "hmi-application" / "application"
REQUIRED_OPERATOR_STAGES = (
    "preview-ready",
    "preview-refreshed",
    "production-started",
    "production-running",
    "production-completed",
    "next-job-ready",
)
REQUIRED_SCREENSHOT_STAGES = (
    "production-running",
    "production-completed",
    "next-job-ready",
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def read_text_if_exists(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8")


def finalize_process_log_capture(
    *,
    stdout_stream: Any | None,
    stderr_stream: Any | None,
    stdout_path: Path,
    stderr_path: Path,
) -> tuple[None, None, str, str]:
    if stdout_stream is not None:
        stdout_stream.close()
    if stderr_stream is not None:
        stderr_stream.close()
    return None, None, read_text_if_exists(stdout_path), read_text_if_exists(stderr_path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Drive Demo-1 production through the HMI and capture operator-facing evidence."
    )
    parser.add_argument("--python-exe", default=sys.executable or "python")
    parser.add_argument("--gateway-exe", type=Path, default=DEFAULT_GATEWAY_EXE)
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DEMO1_DXF)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--gateway-ready-timeout", type=float, default=DEFAULT_GATEWAY_READY_TIMEOUT)
    parser.add_argument("--connect-timeout", type=float, default=DEFAULT_CONNECT_TIMEOUT)
    parser.add_argument("--status-poll-interval-seconds", type=float, default=DEFAULT_STATUS_POLL_INTERVAL_SECONDS)
    parser.add_argument("--ui-timeout-ms", type=int, default=DEFAULT_UI_TIMEOUT_MS)
    parser.add_argument("--max-polyline-points", type=int, default=DEFAULT_MAX_POLYLINE_POINTS)
    parser.add_argument("--max-glue-points", type=int, default=DEFAULT_MAX_GLUE_POINTS)
    parser.add_argument("--min-axis-coverage-ratio", type=float, default=DEFAULT_MIN_AXIS_COVERAGE_RATIO)
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    args = parser.parse_args()
    if args.status_poll_interval_seconds <= 0:
        raise SystemExit("--status-poll-interval-seconds must be > 0")
    if args.ui_timeout_ms <= 0:
        raise SystemExit("--ui-timeout-ms must be > 0")
    if args.max_polyline_points <= 0:
        raise SystemExit("--max-polyline-points must be > 0")
    if args.max_glue_points <= 0:
        raise SystemExit("--max-glue-points must be > 0")
    if args.min_axis_coverage_ratio < 0.0:
        raise SystemExit("--min-axis-coverage-ratio must be >= 0")
    return args


def resolve_listen_port(host: str, requested_port: int) -> int:
    if requested_port > 0:
        return requested_port
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return int(sock.getsockname()[1])


def build_hmi_process_env() -> dict[str, str]:
    env = os.environ.copy()
    py_entries = [str(HMI_APPLICATION_ROOT), str(HMI_SOURCE_ROOT)]
    existing = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = os.pathsep.join(py_entries + ([existing] if existing else []))
    return env


def build_hmi_command(*, args: argparse.Namespace, effective_port: int, screenshot_dir: Path) -> list[str]:
    return [
        args.python_exe,
        "-u",
        str(UI_QTEST),
        "--mode",
        "online",
        "--host",
        args.host,
        "--port",
        str(effective_port),
        "--no-mock",
        "--timeout-ms",
        str(args.ui_timeout_ms),
        "--exercise-runtime-actions",
        "--runtime-action-profile",
        "operator_production",
        "--dxf-browse-path",
        str(args.dxf_file),
        "--screenshot-dir",
        str(screenshot_dir),
    ]


def _parse_key_value_line(*, prefix: str, line: str) -> dict[str, str]:
    if not line.startswith(prefix):
        return {}
    payload = line[len(prefix) :].strip()
    fields: dict[str, str] = {}
    for token in payload.split():
        key, separator, value = token.partition("=")
        if separator:
            fields[key] = value
    return fields


def _contains_stage_sequence(stages: list[str], required_stages: tuple[str, ...]) -> bool:
    cursor = 0
    for stage in stages:
        if cursor >= len(required_stages):
            break
        if stage == required_stages[cursor]:
            cursor += 1
    return cursor == len(required_stages)


def _latest_field(events: list[dict[str, str]], field: str, *, default: str = "null") -> str:
    for event in reversed(events):
        value = str(event.get(field, "")).strip()
        if value and value.lower() != "null":
            return value
    return default


def summarize_operator_output(output: str) -> dict[str, Any]:
    events = [
        _parse_key_value_line(prefix="OPERATOR_CONTEXT ", line=line.strip())
        for line in output.splitlines()
    ]
    events = [event for event in events if event]
    screenshots = [
        _parse_key_value_line(prefix="HMI_SCREENSHOT ", line=line.strip())
        for line in output.splitlines()
    ]
    screenshots = [event for event in screenshots if event]
    stages = [str(event.get("stage", "")).strip() for event in events if str(event.get("stage", "")).strip()]
    failure_stages = [stage for stage in stages if stage.endswith("-failed")]
    by_stage: dict[str, dict[str, str]] = {}
    for event in events:
        stage = str(event.get("stage", "")).strip()
        if stage:
            by_stage[stage] = event

    screenshots_by_stage: dict[str, str] = {}
    unscoped_screenshots: list[str] = []
    for event in screenshots:
        stage = str(event.get("stage", "")).strip()
        path = str(event.get("path", "")).strip()
        if not path:
            continue
        if stage:
            screenshots_by_stage[stage] = path
        else:
            unscoped_screenshots.append(path)

    preview_ready = by_stage.get("preview-ready", {})
    preview_refreshed = by_stage.get("preview-refreshed", {})
    production_started = by_stage.get("production-started", {})
    production_running = by_stage.get("production-running", {})
    production_completed = by_stage.get("production-completed", {})
    next_job_ready = by_stage.get("next-job-ready", {})

    missing_screenshot_stages = [
        stage for stage in REQUIRED_SCREENSHOT_STAGES if stage not in screenshots_by_stage
    ]
    stage_sequence_ok = _contains_stage_sequence(stages, REQUIRED_OPERATOR_STAGES)
    preview_source = str(
        preview_refreshed.get("preview_source")
        or preview_ready.get("preview_source")
        or production_started.get("preview_source")
        or _latest_field(events, "preview_source")
    )
    preview_kind = str(
        preview_refreshed.get("preview_kind")
        or preview_ready.get("preview_kind")
        or production_started.get("preview_kind")
        or _latest_field(events, "preview_kind")
    )
    glue_point_count = int(
        parse_int(
            preview_refreshed.get("glue_point_count")
            or preview_ready.get("glue_point_count")
            or production_started.get("glue_point_count")
            or _latest_field(events, "glue_point_count", default="0")
        )
        or 0
    )
    snapshot_hash = str(
        preview_refreshed.get("snapshot_hash")
        or preview_ready.get("snapshot_hash")
        or _latest_field(events, "snapshot_hash")
    )
    confirmed_snapshot_hash = str(
        production_started.get("confirmed_snapshot_hash")
        or production_running.get("confirmed_snapshot_hash")
        or production_completed.get("confirmed_snapshot_hash")
        or _latest_field(events, "confirmed_snapshot_hash")
    )
    preview_confirmed = any(
        str(event.get("preview_confirmed", "")).strip().lower() == "true"
        for event in (production_started, production_running, production_completed, next_job_ready)
        if event
    )
    plan_fingerprint = _latest_field(events, "plan_fingerprint")
    preview_gate_state = _latest_field(events, "preview_gate_state")
    preview_gate_error = _latest_field(events, "preview_gate_error")

    contract_ok = (
        stage_sequence_ok
        and not failure_stages
        and preview_source == "planned_glue_snapshot"
        and preview_kind == "glue_points"
        and glue_point_count > 0
        and snapshot_hash not in {"", "null"}
        and confirmed_snapshot_hash not in {"", "null"}
        and preview_confirmed
        and not missing_screenshot_stages
    )
    contract_error = ""
    if not contract_ok:
        contract_error = (
            "operator production contract missing required stage sequence, preview authority evidence, "
            "or staged screenshots; "
            f"observed_stages={stages}; "
            f"failure_stages={failure_stages}; "
            f"missing_screenshots={missing_screenshot_stages}; "
            f"preview_gate_state={preview_gate_state}; "
            f"preview_gate_error={preview_gate_error}"
        )

    return {
        "operator_context_stages": stages,
        "failure_stages": failure_stages,
        "stage_sequence_ok": stage_sequence_ok,
        "artifact_id": str(
            preview_refreshed.get("artifact_id")
            or preview_ready.get("artifact_id")
            or production_started.get("artifact_id")
            or "null"
        ),
        "plan_id": str(
            preview_refreshed.get("plan_id")
            or preview_ready.get("plan_id")
            or production_started.get("plan_id")
            or "null"
        ),
        "plan_fingerprint": plan_fingerprint,
        "preview_source": preview_source,
        "preview_kind": preview_kind,
        "glue_point_count": glue_point_count,
        "preview_gate_state": preview_gate_state,
        "preview_gate_error": preview_gate_error,
        "snapshot_hash": snapshot_hash,
        "confirmed_snapshot_hash": confirmed_snapshot_hash,
        "snapshot_ready": str(preview_refreshed.get("snapshot_ready") or preview_ready.get("snapshot_ready") or "false")
        == "true",
        "preview_confirmed": preview_confirmed,
        "job_id": str(
            production_started.get("job_id")
            or production_running.get("job_id")
            or production_completed.get("job_id")
            or "null"
        ),
        "target_count": int(
            parse_int(
                production_started.get("target_count")
                or production_running.get("target_count")
                or production_completed.get("target_count")
                or next_job_ready.get("target_count")
            )
            or 0
        ),
        "completed_count_label": str(
            next_job_ready.get("completed_count")
            or production_completed.get("completed_count")
            or production_running.get("completed_count")
            or "0/0"
        ),
        "current_operation": str(
            next_job_ready.get("current_operation")
            or production_completed.get("current_operation")
            or production_running.get("current_operation")
            or "null"
        ),
        "global_progress_percent": int(
            parse_int(
                next_job_ready.get("global_progress_percent")
                or production_completed.get("global_progress_percent")
                or production_running.get("global_progress_percent")
            )
            or 0
        ),
        "screenshots_by_stage": screenshots_by_stage,
        "unscoped_screenshots": unscoped_screenshots,
        "missing_screenshot_stages": missing_screenshot_stages,
        "contract_ok": contract_ok,
        "contract_error": contract_error,
    }


def collect_observer_sample(
    client: TcpJsonClient,
    *,
    poll_index: int,
    job_id: str,
    timeout_seconds: float = 2.0,
) -> dict[str, Any]:
    sampled_at = utc_now()
    machine_response = client.send_request("status", None, timeout_seconds=timeout_seconds)
    if "error" in machine_response:
        raise RuntimeError("status failed during operator polling: " + json.dumps(machine_response, ensure_ascii=True))

    machine_sample = normalize_machine_status_sample(
        machine_response,
        sampled_at=sampled_at,
        poll_index=poll_index,
    )
    machine_job_id = str((machine_sample.get("job_execution") or {}).get("job_id", "")).strip()

    coord_sample: dict[str, Any] = {}
    if bool(machine_sample.get("connected", False)):
        coord_response = client.send_request(
            "motion.coord.status",
            {"coord_sys": DEFAULT_COORD_SYS},
            timeout_seconds=timeout_seconds,
        )
        if "error" in coord_response:
            raise RuntimeError("motion.coord.status failed during operator polling: " + json.dumps(coord_response, ensure_ascii=True))
        coord_sample = normalize_coord_status_sample(
            coord_response,
            sampled_at=sampled_at,
            poll_index=poll_index,
            coord_sys=DEFAULT_COORD_SYS,
        )

    effective_job_id = job_id or machine_job_id
    job_sample: dict[str, Any] = {}
    if effective_job_id:
        job_response = client.send_request(
            "dxf.job.status",
            {"job_id": effective_job_id},
            timeout_seconds=timeout_seconds,
        )
        if "error" in job_response:
            raise RuntimeError("dxf.job.status failed during operator polling: " + json.dumps(job_response, ensure_ascii=True))
        job_sample = normalize_job_status_sample(
            job_response,
            sampled_at=sampled_at,
            poll_index=poll_index,
        )

    return {
        "machine_status": machine_sample,
        "coord_status": coord_sample,
        "job_status": job_sample,
        "detected_job_id": effective_job_id or machine_job_id,
    }


def read_preview_snapshot(
    client: TcpJsonClient,
    *,
    plan_id: str,
    max_polyline_points: int,
    max_glue_points: int,
) -> dict[str, Any]:
    response = client.send_request(
        "dxf.preview.snapshot",
        {
            "plan_id": plan_id,
            "max_polyline_points": max_polyline_points,
            "max_glue_points": max_glue_points,
        },
        timeout_seconds=15.0,
    )
    if "error" in response:
        raise RuntimeError("dxf.preview.snapshot readback failed: " + json.dumps(response, ensure_ascii=True))
    return status_result(response)


def build_operator_issue(
    *,
    issue_type: str,
    phenomenon: str,
    impact: str,
    reproduction_steps: str,
    current_evidence: str,
    owner_layer: str,
) -> dict[str, str]:
    return {
        "type": issue_type,
        "phenomenon": phenomenon,
        "impact": impact,
        "reproduction_steps": reproduction_steps,
        "current_evidence": current_evidence,
        "owner_layer": owner_layer,
    }


def evaluate_operator_execution(
    *,
    operator_summary: dict[str, Any],
    final_job_status: dict[str, Any],
    hmi_exit_code: int,
    combined_output: str,
) -> dict[str, Any]:
    issues: list[dict[str, str]] = []
    if hmi_exit_code != 0:
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon=f"HMI 自动化退出码为 {hmi_exit_code}",
                impact="操作主线未被正式入口完整覆盖或执行中断。",
                reproduction_steps="运行 run_hmi_operator_production_test.py 并观察 hmi-stdout.log / hmi-stderr.log。",
                current_evidence=f"hmi_exit_code={hmi_exit_code}",
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )
    if not bool(operator_summary.get("stage_sequence_ok", False)):
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="operator_production 缺少完整阶段序列。",
                impact="无法证明 HMI 正式入口已覆盖 browse -> preview -> start -> completed -> next-job-ready。",
                reproduction_steps="检查 OPERATOR_CONTEXT 阶段序列。",
                current_evidence=json.dumps(operator_summary.get("operator_context_stages", []), ensure_ascii=False),
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )
    if operator_summary.get("failure_stages"):
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="operator_production 进入 fail-closed 失败阶段。",
                impact="正式链路在 HMI 阶段已中止，不能把本轮结果视为成功生产。",
                reproduction_steps="检查 *-failed 阶段、preview_gate_state/preview_gate_error 与失败截图。",
                current_evidence=json.dumps(
                    {
                        "failure_stages": operator_summary.get("failure_stages", []),
                        "preview_gate_state": operator_summary.get("preview_gate_state", "null"),
                        "preview_gate_error": operator_summary.get("preview_gate_error", "null"),
                        "plan_fingerprint": operator_summary.get("plan_fingerprint", "null"),
                    },
                    ensure_ascii=False,
                ),
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )
    if str(operator_summary.get("preview_source", "")).strip() != "planned_glue_snapshot":
        issues.append(
            build_operator_issue(
                issue_type="误导类",
                phenomenon="预览 authority 不是 planned_glue_snapshot。",
                impact="正式生产前的 HMI 预览真值不可信。",
                reproduction_steps="检查 preview-ready / preview-refreshed 的 OPERATOR_CONTEXT。",
                current_evidence=f"preview_source={operator_summary.get('preview_source', 'null')}",
                owner_layer="apps/hmi-app/src/hmi_client/ui/main_window.py",
            )
        )
    if str(operator_summary.get("preview_kind", "")).strip() != "glue_points":
        issues.append(
            build_operator_issue(
                issue_type="误导类",
                phenomenon="预览 kind 不是 glue_points。",
                impact="生产前界面与 runtime-owned authority 不一致。",
                reproduction_steps="检查 preview-refreshed 的 OPERATOR_CONTEXT。",
                current_evidence=f"preview_kind={operator_summary.get('preview_kind', 'null')}",
                owner_layer="apps/hmi-app/src/hmi_client/ui/main_window.py",
            )
        )
    if not bool(operator_summary.get("preview_confirmed", False)):
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="生产开始前未观察到 preview.confirm 的已确认状态。",
                impact="不能证明本轮生产仍遵守 HMI 正式预检与确认语义。",
                reproduction_steps="检查 production-started / production-running 的 OPERATOR_CONTEXT。",
                current_evidence=f"confirmed_snapshot_hash={operator_summary.get('confirmed_snapshot_hash', 'null')}",
                owner_layer="apps/hmi-app/src/hmi_client/ui/main_window.py",
            )
        )
    final_state = str(final_job_status.get("state", "")).strip().lower()
    if final_state != "completed":
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="observer 未观察到 completed 终态。",
                impact="不能证明 HMI 发起的正式生产实际完成。",
                reproduction_steps="检查 job-status-history.json 与 hmi-stdout.log。",
                current_evidence=f"final_job_status.state={final_state or 'null'}",
                owner_layer="tests/e2e/hardware-in-loop/run_hmi_operator_production_test.py",
            )
        )
    target_count = int(parse_int(final_job_status.get("target_count")) or 0)
    completed_count = int(parse_int(final_job_status.get("completed_count")) or 0)
    if final_state == "completed" and (target_count <= 0 or completed_count != target_count):
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="completed_count 与 target_count 不一致。",
                impact="生产完成结论与 batch 统计不一致。",
                reproduction_steps="检查 final_job_status 与 HMI completed label。",
                current_evidence=json.dumps(
                    {
                        "completed_count": completed_count,
                        "target_count": target_count,
                        "completed_count_label": operator_summary.get("completed_count_label", "0/0"),
                    },
                    ensure_ascii=False,
                ),
                owner_layer="apps/hmi-app/src/hmi_client/ui/main_window.py",
            )
        )
    if "next-job-ready" not in tuple(operator_summary.get("operator_context_stages", ())):
        issues.append(
            build_operator_issue(
                issue_type="恢复类",
                phenomenon="完成后未观察到 next-job-ready 阶段。",
                impact="不能证明界面已恢复到下一单可发起状态。",
                reproduction_steps="检查 OPERATOR_CONTEXT 与 staged screenshots。",
                current_evidence=json.dumps(operator_summary.get("operator_context_stages", []), ensure_ascii=False),
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )
    if str(operator_summary.get("contract_error", "")).strip():
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="operator_production 合同不完整。",
                impact="本轮 HMI 正式入口证据不收敛。",
                reproduction_steps="检查 OPERATOR_CONTEXT / HMI_SCREENSHOT 输出。",
                current_evidence=str(operator_summary.get("contract_error", "")).strip(),
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )

    return {
        "status": "passed" if not issues else "failed",
        "issues": issues,
        "hmi_exit_code": hmi_exit_code,
        "final_job_status": final_job_status,
        "operator_context": operator_summary,
        "hmi_output_excerpt": "\n".join(
            line
            for line in combined_output.splitlines()
            if line.strip().startswith(("FAIL:", "OPERATOR_CONTEXT ", "HMI_SCREENSHOT "))
        )[:4000],
    }


def evaluate_traceability_correspondence(
    *,
    snapshot_result: dict[str, Any],
    final_job_status: dict[str, Any],
    log_summary: dict[str, Any],
    machine_status_history: list[dict[str, Any]],
    coord_status_history: list[dict[str, Any]],
    min_axis_coverage_ratio: float,
    snapshot_readback_error: str,
) -> dict[str, Any]:
    expected_bbox = summarize_points_bbox(snapshot_result.get("glue_points", []))
    observed_bbox = summarize_machine_bbox(machine_status_history)
    axis_coverage = compute_axis_coverage(expected_bbox, observed_bbox)
    checks: dict[str, dict[str, Any]] = {}
    summary_alignment_status = "not_evaluated"
    summary_alignment_error = ""

    if snapshot_result and final_job_status:
        checks = build_checklist(
            validation_mode="production_execution",
            plan_result={},
            snapshot_result=snapshot_result,
            job_start_response={},
            blocked_motion_observation={},
            final_job_status=final_job_status,
            log_summary=log_summary,
            axis_coverage=axis_coverage,
            observed_bbox=observed_bbox,
            min_axis_coverage_ratio=min_axis_coverage_ratio,
        )
        summary_alignment_status, summary_alignment_error = determine_overall_status(checks, "production_execution")
    elif snapshot_readback_error:
        summary_alignment_status = "failed"
        summary_alignment_error = snapshot_readback_error
    else:
        summary_alignment_status = "failed"
        summary_alignment_error = "missing snapshot readback or final job status for summary alignment"

    if summary_alignment_status == "failed":
        status = "failed"
        reason = summary_alignment_error
    elif len(coord_status_history) <= 0:
        status = "insufficient_evidence"
        reason = "coord-status-history 为空，不能把 summary-level alignment 升级为严格一一对应。"
    else:
        status = "insufficient_evidence"
        reason = "已收集 summary-level alignment，但当前 runner 仍未证明严格一一对应。"

    return {
        "status": status,
        "reason": reason,
        "strict_one_to_one_proven": False,
        "summary_alignment_status": summary_alignment_status,
        "summary_alignment_error": summary_alignment_error,
        "summary_alignment_checks": checks,
        "coord_status_sample_count": len(coord_status_history),
        "expected_bbox": expected_bbox,
        "observed_bbox": observed_bbox,
        "axis_coverage": axis_coverage,
    }


def build_report(
    *,
    args: argparse.Namespace,
    effective_port: int,
    overall_status: str,
    steps: list[dict[str, str]],
    operator_execution: dict[str, Any],
    traceability: dict[str, Any],
    artifacts: dict[str, Any],
    snapshot_result: dict[str, Any],
    log_summary: dict[str, Any],
    observer_poll_errors: list[str],
    error_message: str,
    hmi_command: list[str],
) -> dict[str, Any]:
    stages = tuple(operator_execution.get("operator_context", {}).get("operator_context_stages", ()))
    control_script_allowed = "production-started" in stages
    control_script_gaps: list[str] = []
    if not control_script_allowed:
        control_script_gaps.append("未观察到 production-started 阶段")
    if "next-job-ready" not in stages:
        control_script_gaps.append("未观察到 next-job-ready 阶段")

    return {
        "generated_at": utc_now(),
        "overall_status": overall_status,
        "test_goal": [
            f"通过 HMI 主链路执行 {args.dxf_file.name} 的真实生产。",
            "输出操作问题清单，并把生产可执行结论与追溯严格一一对应结论分开。",
        ],
        "scope": {
            "dxf_file": str(args.dxf_file),
            "hmi_entry": str(UI_QTEST),
            "gateway_exe": str(args.gateway_exe),
            "config_path": str(args.config_path),
            "gateway_host": args.host,
            "gateway_port": effective_port,
            "bypass_hmi_allowed": False,
        },
        "operator_execution": operator_execution,
        "traceability_correspondence": traceability,
        "control_script_capability": {
            "entry_script": str(UI_QTEST),
            "covers_production_start": control_script_allowed,
            "status": "allow" if control_script_allowed else "block",
            "gaps": control_script_gaps,
            "command": hmi_command,
        },
        "steps": steps,
        "artifacts": artifacts,
        "snapshot_result": snapshot_result,
        "log_summary": log_summary,
        "observer_poll_errors": observer_poll_errors,
        "error": error_message,
    }


def build_report_markdown(report: dict[str, Any]) -> str:
    scope = report.get("scope", {})
    operator_execution = report.get("operator_execution", {})
    traceability = report.get("traceability_correspondence", {})
    control_script_capability = report.get("control_script_capability", {})
    artifacts = report.get("artifacts", {})
    issues = operator_execution.get("issues", [])

    lines = [
        "# HMI 生产操作测试单",
        "",
        "### 1. 测试目标",
    ]
    for item in report.get("test_goal", []):
        lines.append(f"- {item}")

    lines.extend(
        [
            "",
            "### 2. 操作范围",
            f"- DXF / 生产对象：`{scope.get('dxf_file', '')}`",
            f"- HMI 入口：`{scope.get('hmi_entry', '')}`",
            "- 是否允许绕过 HMI：不允许",
            "",
            "### 3. 操作主线",
            "1. 启动 runtime gateway 与 HMI qtest 正式入口。",
            "2. 通过 HMI browse DXF，等待 preview-ready 与 preview-refreshed。",
            "3. 触发生产开始，观察运行中状态与 staged screenshots。",
            "4. 观察 completed 终态与 next-job-ready 恢复状态。",
            "5. 同步冻结 gateway 日志、job/machine/coord 历史与最终报告。",
            "",
            "### 4. 操作问题清单",
        ]
    )
    if issues:
        for issue in issues:
            lines.extend(
                [
                    f"- 类型：`{issue['type']}`",
                    f"  现象：{issue['phenomenon']}",
                    f"  影响：{issue['impact']}",
                    f"  复现步骤：{issue['reproduction_steps']}",
                    f"  当前证据：{issue['current_evidence']}",
                    f"  建议归属层：{issue['owner_layer']}",
                ]
            )
    else:
        lines.append("- 无新增阻塞/误导/恢复问题；operator_production 正式入口已完成覆盖。")

    lines.extend(
        [
            "",
            "### 5. 追溯一致性结论",
            f"- 生产执行结论：`{operator_execution.get('status', 'failed')}`",
            f"- 严格一一对应结论：`{traceability.get('status', 'failed')}`",
            f"- 证据边界：{traceability.get('reason', '')}",
            "",
            "### 6. 控制脚本能力结论",
            f"- 入口脚本：`{control_script_capability.get('entry_script', '')}`",
            f"- 是否覆盖 production start：`{str(bool(control_script_capability.get('covers_production_start', False))).lower()}`",
            f"- 结论：`{'允许' if control_script_capability.get('status') == 'allow' else '阻断'}`",
        ]
    )
    gaps = control_script_capability.get("gaps", [])
    if gaps:
        lines.append(f"- 若阻断，缺口：`{json.dumps(gaps, ensure_ascii=False)}`")

    lines.extend(
        [
            "",
            "### 7. 关键证据",
            f"- HMI：`{artifacts.get('hmi_stdout_log', '')}`",
            f"- runtime：`{artifacts.get('gateway_stdout_log', '')}`",
            f"- gateway：`{artifacts.get('gateway_stderr_log', '')}`",
            f"- report：`{artifacts.get('report_json', '')}` / `{artifacts.get('report_markdown', '')}`",
            f"- coord-status-history：`{artifacts.get('coord_status_history', '')}`",
            f"- staged screenshots：`{artifacts.get('hmi_screenshot_dir', '')}`",
            "",
            "### 8. 中止条件与风险",
        ]
    )
    if operator_execution.get("status") != "passed":
        lines.append("- HMI operator 正式入口仍有阻塞，不能把本轮结论升级为“人工操作可稳定完成生产”。")
    if traceability.get("status") == "insufficient_evidence":
        lines.append("- 当前只能给出 summary-level alignment 观察，不能宣称严格一一对应。")
    if report.get("observer_poll_errors"):
        lines.append(f"- observer polling 存在波动：`{json.dumps(report['observer_poll_errors'][:3], ensure_ascii=False)}`")

    lines.extend(["", "### 9. 建议下一步"])
    if operator_execution.get("status") != "passed":
        lines.append("- 先修操作阻塞问题，再重跑本 runner 复核完整阶段序列。")
    elif traceability.get("status") == "failed":
        lines.append("- 先修 summary-level alignment 失败项，再重跑 operator runner。")
    else:
        lines.append("- 先补 strict traceability evaluator 或更强的 coord-level 对齐证据，再讨论严格一一对应结论。")
        lines.append("- 同步把该入口接入文档矩阵，明确它与 P1-04 runtime action matrix 的边界。")

    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    effective_port = resolve_listen_port(args.host, args.port)
    report_dir = args.report_root / time.strftime("%Y%m%d-%H%M%S")
    report_dir.mkdir(parents=True, exist_ok=True)

    report_json_path = report_dir / "hmi-production-operator-test.json"
    report_md_path = report_dir / "hmi-production-operator-test.md"
    job_status_json_path = report_dir / "job-status-history.json"
    machine_status_json_path = report_dir / "machine-status-history.json"
    coord_status_json_path = report_dir / "coord-status-history.json"
    hmi_stdout_log_path = report_dir / "hmi-stdout.log"
    hmi_stderr_log_path = report_dir / "hmi-stderr.log"
    hmi_screenshot_dir = report_dir / "hmi-screenshots"
    gateway_log_copy_path = report_dir / "tcp_server.log"
    gateway_stdout_path = report_dir / "gateway-stdout.log"
    gateway_stderr_path = report_dir / "gateway-stderr.log"

    steps: list[dict[str, str]] = []
    artifacts: dict[str, Any] = {}
    machine_status_history: list[dict[str, Any]] = []
    coord_status_history: list[dict[str, Any]] = []
    job_status_history: list[dict[str, Any]] = []
    observer_poll_errors: list[str] = []
    overall_status = "failed"
    error_message = ""
    gateway_process: subprocess.Popen[str] | None = None
    hmi_process: subprocess.Popen[str] | None = None
    observer_client = TcpJsonClient(args.host, effective_port)
    observer_connected = False
    current_job_id = ""
    final_job_status: dict[str, Any] = {}
    snapshot_result: dict[str, Any] = {}
    snapshot_readback_error = ""
    hmi_stdout = ""
    hmi_stderr = ""
    hmi_exit_code = 1
    hmi_command: list[str] = []
    gateway_log_path = ROOT / "logs" / "tcp_server.log"
    gateway_log_offset = gateway_log_path.stat().st_size if gateway_log_path.exists() else 0
    gateway_stdout_stream = gateway_stdout_path.open("w", encoding="utf-8")
    gateway_stderr_stream = gateway_stderr_path.open("w", encoding="utf-8")
    hmi_stdout_stream = hmi_stdout_log_path.open("w", encoding="utf-8")
    hmi_stderr_stream = hmi_stderr_log_path.open("w", encoding="utf-8")

    try:
        if not args.gateway_exe.exists():
            raise FileNotFoundError(f"gateway executable missing: {args.gateway_exe}")
        if not args.config_path.exists():
            raise FileNotFoundError(f"config path missing: {args.config_path}")
        if not args.dxf_file.exists():
            raise FileNotFoundError(f"dxf file missing: {args.dxf_file}")

        add_step(
            steps,
            "input-check",
            "passed",
            json.dumps(
                {
                    "dxf_file": str(args.dxf_file),
                    "operator_trigger": "hmi_only",
                    "target_count": 1,
                },
                ensure_ascii=True,
            ),
        )

        gateway_process = subprocess.Popen(
            [str(args.gateway_exe), "--config", str(args.config_path), "--port", str(effective_port)],
            cwd=str(ROOT),
            stdout=gateway_stdout_stream,
            stderr=gateway_stderr_stream,
            text=True,
            encoding="utf-8",
            env=build_process_env(args.gateway_exe),
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        add_step(steps, "gateway-launch", "passed", f"pid={gateway_process.pid}")

        ready_status, ready_note = wait_gateway_ready(gateway_process, args.host, effective_port, args.gateway_ready_timeout)
        add_step(steps, "gateway-ready", ready_status, ready_note)
        if ready_status != "passed":
            raise RuntimeError(ready_note)

        observer_client.connect(timeout_seconds=args.connect_timeout)
        observer_connected = True
        add_step(steps, "observer-session-open", "passed", f"connected to {args.host}:{effective_port}")

        hmi_command = build_hmi_command(args=args, effective_port=effective_port, screenshot_dir=hmi_screenshot_dir)
        add_step(steps, "hmi-command", "passed", json.dumps(hmi_command, ensure_ascii=False))
        hmi_process = subprocess.Popen(
            hmi_command,
            cwd=str(ROOT),
            stdout=hmi_stdout_stream,
            stderr=hmi_stderr_stream,
            text=True,
            encoding="utf-8",
            env=build_hmi_process_env(),
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        add_step(steps, "hmi-launch", "passed", f"pid={hmi_process.pid}")

        deadline = time.time() + (args.ui_timeout_ms / 1000.0) + 5.0
        poll_index = 0
        while True:
            if time.time() >= deadline:
                raise TimeoutError(f"HMI operator production timed out after {args.ui_timeout_ms}ms")
            if observer_connected:
                try:
                    sample = collect_observer_sample(
                        observer_client,
                        poll_index=poll_index,
                        job_id=current_job_id,
                    )
                    machine_status_history.append(sample["machine_status"])
                    if sample["coord_status"]:
                        coord_status_history.append(sample["coord_status"])
                    if sample["job_status"]:
                        job_status_history.append(sample["job_status"])
                        final_job_status = sample["job_status"]
                    detected_job_id = str(sample.get("detected_job_id", "") or "").strip()
                    if detected_job_id:
                        current_job_id = detected_job_id
                    poll_index += 1
                except Exception as exc:  # noqa: BLE001
                    message = str(exc).strip()
                    if not observer_poll_errors or observer_poll_errors[-1] != message:
                        observer_poll_errors.append(message)
            if hmi_process.poll() is not None:
                break
            time.sleep(args.status_poll_interval_seconds)

        hmi_exit_code = int(hmi_process.wait(timeout=5) or 0)
        hmi_stdout_stream, hmi_stderr_stream, hmi_stdout, hmi_stderr = finalize_process_log_capture(
            stdout_stream=hmi_stdout_stream,
            stderr_stream=hmi_stderr_stream,
            stdout_path=hmi_stdout_log_path,
            stderr_path=hmi_stderr_log_path,
        )
        add_step(
            steps,
            "hmi-exit",
            "passed" if hmi_exit_code == 0 else "failed",
            f"exit_code={hmi_exit_code}",
        )

        if observer_connected:
            try:
                sample = collect_observer_sample(
                    observer_client,
                    poll_index=poll_index,
                    job_id=current_job_id,
                )
                machine_status_history.append(sample["machine_status"])
                if sample["coord_status"]:
                    coord_status_history.append(sample["coord_status"])
                if sample["job_status"]:
                    job_status_history.append(sample["job_status"])
                    final_job_status = sample["job_status"]
            except Exception as exc:  # noqa: BLE001
                message = str(exc).strip()
                if not observer_poll_errors or observer_poll_errors[-1] != message:
                    observer_poll_errors.append(message)

        combined_output = (hmi_stdout or "") + ("\n" if hmi_stderr else "") + (hmi_stderr or "")
        operator_summary = summarize_operator_output(combined_output)

        plan_id = str(operator_summary.get("plan_id", "") or "").strip()
        if observer_connected and plan_id and plan_id != "null":
            try:
                snapshot_result = read_preview_snapshot(
                    observer_client,
                    plan_id=plan_id,
                    max_polyline_points=args.max_polyline_points,
                    max_glue_points=args.max_glue_points,
                )
                add_step(steps, "snapshot-readback", "passed", f"plan_id={plan_id}")
            except Exception as exc:  # noqa: BLE001
                snapshot_readback_error = str(exc).strip()
                add_step(steps, "snapshot-readback", "failed", snapshot_readback_error)
        else:
            snapshot_readback_error = "plan_id missing from operator output; cannot read back preview snapshot"
            add_step(steps, "snapshot-readback", "failed", snapshot_readback_error)

        operator_execution = evaluate_operator_execution(
            operator_summary=operator_summary,
            final_job_status=final_job_status,
            hmi_exit_code=hmi_exit_code,
            combined_output=combined_output,
        )
        overall_status = "passed" if operator_execution["status"] == "passed" else "failed"
    except Exception as exc:  # noqa: BLE001
        error_message = str(exc).strip()
        add_step(steps, "exception", "failed", error_message)
        if hmi_process is not None:
            if hmi_process.poll() is None:
                hmi_process.terminate()
                try:
                    hmi_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    hmi_process.kill()
                    hmi_process.wait(timeout=5)
            hmi_exit_code = int(hmi_process.returncode or 0)
        hmi_stdout_stream, hmi_stderr_stream, hmi_stdout, hmi_stderr = finalize_process_log_capture(
            stdout_stream=hmi_stdout_stream,
            stderr_stream=hmi_stderr_stream,
            stdout_path=hmi_stdout_log_path,
            stderr_path=hmi_stderr_log_path,
        )

        combined_output = (hmi_stdout or "") + ("\n" if hmi_stderr else "") + (hmi_stderr or "")
        operator_summary = summarize_operator_output(combined_output)
        plan_id = str(operator_summary.get("plan_id", "") or "").strip()
        if observer_connected and plan_id and plan_id != "null":
            try:
                snapshot_result = read_preview_snapshot(
                    observer_client,
                    plan_id=plan_id,
                    max_polyline_points=args.max_polyline_points,
                    max_glue_points=args.max_glue_points,
                )
                add_step(steps, "snapshot-readback", "passed", f"plan_id={plan_id}")
            except Exception as snapshot_exc:  # noqa: BLE001
                snapshot_readback_error = str(snapshot_exc).strip()
                add_step(steps, "snapshot-readback", "failed", snapshot_readback_error)
        elif not snapshot_readback_error:
            snapshot_readback_error = "plan_id missing from operator output; cannot read back preview snapshot"
            add_step(steps, "snapshot-readback", "failed", snapshot_readback_error)

        operator_execution = evaluate_operator_execution(
            operator_summary=operator_summary,
            final_job_status=final_job_status,
            hmi_exit_code=hmi_exit_code,
            combined_output=combined_output,
        )
        operator_execution["issues"].insert(
            0,
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="runner 执行异常",
                impact="本轮 HMI operator 证据未收敛。",
                reproduction_steps="查看 runner exception 与 artifacts。",
                current_evidence=error_message,
                owner_layer="tests/e2e/hardware-in-loop/run_hmi_operator_production_test.py",
            ),
        )
        operator_execution["status"] = "failed"
        overall_status = "failed"
    finally:
        if hmi_process is not None and hmi_process.poll() is None:
            hmi_process.terminate()
            try:
                hmi_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                hmi_process.kill()
                hmi_process.wait(timeout=5)
            hmi_exit_code = int(hmi_process.returncode or 0)
        if hmi_stdout_stream is not None:
            hmi_stdout_stream.close()
            hmi_stdout_stream = None
        if hmi_stderr_stream is not None:
            hmi_stderr_stream.close()
            hmi_stderr_stream = None
        if not hmi_stdout:
            hmi_stdout = read_text_if_exists(hmi_stdout_log_path)
        if not hmi_stderr:
            hmi_stderr = read_text_if_exists(hmi_stderr_log_path)

        if observer_connected:
            observer_client.close()

        if gateway_process is not None:
            if gateway_process.poll() is None:
                gateway_process.terminate()
                try:
                    gateway_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    gateway_process.kill()
                    gateway_process.wait(timeout=5)
        if gateway_stdout_stream is not None:
            gateway_stdout_stream.close()
            gateway_stdout_stream = None
        if gateway_stderr_stream is not None:
            gateway_stderr_stream.close()
            gateway_stderr_stream = None
        gateway_stdout = read_text_if_exists(gateway_stdout_path)
        gateway_stderr = read_text_if_exists(gateway_stderr_path)
        time.sleep(0.3)
        gateway_log_text = read_current_run_gateway_log(gateway_log_path, gateway_log_offset, effective_port)
        gateway_log_copy_path.write_text(gateway_log_text, encoding="utf-8")

        write_json(job_status_json_path, job_status_history)
        write_json(machine_status_json_path, machine_status_history)
        write_json(coord_status_json_path, coord_status_history)

        log_summary = extract_gateway_log_summary(gateway_log_text)
        traceability = evaluate_traceability_correspondence(
            snapshot_result=snapshot_result,
            final_job_status=final_job_status,
            log_summary=log_summary,
            machine_status_history=machine_status_history,
            coord_status_history=coord_status_history,
            min_axis_coverage_ratio=args.min_axis_coverage_ratio,
            snapshot_readback_error=snapshot_readback_error,
        )

        if operator_execution.get("status") != "passed" or traceability.get("status") == "failed":
            overall_status = "failed"
        else:
            overall_status = "passed"

        artifacts.update(
            {
                "hmi_stdout_log": str(hmi_stdout_log_path),
                "hmi_stderr_log": str(hmi_stderr_log_path),
                "hmi_screenshot_dir": str(hmi_screenshot_dir),
                "gateway_stdout_log": str(gateway_stdout_path),
                "gateway_stderr_log": str(gateway_stderr_path),
                "gateway_log_copy": str(gateway_log_copy_path),
                "job_status_history": str(job_status_json_path),
                "machine_status_history": str(machine_status_json_path),
                "coord_status_history": str(coord_status_json_path),
                "report_json": str(report_json_path),
                "report_markdown": str(report_md_path),
            }
        )

        report = build_report(
            args=args,
            effective_port=effective_port,
            overall_status=overall_status,
            steps=steps,
            operator_execution=operator_execution,
            traceability=traceability,
            artifacts=artifacts,
            snapshot_result=snapshot_result,
            log_summary=log_summary,
            observer_poll_errors=observer_poll_errors,
            error_message=error_message,
            hmi_command=hmi_command,
        )
        write_json(report_json_path, report)
        report_md_path.write_text(build_report_markdown(report), encoding="utf-8")
        print(f"report json: {report_json_path}")
        print(f"report md: {report_md_path}")

    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
