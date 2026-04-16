from __future__ import annotations

import argparse
import base64
import json
import os
import socket
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11
ROOT = Path(__file__).resolve().parents[3]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from runtime_gateway_harness import build_process_env  # noqa: E402
from runtime_gateway_harness import load_connection_params as _shared_load_connection_params  # noqa: E402
from runtime_gateway_harness import resolve_default_exe  # noqa: E402
from runtime_gateway_harness import tcp_connect_and_ensure_ready  # noqa: E402
from test_kit.evidence_bundle import (
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    default_failure_classification,
    trace_fields,
    write_bundle_artifacts,
)  # noqa: E402
from test_kit.dxf_truth_matrix import default_hil_case, full_chain_cases, resolve_full_chain_case  # noqa: E402

DEFAULT_DXF_CASE = default_hil_case(ROOT)
DEFAULT_DXF_FILE = DEFAULT_DXF_CASE.source_sample_absolute(ROOT)
DEFAULT_DXF_CASE_ID = DEFAULT_DXF_CASE.case_id
DEFAULT_CONFIG_PATH = ROOT / "config" / "machine" / "machine_config.ini"
HIL_ADMISSION_SCHEMA_VERSION = "hil-admission.v1"
REQUIRED_OFFLINE_CASES = (
    "protocol-compatibility",
    "engineering-regression",
    "simulated-line",
)

KNOWN_FAILURE_PATTERNS = (
    "IDiagnosticsPort 未注册",
    "IDiagnosticsPort not registered",
    "diagnostics port not registered",
    "Service not registered",
    "SERVICE_NOT_REGISTERED",
)


@dataclass
class StepResult:
    name: str
    status: str
    duration_seconds: float
    return_code: int | None
    command: list[str]
    note: str = ""
    stdout: str = ""
    stderr: str = ""


class TcpJsonClient:
    def __init__(self, host: str, port: int) -> None:
        self._host = host
        self._port = port
        self._socket: socket.socket | None = None
        self._recv_buffer = ""
        self._request_id = 0

    def connect(self, timeout_seconds: float) -> None:
        self._socket = socket.create_connection((self._host, self._port), timeout=timeout_seconds)
        self._socket.settimeout(timeout_seconds)
        self._recv_buffer = ""
        self._request_id = 0

    def close(self) -> None:
        if self._socket is None:
            return
        try:
            self._socket.close()
        finally:
            self._socket = None
            self._recv_buffer = ""

    def is_connected(self) -> bool:
        return self._socket is not None

    def send_request(self, method: str, params: dict[str, Any] | None, timeout_seconds: float) -> dict[str, Any]:
        if self._socket is None:
            raise RuntimeError("tcp session not connected")

        self._request_id += 1
        request_id = str(self._request_id)
        payload: dict[str, Any] = {"id": request_id, "method": method}
        if params:
            payload["params"] = params

        wire = json.dumps(payload, ensure_ascii=True) + "\n"
        self._socket.settimeout(timeout_seconds)
        self._socket.sendall(wire.encode("utf-8"))

        deadline = time.perf_counter() + max(0.1, timeout_seconds)
        while time.perf_counter() < deadline:
            line = self._recv_line(deadline)
            if line is None:
                break
            if not line.strip():
                continue
            message = json.loads(line)
            if str(message.get("id", "")) == request_id:
                return message

        raise TimeoutError(f"tcp response timeout method={method}")

    def _recv_line(self, deadline: float) -> str | None:
        if self._socket is None:
            return None

        while True:
            if "\n" in self._recv_buffer:
                line, self._recv_buffer = self._recv_buffer.split("\n", 1)
                return line

            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                return None

            self._socket.settimeout(min(1.0, max(0.05, remaining)))
            try:
                chunk = self._socket.recv(4096)
            except TimeoutError:
                continue
            if not chunk:
                raise ConnectionError("tcp socket closed by peer")
            self._recv_buffer += chunk.decode("utf-8", errors="replace")


def _truncate(text: str, limit: int = 2000) -> str:
    text = text.strip()
    if len(text) <= limit:
        return text
    return text[:limit] + "\n...[truncated]..."


def _load_connection_params(config_path: Path) -> dict[str, str]:
    return _shared_load_connection_params(config_path)


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _resolve_dxf_selection(
    case_id: str | None,
    dxf_file: Path | None,
) -> tuple[str | None, Path, tuple[str, ...]]:
    if case_id:
        truth_case = resolve_full_chain_case(ROOT, case_id)
        return (
            truth_case.case_id,
            truth_case.source_sample_absolute(ROOT),
            (truth_case.sample_asset_id,),
        )

    if dxf_file is None:
        return DEFAULT_DXF_CASE_ID, DEFAULT_DXF_FILE, (DEFAULT_DXF_CASE.sample_asset_id,)

    resolved_file = dxf_file.resolve()
    for truth_case in full_chain_cases(ROOT):
        if truth_case.source_sample_absolute(ROOT) == resolved_file:
            return truth_case.case_id, resolved_file, (truth_case.sample_asset_id,)
    return None, resolved_file, ()


