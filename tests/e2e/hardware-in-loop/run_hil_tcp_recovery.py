from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from run_hil_closed_loop import (  # noqa: E402
    DEFAULT_CONFIG_PATH,
    DEFAULT_DXF_FILE,
    KNOWN_FAILURE_EXIT_CODE,
    SKIPPED_EXIT_CODE,
    StepResult,
    TcpJsonClient,
    _build_failure_context,
    _evaluate_offline_admission,
    _evaluate_safety_preflight,
    _load_connection_params,
    _resolve_default_exe,
    _run_tcp_step,
    _status_snapshot_from_response,
    _wait_gateway_ready,
    build_process_env,
)
from test_kit.evidence_bundle import (  # noqa: E402
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    default_failure_classification,
    trace_fields,
    write_bundle_artifacts,
)


@dataclass
class RecoveryRoundReport:
    round_index: int
    status: str = "pending"
    baseline_status: dict[str, Any] = field(default_factory=dict)
    probe_before_disconnect: dict[str, Any] = field(default_factory=dict)
    disconnect_ack: dict[str, Any] = field(default_factory=dict)
    post_reconnect_status: dict[str, Any] = field(default_factory=dict)
    probe_after_reconnect: dict[str, Any] = field(default_factory=dict)
    steps: list[dict[str, Any]] = field(default_factory=list)
    error: str = ""


def _normalize_probe_payload(step: StepResult, response: dict[str, Any] | None, dxf_file: Path) -> dict[str, Any]:
    return {
        "method": "dxf.load",
        "filepath": str(dxf_file),
        "status": step.status,
        "note": step.note,
        "response": response or {},
    }


def _disconnect_ack_payload(response: dict[str, Any] | None) -> dict[str, Any]:
    result = response.get("result", {}) if isinstance(response, dict) else {}
    if not isinstance(result, dict):
        result = {}
    disconnected = result.get("disconnected")
    payload = {
        "disconnected": disconnected if isinstance(disconnected, bool) else None,
        "message": "",
    }
    if isinstance(disconnected, bool):
        payload["message"] = f"result.disconnected={disconnected}"
    else:
        payload["message"] = "result.disconnected missing"
    return payload


def _record_step(target: list[StepResult], round_report: RecoveryRoundReport, step: StepResult) -> None:
    target.append(step)
    round_report.steps.append(asdict(step))


def _assert_connected_snapshot(name: str, snapshot: dict[str, Any]) -> None:
    if not snapshot.get("connected", False):
        raise RuntimeError(f"{name} status connected=false")


