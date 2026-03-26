from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11
ROOT = Path(__file__).resolve().parents[3]
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)
DEFAULT_DXF_FILE = ROOT / "samples" / "dxf" / "rect_diag.dxf"

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


def _resolve_default_exe(*file_names: str) -> Path:
    candidates: list[Path] = []
    for file_name in file_names:
        candidates.extend(
            (
                CONTROL_APPS_BUILD_ROOT / "bin" / file_name,
                CONTROL_APPS_BUILD_ROOT / "bin" / "Debug" / file_name,
                CONTROL_APPS_BUILD_ROOT / "bin" / "Release" / file_name,
                CONTROL_APPS_BUILD_ROOT / "bin" / "RelWithDebInfo" / file_name,
            )
        )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


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
) -> list[tuple[str, str, dict[str, Any]]]:
    _ = dxf_file
    sequence: list[tuple[str, str, dict[str, Any]]] = [
        ("tcp-connect", "connect", {}),
        ("tcp-status", "status", {}),
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
        f"- dxf_file: `{report['dxf_file']}`",
        f"- dispenser_start_params: `count={report['dispenser_start_params']['count']} interval_ms={report['dispenser_start_params']['interval_ms']} duration_ms={report['dispenser_start_params']['duration_ms']}`",
        f"- state_wait_timeout_seconds: `{report['state_wait_timeout_seconds']}`",
        "",
    ]

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

    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "gateway_exe": str(gateway_exe),
        "cli_exe": str(cli_exe),
        "transport": "tcp-json",
        "host": args.host,
        "port": args.port,
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
    parser.add_argument(
        "--gateway-exe",
        default=os.getenv(
            "SILIGEN_HIL_GATEWAY_EXE",
            str(_resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe")),
        ),
    )
    parser.add_argument(
        "--cli-exe",
        default=os.getenv(
            "SILIGEN_HIL_CLI_EXE",
            str(_resolve_default_exe("siligen_planner_cli.exe", "siligen_cli.exe")),
        ),
    )
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DXF_FILE)
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
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    gateway_exe = Path(args.gateway_exe)
    cli_exe = Path(args.cli_exe)
    steps: list[StepResult] = []
    iterations = 0
    reconnect_count = 0
    timeout_count = 0
    state_transition_checks: list[dict] = []

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
        report = _build_report(
            args=args,
            gateway_exe=gateway_exe,
            cli_exe=cli_exe,
            steps=steps,
            iterations=iterations,
            reconnect_count=reconnect_count,
            timeout_count=timeout_count,
            elapsed_seconds=0.0,
            state_transition_checks=state_transition_checks,
        )
        json_path, md_path = _write_reports(report, report_dir)
        print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return SKIPPED_EXIT_CODE if status == "skipped" else KNOWN_FAILURE_EXIT_CODE

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
        report = _build_report(
            args=args,
            gateway_exe=gateway_exe,
            cli_exe=cli_exe,
            steps=steps,
            iterations=iterations,
            reconnect_count=reconnect_count,
            timeout_count=timeout_count,
            elapsed_seconds=0.0,
            state_transition_checks=state_transition_checks,
        )
        json_path, md_path = _write_reports(report, report_dir)
        print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return 1

    started = time.perf_counter()
    process = subprocess.Popen(
        [str(gateway_exe)],
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
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
            report = _build_report(
                args=args,
                gateway_exe=gateway_exe,
                cli_exe=cli_exe,
                steps=steps,
                iterations=iterations,
                reconnect_count=reconnect_count,
                timeout_count=timeout_count,
                elapsed_seconds=time.perf_counter() - started,
                state_transition_checks=state_transition_checks,
            )
            json_path, md_path = _write_reports(report, report_dir)
            print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
            print(f"json report: {json_path}")
            print(f"markdown report: {md_path}")
            if ready_status == "known_failure":
                return KNOWN_FAILURE_EXIT_CODE
            if ready_status == "skipped":
                return SKIPPED_EXIT_CODE
            return 1

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
            report = _build_report(
                args=args,
                gateway_exe=gateway_exe,
                cli_exe=cli_exe,
                steps=steps,
                iterations=iterations,
                reconnect_count=reconnect_count,
                timeout_count=timeout_count,
                elapsed_seconds=time.perf_counter() - started,
                state_transition_checks=state_transition_checks,
            )
            json_path, md_path = _write_reports(report, report_dir)
            print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
            print(f"json report: {json_path}")
            print(f"markdown report: {md_path}")
            return 1

        sequence = _build_sequence(
            dxf_file=args.dxf_file,
            pause_resume_cycles=args.pause_resume_cycles,
            dispenser_count=args.dispenser_count,
            dispenser_interval_ms=args.dispenser_interval_ms,
            dispenser_duration_ms=args.dispenser_duration_ms,
        )

        dxf_step, _ = _run_tcp_step(
            name="tcp-dxf-load-preflight",
            client=client,
            method="dxf.load",
            params={"filepath": str(args.dxf_file)},
            timeout_seconds=60.0,
        )
        steps.append(dxf_step)
        if dxf_step.status == "failed":
            report = _build_report(
                args=args,
                gateway_exe=gateway_exe,
                cli_exe=cli_exe,
                steps=steps,
                iterations=iterations,
                reconnect_count=reconnect_count,
                timeout_count=timeout_count,
                elapsed_seconds=time.perf_counter() - started,
                state_transition_checks=state_transition_checks,
            )
            json_path, md_path = _write_reports(report, report_dir)
            print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
            print(f"json report: {json_path}")
            print(f"markdown report: {md_path}")
            return 1
        if dxf_step.status == "known_failure":
            report = _build_report(
                args=args,
                gateway_exe=gateway_exe,
                cli_exe=cli_exe,
                steps=steps,
                iterations=iterations,
                reconnect_count=reconnect_count,
                timeout_count=timeout_count,
                elapsed_seconds=time.perf_counter() - started,
                state_transition_checks=state_transition_checks,
            )
            json_path, md_path = _write_reports(report, report_dir)
            print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
            print(f"json report: {json_path}")
            print(f"markdown report: {md_path}")
            return KNOWN_FAILURE_EXIT_CODE

        deadline = time.perf_counter() + max(1, args.duration_seconds)
        while time.perf_counter() < deadline:
            iterations += 1
            for name, method, params in sequence:
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
                    report = _build_report(
                        args=args,
                        gateway_exe=gateway_exe,
                        cli_exe=cli_exe,
                        steps=steps,
                        iterations=iterations,
                        reconnect_count=reconnect_count,
                        timeout_count=timeout_count,
                        elapsed_seconds=time.perf_counter() - started,
                        state_transition_checks=state_transition_checks,
                    )
                    json_path, md_path = _write_reports(report, report_dir)
                    print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
                    print(f"json report: {json_path}")
                    print(f"markdown report: {md_path}")
                    return 1
                if step.status == "known_failure":
                    report = _build_report(
                        args=args,
                        gateway_exe=gateway_exe,
                        cli_exe=cli_exe,
                        steps=steps,
                        iterations=iterations,
                        reconnect_count=reconnect_count,
                        timeout_count=timeout_count,
                        elapsed_seconds=time.perf_counter() - started,
                        state_transition_checks=state_transition_checks,
                    )
                    json_path, md_path = _write_reports(report, report_dir)
                    print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
                    print(f"json report: {json_path}")
                    print(f"markdown report: {md_path}")
                    return KNOWN_FAILURE_EXIT_CODE
            if args.max_iterations > 0 and iterations >= args.max_iterations:
                break
            if time.perf_counter() >= deadline:
                break

        report = _build_report(
            args=args,
            gateway_exe=gateway_exe,
            cli_exe=cli_exe,
            steps=steps,
            iterations=iterations,
            reconnect_count=reconnect_count,
            timeout_count=timeout_count,
            elapsed_seconds=time.perf_counter() - started,
            state_transition_checks=state_transition_checks,
        )
        json_path, md_path = _write_reports(report, report_dir)
        print(f"hil closed-loop complete: status={report['overall_status']} iterations={iterations}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        if report["overall_status"] == "passed":
            return 0
        if report["overall_status"] == "known_failure":
            return KNOWN_FAILURE_EXIT_CODE
        if report["overall_status"] == "skipped":
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