def _build_admission_check(
    *,
    name: str,
    status: str,
    expected: str,
    actual: str,
    note: str = "",
) -> dict[str, str]:
    return {
        "name": name,
        "status": status,
        "expected": expected,
        "actual": actual,
        "note": note,
    }


def _evaluate_offline_admission(
    *,
    offline_prereq_report: str,
    operator_override_reason: str,
) -> dict[str, Any]:
    trimmed_reason = operator_override_reason.strip()
    trimmed_report = offline_prereq_report.strip()
    checks: list[dict[str, str]] = []
    payload: dict[str, Any] = {
        "schema_version": HIL_ADMISSION_SCHEMA_VERSION,
        "required_layers": ["L0", "L1", "L2", "L3", "L4"],
        "required_cases": list(REQUIRED_OFFLINE_CASES),
        "offline_prerequisites_source": trimmed_report,
        "offline_prerequisites_passed": False,
        "operator_override_used": False,
        "operator_override_reason": trimmed_reason,
        "admission_decision": "blocked",
        "checks": checks,
    }

    if not trimmed_report:
        checks.append(
            _build_admission_check(
                name="offline-prereq-report",
                status="failed",
                expected="workspace-validation report path is provided",
                actual="missing",
                note="limited-hil requires an offline prerequisite report or explicit operator override",
            )
        )
        if trimmed_reason:
            payload["operator_override_used"] = True
            payload["admission_decision"] = "override-admitted"
            payload["failure_classification"] = {
                "category": "validation",
                "code": "override-admitted",
                "blocking": False,
                "message": trimmed_reason,
            }
            return payload
        payload["failure_classification"] = {
            "category": "prerequisite",
            "code": "operator_override_required",
            "blocking": True,
            "message": "limited-hil requires operator override when offline prerequisite evidence is missing",
        }
        return payload

    report_path = Path(trimmed_report).resolve()
    payload["offline_prerequisites_source"] = str(report_path)
    if not report_path.exists():
        checks.append(
            _build_admission_check(
                name="offline-prereq-report",
                status="failed",
                expected="workspace-validation report exists on disk",
                actual=f"missing={report_path}",
            )
        )
        if trimmed_reason:
            payload["operator_override_used"] = True
            payload["admission_decision"] = "override-admitted"
            payload["failure_classification"] = {
                "category": "validation",
                "code": "override-admitted",
                "blocking": False,
                "message": trimmed_reason,
            }
            return payload
        payload["failure_classification"] = {
            "category": "prerequisite",
            "code": "offline_gate_missing",
            "blocking": True,
            "message": f"offline prerequisite report missing: {report_path}",
        }
        return payload

    prereq_payload = _load_json(report_path)
    counts = prereq_payload.get("counts", {})
    failed = int(counts.get("failed", 0))
    known_failure = int(counts.get("known_failure", 0))
    skipped = int(counts.get("skipped", 0))
    counts_ok = failed == 0 and known_failure == 0 and skipped == 0
    checks.append(
        _build_admission_check(
            name="offline-prereq-counts",
            status="passed" if counts_ok else "failed",
            expected="failed=0, known_failure=0, skipped=0",
            actual=f"failed={failed}, known_failure={known_failure}, skipped={skipped}",
        )
    )

    status_by_name: dict[str, str] = {}
    for item in prereq_payload.get("results", []):
        name = str(item.get("name", ""))
        status = str(item.get("status", ""))
        if name:
            status_by_name[name] = status
    missing_cases = [name for name in REQUIRED_OFFLINE_CASES if name not in status_by_name]
    non_passed_cases = [name for name in REQUIRED_OFFLINE_CASES if status_by_name.get(name) != "passed"]
    cases_ok = not missing_cases and not non_passed_cases
    actual_parts: list[str] = []
    if missing_cases:
        actual_parts.append("missing=" + ",".join(missing_cases))
    if non_passed_cases:
        actual_parts.append("non_passed=" + ",".join(f"{name}:{status_by_name.get(name, 'missing')}" for name in non_passed_cases))
    checks.append(
        _build_admission_check(
            name="offline-prereq-required-cases",
            status="passed" if cases_ok else "failed",
            expected="protocol-compatibility, engineering-regression, simulated-line all passed",
            actual="all passed" if cases_ok else "; ".join(actual_parts),
        )
    )

    payload["offline_prerequisites_passed"] = counts_ok and cases_ok
    if payload["offline_prerequisites_passed"]:
        payload["admission_decision"] = "admitted"
        payload["failure_classification"] = default_failure_classification(status="passed")
        return payload

    if trimmed_reason:
        payload["operator_override_used"] = True
        payload["admission_decision"] = "override-admitted"
        payload["failure_classification"] = {
            "category": "validation",
            "code": "override-admitted",
            "blocking": False,
            "message": trimmed_reason,
        }
        return payload

    payload["failure_classification"] = {
        "category": "prerequisite",
        "code": "operator_override_required",
        "blocking": True,
        "message": "limited-hil requires an explicit operator override when offline prerequisites are not fully passed",
    }
    return payload