def _run_round(
    *,
    args: argparse.Namespace,
    round_index: int,
    client: TcpJsonClient,
    connect_params: dict[str, Any],
    steps: list[StepResult],
) -> RecoveryRoundReport:
    round_report = RecoveryRoundReport(round_index=round_index)
    connect_timeout = max(3.0, float(args.reconnect_wait_timeout_seconds))

    try:
        connect_step, _ = _run_tcp_step(
            name=f"round-{round_index:02d}-tcp-connect-initial",
            client=client,
            method="connect",
            params=connect_params,
            timeout_seconds=connect_timeout,
        )
        _record_step(steps, round_report, connect_step)
        if connect_step.status != "passed":
            raise RuntimeError(connect_step.note or "connect failed before recovery probe")

        baseline_step, baseline_response = _run_tcp_step(
            name=f"round-{round_index:02d}-status-baseline",
            client=client,
            method="status",
            params={},
            timeout_seconds=8.0,
        )
        _record_step(steps, round_report, baseline_step)
        round_report.baseline_status = _status_snapshot_from_response(baseline_response)
        if baseline_step.status != "passed":
            raise RuntimeError(baseline_step.note or "baseline status failed")
        _assert_connected_snapshot("baseline", round_report.baseline_status)

        probe_before_step, probe_before_response = _run_tcp_step(
            name=f"round-{round_index:02d}-probe-before-disconnect",
            client=client,
            method="dxf.load",
            params={"filepath": str(args.dxf_file)},
            timeout_seconds=60.0,
        )
        _record_step(steps, round_report, probe_before_step)
        round_report.probe_before_disconnect = _normalize_probe_payload(
            probe_before_step,
            probe_before_response,
            args.dxf_file,
        )
        if probe_before_step.status != "passed":
            raise RuntimeError(probe_before_step.note or "probe before disconnect failed")

        disconnect_step, disconnect_response = _run_tcp_step(
            name=f"round-{round_index:02d}-tcp-disconnect-before-reconnect",
            client=client,
            method="disconnect",
            params={},
            timeout_seconds=8.0,
        )
        _record_step(steps, round_report, disconnect_step)
        round_report.disconnect_ack = _disconnect_ack_payload(disconnect_response)
        if disconnect_step.status != "passed":
            raise RuntimeError(disconnect_step.note or "disconnect failed before recovery reconnect")
        if round_report.disconnect_ack.get("disconnected") is not True:
            raise RuntimeError(str(round_report.disconnect_ack.get("message", "disconnect acknowledgement invalid")))

        reconnect_step, _ = _run_tcp_step(
            name=f"round-{round_index:02d}-tcp-connect-recovery",
            client=client,
            method="connect",
            params=connect_params,
            timeout_seconds=connect_timeout,
        )
        _record_step(steps, round_report, reconnect_step)
        if reconnect_step.status != "passed":
            raise RuntimeError(reconnect_step.note or "reconnect failed")

        post_reconnect_step, post_reconnect_response = _run_tcp_step(
            name=f"round-{round_index:02d}-status-after-reconnect",
            client=client,
            method="status",
            params={},
            timeout_seconds=8.0,
        )
        _record_step(steps, round_report, post_reconnect_step)
        round_report.post_reconnect_status = _status_snapshot_from_response(post_reconnect_response)
        if post_reconnect_step.status != "passed":
            raise RuntimeError(post_reconnect_step.note or "status failed after reconnect")
        _assert_connected_snapshot("post-reconnect", round_report.post_reconnect_status)

        probe_after_step, probe_after_response = _run_tcp_step(
            name=f"round-{round_index:02d}-probe-after-reconnect",
            client=client,
            method="dxf.load",
            params={"filepath": str(args.dxf_file)},
            timeout_seconds=60.0,
        )
        _record_step(steps, round_report, probe_after_step)
        round_report.probe_after_reconnect = _normalize_probe_payload(
            probe_after_step,
            probe_after_response,
            args.dxf_file,
        )
        if probe_after_step.status != "passed":
            raise RuntimeError(probe_after_step.note or "probe after reconnect failed")

        cleanup_step, _ = _run_tcp_step(
            name=f"round-{round_index:02d}-tcp-disconnect-cleanup",
            client=client,
            method="disconnect",
            params={},
            timeout_seconds=8.0,
        )
        _record_step(steps, round_report, cleanup_step)
        if cleanup_step.status != "passed":
            raise RuntimeError(cleanup_step.note or "cleanup disconnect failed")

        round_report.status = "passed"
    except Exception as exc:  # pragma: no cover - HIL runtime failure path
        round_report.status = "failed"
        round_report.error = str(exc)

    return round_report


def _bundle_verdict(overall_status: str) -> str:
    if overall_status == "known_failure":
        return "known-failure"
    return overall_status


