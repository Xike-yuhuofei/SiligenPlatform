from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import tempfile
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11

ROOT = Path(__file__).resolve().parents[4]
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)
CANONICAL_CONFIG = ROOT / "config" / "machine" / "machine_config.ini"
VENDOR_DIR = ROOT / "modules" / "runtime-execution" / "adapters" / "device" / "vendor" / "multicard"


@dataclass
class CheckResult:
    check_id: str
    status: str
    expected: str
    actual: str
    note: str = ""
    response: str = ""


@dataclass
class StepRecord:
    step_id: str
    method: str
    response: str


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


def _truncate_json(payload: Any, max_chars: int = 1600) -> str:
    text = json.dumps(payload, ensure_ascii=True) if not isinstance(payload, str) else payload
    if len(text) <= max_chars:
        return text
    return text[:max_chars] + "...[truncated]"


def _compute_overall_status(results: list[CheckResult]) -> str:
    statuses = {item.status for item in results}
    if "failed" in statuses:
        return "failed"
    if "known_failure" in statuses:
        return "known_failure"
    if statuses == {"skipped"}:
        return "skipped"
    return "passed"


def _resolve_default_gateway() -> Path:
    candidates = [
        ROOT / "build" / "hmi-home-fix" / "bin" / "Debug" / "siligen_runtime_gateway.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "siligen_runtime_gateway.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "Debug" / "siligen_runtime_gateway.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "Release" / "siligen_runtime_gateway.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "RelWithDebInfo" / "siligen_runtime_gateway.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _pick_free_port(host: str) -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        sock.listen(1)
        return int(sock.getsockname()[1])


def _tail_text(path: Path, max_chars: int = 4000) -> str:
    if not path.exists():
        return ""
    text = path.read_text(encoding="utf-8", errors="replace")
    if len(text) <= max_chars:
        return text
    return text[-max_chars:]


def _rewrite_mock_config(source_text: str) -> str:
    lines = source_text.splitlines()
    in_hardware = False
    replaced = False
    for index, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            in_hardware = stripped.lower() == "[hardware]"
            continue
        if in_hardware and stripped.lower().startswith("mode="):
            indent = line[: len(line) - len(line.lstrip())]
            lines[index] = f"{indent}mode=Mock"
            replaced = True
            break
    if not replaced:
        raise ValueError("failed to locate [Hardware] mode entry in machine config")
    return "\n".join(lines) + "\n"


def _prepare_mock_config(temp_dir: Path, source_config: Path) -> Path:
    rewritten = _rewrite_mock_config(source_config.read_text(encoding="utf-8"))
    target = temp_dir / "machine_config.mock.ini"
    target.write_text(rewritten, encoding="utf-8")
    return target


def _wait_gateway_ready(process: subprocess.Popen[str], host: str, port: int, timeout_seconds: float) -> str:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        if process.poll() is not None:
            return "gateway exited before TCP endpoint became ready"
        try:
            with socket.create_connection((host, port), timeout=1.0):
                return ""
        except OSError:
            time.sleep(0.2)
    return f"gateway readiness timeout: {host}:{port}"


def _build_process_env(gateway_exe: Path) -> dict[str, str]:
    env = os.environ.copy()
    candidate_dirs: list[str] = []

    build_config_dir = gateway_exe.parent
    candidate_dirs.append(str(build_config_dir))

    if build_config_dir.parent.name.lower() == "bin":
        sibling_lib_dir = build_config_dir.parent.parent / "lib" / build_config_dir.name
        if sibling_lib_dir.exists():
            candidate_dirs.append(str(sibling_lib_dir))

    if VENDOR_DIR.exists():
        candidate_dirs.append(str(VENDOR_DIR))
        env["SILIGEN_MULTICARD_VENDOR_DIR"] = str(VENDOR_DIR)

    existing_path = env.get("PATH", "")
    env["PATH"] = os.pathsep.join(candidate_dirs + ([existing_path] if existing_path else []))
    return env


def _extract_axes(status_response: dict[str, Any]) -> dict[str, dict[str, Any]]:
    result = status_response.get("result", {})
    axes_payload = result.get("axes", {})
    if not isinstance(axes_payload, dict):
        return {}
    return {str(name): payload for name, payload in axes_payload.items() if isinstance(payload, dict)}


def _all_axes_homed(status_response: dict[str, Any]) -> bool:
    axes = _extract_axes(status_response)
    if not axes:
        return False
    for axis in axes.values():
        if not bool(axis.get("homed", False)):
            return False
        if str(axis.get("homing_state", "")).lower() != "homed":
            return False
    return True


def _any_axis_velocity_nonzero(status_response: dict[str, Any]) -> bool:
    for axis in _extract_axes(status_response).values():
        if abs(float(axis.get("velocity", 0.0))) > 1e-6:
            return True
    return False


def _all_axis_velocity_zero(status_response: dict[str, Any]) -> bool:
    axes = _extract_axes(status_response)
    if not axes:
        return False
    return all(abs(float(axis.get("velocity", 0.0))) <= 1e-6 for axis in axes.values())


def _connection_state(status_response: dict[str, Any]) -> str:
    return str(status_response.get("result", {}).get("connection_state", ""))


def _machine_state_reason(status_response: dict[str, Any]) -> str:
    return str(status_response.get("result", {}).get("machine_state_reason", ""))


def _write_report(report_dir: Path, report: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "tcp-jog-focus-smoke.json"
    md_path = report_dir / "tcp-jog-focus-smoke.md"

    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# TCP Jog Focus Smoke",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
        f"- temp_config: `{report['temp_config']}`",
        f"- host: `{report['host']}`",
        f"- port: `{report['port']}`",
        "",
        "## Check Results",
        "",
    ]
    for item in report["results"]:
        lines.append(
            f"- `{item['status']}` `{item['check_id']}` expected=`{item['expected']}` actual=`{item['actual']}`"
        )
        if item.get("note"):
            lines.append(f"  note: {item['note']}")
        if item.get("response"):
            lines.append("  response:")
            lines.append("  ```json")
            lines.append(f"  {item['response']}")
            lines.append("  ```")
    lines.extend(("", "## Step Transcript", ""))
    for step in report["steps"]:
        lines.append(f"- `{step['step_id']}` method=`{step['method']}`")
        lines.append("  ```json")
        lines.append(f"  {step['response']}")
        lines.append("  ```")
    lines.extend(
        (
            "",
            "## Gateway Output",
            "",
            "### stdout",
            "```text",
            report.get("gateway_stdout_tail", ""),
            "```",
            "",
            "### stderr",
            "```text",
            report.get("gateway_stderr_tail", ""),
            "```",
            "",
        )
    )
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run focused TCP jog smoke against runtime gateway in mock mode.")
    parser.add_argument(
        "--report-dir",
        default=str(ROOT / "tests" / "reports" / "first-layer" / "s1-tcp-jog-focus-smoke"),
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--config", default=str(CANONICAL_CONFIG))
    parser.add_argument(
        "--gateway-exe",
        default=os.getenv("SILIGEN_HIL_GATEWAY_EXE", str(_resolve_default_gateway())),
    )
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    gateway_exe = Path(args.gateway_exe)
    source_config = Path(args.config)
    results: list[CheckResult] = []
    steps: list[StepRecord] = []

    if not gateway_exe.exists():
        status = "skipped" if args.allow_skip_on_missing_gateway else "known_failure"
        results.append(
            CheckResult(
                check_id="J0-bootstrap",
                status=status,
                expected="gateway executable exists",
                actual=str(gateway_exe),
                note="gateway executable missing",
            )
        )
        report = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(gateway_exe),
            "temp_config": "",
            "host": args.host,
            "port": args.port,
            "overall_status": _compute_overall_status(results),
            "results": [asdict(item) for item in results],
            "steps": [asdict(item) for item in steps],
            "gateway_stdout_tail": "",
            "gateway_stderr_tail": "",
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp jog focus smoke complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return SKIPPED_EXIT_CODE if status == "skipped" else KNOWN_FAILURE_EXIT_CODE

    if not source_config.exists():
        raise FileNotFoundError(f"machine config not found: {source_config}")

    port = args.port if args.port > 0 else _pick_free_port(args.host)

    client = TcpJsonClient(args.host, port)
    process: subprocess.Popen[str] | None = None
    stdout_path: Path | None = None
    stderr_path: Path | None = None
    fatal_error = ""

    with tempfile.TemporaryDirectory(prefix="tcp-jog-focus-") as temp_dir_str:
        temp_dir = Path(temp_dir_str)
        temp_config = _prepare_mock_config(temp_dir, source_config)
        stdout_path = temp_dir / "gateway.stdout.log"
        stderr_path = temp_dir / "gateway.stderr.log"

        with stdout_path.open("w", encoding="utf-8") as stdout_file, stderr_path.open("w", encoding="utf-8") as stderr_file:
            process = subprocess.Popen(
                [str(gateway_exe), "--config", str(temp_config), "--port", str(port)],
                cwd=str(ROOT),
                stdout=stdout_file,
                stderr=stderr_file,
                text=True,
                env=_build_process_env(gateway_exe),
            )

            try:
                try:
                    ready_error = _wait_gateway_ready(process, args.host, port, timeout_seconds=25.0)
                    if ready_error:
                        results.append(
                            CheckResult(
                                check_id="J0-gateway-ready",
                                status="failed",
                                expected="TCP endpoint is reachable",
                                actual=ready_error,
                            )
                        )
                        raise RuntimeError(ready_error)

                    results.append(
                        CheckResult(
                            check_id="J0-gateway-ready",
                            status="passed",
                            expected="TCP endpoint is reachable",
                            actual=f"{args.host}:{port}",
                        )
                    )

                    client.connect(timeout_seconds=5.0)

                    def call(step_id: str, method: str, params: dict[str, Any] | None = None, timeout_seconds: float = 8.0) -> dict[str, Any]:
                        response = client.send_request(method, params or {}, timeout_seconds=timeout_seconds)
                        steps.append(StepRecord(step_id=step_id, method=method, response=_truncate_json(response, max_chars=2400)))
                        return response

                    def poll_status(
                        step_prefix: str,
                        predicate: Callable[[dict[str, Any]], bool],
                        attempts: int,
                        interval_seconds: float,
                    ) -> tuple[bool, dict[str, Any]]:
                        last_response: dict[str, Any] = {}
                        for attempt in range(1, attempts + 1):
                            if attempt > 1:
                                time.sleep(interval_seconds)
                            last_response = call(f"{step_prefix}-{attempt}", "status", {}, timeout_seconds=5.0)
                            if "result" in last_response and predicate(last_response):
                                return True, last_response
                        return False, last_response

                    connect_response = call("step-connect", "connect", {}, timeout_seconds=12.0)
                    connect_ok = "error" not in connect_response and bool(connect_response.get("result", {}).get("connected", False))
                    results.append(
                        CheckResult(
                            check_id="J1-connect",
                            status="passed" if connect_ok else "failed",
                            expected="connect succeeds",
                            actual=_truncate_json(connect_response),
                            response=_truncate_json(connect_response),
                        )
                    )
                    if not connect_ok:
                        raise RuntimeError("connect failed")

                    jog_before_home = call("step-jog-before-home", "jog", {"axis": "X", "direction": 1, "speed": 5.0})
                    time.sleep(0.2)
                    status_after_jog_before_home = call("step-status-after-jog-before-home", "status", {}, timeout_seconds=5.0)
                    jog_before_home_blocked = "error" in jog_before_home and not _any_axis_velocity_nonzero(status_after_jog_before_home)
                    results.append(
                        CheckResult(
                            check_id="J2-jog-before-home-blocked",
                            status="passed" if jog_before_home_blocked else "failed",
                            expected="jog before home is rejected and no axis motion is visible",
                            actual=_truncate_json(jog_before_home),
                            note=f"status_after={_truncate_json(status_after_jog_before_home)}",
                            response=_truncate_json(jog_before_home),
                        )
                    )

                    home_response = call("step-home", "home", {}, timeout_seconds=45.0)
                    home_completed = "error" not in home_response and bool(home_response.get("result", {}).get("completed", False))
                    homed_ok, status_after_home = poll_status("step-status-after-home", _all_axes_homed, attempts=20, interval_seconds=0.25)
                    results.append(
                        CheckResult(
                            check_id="J3-home-completes",
                            status="passed" if home_completed and homed_ok else "failed",
                            expected="home completes and all axes reach homed state",
                            actual=_truncate_json(home_response),
                            note=f"status_after={_truncate_json(status_after_home)}",
                            response=_truncate_json(home_response),
                        )
                    )

                    home_connection_ok = "result" in status_after_home and _connection_state(status_after_home) != "degraded"
                    results.append(
                        CheckResult(
                            check_id="J4-home-status-not-degraded",
                            status="passed" if home_connection_ok else "failed",
                            expected="status.connection_state != degraded after home",
                            actual=f"connection_state={_connection_state(status_after_home)} reason={_machine_state_reason(status_after_home)}",
                            response=_truncate_json(status_after_home),
                        )
                    )

                    jog_after_home = call("step-jog-after-home", "jog", {"axis": "X", "direction": 1, "speed": 5.0})
                    jog_started = "error" not in jog_after_home
                    motion_visible, status_after_jog = poll_status(
                        "step-status-after-jog",
                        _any_axis_velocity_nonzero,
                        attempts=15,
                        interval_seconds=0.15,
                    )
                    results.append(
                        CheckResult(
                            check_id="J5-jog-after-home-has-velocity",
                            status="passed" if jog_started and motion_visible else "failed",
                            expected="jog succeeds after home and at least one axis exposes non-zero velocity",
                            actual=_truncate_json(jog_after_home),
                            note=f"status_after={_truncate_json(status_after_jog)}",
                            response=_truncate_json(jog_after_home),
                        )
                    )

                    jog_connection_ok = "result" in status_after_jog and _connection_state(status_after_jog) != "degraded"
                    results.append(
                        CheckResult(
                            check_id="J6-jog-status-not-degraded",
                            status="passed" if jog_connection_ok else "failed",
                            expected="status.connection_state != degraded during jog",
                            actual=f"connection_state={_connection_state(status_after_jog)} reason={_machine_state_reason(status_after_jog)}",
                            response=_truncate_json(status_after_jog),
                        )
                    )

                    stop_response = call("step-stop-after-jog", "stop", {}, timeout_seconds=8.0)
                    stop_ack = "error" not in stop_response
                    zero_velocity, status_after_stop = poll_status(
                        "step-status-after-stop",
                        _all_axis_velocity_zero,
                        attempts=15,
                        interval_seconds=0.15,
                    )
                    results.append(
                        CheckResult(
                            check_id="J7-stop-clears-velocity",
                            status="passed" if stop_ack and zero_velocity else "failed",
                            expected="stop succeeds and all axes velocity return to zero",
                            actual=_truncate_json(stop_response),
                            note=f"status_after={_truncate_json(status_after_stop)}",
                            response=_truncate_json(stop_response),
                        )
                    )
                except Exception as exc:
                    fatal_error = repr(exc)
                    results.append(
                        CheckResult(
                            check_id="JX-runtime-exception",
                            status="failed",
                            expected="smoke flow completes without unhandled exception",
                            actual=repr(exc),
                        )
                    )

            finally:
                try:
                    client.close()
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=5.0)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait(timeout=5.0)

        report = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(gateway_exe),
            "temp_config": str(temp_config),
            "host": args.host,
            "port": port,
            "overall_status": _compute_overall_status(results),
            "results": [asdict(item) for item in results],
            "steps": [asdict(item) for item in steps],
            "gateway_stdout_tail": _tail_text(stdout_path),
            "gateway_stderr_tail": _tail_text(stderr_path),
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp jog focus smoke complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        if report["overall_status"] == "known_failure":
            return KNOWN_FAILURE_EXIT_CODE
        if report["overall_status"] == "skipped":
            return SKIPPED_EXIT_CODE
        return 0 if report["overall_status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