def _status_snapshot_from_response(response: dict[str, Any] | None) -> dict[str, Any]:
    result = response.get("result", {}) if isinstance(response, dict) else {}
    if not isinstance(result, dict):
        result = {}
    io_payload = result.get("io", {})
    if not isinstance(io_payload, dict):
        io_payload = {}
    return {
        "machine_state": str(result.get("machine_state", "")),
        "machine_state_reason": str(result.get("machine_state_reason", "")),
        "connected": bool(result.get("connected", False)),
        "io": {
            "estop": bool(io_payload.get("estop", False)),
            "limit_x_pos": bool(io_payload.get("limit_x_pos", False)),
            "limit_x_neg": bool(io_payload.get("limit_x_neg", False)),
            "limit_y_pos": bool(io_payload.get("limit_y_pos", False)),
            "limit_y_neg": bool(io_payload.get("limit_y_neg", False)),
        },
    }


def _evaluate_safety_preflight(snapshot: dict[str, Any]) -> dict[str, Any]:
    io_payload = snapshot.get("io", {}) if isinstance(snapshot, dict) else {}
    limit_blockers = [
        name
        for name, active in (
            ("limit_x_pos", bool(io_payload.get("limit_x_pos", False))),
            ("limit_x_neg", bool(io_payload.get("limit_x_neg", False))),
            ("limit_y_pos", bool(io_payload.get("limit_y_pos", False))),
            ("limit_y_neg", bool(io_payload.get("limit_y_neg", False))),
        )
        if active
    ]
    estop_active = bool(io_payload.get("estop", False))
    checks = [
        _build_admission_check(
            name="safety-preflight-estop",
            status="passed" if not estop_active else "failed",
            expected="estop=false",
            actual=f"estop={estop_active}",
        ),
        _build_admission_check(
            name="safety-preflight-limits",
            status="passed" if not limit_blockers else "failed",
            expected="no active limit blockers",
            actual="none" if not limit_blockers else ",".join(limit_blockers),
        ),
    ]
    passed = (not estop_active) and (not limit_blockers)
    if estop_active:
        failure = {
            "category": "safety",
            "code": "estop_active",
            "blocking": True,
            "message": "safety preflight detected estop=true before limited-hil execution",
        }
    elif limit_blockers:
        failure = {
            "category": "safety",
            "code": "limit_active",
            "blocking": True,
            "message": "safety preflight detected active limit blocker before limited-hil execution",
        }
    else:
        failure = default_failure_classification(status="passed")
    return {
        "passed": passed,
        "snapshot": snapshot,
        "limit_blockers": limit_blockers,
        "estop_active": estop_active,
        "checks": checks,
        "failure_classification": failure,
    }
def _matches_known_failure(text: str) -> bool:
    lowered = text.lower()
    for pattern in KNOWN_FAILURE_PATTERNS:
        if pattern.lower() in lowered:
            return True
    return False


def _read_process_output(process: subprocess.Popen[str]) -> tuple[str, str]:
    try:
        stdout, stderr = process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate(timeout=5)
    return stdout or "", stderr or ""


def _wait_gateway_ready(process: subprocess.Popen[str], host: str, port: int, timeout_seconds: float) -> tuple[str, str]:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        if process.poll() is not None:
            stdout, stderr = _read_process_output(process)
            combined = "\n".join(item for item in (stdout, stderr) if item)
            if _matches_known_failure(combined):
                return "known_failure", combined or "gateway exited with known failure pattern"
            return "failed", combined or "gateway exited before TCP endpoint became ready"
        try:
            with socket.create_connection((host, port), timeout=1.0):
                return "passed", "TCP endpoint is reachable"
        except OSError:
            time.sleep(0.2)
    return "failed", f"gateway readiness timeout: {host}:{port}"


def _run_tcp_step(
    *,
    name: str,
    client: TcpJsonClient,
    method: str,
    params: dict[str, Any] | None,
    timeout_seconds: float,
) -> tuple[StepResult, dict[str, Any] | None]:
    started = time.perf_counter()
    command = ["tcp", method, json.dumps(params or {}, ensure_ascii=True)]

    try:
        response = client.send_request(method, params or {}, timeout_seconds=timeout_seconds)
    except Exception as exc:  # pragma: no cover - HIL runtime failure path
        elapsed = time.perf_counter() - started
        stderr = _truncate(str(exc))
        status = "known_failure" if _matches_known_failure(stderr) else "failed"
        note = "tcp request exception"
        return (
            StepResult(
                name=name,
                status=status,
                duration_seconds=elapsed,
                return_code=1,
                command=command,
                note=note,
                stderr=stderr,
            ),
            None,
        )

    elapsed = time.perf_counter() - started
    stdout = _truncate(json.dumps(response, ensure_ascii=True))
    if "error" in response:
        message = str(response.get("error", {}).get("message", "unknown tcp error"))
        combined = f"{message}\n{stdout}"
        status = "known_failure" if _matches_known_failure(combined) else "failed"
        return (
            StepResult(
                name=name,
                status=status,
                duration_seconds=elapsed,
                return_code=1,
                command=command,
                note=f"tcp error: {message}",
                stdout=stdout,
            ),
            response,
        )

    return (
        StepResult(
            name=name,
            status="passed",
            duration_seconds=elapsed,
            return_code=0,
            command=command,
            stdout=stdout,
        ),
        response,
    )