def _write_evidence_bundle(report: dict[str, Any], report_dir: Path, summary_json_path: Path, summary_md_path: Path) -> None:
    overall_status = _bundle_verdict(str(report.get("overall_status", "incomplete")))
    admission = report.get("admission", {}) if isinstance(report.get("admission"), dict) else {}
    failure_classification = report.get("failure_classification", {}) if isinstance(report.get("failure_classification"), dict) else {}
    bundle = EvidenceBundle(
        bundle_id=f"hil-tcp-recovery-{report['generated_at']}",
        request_ref="hil-tcp-recovery",
        producer_lane_ref="limited-hil",
        report_root=str(report_dir.resolve()),
        summary_file=str(summary_md_path.resolve()),
        machine_file=str(summary_json_path.resolve()),
        verdict=overall_status,
        linked_asset_refs=("sample.dxf.rect_diag",),
        offline_prerequisites=tuple(admission.get("required_layers", ("L0-structure-gate", "L2-offline-integration", "L3-simulated-e2e"))),
        abort_metadata={
            "failure_context": report.get("failure_context", {}),
            "admission": admission,
        },
        metadata={
            "rounds_requested": report.get("rounds_requested", 0),
            "rounds_executed": report.get("rounds_executed", 0),
            "reconnect_count": report.get("reconnect_count", 0),
            "recovery_success_count": report.get("recovery_success_count", 0),
            "recovery_probe": report.get("recovery_probe", {}),
            "admission": admission,
        },
        case_records=[
            EvidenceCaseRecord(
                case_id="hil-tcp-recovery",
                name="hil-tcp-recovery",
                suite_ref="e2e",
                owner_scope="runtime-execution",
                primary_layer="L5-limited-hil",
                producer_lane_ref="limited-hil",
                status=overall_status,
                evidence_profile="hil-report",
                stability_state="stable",
                required_assets=("sample.dxf.rect_diag",),
                required_fixtures=("fixture.hil-tcp-recovery", "fixture.validation-evidence-bundle"),
                risk_tags=("hardware", "recovery"),
                note=str(report.get("failure_context", {}).get("error_message", "")),
                failure_classification=failure_classification,
                trace_fields=trace_fields(
                    stage_id="L5-limited-hil",
                    artifact_id="hil-tcp-recovery",
                    module_id="runtime-execution",
                    workflow_state="executed",
                    execution_state=overall_status,
                    event_name="hil-tcp-recovery",
                    failure_code=f"hil.{overall_status}" if overall_status != "passed" else "",
                    evidence_path=str(summary_json_path.resolve()),
                ),
            )
        ],
    )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=summary_json_path,
        summary_md_path=summary_md_path,
        evidence_links=[
            EvidenceLink(label="hil-tcp-recovery-summary.json", path=str(summary_json_path.resolve()), role="machine-summary"),
            EvidenceLink(label="hil-tcp-recovery-summary.md", path=str(summary_md_path.resolve()), role="human-summary"),
        ],
    )


