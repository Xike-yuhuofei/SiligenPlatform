from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import subprocess
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11

ROOT = Path(__file__).resolve().parents[4]
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)
VENDOR_DIR = ROOT / "modules" / "runtime-execution" / "adapters" / "device" / "vendor" / "multicard"
DEFAULT_DXF_FILE = ROOT / "samples" / "dxf" / "rect_diag.dxf"
DEFAULT_DXF_PB_SCRIPT = ROOT / "scripts" / "engineering-data" / "dxf_to_pb.py"
DEFAULT_DXF_TRAJECTORY_SCRIPT = ROOT / "scripts" / "engineering-data" / "path_to_trajectory.py"
DEFAULT_WORKSPACE_MARKER = ROOT / "WORKSPACE.md"
DEFAULT_LAYOUT_FILE = ROOT / "cmake" / "workspace-layout.env"
DEFAULT_AGENTS_FILE = ROOT / "AGENTS.md"

KNOWN_FAILURE_PATTERNS = (
    "IDiagnosticsPort 未注册",
    "IDiagnosticsPort not registered",
    "diagnostics port not registered",
    "Service not registered",
    "SERVICE_NOT_REGISTERED",
)


@dataclass
class ScenarioResult:
    scenario_id: str
    status: str
    expected: str
    actual: str
    note: str = ""
    response: str = ""


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