def _run_connect_step(
    *,
    name: str,
    client: TcpJsonClient,
    config_path: Path,
    timeout_seconds: float,
    ready_timeout_seconds: float = 3.0,
) -> tuple[StepResult, dict[str, Any] | None]:
    started = time.perf_counter()
    admission = tcp_connect_and_ensure_ready(
        client,
        config_path=config_path,
        connect_timeout_seconds=timeout_seconds,
        status_timeout_seconds=min(timeout_seconds, 8.0),
        ready_timeout_seconds=ready_timeout_seconds,
    )
    elapsed = time.perf_counter() - started
    payload = {
        "connect_params": admission.connect_params,
        "connect_response": admission.connect_response,
        "status_response": admission.status_response,
    }
    stdout = _truncate(json.dumps(payload, ensure_ascii=True))
    if admission.status != "passed":
        return (
            StepResult(
                name=name,
                status=admission.status,
                duration_seconds=elapsed,
                return_code=1,
                command=["tcp", "connect", json.dumps(admission.connect_params, ensure_ascii=True)],
                note=admission.note,
                stdout=stdout,
            ),
            admission.status_response,
        )

    return (
        StepResult(
            name=name,
            status="passed",
            duration_seconds=elapsed,
            return_code=0,
            command=["tcp", "connect", json.dumps(admission.connect_params, ensure_ascii=True)],
            note=admission.note,
            stdout=stdout,
        ),
        admission.status_response,
    )


def _normalize_dispenser_state(value: str | None) -> str | None:
    if not value:
        return None
    lowered = value.strip().lower()
    aliases = {
        "idle": "idle",
        "running": "running",
        "paused": "paused",
        "stopped": "stopped",
        "error": "error",
        "unknown": "unknown",
    }
    return aliases.get(lowered, lowered)


def _extract_dispenser_state_from_status_response(
    response: dict[str, Any] | None,
    expected_state: str,
) -> str | None:
    if not isinstance(response, dict):
        return None
    result = response.get("result", {})
    if not isinstance(result, dict):
        return None
    dispenser = result.get("dispenser", {})
    if not isinstance(dispenser, dict):
        return None

    explicit = dispenser.get("status")
    if isinstance(explicit, str):
        normalized = _normalize_dispenser_state(explicit)
        if normalized:
            return normalized

    valve_open = dispenser.get("valve_open")
    if isinstance(valve_open, bool):
        if valve_open:
            return "running"
        expected = _normalize_dispenser_state(expected_state) or expected_state
        if expected == "paused":
            return "paused"
        return "idle"
    return None


def _wait_for_dispenser_state(
    *,
    name: str,
    client: TcpJsonClient,
    expected_state: str,
    timeout_seconds: float,
) -> tuple[StepResult, dict]:
    started = time.perf_counter()
    expected_normalized = _normalize_dispenser_state(expected_state) or expected_state
    status_command = ["tcp", "status", "{}"]
    attempts = 0
    last_state: str | None = None
    last_probe = StepResult(
        name=f"{name}-probe",
        status="failed",
        duration_seconds=0.0,
        return_code=None,
        command=status_command,
        note="no probe executed",
    )

    while (time.perf_counter() - started) <= max(0.2, timeout_seconds):
        attempts += 1
        probe, response = _run_tcp_step(
            name=f"{name}-probe-{attempts}",
            client=client,
            method="status",
            params={},
            timeout_seconds=3.0,
        )
        last_probe = probe
        observed = _extract_dispenser_state_from_status_response(response, expected_normalized)
        if observed:
            last_state = observed

        if probe.status == "known_failure":
            elapsed = time.perf_counter() - started
            note = f"status probe matched known failure pattern after {attempts} probes"
            result = StepResult(
                name=name,
                status="known_failure",
                duration_seconds=elapsed,
                return_code=probe.return_code,
                command=status_command,
                note=note,
                stdout=probe.stdout,
                stderr=probe.stderr,
            )
            transition = {
                "name": name,
                "status": "known_failure",
                "expected_state": expected_normalized,
                "observed_state": last_state,
                "attempts": attempts,
                "duration_seconds": round(elapsed, 3),
                "note": note,
            }
            return result, transition

        if probe.status == "failed":
            elapsed = time.perf_counter() - started
            note = f"status probe command failed after {attempts} probes"
            result = StepResult(
                name=name,
                status="failed",
                duration_seconds=elapsed,
                return_code=probe.return_code,
                command=status_command,
                note=note,
                stdout=probe.stdout,
                stderr=probe.stderr,
            )
            transition = {
                "name": name,
                "status": "failed",
                "expected_state": expected_normalized,
                "observed_state": last_state,
                "attempts": attempts,
                "duration_seconds": round(elapsed, 3),
                "note": note,
            }
            return result, transition

        if observed == expected_normalized:
            elapsed = time.perf_counter() - started
            note = f"state reached `{expected_normalized}` after {attempts} probes"
            result = StepResult(
                name=name,
                status="passed",
                duration_seconds=elapsed,
                return_code=probe.return_code,
                command=status_command,
                note=note,
                stdout=probe.stdout,
                stderr=probe.stderr,
            )
            transition = {
                "name": name,
                "status": "passed",
                "expected_state": expected_normalized,
                "observed_state": observed,
                "attempts": attempts,
                "duration_seconds": round(elapsed, 3),
                "note": note,
            }
            return result, transition

        time.sleep(0.2)

    elapsed = time.perf_counter() - started
    note = (
        f"wait state timeout: expected `{expected_normalized}`, "
        f"last_observed=`{last_state or 'unknown'}`, probes={attempts}"
    )
    result = StepResult(
        name=name,
        status="failed",
        duration_seconds=elapsed,
        return_code=last_probe.return_code,
        command=status_command,
        note=note,
        stdout=last_probe.stdout,
        stderr=last_probe.stderr,
    )
    transition = {
        "name": name,
        "status": "failed",
        "expected_state": expected_normalized,
        "observed_state": last_state,
        "attempts": attempts,
        "duration_seconds": round(elapsed, 3),
        "note": note,
    }
    return result, transition


