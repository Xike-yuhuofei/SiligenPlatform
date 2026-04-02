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
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11

ROOT = Path(__file__).resolve().parents[4]
CANONICAL_CONFIG = ROOT / "config" / "machine" / "machine_config.ini"
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)
VENDOR_DIR = ROOT / "modules" / "runtime-execution" / "adapters" / "device" / "vendor" / "multicard"

KNOWN_FAILURE_PATTERNS = (
    "IDiagnosticsPort 未注册",
    "IDiagnosticsPort not registered",
    "diagnostics port not registered",
    "Service not registered",
    "SERVICE_NOT_REGISTERED",
)


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


def _truncate_json(payload: dict[str, Any] | None, max_chars: int = 1600) -> str:
    if not payload:
        return ""
    text = json.dumps(payload, ensure_ascii=True)
    if len(text) <= max_chars:
        return text
    return text[:max_chars] + "...[truncated]"


def _read_process_output(process: subprocess.Popen[str]) -> tuple[str, str]:
    try:
        stdout, stderr = process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate(timeout=5)
    return stdout or "", stderr or ""


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


def _extract_axes(status_response: dict[str, Any]) -> dict[str, dict[str, Any]]:
    result = status_response.get("result", {})
    axes_payload = result.get("axes", {})
    if not isinstance(axes_payload, dict):
        return {}
    output: dict[str, dict[str, Any]] = {}
    for axis_name, axis_payload in axes_payload.items():
        if isinstance(axis_payload, dict):
            output[str(axis_name)] = axis_payload
    return output


def _extract_position(status_response: dict[str, Any]) -> tuple[float, float]:
    result = status_response.get("result", {})
    position = result.get("position", {})
    if not isinstance(position, dict):
        return 0.0, 0.0
    return float(position.get("x", 0.0)), float(position.get("y", 0.0))


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


def _extract_estop_visible(status_response: dict[str, Any]) -> tuple[bool, str]:
    result = status_response.get("result", {})
    io_payload = _extract_io(status_response)
    machine_state = str(result.get("machine_state", ""))
    machine_state_reason = str(result.get("machine_state_reason", ""))
    estop_flag = False
    if io_payload:
        estop_flag = bool(io_payload.get("estop", False))
    visible = estop_flag and machine_state == "Estop" and machine_state_reason == "interlock_estop"
    desc = f"io.estop={estop_flag}; machine_state={machine_state}; reason={machine_state_reason}"
    return visible, desc


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
    return not non_homed, non_homed, inconsistent


def _has_axis_motion(status_response: dict[str, Any]) -> bool:
    for axis in _extract_axes(status_response).values():
        if abs(float(axis.get("velocity", 0.0))) > 1e-6:
            return True
    return False


def _choose_safe_axis_target(
    current_position: float,
    limit_negative: bool,
    limit_positive: bool,
    step_mm: float,
) -> tuple[float, str]:
    if limit_negative and not limit_positive:
        return current_position + step_mm, "positive"
    if limit_positive and not limit_negative:
        return current_position - step_mm, "negative"
    if not limit_positive:
        return current_position + step_mm, "positive"
    if not limit_negative:
        return current_position - step_mm, "negative"
    return current_position, "hold"


def _build_safe_move_after_home_params(status_response: dict[str, Any]) -> tuple[dict[str, float], str]:
    current_x, current_y = _extract_position(status_response)
    io_payload = _extract_io(status_response)
    target_x, direction_x = _choose_safe_axis_target(
        current_x,
        bool(io_payload.get("limit_x_neg", False)),
        bool(io_payload.get("limit_x_pos", False)),
        1.25,
    )
    target_y, direction_y = _choose_safe_axis_target(
        current_y,
        bool(io_payload.get("limit_y_neg", False)),
        bool(io_payload.get("limit_y_pos", False)),
        2.5,
    )
    payload = {"x": round(target_x, 3), "y": round(target_y, 3), "speed": 5.0}
    note = (
        f"move_target={json.dumps(payload, ensure_ascii=True)} "
        f"current=({current_x:.3f},{current_y:.3f}) "
        f"x_direction={direction_x} y_direction={direction_y} "
        f"limits={json.dumps(io_payload, ensure_ascii=True, sort_keys=True)}"
    )
    return payload, note