def _write_reports(report: dict[str, Any], report_dir: Path) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "hil-tcp-recovery-summary.json"
    md_path = report_dir / "hil-tcp-recovery-summary.md"

    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# HIL TCP Recovery Summary",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- rounds_requested: `{report['rounds_requested']}`",
        f"- rounds_executed: `{report['rounds_executed']}`",
        f"- rounds_passed: `{report['rounds_passed']}`",
        f"- rounds_failed: `{report['rounds_failed']}`",
        f"- reconnect_count: `{report['reconnect_count']}`",
        f"- recovery_success_count: `{report['recovery_success_count']}`",
        f"- elapsed_seconds: `{report['elapsed_seconds']}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
        f"- transport: `{report['transport']}`",
        f"- host: `{report['host']}`",
        f"- port: `{report['port']}`",
        f"- config_path: `{report['config_path']}`",
        f"- dxf_file: `{report['dxf_file']}`",
        f"- reconnect_wait_timeout_seconds: `{report['reconnect_wait_timeout_seconds']}`",
        "",
    ]

    admission = report.get("admission")
    if isinstance(admission, dict):
        lines.extend(
            [
                "## Admission",
                "",
                f"- schema_version: `{admission.get('schema_version', '')}`",
                f"- offline_prerequisites_source: `{admission.get('offline_prerequisites_source', '')}`",
                f"- offline_prerequisites_passed: `{admission.get('offline_prerequisites_passed', False)}`",
                f"- operator_override_used: `{admission.get('operator_override_used', False)}`",
                f"- operator_override_reason: `{admission.get('operator_override_reason', '')}`",
                f"- admission_decision: `{admission.get('admission_decision', '')}`",
                f"- safety_preflight_passed: `{admission.get('safety_preflight_passed', False)}`",
                "",
            ]
        )
        checks = admission.get("checks", [])
        if isinstance(checks, list) and checks:
            lines.extend(["### Admission Checks", ""])
            for check in checks:
                lines.append(
                    f"- `{check.get('status', 'unknown')}` `{check.get('name', 'unknown')}` "
                    f"expected=`{check.get('expected', '')}` actual=`{check.get('actual', '')}`"
                )
            lines.append("")

        safety_preflight = admission.get("safety_preflight")
        if isinstance(safety_preflight, dict):
            lines.extend(
                [
                    "### Safety Preflight",
                    "",
                    f"- passed: `{safety_preflight.get('passed', False)}`",
                    f"- estop_active: `{safety_preflight.get('estop_active', False)}`",
                    f"- limit_blockers: `{','.join(safety_preflight.get('limit_blockers', []))}`",
                    "",
                ]
            )

    preflight_status = report.get("preflight_status_snapshot")
    if isinstance(preflight_status, dict) and preflight_status:
        lines.extend(
            [
                "## Preflight Status Snapshot",
                "",
                f"- machine_state: `{preflight_status.get('machine_state', '')}`",
                f"- machine_state_reason: `{preflight_status.get('machine_state_reason', '')}`",
                f"- connected: `{preflight_status.get('connected', False)}`",
                "",
            ]
        )

    failure_classification = report.get("failure_classification")
    if isinstance(failure_classification, dict):
        lines.extend(
            [
                "## Failure Classification",
                "",
                f"- category: `{failure_classification.get('category', '')}`",
                f"- code: `{failure_classification.get('code', '')}`",
                f"- blocking: `{failure_classification.get('blocking', False)}`",
                f"- message: `{failure_classification.get('message', '')}`",
                "",
            ]
        )

    failure_context = report.get("failure_context")
    if isinstance(failure_context, dict):
        lines.extend(
            [
                "## Failure Context",
                "",
                f"- iteration: `{failure_context.get('iteration', '')}`",
                f"- step_name: `{failure_context.get('step_name', '')}`",
                f"- method: `{failure_context.get('method', '')}`",
                f"- error_message: `{failure_context.get('error_message', '')}`",
                f"- last_success_step: `{failure_context.get('last_success_step', '')}`",
                f"- socket_connected: `{failure_context.get('socket_connected', '')}`",
                "",
            ]
        )
        snapshot = failure_context.get("recent_status_snapshot", [])
        if isinstance(snapshot, list) and snapshot:
            lines.extend(["### Recent Status Snapshot", ""])
            for item in snapshot:
                lines.append(
                    f"- `{item.get('status', 'unknown')}` `{item.get('name', 'unknown')}` note=`{item.get('note', '')}`"
                )
            lines.append("")

    lines.extend(["## Recovery Probe", ""])
    recovery_probe = report.get("recovery_probe", {})
    lines.append(f"- method: `{recovery_probe.get('method', '')}`")
    lines.append(f"- filepath: `{recovery_probe.get('filepath', '')}`")
    lines.append("")

    lines.extend(["## Rounds", ""])
    for round_report in report.get("rounds", []):
        lines.append(
            f"- `round-{round_report['round_index']:02d}` status=`{round_report['status']}` "
            f"baseline_connected=`{round_report.get('baseline_status', {}).get('connected', False)}` "
            f"post_reconnect_connected=`{round_report.get('post_reconnect_status', {}).get('connected', False)}` "
            f"disconnect_ack=`{round_report.get('disconnect_ack', {}).get('disconnected', None)}`"
        )
        if round_report.get("error"):
            lines.append(f"  error: {round_report['error']}")
    lines.append("")

    md_path.write_text("\n".join(lines), encoding="utf-8")
    _write_evidence_bundle(report, report_dir, json_path, md_path)
    return json_path, md_path