def _build_sequence(
    dxf_file: Path,
    pause_resume_cycles: int,
    dispenser_count: int,
    dispenser_interval_ms: int,
    dispenser_duration_ms: int,
    connect_params: dict[str, Any],
) -> list[tuple[str, str, dict[str, Any]]]:
    _ = dxf_file
    sequence: list[tuple[str, str, dict[str, Any]]] = [
        ("tcp-connect", "connect", connect_params),
        ("tcp-status", "status", {}),
        ("tcp-supply-open", "supply.open", {}),
        (
            "tcp-dispenser-start",
            "dispenser.start",
            {
                "count": dispenser_count,
                "interval_ms": dispenser_interval_ms,
                "duration_ms": dispenser_duration_ms,
            },
        ),
    ]
    for idx in range(max(1, pause_resume_cycles)):
        sequence.append((f"tcp-dispenser-pause-{idx + 1}", "dispenser.pause", {}))
        sequence.append((f"tcp-dispenser-resume-{idx + 1}", "dispenser.resume", {}))
    sequence.extend(
        [
            ("tcp-dispenser-stop", "dispenser.stop", {}),
            ("tcp-supply-close", "supply.close", {}),
            ("tcp-stop", "stop", {}),
            ("tcp-disconnect", "disconnect", {}),
        ]
    )
    return sequence


def _extract_method_from_step(step: StepResult | None) -> str:
    if step is None:
        return ""
    if step.command and step.command[0] == "tcp" and len(step.command) >= 2:
        return str(step.command[1])
    if step.name.startswith("wait-dispenser-"):
        return "status(wait)"
    if step.name.startswith("tcp-session-open"):
        return "connect(session)"
    if step.name.startswith("gateway-ready"):
        return "gateway-ready"
    return ""


def _build_failure_context(
    *,
    steps: list[StepResult],
    iterations: int,
    socket_connected: bool | None = None,
) -> dict[str, Any] | None:
    latest_non_passed: StepResult | None = None
    for item in reversed(steps):
        if item.status != "passed":
            latest_non_passed = item
            break
    if latest_non_passed is None:
        return None

    if socket_connected is None:
        has_session_open = any(item.name == "tcp-session-open" and item.status == "passed" for item in steps)
        has_disconnect = any(item.name == "tcp-disconnect" and item.status == "passed" for item in steps)
        socket_connected = has_session_open and not has_disconnect

    last_success_step = ""
    for item in reversed(steps):
        if item.status == "passed":
            last_success_step = item.name
            break

    error_message = latest_non_passed.note or latest_non_passed.stderr or latest_non_passed.stdout or ""
    if len(error_message) > 400:
        error_message = error_message[:400] + "...[truncated]"

    snapshot = [
        {
            "name": item.name,
            "status": item.status,
            "note": item.note,
        }
        for item in steps[-10:]
    ]

    return {
        "iteration": iterations,
        "step_name": latest_non_passed.name,
        "method": _extract_method_from_step(latest_non_passed),
        "error_message": error_message,
        "last_success_step": last_success_step,
        "socket_connected": socket_connected,
        "recent_status_snapshot": snapshot,
    }


def _bundle_verdict(overall_status: str) -> str:
    if overall_status == "known_failure":
        return "known-failure"
    return overall_status