def _build_safe_home_go_params(status_response: dict[str, Any]) -> tuple[dict[str, Any] | None, str]:
    io_payload = _extract_io(status_response)
    axes: list[str] = []
    blocked_axes: list[str] = []
    for axis_name, limit_key in (("X", "limit_x_neg"), ("Y", "limit_y_neg")):
        if bool(io_payload.get(limit_key, False)):
            blocked_axes.append(axis_name)
        else:
            axes.append(axis_name)

    if not axes:
        note = (
            "home_go axes unavailable because all candidate axes are already on the negative home-side limit; "
            f"limits={json.dumps(io_payload, ensure_ascii=True, sort_keys=True)}"
        )
        return None, note

    payload = {"axes": axes}
    note = (
        f"home_go_axes={json.dumps(payload, ensure_ascii=True)} "
        f"blocked_axes={','.join(blocked_axes) if blocked_axes else 'none'} "
        f"limits={json.dumps(io_payload, ensure_ascii=True, sort_keys=True)}"
    )
    return payload, note


def _compute_overall_status(results: list[CheckResult]) -> str:
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
    json_path = report_dir / "tcp-manual-motion-matrix.json"
    md_path = report_dir / "tcp-manual-motion-matrix.md"

    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# TCP Manual Motion Matrix",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
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
    lines.append("")

    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run manual motion/valve TCP matrix and write a report.")
    parser.add_argument(
        "--report-dir",
        default=str(ROOT / "tests" / "reports" / "first-layer" / "s1-manual-motion-matrix"),
    )
    parser.add_argument("--host", default=os.getenv("SILIGEN_HIL_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.getenv("SILIGEN_HIL_PORT", "9527")))
    parser.add_argument(
        "--gateway-exe",
        default=os.getenv(
            "SILIGEN_HIL_GATEWAY_EXE",
            str(_resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe")),
        ),
    )
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    gateway_exe = Path(args.gateway_exe)
    source_config = CANONICAL_CONFIG
    results: list[CheckResult] = []
    steps: list[StepRecord] = []

    if not gateway_exe.exists():
        status = "skipped" if args.allow_skip_on_missing_gateway else "known_failure"
        results.append(
            CheckResult(
                check_id="M0-bootstrap",
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
            "host": args.host,
            "port": args.port,
            "overall_status": _compute_overall_status(results),
            "results": [asdict(item) for item in results],
            "steps": [asdict(item) for item in steps],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp manual motion matrix complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return SKIPPED_EXIT_CODE if status == "skipped" else KNOWN_FAILURE_EXIT_CODE

    temp_config_dir = tempfile.TemporaryDirectory(prefix="tcp-manual-motion-")
    temp_config = _prepare_mock_config(Path(temp_config_dir.name), source_config)

    process = subprocess.Popen(
        [str(gateway_exe), "--config", str(temp_config), "--port", str(args.port)],
        cwd=str(ROOT),
        env=_build_process_env(gateway_exe),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    client = TcpJsonClient(args.host, args.port)
    home_ready = False
    try:
        ready_status, ready_note = _wait_gateway_ready(process, args.host, args.port, timeout_seconds=20.0)
        results.append(
            CheckResult(
                check_id="M0-gateway-ready",
                status=ready_status,
                expected="TCP endpoint is reachable",
                actual=ready_note,
            )
        )
        if ready_status != "passed":
            raise RuntimeError(ready_note)

        client.connect(timeout_seconds=5.0)

        def call(step_id: str, method: str, params: dict[str, Any] | None = None, timeout_seconds: float = 8.0) -> dict[str, Any]:
            response = client.send_request(method, params or {}, timeout_seconds=timeout_seconds)
            steps.append(StepRecord(step_id=step_id, method=method, response=_truncate_json(response, max_chars=2000)))
            return response

        def call_best_effort(
            step_id: str,
            method: str,
            params: dict[str, Any] | None = None,
            timeout_seconds: float = 8.0,
        ) -> dict[str, Any]:
            try:
                return call(step_id, method, params, timeout_seconds=timeout_seconds)
            except Exception as exc:  # pragma: no cover - runtime path
                response = {"exception": repr(exc)}
                steps.append(StepRecord(step_id=step_id, method=method, response=_truncate_json(response, max_chars=2000)))
                return response

        def stabilize_runtime(prefix: str) -> tuple[dict[str, Any], str]:
            for index, method in enumerate(("dxf.job.stop", "stop", "dispenser.stop", "supply.close", "estop.reset"), start=1):
                call_best_effort(f"{prefix}-cleanup-{index}", method, {}, timeout_seconds=5.0)
                time.sleep(0.1)

            last_status: dict[str, Any] = {}
            for attempt in range(1, 7):
                time.sleep(0.2)
                last_status = call_best_effort(f"{prefix}-status-{attempt}", "status", {}, timeout_seconds=5.0)
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

        connect_response = call("step-connect", "connect", {}, timeout_seconds=10.0)
        if "error" in connect_response:
            results.append(CheckResult("M1-connect", "failed", "connect succeeds", _truncate_json(connect_response), response=_truncate_json(connect_response)))
            raise RuntimeError("connect failed")
        results.append(CheckResult("M1-connect", "passed", "connect succeeds", "connected=true", response=_truncate_json(connect_response)))

        status_before_raw = call_best_effort("step-status-before-raw", "status", {}, timeout_seconds=5.0)
        status_before, baseline_note = stabilize_runtime("step-baseline")
        baseline_axes_homed, _, baseline_inconsistent_axes = _all_axes_homed_consistently(status_before)
        baseline_active_job_id = _extract_active_job_id(status_before)
        baseline_estop_active = bool(_extract_io(status_before).get("estop", False))
        homing_precondition_obscured = baseline_axes_homed or bool(baseline_inconsistent_axes) or bool(baseline_active_job_id) or baseline_estop_active
        supply_precondition_obscured = bool(baseline_active_job_id) or baseline_estop_active or _extract_supply_open(status_before) or _extract_valve_open(status_before)

        if homing_precondition_obscured:
            contamination_note = (
                f"prehome gate cannot be proven; {baseline_note}; "
                f"raw_status_before={_truncate_json(status_before_raw)} "
                f"cleaned_status={_truncate_json(status_before)}"
            )
            results.append(CheckResult("M2-move-requires-home", "skipped", "move is blocked before homing", "environment already satisfies or obscures homing precondition", note=contamination_note))
            results.append(CheckResult("M3-jog-requires-home", "skipped", "jog is blocked before homing", "environment already satisfies or obscures homing precondition", note=contamination_note))
        else:
            move_without_home = call("step-move-without-home", "move", {"x": 2.0, "y": 2.0, "speed": 5.0})
            time.sleep(0.25)
            status_after_move_without_home = call("step-status-after-move-without-home", "status")
            stop_after_move_without_home = call("step-stop-after-move-without-home", "stop")

            if "error" in move_without_home:
                results.append(CheckResult("M2-move-requires-home", "passed", "move is blocked before homing", _truncate_json(move_without_home), response=_truncate_json(move_without_home)))
            else:
                results.append(CheckResult("M2-move-requires-home", "failed", "move is blocked before homing", _truncate_json(move_without_home), note=f"status_after_move={_truncate_json(status_after_move_without_home)}", response=_truncate_json(move_without_home)))

            jog_without_home = call("step-jog-without-home", "jog", {"axis": "X", "direction": 1, "speed": 3.0})
            time.sleep(0.25)
            status_after_jog_without_home = call("step-status-after-jog-without-home", "status")
            stop_after_jog_without_home = call("step-stop-after-jog-without-home", "stop")
            if "error" in jog_without_home:
                results.append(CheckResult("M3-jog-requires-home", "passed", "jog is blocked before homing", _truncate_json(jog_without_home), response=_truncate_json(jog_without_home)))
            else:
                results.append(CheckResult("M3-jog-requires-home", "failed", "jog is blocked before homing", _truncate_json(jog_without_home), note=f"status_after_jog={_truncate_json(status_after_jog_without_home)}", response=_truncate_json(jog_without_home)))

        if supply_precondition_obscured:
            contamination_note = f"supply gate cannot be proven; {baseline_note}; cleaned_status={_truncate_json(status_before)}"
            results.append(CheckResult("M4-dispenser-requires-supply-open", "skipped", "dispenser.start is blocked when supply is closed", "environment did not reach supply-closed idle baseline", note=contamination_note))
        else:
            dispenser_without_supply = call("step-dispenser-start-without-supply", "dispenser.start", {"count": 1, "interval_ms": 100, "duration_ms": 50})
            status_after_dispenser_without_supply = call("step-status-after-dispenser-without-supply", "status")
            error_payload = dispenser_without_supply.get("error")
            if isinstance(error_payload, dict) and int(error_payload.get("code", 0)) == 2701:
                results.append(CheckResult("M4-dispenser-requires-supply-open", "passed", "dispenser.start is blocked when supply is closed", f"error_code={error_payload.get('code')}", response=_truncate_json(dispenser_without_supply)))
            else:
                results.append(CheckResult("M4-dispenser-requires-supply-open", "failed", "dispenser.start is blocked when supply is closed", _truncate_json(dispenser_without_supply), note=f"status_after={_truncate_json(status_after_dispenser_without_supply)}", response=_truncate_json(dispenser_without_supply)))

        _, status_before_home_note = stabilize_runtime("step-before-home")

        home_attempt_details: list[str] = []
        inconsistent_axes_found: list[str] = []
        latest_home_status: dict[str, Any] = {}
        for attempt in range(1, 4):
            home_response = call(f"step-home-attempt-{attempt}", "home", {}, timeout_seconds=45.0)
            status_after_home = call(f"step-status-after-home-attempt-{attempt}", "status")
            latest_home_status = status_after_home
            fully_homed, non_homed_axes, inconsistent_axes = _all_axes_homed_consistently(status_after_home)
            inconsistent_axes_found.extend(inconsistent_axes)
            home_attempt_details.append(
                f"attempt={attempt} completed={home_response.get('result', {}).get('completed', False)} non_homed={','.join(non_homed_axes) if non_homed_axes else 'none'} inconsistent={','.join(inconsistent_axes) if inconsistent_axes else 'none'}"
            )
            if bool(home_response.get("result", {}).get("completed", False)) and fully_homed:
                home_ready = True
                break

        if inconsistent_axes_found:
            results.append(CheckResult("M5-home-status-consistency", "failed", "status.homed and homing_state are consistent after home", "inconsistent axes: " + ",".join(sorted(set(inconsistent_axes_found))), note="; ".join(home_attempt_details)))
        else:
            results.append(CheckResult("M5-home-status-consistency", "passed", "status.homed and homing_state are consistent after home", "no inconsistency observed", note="; ".join(home_attempt_details)))

        if not home_ready:
            results.append(CheckResult("M6-home-normal-path", "failed", "home reaches fully homed state", "home did not reach consistent fully homed state in 3 attempts", note=f"{status_before_home_note}; {'; '.join(home_attempt_details)}"))
        else:
            results.append(CheckResult("M6-home-normal-path", "passed", "home reaches fully homed state", "all axes homed consistently", note=f"{status_before_home_note}; {'; '.join(home_attempt_details)}"))

        if home_ready:
            home_go_params, home_go_note = _build_safe_home_go_params(latest_home_status)
            home_go = {"skipped": True, "note": home_go_note} if home_go_params is None else call("step-home-go", "home.go", home_go_params)
            move_after_home_params, move_after_home_note = _build_safe_move_after_home_params(latest_home_status)
            move_after_home = call("step-move-after-home", "move", move_after_home_params)
            time.sleep(0.25)
            status_after_move_after_home = call("step-status-after-move-after-home", "status")
            jog_after_home = call("step-jog-after-home", "jog", {"axis": "X", "direction": 1, "speed": 3.0})
            time.sleep(0.25)
            status_after_jog_after_home = call("step-status-after-jog-after-home", "status")
            stop_after_jog_after_home = call("step-stop-after-jog-after-home", "stop")
            time.sleep(0.1)
            status_after_stop_after_jog = call("step-status-after-stop-after-jog-after-home", "status")
            supply_open = call("step-supply-open", "supply.open")
            status_after_supply_open = call("step-status-after-supply-open", "status")
            dispenser_start = call("step-dispenser-start", "dispenser.start", {"count": 1, "interval_ms": 100, "duration_ms": 50})
            time.sleep(0.1)
            status_after_dispenser_start = call("step-status-after-dispenser-start", "status")
            dispenser_stop = call("step-dispenser-stop", "dispenser.stop")
            supply_close = call("step-supply-close", "supply.close")
            status_after_supply_close = call("step-status-after-supply-close", "status")
            estop = call("step-estop", "estop")
            status_after_estop = call("step-status-after-estop", "status")
            home_go_after_estop = call("step-home-go-after-estop", "home.go")
            jog_after_estop = call("step-jog-after-estop", "jog", {"axis": "X", "direction": 1, "speed": 3.0})
            time.sleep(0.25)
            status_after_jog_after_estop = call("step-status-after-jog-after-estop", "status")
            stop_after_estop = call("step-stop-after-estop", "stop")
            time.sleep(0.1)
            status_after_stop_after_estop = call("step-status-after-stop-after-estop", "status")
            position_before_move_after_estop = _extract_position(status_after_stop_after_estop)
            move_after_estop = call("step-move-after-estop", "move", {"x": 1.0, "y": 1.0, "speed": 5.0})
            time.sleep(0.25)
            status_after_move_after_estop = call("step-status-after-move-after-estop", "status")
            position_after_move_after_estop = _extract_position(status_after_move_after_estop)
            supply_open_after_estop = call("step-supply-open-after-estop", "supply.open")
            status_after_supply_open_after_estop = call("step-status-after-supply-open-after-estop", "status")
            dispenser_start_after_estop = call("step-dispenser-start-after-estop", "dispenser.start", {"count": 1, "interval_ms": 100, "duration_ms": 50})
            time.sleep(0.1)
            status_after_dispenser_start_after_estop = call("step-status-after-dispenser-start-after-estop", "status")
            dispenser_stop_after_estop = call("step-dispenser-stop-after-estop", "dispenser.stop")
            supply_close_after_estop = call("step-supply-close-after-estop", "supply.close")
            status_after_supply_close_after_estop = call("step-status-after-supply-close-after-estop", "status")
            disconnect_response = call("step-disconnect", "disconnect")

            home_go_status = "skipped" if home_go_params is None else ("passed" if "error" not in home_go else "failed")
            results.append(CheckResult("M7-home-go-normal-path", home_go_status, "home.go succeeds after homing", _truncate_json(home_go), note=home_go_note, response=_truncate_json(home_go)))
            results.append(CheckResult("M8-move-normal-path", "passed" if "error" not in move_after_home else "failed", "move succeeds after homing", _truncate_json(move_after_home), note=f"{move_after_home_note}; status_after_move={_truncate_json(status_after_move_after_home)}", response=_truncate_json(move_after_home)))
            results.append(CheckResult("M9-jog-normal-path", "passed" if "error" not in jog_after_home else "failed", "jog succeeds after homing", _truncate_json(jog_after_home), note=f"status_after_jog={_truncate_json(status_after_jog_after_home)}", response=_truncate_json(jog_after_home)))
            zero_velocity = all(float(axis.get("velocity", 0.0)) == 0.0 for axis in _extract_axes(status_after_stop_after_jog).values())
            results.append(CheckResult("M10-stop-normal-path", "passed" if "error" not in stop_after_jog_after_home and zero_velocity else "failed", "stop succeeds and clears axis velocity", _truncate_json(stop_after_jog_after_home), note=f"status_after_stop={_truncate_json(status_after_stop_after_jog)}", response=_truncate_json(stop_after_jog_after_home)))
            results.append(CheckResult("M11-supply-open-close-normal-path", "passed" if "error" not in supply_open and "error" not in supply_close and _extract_supply_open(status_after_supply_open) and not _extract_supply_open(status_after_supply_close) else "failed", "supply.open / supply.close succeed and status reflects transitions", f"open={_truncate_json(supply_open)} close={_truncate_json(supply_close)}", note=f"status_after_open={_truncate_json(status_after_supply_open)} status_after_close={_truncate_json(status_after_supply_close)}"))
            results.append(CheckResult("M12-dispenser-start-stop-normal-path", "passed" if "error" not in dispenser_start and "error" not in dispenser_stop else "failed", "dispenser.start / dispenser.stop succeed when supply is open", f"start={_truncate_json(dispenser_start)} stop={_truncate_json(dispenser_stop)}", note=f"status_after_start={_truncate_json(status_after_dispenser_start)}"))
            estop_visible, estop_desc = _extract_estop_visible(status_after_estop)
            results.append(CheckResult("M13-estop-visible", "passed" if "error" not in estop and estop_visible else "failed", "estop succeeds and status enters Estop", estop_desc, response=_truncate_json(estop)))
            results.append(CheckResult("M14-home-go-blocked-by-estop", "passed" if "error" in home_go_after_estop else "failed", "home.go is blocked by estop", _truncate_json(home_go_after_estop), note=f"status_after_estop={_truncate_json(status_after_estop)}", response=_truncate_json(home_go_after_estop)))
            jog_motion_visible = any(float(axis.get("velocity", 0.0)) != 0.0 for axis in _extract_axes(status_after_jog_after_estop).values())
            results.append(CheckResult("M15-jog-blocked-by-estop", "passed" if "error" in jog_after_estop and not jog_motion_visible else "failed", "jog is blocked by estop and no axis motion is visible", _truncate_json(jog_after_estop), note=f"status_after_jog_after_estop={_truncate_json(status_after_jog_after_estop)}", response=_truncate_json(jog_after_estop)))
            moved_after_estop = position_after_move_after_estop != position_before_move_after_estop
            results.append(CheckResult("M16-move-blocked-by-estop", "passed" if "error" in move_after_estop and not moved_after_estop else "failed", "move is blocked by estop and position does not change", _truncate_json(move_after_estop), note=f"position_before={position_before_move_after_estop} position_after={position_after_move_after_estop} status_after_move_after_estop={_truncate_json(status_after_move_after_estop)}", response=_truncate_json(move_after_estop)))
            results.append(CheckResult("M17-supply-open-blocked-by-estop", "passed" if "error" in supply_open_after_estop and not _extract_supply_open(status_after_supply_open_after_estop) else "failed", "supply.open is blocked by estop", _truncate_json(supply_open_after_estop), note=f"status_after_supply_open_after_estop={_truncate_json(status_after_supply_open_after_estop)}", response=_truncate_json(supply_open_after_estop)))
            results.append(CheckResult("M18-dispenser-start-blocked-by-estop", "passed" if "error" in dispenser_start_after_estop else "failed", "dispenser.start is blocked by estop", _truncate_json(dispenser_start_after_estop), note=f"status_after_dispenser_after_estop={_truncate_json(status_after_dispenser_start_after_estop)}", response=_truncate_json(dispenser_start_after_estop)))
            results.append(CheckResult("M19-post-estop-close-actions", "passed" if "error" not in stop_after_estop and "error" not in dispenser_stop_after_estop and "error" not in supply_close_after_estop and not _extract_supply_open(status_after_supply_close_after_estop) and not _extract_valve_open(status_after_supply_close_after_estop) else "failed", "stop / dispenser.stop / supply.close complete after estop", f"stop={_truncate_json(stop_after_estop)} dispenser_stop={_truncate_json(dispenser_stop_after_estop)} supply_close={_truncate_json(supply_close_after_estop)}", note=f"disconnect={_truncate_json(disconnect_response)} status_after_close={_truncate_json(status_after_supply_close_after_estop)}"))

    except Exception as exc:
        results.append(CheckResult("M99-unhandled-exception", "failed", "manual motion matrix completes", repr(exc)))
    finally:
        try:
            client.close()
        finally:
            if process.poll() is None:
                process.kill()
            stdout, stderr = _read_process_output(process)
            if stdout.strip():
                steps.append(StepRecord("gateway-stdout", "process.stdout", stdout.strip()))
            if stderr.strip():
                steps.append(StepRecord("gateway-stderr", "process.stderr", stderr.strip()))
            temp_config_dir.cleanup()

    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "gateway_exe": str(gateway_exe),
        "host": args.host,
        "port": args.port,
        "overall_status": _compute_overall_status(results),
        "results": [asdict(item) for item in results],
        "steps": [asdict(item) for item in steps],
    }
    json_path, md_path = _write_report(report_dir, report)
    print(f"tcp manual motion matrix complete: status={report['overall_status']}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    if report["overall_status"] == "known_failure":
        return KNOWN_FAILURE_EXIT_CODE
    if report["overall_status"] == "skipped":
        return SKIPPED_EXIT_CODE
    return 0 if report["overall_status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