def _resolve_default_exe(*file_names: str) -> Path:
    candidates: list[Path] = []
    for file_name in file_names:
        candidates.extend(
            (
                ROOT / "build" / "hmi-home-fix" / "bin" / file_name,
                ROOT / "build" / "hmi-home-fix" / "bin" / "Debug" / file_name,
                ROOT / "build" / "hmi-home-fix" / "bin" / "Release" / file_name,
                ROOT / "build" / "hmi-home-fix" / "bin" / "RelWithDebInfo" / file_name,
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


def _matches_known_failure(text: str) -> bool:
    lowered = text.lower()
    for pattern in KNOWN_FAILURE_PATTERNS:
        if pattern.lower() in lowered:
            return True
    return False


def _truncate_json(payload: dict[str, Any] | None, max_chars: int = 1500) -> str:
    if not payload:
        return ""
    text = json.dumps(payload, ensure_ascii=True)
    if len(text) <= max_chars:
        return text
    return text[:max_chars] + "...[truncated]"


def _is_tcp_endpoint_reachable(host: str, port: int, timeout_seconds: float = 0.5) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout_seconds):
            return True
    except OSError:
        return False


def _resolve_runtime_port(host: str, cli_port: int | None) -> tuple[int, bool]:
    env_port = os.getenv("SILIGEN_HIL_PORT", "").strip()
    requested_port = cli_port
    if requested_port is None and env_port:
        requested_port = int(env_port)
    if requested_port is not None:
        return requested_port, True

    bind_host = host.strip() or "127.0.0.1"
    if bind_host in {"0.0.0.0", "localhost"}:
        bind_host = "127.0.0.1"

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((bind_host, 0))
        return int(sock.getsockname()[1]), False


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


def _read_process_output(process: subprocess.Popen[str]) -> tuple[str, str]:
    try:
        stdout, stderr = process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate(timeout=5)
    return stdout or "", stderr or ""


def _prepare_isolated_workspace(report_dir: Path) -> Path:
    isolated_root = report_dir / "isolated-empty-recipe-workspace"
    if isolated_root.exists():
        shutil.rmtree(isolated_root)

    required_dirs = (
        isolated_root / "cmake",
        isolated_root / "config" / "machine",
        isolated_root / "data" / "recipes",
        isolated_root / "data" / "recipes" / "recipes",
        isolated_root / "data" / "recipes" / "versions",
        isolated_root / "data" / "recipes" / "audit",
        isolated_root / "data" / "recipes" / "templates",
        isolated_root / "data" / "schemas",
        isolated_root / "data" / "schemas" / "recipes",
        isolated_root / "logs" / "audit",
        isolated_root / "uploads" / "dxf",
    )
    for path in required_dirs:
        path.mkdir(parents=True, exist_ok=True)

    if DEFAULT_WORKSPACE_MARKER.exists():
        shutil.copy2(DEFAULT_WORKSPACE_MARKER, isolated_root / "WORKSPACE.md")
    if DEFAULT_LAYOUT_FILE.exists():
        shutil.copy2(DEFAULT_LAYOUT_FILE, isolated_root / "cmake" / "workspace-layout.env")
    if DEFAULT_AGENTS_FILE.exists():
        shutil.copy2(DEFAULT_AGENTS_FILE, isolated_root / "AGENTS.md")

    source_config = ROOT / "config" / "machine" / "machine_config.ini"
    config_text = source_config.read_text(encoding="utf-8")
    trajectory_script = str(DEFAULT_DXF_TRAJECTORY_SCRIPT.resolve())
    rewritten_lines: list[str] = []
    in_hardware = False
    mock_mode_rewritten = False
    for line in config_text.splitlines():
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            in_hardware = stripped.lower() == "[hardware]"
            rewritten_lines.append(line)
            continue
        if in_hardware and stripped.lower().startswith("mode="):
            indent = line[: len(line) - len(line.lstrip())]
            rewritten_lines.append(f"{indent}mode=Mock")
            mock_mode_rewritten = True
        elif line.startswith("script="):
            rewritten_lines.append(f"script={trajectory_script}")
        else:
            rewritten_lines.append(line)
    if not mock_mode_rewritten:
        raise ValueError("failed to rewrite [Hardware] mode to Mock in isolated workspace config")
    isolated_config = isolated_root / "config" / "machine" / "machine_config.ini"
    isolated_config.write_text("\n".join(rewritten_lines) + "\n", encoding="utf-8")

    return isolated_root.resolve()


def _expect_error_code(
    *,
    scenario_id: str,
    response: dict[str, Any],
    expected_code: int,
    expected_desc: str,
) -> ScenarioResult:
    error = response.get("error")
    actual = _truncate_json(response)
    if not isinstance(error, dict):
        return ScenarioResult(
            scenario_id=scenario_id,
            status="failed",
            expected=expected_desc,
            actual="success response",
            note="response has no error payload",
            response=actual,
        )

    actual_code = error.get("code")
    if actual_code == expected_code:
        return ScenarioResult(
            scenario_id=scenario_id,
            status="passed",
            expected=expected_desc,
            actual=f"error_code={actual_code}",
            response=actual,
        )

    return ScenarioResult(
        scenario_id=scenario_id,
        status="failed",
        expected=expected_desc,
        actual=f"error_code={actual_code}",
        note=f"unexpected error code, message={error.get('message', '')}",
        response=actual,
    )


def _extract_axes(status_response: dict[str, Any]) -> dict[str, dict[str, Any]]:
    result = status_response.get("result", {})
    axes = result.get("axes", {})
    if not isinstance(axes, dict):
        return {}
    return {str(axis_name): axis for axis_name, axis in axes.items() if isinstance(axis, dict)}


def _extract_io(status_response: dict[str, Any]) -> dict[str, Any]:
    result = status_response.get("result", {})
    io_payload = result.get("io", {})
    if not isinstance(io_payload, dict):
        return {}
    return io_payload


def _extract_active_job_id(status_response: dict[str, Any]) -> str:
    result = status_response.get("result", {})
    return str(result.get("active_job_id", "")).strip()


def _extract_supply_open(status_response: dict[str, Any]) -> bool:
    result = status_response.get("result", {})
    dispenser = result.get("dispenser", {})
    if not isinstance(dispenser, dict):
        return False
    return bool(dispenser.get("supply_open", False))


def _extract_valve_open(status_response: dict[str, Any]) -> bool:
    result = status_response.get("result", {})
    dispenser = result.get("dispenser", {})
    if not isinstance(dispenser, dict):
        return False
    return bool(dispenser.get("valve_open", False))


def _all_axes_homed_consistently(status_response: dict[str, Any]) -> tuple[bool, list[str], list[str]]:
    axes = _extract_axes(status_response)
    non_homed: list[str] = []
    inconsistent: list[str] = []
    for axis_name, axis in axes.items():
        homed = bool(axis.get("homed", False))
        homing_state = str(axis.get("homing_state", ""))
        if not homed or homing_state != "homed":
            non_homed.append(axis_name)
        if homed != (homing_state == "homed"):
            inconsistent.append(axis_name)
    return bool(axes) and not non_homed, non_homed, inconsistent


def _has_axis_motion(status_response: dict[str, Any]) -> bool:
    for axis in _extract_axes(status_response).values():
        if abs(float(axis.get("velocity", 0.0))) > 1e-6:
            return True
    return False


def _best_effort_request(
    client: TcpJsonClient,
    method: str,
    params: dict[str, Any] | None = None,
    *,
    timeout_seconds: float = 5.0,
) -> dict[str, Any]:
    try:
        return client.send_request(method, params or {}, timeout_seconds=timeout_seconds)
    except Exception as exc:  # pragma: no cover - runtime path
        return {"exception": repr(exc)}


def _stabilize_runtime_state(client: TcpJsonClient) -> tuple[dict[str, Any], str]:
    for method in ("dxf.job.stop", "stop", "dispenser.stop", "supply.close", "estop.reset"):
        _best_effort_request(client, method, timeout_seconds=5.0)
        time.sleep(0.1)

    last_status: dict[str, Any] = {}
    for _ in range(6):
        time.sleep(0.2)
        last_status = _best_effort_request(client, "status", timeout_seconds=5.0)
        if "result" not in last_status:
            continue
        if (
            not _extract_active_job_id(last_status)
            and not _extract_supply_open(last_status)
            and not _extract_valve_open(last_status)
            and not _has_axis_motion(last_status)
            and not bool(_extract_io(last_status).get("estop", False))
        ):
            break

    note = (
        f"active_job_id={_extract_active_job_id(last_status) or 'none'} "
        f"supply_open={_extract_supply_open(last_status)} "
        f"valve_open={_extract_valve_open(last_status)} "
        f"axis_motion={_has_axis_motion(last_status)} "
        f"estop={bool(_extract_io(last_status).get('estop', False))}"
    )
    return last_status, note


def _home_until_ready(
    client: TcpJsonClient,
    *,
    max_attempts: int = 3,
    home_timeout_seconds: float = 45.0,
    status_timeout_seconds: float = 10.0,
) -> tuple[dict[str, Any], dict[str, Any], bool, list[str]]:
    last_home_response: dict[str, Any] = {}
    last_status_response: dict[str, Any] = {}
    attempt_notes: list[str] = []

    for attempt in range(1, max_attempts + 1):
        last_home_response = client.send_request("home", {}, timeout_seconds=home_timeout_seconds)
        if "error" in last_home_response:
            attempt_notes.append(
                f"attempt={attempt} home_error={_truncate_json(last_home_response, max_chars=240)}"
            )
            break

        last_status_response = client.send_request("status", {}, timeout_seconds=status_timeout_seconds)
        fully_homed, non_homed_axes, inconsistent_axes = _all_axes_homed_consistently(last_status_response)
        completed = bool(last_home_response.get("result", {}).get("completed", False))
        attempt_notes.append(
            "attempt="
            f"{attempt} completed={completed} "
            f"non_homed={','.join(non_homed_axes) if non_homed_axes else 'none'} "
            f"inconsistent={','.join(inconsistent_axes) if inconsistent_axes else 'none'}"
        )
        if completed and fully_homed:
            return last_home_response, last_status_response, True, attempt_notes

    return last_home_response, last_status_response, False, attempt_notes


def _compute_overall_status(results: list[ScenarioResult]) -> str:
    statuses = {item.status for item in results}
    if "failed" in statuses:
        return "failed"
    if "known_failure" in statuses:
        return "known_failure"
    if statuses == {"skipped"}:
        return "skipped"
    return "passed"


def _write_report(report_dir: Path, report: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "tcp-precondition-matrix.json"
    md_path = report_dir / "tcp-precondition-matrix.md"

    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# TCP Precondition Matrix",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
        f"- gateway_cwd: `{report['gateway_cwd']}`",
        f"- host: `{report['host']}`",
        f"- port: `{report['port']}`",
        f"- dxf_file: `{report['dxf_file']}`",
        "",
        "## Scenario Results",
        "",
    ]
    for item in report["results"]:
        lines.append(f"- `{item['status']}` `{item['scenario_id']}` expected=`{item['expected']}` actual=`{item['actual']}`")
        if item.get("note"):
            lines.append(f"  note: {item['note']}")
        if item.get("response"):
            lines.append("  response:")
            lines.append("  ```json")
            lines.append(f"  {item['response']}")
            lines.append("  ```")
    lines.append("")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run S2/S3/S4 TCP precondition matrix and write a report.")
    parser.add_argument("--report-dir", default=str(ROOT / "tests" / "reports" / "first-layer" / "s2-s4-precondition"))
    parser.add_argument("--host", default=os.getenv("SILIGEN_HIL_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=None)
    parser.add_argument(
        "--gateway-exe",
        default=os.getenv(
            "SILIGEN_HIL_GATEWAY_EXE",
            str(_resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe")),
        ),
    )
    parser.add_argument("--gateway-cwd", default=os.getenv("SILIGEN_HIL_GATEWAY_CWD", ""))
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DXF_FILE)
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    gateway_exe = Path(args.gateway_exe)
    port, port_is_requested = _resolve_runtime_port(args.host, args.port)
    gateway_cwd = (
        Path(args.gateway_cwd).resolve()
        if str(args.gateway_cwd).strip()
        else _prepare_isolated_workspace(report_dir)
    )
    gateway_config = gateway_cwd / "config" / "machine" / "machine_config.ini"
    results: list[ScenarioResult] = []

    if not gateway_exe.exists():
        status = "skipped" if args.allow_skip_on_missing_gateway else "known_failure"
        results.append(
            ScenarioResult(
                scenario_id="S2-S4-bootstrap",
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
            "gateway_cwd": str(gateway_cwd),
            "host": args.host,
            "port": port,
            "dxf_file": str(args.dxf_file),
            "overall_status": _compute_overall_status(results),
            "results": [asdict(item) for item in results],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp precondition matrix complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        if status == "skipped":
            return SKIPPED_EXIT_CODE
        return KNOWN_FAILURE_EXIT_CODE

    if not gateway_cwd.exists():
        results.append(
            ScenarioResult(
                scenario_id="S2-S4-bootstrap",
                status="failed",
                expected="gateway cwd exists",
                actual=str(gateway_cwd),
                note="gateway working directory missing",
            )
        )
        report = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(gateway_exe),
            "gateway_cwd": str(gateway_cwd),
            "host": args.host,
            "port": port,
            "dxf_file": str(args.dxf_file),
            "overall_status": _compute_overall_status(results),
            "results": [asdict(item) for item in results],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp precondition matrix complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return 1

    if not args.dxf_file.exists():
        results.append(
            ScenarioResult(
                scenario_id="S4-input-fixture",
                status="failed",
                expected="dxf fixture exists",
                actual=str(args.dxf_file),
                note="dxf fixture file missing",
            )
        )
        report = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(gateway_exe),
            "gateway_cwd": str(gateway_cwd),
            "host": args.host,
            "port": port,
            "dxf_file": str(args.dxf_file),
            "overall_status": _compute_overall_status(results),
            "results": [asdict(item) for item in results],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp precondition matrix complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return 1

    process_env = _build_process_env(gateway_exe)
    if "SILIGEN_DXF_PB_SCRIPT" not in process_env and DEFAULT_DXF_PB_SCRIPT.exists():
        process_env["SILIGEN_DXF_PB_SCRIPT"] = str(DEFAULT_DXF_PB_SCRIPT)

    if port_is_requested and _is_tcp_endpoint_reachable(args.host, port):
        results.append(
            ScenarioResult(
                scenario_id="S2-S4-bootstrap",
                status="failed",
                expected="requested TCP port is available before gateway launch",
                actual=f"{args.host}:{port}",
                note="requested port already has a reachable listener; refusing to connect to a stale process",
            )
        )
        report = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(gateway_exe),
            "gateway_cwd": str(gateway_cwd),
            "host": args.host,
            "port": port,
            "dxf_file": str(args.dxf_file),
            "overall_status": _compute_overall_status(results),
            "results": [asdict(item) for item in results],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp precondition matrix complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return 1

    process = subprocess.Popen(
        [str(gateway_exe), "--config", str(gateway_config), "--port", str(port)],
        cwd=str(gateway_cwd),
        env=process_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    client = TcpJsonClient(args.host, port)
    try:
        ready_status, ready_note = _wait_gateway_ready(process, args.host, port, timeout_seconds=8.0)
        if ready_status != "passed":
            results.append(
                ScenarioResult(
                    scenario_id="S2-S4-bootstrap",
                    status=ready_status,
                    expected="gateway ready",
                    actual=ready_note,
                    note="gateway readiness check failed",
                )
            )
            report = {
                "generated_at": datetime.now(timezone.utc).isoformat(),
                "workspace_root": str(ROOT),
                "gateway_exe": str(gateway_exe),
                "gateway_cwd": str(gateway_cwd),
                "host": args.host,
                "port": port,
                "dxf_file": str(args.dxf_file),
                "overall_status": _compute_overall_status(results),
                "results": [asdict(item) for item in results],
            }
            json_path, md_path = _write_report(report_dir, report)
            print(f"tcp precondition matrix complete: status={report['overall_status']}")
            print(f"json report: {json_path}")
            print(f"markdown report: {md_path}")
            if ready_status == "known_failure":
                return KNOWN_FAILURE_EXIT_CODE
            return 1

        client.connect(timeout_seconds=3.0)
        client.send_request("connect", {}, timeout_seconds=5.0)
        baseline_status, baseline_note = _stabilize_runtime_state(client)
        baseline_homed, _, baseline_inconsistent_axes = _all_axes_homed_consistently(baseline_status)
        baseline_active_job_id = _extract_active_job_id(baseline_status)
        baseline_estop_active = bool(_extract_io(baseline_status).get("estop", False))

        # S3: 未回零时 home.go 阻断
        if baseline_homed or baseline_inconsistent_axes or baseline_active_job_id or baseline_estop_active:
            results.append(
                ScenarioResult(
                    scenario_id="S3-home-go-without-homing",
                    status="skipped",
                    expected="error_code=2414",
                    actual="environment already satisfies or obscures homing precondition",
                    note=f"{baseline_note}; cleaned_status={_truncate_json(baseline_status, max_chars=2000)}",
                )
            )
        else:
            s3_response = client.send_request("home.go", {}, timeout_seconds=10.0)
            results.append(
                _expect_error_code(
                    scenario_id="S3-home-go-without-homing",
                    response=s3_response,
                    expected_code=2414,
                    expected_desc="error_code=2414",
                )
            )

        # S4: 未准备 plan 时 dxf.job.start 阻断
        s4_response = client.send_request(
            "dxf.job.start",
            {"target_count": 1},
            timeout_seconds=10.0,
        )
        results.append(
            _expect_error_code(
                scenario_id="S4-dxf-job-start-without-plan",
                response=s4_response,
                expected_code=2899,
                expected_desc="error_code=2899",
            )
        )

        # S2: 已生成 preview snapshot 但未 confirm 时 dxf.job.start 阻断
        home_response, status_after_home, home_ready, home_attempt_notes = _home_until_ready(client)
        home_attempt_note = "; ".join(home_attempt_notes)
        if "error" in home_response:
            results.append(
                ScenarioResult(
                    scenario_id="S2-preview-confirm-required",
                    status="failed",
                    expected="home success before preview confirm gate",
                    actual=_truncate_json(home_response),
                    note=(
                        "home failed; cannot evaluate preview confirm gate"
                        if not home_attempt_note
                        else f"home failed; cannot evaluate preview confirm gate; {home_attempt_note}"
                    ),
                    response=_truncate_json(home_response, max_chars=2000),
                )
            )
        else:
            _, non_homed_axes, inconsistent_axes = _all_axes_homed_consistently(status_after_home)
            if not home_ready:
                if inconsistent_axes:
                    note = f"status remains inconsistent after home attempts: {', '.join(inconsistent_axes)}"
                elif non_homed_axes:
                    note = f"status still reports non-homed axes after home attempts: {', '.join(non_homed_axes)}"
                else:
                    note = "home did not reach completed=true and fully-homed state within retry budget"
                if home_attempt_note:
                    note = f"{note}; {home_attempt_note}"
                results.append(
                    ScenarioResult(
                        scenario_id="S2-preview-confirm-required",
                        status="failed",
                        expected="home stabilizes before preview confirm gate",
                        actual=_truncate_json(home_response),
                        note=note,
                        response=_truncate_json(
                            {
                                "last_home_response": home_response,
                                "last_status_after_home": status_after_home,
                            },
                            max_chars=2000,
                        ),
                    )
                )
            else:
                load_response = client.send_request(
                    "dxf.load",
                    {"filepath": str(args.dxf_file)},
                    timeout_seconds=30.0,
                )
                if "error" in load_response:
                    results.append(
                        ScenarioResult(
                            scenario_id="S2-preview-confirm-required",
                            status="failed",
                            expected="dxf.load success before preview confirm gate",
                            actual=_truncate_json(load_response),
                            note="dxf.load failed; cannot evaluate preview confirm gate",
                            response=_truncate_json(load_response),
                        )
                    )
                else:
                    plan_params: dict[str, Any] = {
                        "dispensing_speed_mm_s": 10.0,
                        "dry_run": False,
                    }
                    artifact_id = str(load_response.get("result", {}).get("artifact_id", "")).strip()
                    if artifact_id:
                        plan_params["artifact_id"] = artifact_id

                    plan_response = client.send_request(
                        "dxf.plan.prepare",
                        plan_params,
                        timeout_seconds=30.0,
                    )
                    if "error" in plan_response:
                        results.append(
                            ScenarioResult(
                                scenario_id="S2-preview-confirm-required",
                                status="failed",
                                expected="dxf.plan.prepare success before preview confirm gate",
                                actual=_truncate_json(plan_response),
                                note="dxf.plan.prepare failed; cannot evaluate preview confirm gate",
                                response=_truncate_json(plan_response),
                            )
                        )
                    else:
                        plan_id = str(plan_response.get("result", {}).get("plan_id", "")).strip()
                        snapshot_response = client.send_request(
                            "dxf.preview.snapshot",
                            {"plan_id": plan_id},
                            timeout_seconds=30.0,
                        )
                        if "error" in snapshot_response:
                            results.append(
                                ScenarioResult(
                                    scenario_id="S2-preview-confirm-required",
                                    status="failed",
                                    expected="dxf.preview.snapshot success before preview confirm gate",
                                    actual=_truncate_json(snapshot_response),
                                    note="dxf.preview.snapshot failed; cannot evaluate preview confirm gate",
                                    response=_truncate_json(snapshot_response),
                                )
                            )
                        else:
                            s2_response = client.send_request(
                                "dxf.job.start",
                                {
                                    "plan_id": str(plan_response.get("result", {}).get("plan_id", "")).strip(),
                                    "plan_fingerprint": str(
                                        plan_response.get("result", {}).get("plan_fingerprint", "")
                                    ).strip(),
                                    "target_count": 1,
                                },
                                timeout_seconds=15.0,
                            )

                            s2_result = _expect_error_code(
                                scenario_id="S2-dxf-job-start-without-preview-confirm",
                                response=s2_response,
                                expected_code=2901,
                                expected_desc="error_code=2901",
                            )
                            if home_attempt_note:
                                s2_result.note = (
                                    f"{home_attempt_note}; {s2_result.note}"
                                    if s2_result.note
                                    else home_attempt_note
                                )

                            if s2_result.status == "failed" and "response has no error payload" in s2_result.note:
                                s2_result.note = "dxf.job.start unexpectedly succeeded before preview.confirm"
                            results.append(s2_result)

        try:
            client.send_request("disconnect", {}, timeout_seconds=5.0)
        except Exception:
            pass

    except Exception as exc:  # pragma: no cover - runtime path
        text = str(exc)
        status = "known_failure" if _matches_known_failure(text) else "failed"
        results.append(
            ScenarioResult(
                scenario_id="S2-S4-runtime",
                status=status,
                expected="matrix execution completed",
                actual=text,
                note="unhandled exception during matrix execution",
            )
        )
    finally:
        client.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "gateway_exe": str(gateway_exe),
        "gateway_cwd": str(gateway_cwd),
        "host": args.host,
        "port": port,
        "dxf_file": str(args.dxf_file),
        "overall_status": _compute_overall_status(results),
        "results": [asdict(item) for item in results],
    }
    json_path, md_path = _write_report(report_dir, report)
    print(f"tcp precondition matrix complete: status={report['overall_status']}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")

    if report["overall_status"] == "passed":
        return 0
    if report["overall_status"] == "known_failure":
        return KNOWN_FAILURE_EXIT_CODE
    if report["overall_status"] == "skipped":
        return SKIPPED_EXIT_CODE
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