def _write_evidence_bundle(report: dict[str, Any], report_dir: Path, summary_json_path: Path, summary_md_path: Path) -> None:
    overall_status = _bundle_verdict(str(report.get("overall_status", "incomplete")))
    admission = report.get("admission", {}) if isinstance(report.get("admission"), dict) else {}
    failure_classification = report.get("failure_classification", {}) if isinstance(report.get("failure_classification"), dict) else {}
    dxf_asset_refs = tuple(str(item) for item in report.get("dxf_asset_refs", ()))
    bundle = EvidenceBundle(
        bundle_id=f"hil-closed-loop-{report['generated_at']}",
        request_ref="hil-closed-loop",
        producer_lane_ref="limited-hil",
        report_root=str(report_dir.resolve()),
        summary_file=str(summary_md_path.resolve()),
        machine_file=str(summary_json_path.resolve()),
        verdict=overall_status,
        linked_asset_refs=dxf_asset_refs,
        offline_prerequisites=tuple(admission.get("required_layers", ("L0", "L1", "L2", "L3", "L4"))),
        abort_metadata={
            "failure_context": report.get("failure_context", {}),
            "admission": admission,
        },
        metadata={
            "dxf_case_id": report.get("dxf_case_id", ""),
            "dxf_asset_refs": list(dxf_asset_refs),
            "duration_seconds": report.get("duration_seconds", 0),
            "iterations": report.get("iterations", 0),
            "timeout_count": report.get("timeout_count", 0),
            "admission": admission,
        },
        case_records=[
            EvidenceCaseRecord(
                case_id="hil-closed-loop",
                name="hil-closed-loop",
                suite_ref="e2e",
                owner_scope="runtime-execution",
                primary_layer="L5",
                producer_lane_ref="limited-hil",
                status=overall_status,
                evidence_profile="hil-report",
                stability_state="stable",
                required_assets=dxf_asset_refs,
                required_fixtures=("fixture.hil-closed-loop", "fixture.validation-evidence-bundle"),
                risk_tags=("hardware", "state-machine"),
                note=str(report.get("failure_context", {}).get("error_message", "")),
                failure_classification=failure_classification,
                trace_fields=trace_fields(
                    stage_id="L5",
                    artifact_id="hil-closed-loop",
                    module_id="runtime-execution",
                    workflow_state="executed",
                    execution_state=overall_status,
                    event_name="hil-closed-loop",
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
            EvidenceLink(label="hil-closed-loop-summary.json", path=str(summary_json_path.resolve()), role="machine-summary"),
            EvidenceLink(label="hil-closed-loop-summary.md", path=str(summary_md_path.resolve()), role="human-summary"),
        ],
    )


def _write_reports(report: dict, report_dir: Path) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "hil-closed-loop-summary.json"
    md_path = report_dir / "hil-closed-loop-summary.md"

    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# HIL Closed Loop Summary",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- duration_seconds: `{report['duration_seconds']}`",
        f"- iterations: `{report['iterations']}`",
        f"- reconnect_count: `{report['reconnect_count']}`",
        f"- timeout_count: `{report['timeout_count']}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
        f"- cli_exe: `{report['cli_exe']}`",
        f"- transport: `{report['transport']}`",
        f"- dxf_case_id: `{report.get('dxf_case_id', '')}`",
        f"- dxf_file: `{report['dxf_file']}`",
        f"- dispenser_start_params: `count={report['dispenser_start_params']['count']} interval_ms={report['dispenser_start_params']['interval_ms']} duration_ms={report['dispenser_start_params']['duration_ms']}`",
        f"- state_wait_timeout_seconds: `{report['state_wait_timeout_seconds']}`",
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
                if check.get("note"):
                    lines.append(f"  note: {check['note']}")
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

    transition_checks = report.get("state_transition_checks", [])
    if transition_checks:
        lines.extend(["## State Transition Checks", ""])
        for check in transition_checks:
            lines.append(
                "- "
                f"`{check['status']}` `{check['name']}` "
                f"expected=`{check['expected_state']}` observed=`{check.get('observed_state', 'unknown')}` "
                f"attempts=`{check['attempts']}` duration_seconds=`{check['duration_seconds']}`"
            )
            if check.get("note"):
                lines.append(f"  note: {check['note']}")
        lines.append("")

    lines.extend(["## Step Details", ""])

    for step in report["steps"]:
        lines.append(f"- `{step['status']}` `{step['name']}`")
        lines.append(f"  duration_seconds: `{step['duration_seconds']:.3f}`")
        lines.append(f"  return_code: `{step['return_code']}`")
        lines.append(f"  command: `{subprocess.list2cmdline(step['command'])}`")
        if step["note"]:
            lines.append(f"  note: {step['note']}")
        if step["stdout"]:
            lines.append("  stdout:")
            lines.append("  ```text")
            lines.extend(f"  {line}" for line in step["stdout"].splitlines())
            lines.append("  ```")
        if step["stderr"]:
            lines.append("  stderr:")
            lines.append("  ```text")
            lines.extend(f"  {line}" for line in step["stderr"].splitlines())
            lines.append("  ```")
    lines.append("")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    _write_evidence_bundle(report, report_dir, json_path, md_path)
    return json_path, md_path


def _build_report(
    *,
    args: argparse.Namespace,
    gateway_exe: Path,
    cli_exe: Path,
    steps: list[StepResult],
    iterations: int,
    reconnect_count: int,
    timeout_count: int,
    elapsed_seconds: float,
    state_transition_checks: list[dict],
    socket_connected: bool | None = None,
    failure_context: dict[str, Any] | None = None,
    admission: dict[str, Any] | None = None,
    failure_classification: dict[str, Any] | None = None,
) -> dict:
    statuses = [step.status for step in steps]
    overall_status = "passed"
    if "failed" in statuses:
        overall_status = "failed"
    elif "known_failure" in statuses:
        overall_status = "known_failure"
    elif "skipped" in statuses:
        overall_status = "skipped"

    resolved_failure_context = failure_context
    if overall_status != "passed" and resolved_failure_context is None:
        resolved_failure_context = _build_failure_context(
            steps=steps,
            iterations=iterations,
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
        "cli_exe": str(cli_exe),
        "transport": "tcp-json",
        "host": args.host,
        "port": args.port,
        "dxf_case_id": getattr(args, "dxf_case_id", "") or "",
        "dxf_asset_refs": list(getattr(args, "dxf_asset_refs", ()) or ()),
        "dxf_file": str(args.dxf_file),
        "duration_seconds": args.duration_seconds,
        "pause_resume_cycles": args.pause_resume_cycles,
        "dispenser_start_params": {
            "count": args.dispenser_count,
            "interval_ms": args.dispenser_interval_ms,
            "duration_ms": args.dispenser_duration_ms,
        },
        "max_iterations": args.max_iterations,
        "state_wait_timeout_seconds": args.state_wait_timeout_seconds,
        "iterations": iterations,
        "reconnect_count": reconnect_count,
        "timeout_count": timeout_count,
        "elapsed_seconds": round(elapsed_seconds, 3),
        "overall_status": overall_status,
        "admission": admission or {},
        "failure_classification": resolved_failure_classification,
        "state_transition_checks": state_transition_checks,
        "steps": [asdict(step) for step in steps],
    }
    if resolved_failure_context is not None:
        payload["failure_context"] = resolved_failure_context
    return payload


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run HIL closed-loop workflow and write structured reports.")
    parser.add_argument("--report-dir", default=str(ROOT / "tests" / "reports" / "hil-controlled-test"))
    parser.add_argument("--duration-seconds", type=int, default=300)
    parser.add_argument("--host", default=os.getenv("SILIGEN_HIL_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.getenv("SILIGEN_HIL_PORT", "9527")))
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument(
        "--gateway-exe",
        default=os.getenv(
            "SILIGEN_HIL_GATEWAY_EXE",
            str(resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe")),
        ),
    )
    parser.add_argument(
        "--cli-exe",
        default=os.getenv(
            "SILIGEN_HIL_CLI_EXE",
            str(resolve_default_exe("siligen_planner_cli.exe", "siligen_cli.exe")),
        ),
    )
    dxf_group = parser.add_mutually_exclusive_group()
    dxf_group.add_argument("--dxf-file", type=Path)
    dxf_group.add_argument("--dxf-case-id", default="")
    parser.add_argument("--pause-resume-cycles", type=int, default=3)
    parser.add_argument("--max-iterations", type=int, default=0)
    parser.add_argument("--dispenser-count", type=int, default=int(os.getenv("SILIGEN_HIL_DISPENSER_COUNT", "300")))
    parser.add_argument("--dispenser-interval-ms", type=int, default=int(os.getenv("SILIGEN_HIL_DISPENSER_INTERVAL_MS", "200")))
    parser.add_argument("--dispenser-duration-ms", type=int, default=int(os.getenv("SILIGEN_HIL_DISPENSER_DURATION_MS", "80")))
    parser.add_argument(
        "--state-wait-timeout-seconds",
        type=float,
        default=float(os.getenv("SILIGEN_HIL_STATE_WAIT_TIMEOUT_SECONDS", "8")),
    )
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    parser.add_argument("--allow-skip-on-missing-cli", action="store_true")
    parser.add_argument("--offline-prereq-report", default="")
    parser.add_argument("--operator-override-reason", default="")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    resolved_case_id, resolved_dxf_file, resolved_asset_refs = _resolve_dxf_selection(
        getattr(args, "dxf_case_id", "") or None,
        getattr(args, "dxf_file", None),
    )
    args.dxf_case_id = resolved_case_id or ""
    args.dxf_file = resolved_dxf_file
    args.dxf_asset_refs = resolved_asset_refs
    report_dir = Path(args.report_dir)
    gateway_exe = Path(args.gateway_exe)
    cli_exe = Path(args.cli_exe)
    steps: list[StepResult] = []
    iterations = 0
    reconnect_count = 0
    timeout_count = 0
    state_transition_checks: list[dict] = []
    admission = _evaluate_offline_admission(
        offline_prereq_report=args.offline_prereq_report,
        operator_override_reason=args.operator_override_reason,
    )
    started: float | None = None

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
            cli_exe=cli_exe,
            steps=steps,
            iterations=iterations,
            reconnect_count=reconnect_count,
            timeout_count=timeout_count,
            elapsed_seconds=0.0 if started is None else (time.perf_counter() - started),
            state_transition_checks=state_transition_checks,
            socket_connected=socket_connected,
            failure_context=failure_context,
            admission=admission,
            failure_classification=failure_classification,
        )
        json_path, md_path = _write_reports(report, report_dir)
        print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
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
                "iteration": iterations,
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

    if cli_exe.exists():
        steps.append(
            StepResult(
                name="resolve-cli",
                status="passed",
                duration_seconds=0.0,
                return_code=0,
                command=[str(cli_exe)],
                note="cli executable found (tcp-driven flow does not depend on CLI process lifecycle)",
            )
        )
    else:
        note = (
            f"cli executable missing: {cli_exe}; "
            "tcp-driven flow does not require CLI binary"
        )
        if args.allow_skip_on_missing_cli:
            note = f"{note} (allow-skip-on-missing-cli enabled)"
        steps.append(
            StepResult(
                name="resolve-cli",
                status="passed",
                duration_seconds=0.0,
                return_code=0,
                command=[str(cli_exe)],
                note=note,
            )
        )

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

    started = time.perf_counter()
    connect_params = _load_connection_params(args.config_path)
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
            if "timeout" in ready_note.lower():
                timeout_count += 1
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
        if status_preflight_step.status == "known_failure":
            return _finalize(KNOWN_FAILURE_EXIT_CODE)
        if status_preflight_step.status != "passed":
            return _finalize(1)

        safety_preflight = _evaluate_safety_preflight(_status_snapshot_from_response(status_preflight_response))
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
                stdout=_truncate(json.dumps(safety_preflight.get("snapshot", {}), ensure_ascii=True)),
            )
        )
        if not safety_preflight["passed"]:
            return _finalize(
                1,
                failure_context={
                    "iteration": iterations,
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

        sequence = _build_sequence(
            dxf_file=args.dxf_file,
            pause_resume_cycles=args.pause_resume_cycles,
            dispenser_count=args.dispenser_count,
            dispenser_interval_ms=args.dispenser_interval_ms,
            dispenser_duration_ms=args.dispenser_duration_ms,
            connect_params=connect_params,
        )

        dxf_step, _ = _run_tcp_step(
            name="tcp-dxf-artifact-create-preflight",
            client=client,
            method="dxf.artifact.create",
            params={
                "filename": args.dxf_file.name,
                "original_filename": args.dxf_file.name,
                "content_type": "application/dxf",
                "file_content_b64": base64.b64encode(args.dxf_file.read_bytes()).decode("ascii"),
            },
            timeout_seconds=60.0,
        )
        steps.append(dxf_step)
        if dxf_step.status == "failed":
            return _finalize(1, socket_connected=client.is_connected())
        if dxf_step.status == "known_failure":
            return _finalize(KNOWN_FAILURE_EXIT_CODE, socket_connected=client.is_connected())

        deadline = time.perf_counter() + max(1, args.duration_seconds)
        while time.perf_counter() < deadline:
            iterations += 1
            for name, method, params in sequence:
                if method == "connect":
                    step, _ = _run_connect_step(
                        name=name,
                        client=client,
                        config_path=args.config_path,
                        timeout_seconds=15.0,
                    )
                else:
                    step, _ = _run_tcp_step(
                        name=name,
                        client=client,
                        method=method,
                        params=params,
                        timeout_seconds=15.0,
                    )
                steps.append(step)
                if step.status == "passed" and name == "tcp-connect":
                    reconnect_count += 1
                if step.status == "passed" and name == "tcp-dispenser-start":
                    wait_step, transition = _wait_for_dispenser_state(
                        name=f"wait-dispenser-running-{iterations}",
                        client=client,
                        expected_state="running",
                        timeout_seconds=args.state_wait_timeout_seconds,
                    )
                    steps.append(wait_step)
                    state_transition_checks.append(transition)
                    if wait_step.status != "passed":
                        if "timeout" in wait_step.note.lower():
                            timeout_count += 1
                        step = wait_step
                elif step.status == "passed" and name.startswith("tcp-dispenser-pause-"):
                    cycle = name.rsplit("-", maxsplit=1)[-1]
                    wait_step, transition = _wait_for_dispenser_state(
                        name=f"wait-dispenser-paused-{iterations}-{cycle}",
                        client=client,
                        expected_state="paused",
                        timeout_seconds=args.state_wait_timeout_seconds,
                    )
                    steps.append(wait_step)
                    state_transition_checks.append(transition)
                    if wait_step.status != "passed":
                        if "timeout" in wait_step.note.lower():
                            timeout_count += 1
                        step = wait_step
                elif step.status == "passed" and name.startswith("tcp-dispenser-resume-"):
                    cycle = name.rsplit("-", maxsplit=1)[-1]
                    wait_step, transition = _wait_for_dispenser_state(
                        name=f"wait-dispenser-running-after-resume-{iterations}-{cycle}",
                        client=client,
                        expected_state="running",
                        timeout_seconds=args.state_wait_timeout_seconds,
                    )
                    steps.append(wait_step)
                    state_transition_checks.append(transition)
                    if wait_step.status != "passed":
                        if "timeout" in wait_step.note.lower():
                            timeout_count += 1
                        step = wait_step

                if step.status == "failed":
                    return _finalize(1, socket_connected=client.is_connected())
                if step.status == "known_failure":
                    return _finalize(KNOWN_FAILURE_EXIT_CODE, socket_connected=client.is_connected())
            if args.max_iterations > 0 and iterations >= args.max_iterations:
                break
            if time.perf_counter() >= deadline:
                break

        final_report = _build_report(
            args=args,
            gateway_exe=gateway_exe,
            cli_exe=cli_exe,
            steps=steps,
            iterations=iterations,
            reconnect_count=reconnect_count,
            timeout_count=timeout_count,
            elapsed_seconds=time.perf_counter() - started,
            state_transition_checks=state_transition_checks,
            admission=admission,
        )
        json_path, md_path = _write_reports(final_report, report_dir)
        print(f"hil closed-loop complete: status={final_report['overall_status']} iterations={iterations}")
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