def _build_report(
    *,
    args: argparse.Namespace,
    gateway_exe: Path,
    steps: list[StepResult],
    rounds: list[RecoveryRoundReport],
    elapsed_seconds: float,
    admission: dict[str, Any],
    preflight_status_snapshot: dict[str, Any] | None,
    socket_connected: bool | None = None,
    failure_context: dict[str, Any] | None = None,
    failure_classification: dict[str, Any] | None = None,
) -> dict[str, Any]:
    statuses = [step.status for step in steps]
    overall_status = "passed"
    if "failed" in statuses:
        overall_status = "failed"
    elif "known_failure" in statuses:
        overall_status = "known_failure"
    elif "skipped" in statuses and not rounds:
        overall_status = "skipped"

    rounds_passed = sum(1 for item in rounds if item.status == "passed")
    rounds_failed = sum(1 for item in rounds if item.status != "passed")
    if overall_status == "passed" and rounds_passed != args.rounds:
        overall_status = "failed"

    reconnect_count = sum(
        1
        for step in steps
        if step.name.endswith("tcp-connect-recovery") and step.status == "passed"
    )
    recovery_success_count = sum(
        1
        for item in rounds
        if item.status == "passed" and item.probe_after_reconnect.get("status") == "passed"
    )

    resolved_failure_context = failure_context
    if overall_status != "passed" and resolved_failure_context is None:
        resolved_failure_context = _build_failure_context(
            steps=steps,
            iterations=len(rounds),
            socket_connected=socket_connected,
        )

    resolved_failure_classification = failure_classification
    if resolved_failure_classification is None:
        resolved_failure_classification = default_failure_classification(
            status=overall_status,
            failure_code=f"hil.{overall_status}" if overall_status != "passed" else "",
            note=str((resolved_failure_context or {}).get("error_message", "")),
        )

    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "gateway_exe": str(gateway_exe),
        "transport": "tcp-json",
        "host": args.host,
        "port": args.port,
        "config_path": str(args.config_path),
        "dxf_file": str(args.dxf_file),
        "rounds_requested": args.rounds,
        "rounds_executed": len(rounds),
        "rounds_passed": rounds_passed,
        "rounds_failed": rounds_failed,
        "reconnect_count": reconnect_count,
        "recovery_success_count": recovery_success_count,
        "recovery_probe": {
            "method": "dxf.load",
            "filepath": str(args.dxf_file),
        },
        "reconnect_wait_timeout_seconds": args.reconnect_wait_timeout_seconds,
        "elapsed_seconds": round(elapsed_seconds, 3),
        "preflight_status_snapshot": preflight_status_snapshot or {},
        "overall_status": overall_status,
        "admission": admission,
        "failure_classification": resolved_failure_classification,
        "steps": [asdict(step) for step in steps],
        "rounds": [asdict(item) for item in rounds],
    }
    if resolved_failure_context is not None:
        payload["failure_context"] = resolved_failure_context
    return payload


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run HIL TCP disconnect/reconnect recovery workflow and write structured reports.")
    parser.add_argument("--report-dir", default=str(ROOT / "tests" / "reports" / "adhoc" / "hil-tcp-recovery"))
    parser.add_argument("--rounds", type=int, default=10)
    parser.add_argument("--host", default=os.getenv("SILIGEN_HIL_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.getenv("SILIGEN_HIL_PORT", "9527")))
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument(
        "--gateway-exe",
        default=os.getenv(
            "SILIGEN_HIL_GATEWAY_EXE",
            str(_resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe")),
        ),
    )
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DXF_FILE)
    parser.add_argument(
        "--reconnect-wait-timeout-seconds",
        type=float,
        default=float(os.getenv("SILIGEN_HIL_RECONNECT_WAIT_TIMEOUT_SECONDS", "15")),
    )
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    parser.add_argument("--offline-prereq-report", default="")
    parser.add_argument("--operator-override-reason", default="")
    args = parser.parse_args()
    if args.rounds < 1:
        parser.error("--rounds must be >= 1")
    if args.reconnect_wait_timeout_seconds <= 0:
        parser.error("--reconnect-wait-timeout-seconds must be > 0")
    return args


def _best_effort_disconnect(client: TcpJsonClient) -> None:
    if not client.is_connected():
        return
    try:
        client.send_request("disconnect", {}, timeout_seconds=3.0)
    except Exception:
        return


def _has_known_failure(steps: list[StepResult]) -> bool:
    return any(step.status == "known_failure" for step in steps)


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    gateway_exe = Path(args.gateway_exe)
    steps: list[StepResult] = []
    rounds: list[RecoveryRoundReport] = []
    started: float | None = None
    preflight_status_snapshot: dict[str, Any] = {}
    admission = _evaluate_offline_admission(
        offline_prereq_report=args.offline_prereq_report,
        operator_override_reason=args.operator_override_reason,
    )

    def _finalize(
        exit_code: int,
        *,
        socket_connected: bool | None = None,
        failure_context: dict[str, Any] | None = None,
        failure_classification: dict[str, Any] | None = None,
    ) -> int:
        report = _build_report(
            args=args,
            gateway_exe=gateway_exe,
            steps=steps,
            rounds=rounds,
            elapsed_seconds=0.0 if started is None else (time.perf_counter() - started),
            admission=admission,
            preflight_status_snapshot=preflight_status_snapshot,
            socket_connected=socket_connected,
            failure_context=failure_context,
            failure_classification=failure_classification,
        )
        json_path, md_path = _write_reports(report, report_dir)
        print(f"hil tcp recovery complete: status={report['overall_status']} rounds={report['rounds_executed']}/{report['rounds_requested']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return exit_code

    admission_step_status = "passed" if admission.get("admission_decision") in {"admitted", "override-admitted"} else "failed"
    steps.append(
        StepResult(
            name="hil-admission",
            status=admission_step_status,
            duration_seconds=0.0,
            return_code=0 if admission_step_status == "passed" else 1,
            command=["hil-admission", admission.get("offline_prerequisites_source", "")],
            note=f"decision={admission.get('admission_decision', 'blocked')}",
        )
    )
    if admission_step_status != "passed":
        return _finalize(
            1,
            failure_context={
                "iteration": 0,
                "step_name": "hil-admission",
                "method": "admission",
                "error_message": str(admission.get("failure_classification", {}).get("message", "limited-hil admission blocked")),
                "last_success_step": "",
                "socket_connected": False,
                "recent_status_snapshot": [
                    {
                        "name": "hil-admission",
                        "status": admission_step_status,
                        "note": str(admission.get("failure_classification", {}).get("message", "")),
                    }
                ],
            },
            failure_classification=admission.get("failure_classification"),
        )

    if not args.config_path.exists():
        steps.append(
            StepResult(
                name="resolve-config-path",
                status="failed",
                duration_seconds=0.0,
                return_code=None,
                command=[str(args.config_path)],
                note=f"config path missing: {args.config_path}",
            )
        )
        return _finalize(1)

    if not gateway_exe.exists():
        status = "skipped" if args.allow_skip_on_missing_gateway else "known_failure"
        steps.append(
            StepResult(
                name="resolve-gateway",
                status=status,
                duration_seconds=0.0,
                return_code=None,
                command=[str(gateway_exe)],
                note=f"gateway executable missing: {gateway_exe}",
            )
        )
        return _finalize(SKIPPED_EXIT_CODE if status == "skipped" else KNOWN_FAILURE_EXIT_CODE)

    if not args.dxf_file.exists():
        steps.append(
            StepResult(
                name="resolve-dxf-file",
                status="failed",
                duration_seconds=0.0,
                return_code=None,
                command=[str(args.dxf_file)],
                note=f"dxf file missing: {args.dxf_file}",
            )
        )
        return _finalize(1)

    connect_params = _load_connection_params(args.config_path)
    started = time.perf_counter()
    process = subprocess.Popen(
        [str(gateway_exe), "--config", str(args.config_path), "--port", str(args.port)],
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        env=build_process_env(gateway_exe),
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    client = TcpJsonClient(args.host, args.port)

    try:
        ready_status, ready_note = _wait_gateway_ready(process, args.host, args.port, timeout_seconds=8.0)
        steps.append(
            StepResult(
                name="gateway-ready",
                status=ready_status,
                duration_seconds=0.0,
                return_code=0 if ready_status == "passed" else 1,
                command=[str(gateway_exe)],
                note=ready_note,
            )
        )
        if ready_status != "passed":
            if ready_status == "known_failure":
                return _finalize(KNOWN_FAILURE_EXIT_CODE)
            if ready_status == "skipped":
                return _finalize(SKIPPED_EXIT_CODE)
            return _finalize(1)

        session_started = time.perf_counter()
        session_note = ""
        session_status = "passed"
        try:
            client.connect(timeout_seconds=3.0)
            session_note = "tcp session established"
        except Exception as exc:  # pragma: no cover - HIL runtime failure path
            session_status = "failed"
            session_note = f"tcp session open failed: {exc}"
        steps.append(
            StepResult(
                name="tcp-session-open",
                status=session_status,
                duration_seconds=time.perf_counter() - session_started,
                return_code=0 if session_status == "passed" else 1,
                command=["tcp-session-open", f"{args.host}:{args.port}"],
                note=session_note,
            )
        )
        if session_status != "passed":
            return _finalize(1)

        status_preflight_step, status_preflight_response = _run_tcp_step(
            name="tcp-status-preflight",
            client=client,
            method="status",
            params={},
            timeout_seconds=8.0,
        )
        steps.append(status_preflight_step)
        preflight_status_snapshot = _status_snapshot_from_response(status_preflight_response)
        if status_preflight_step.status == "known_failure":
            return _finalize(KNOWN_FAILURE_EXIT_CODE, socket_connected=client.is_connected())
        if status_preflight_step.status != "passed":
            return _finalize(1, socket_connected=client.is_connected())

        safety_preflight = _evaluate_safety_preflight(preflight_status_snapshot)
        admission["safety_preflight_passed"] = bool(safety_preflight.get("passed", False))
        admission["safety_preflight"] = safety_preflight
        steps.append(
            StepResult(
                name="safety-preflight",
                status="passed" if safety_preflight["passed"] else "failed",
                duration_seconds=0.0,
                return_code=0 if safety_preflight["passed"] else 1,
                command=["safety-preflight", "status"],
                note="safety preflight passed"
                if safety_preflight["passed"]
                else str(safety_preflight.get("failure_classification", {}).get("message", "safety preflight failed")),
                stdout=json.dumps(safety_preflight.get("snapshot", {}), ensure_ascii=True),
            )
        )
        if not safety_preflight["passed"]:
            return _finalize(
                1,
                failure_context={
                    "iteration": 0,
                    "step_name": "safety-preflight",
                    "method": "status",
                    "error_message": str(safety_preflight.get("failure_classification", {}).get("message", "safety preflight failed")),
                    "last_success_step": "tcp-status-preflight",
                    "socket_connected": client.is_connected(),
                    "recent_status_snapshot": [
                        {
                            "name": "safety-preflight",
                            "status": "failed",
                            "note": str(safety_preflight.get("failure_classification", {}).get("message", "")),
                        }
                    ],
                },
                failure_classification=safety_preflight.get("failure_classification"),
                socket_connected=client.is_connected(),
            )

        for round_index in range(1, args.rounds + 1):
            round_report = _run_round(
                args=args,
                round_index=round_index,
                client=client,
                connect_params=connect_params,
                steps=steps,
            )
            rounds.append(round_report)
            print(f"hil tcp recovery round {round_index}/{args.rounds}: status={round_report.status}")
            if round_report.status != "passed":
                exit_code = KNOWN_FAILURE_EXIT_CODE if _has_known_failure(steps) else 1
                return _finalize(exit_code, socket_connected=client.is_connected())

        final_report = _build_report(
            args=args,
            gateway_exe=gateway_exe,
            steps=steps,
            rounds=rounds,
            elapsed_seconds=time.perf_counter() - started,
            admission=admission,
            preflight_status_snapshot=preflight_status_snapshot,
            socket_connected=client.is_connected(),
        )
        json_path, md_path = _write_reports(final_report, report_dir)
        print(f"hil tcp recovery complete: status={final_report['overall_status']} rounds={final_report['rounds_executed']}/{final_report['rounds_requested']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        if final_report["overall_status"] == "passed":
            return 0
        if final_report["overall_status"] == "known_failure":
            return KNOWN_FAILURE_EXIT_CODE
        if final_report["overall_status"] == "skipped":
            return SKIPPED_EXIT_CODE
        return 1
    finally:
        _best_effort_disconnect(client)
        client.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
