from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11

ROOT = Path(__file__).resolve().parents[4]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))
CANONICAL_CONFIG = ROOT / "config" / "machine" / "machine_config.ini"

from runtime_gateway_harness import build_process_env as _shared_build_process_env  # noqa: E402
from runtime_gateway_harness import resolve_default_exe  # noqa: E402

KNOWN_FAILURE_PATTERNS = (
    "IDiagnosticsPort 未注册",
    "IDiagnosticsPort not registered",
    "diagnostics port not registered",
    "Service not registered",
    "SERVICE_NOT_REGISTERED",
)


@dataclass
class ChainResult:
    check_id: str
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

def _build_process_env(gateway_exe: Path) -> dict[str, str]:
    return _shared_build_process_env(gateway_exe)


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


def _compute_overall_status(results: list[ChainResult]) -> str:
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
    json_path = report_dir / "tcp-estop-chain.json"
    md_path = report_dir / "tcp-estop-chain.md"

    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# TCP EStop Chain",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
        f"- host: `{report['host']}`",
        f"- port: `{report['port']}`",
        "",
        "## Chain Results",
        "",
    ]
    for item in report["results"]:
        lines.append(f"- `{item['status']}` `{item['check_id']}` expected=`{item['expected']}` actual=`{item['actual']}`")
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
    parser = argparse.ArgumentParser(description="Run S8 e-stop chain probe and write a report.")
    parser.add_argument("--report-dir", default=str(ROOT / "tests" / "reports" / "first-layer" / "s8-estop-chain"))
    parser.add_argument("--host", default=os.getenv("SILIGEN_HIL_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=None)
    parser.add_argument(
        "--gateway-exe",
        default=os.getenv(
            "SILIGEN_HIL_GATEWAY_EXE",
            str(resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe")),
        ),
    )
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    return parser.parse_args()


def _is_estop_state(status_response: dict[str, Any]) -> tuple[bool, str]:
    result = status_response.get("result", {})
    if not isinstance(result, dict):
        return False, "result missing"

    io_payload = result.get("io", {})
    if isinstance(io_payload, dict):
        estop_flag = io_payload.get("estop")
        if isinstance(estop_flag, bool):
            return estop_flag, f"io.estop={estop_flag}"

    machine_state = result.get("machine_state")
    if isinstance(machine_state, str):
        normalized = machine_state.strip().upper()
        if "ESTOP" in normalized or "EMERGENCY" in normalized:
            return True, f"machine_state={machine_state}"
        return False, f"machine_state={machine_state}"

    return False, "no estop signal in status payload"


def _is_disconnect_ack(disconnect_response: dict[str, Any]) -> tuple[bool, str]:
    result = disconnect_response.get("result", {})
    if not isinstance(result, dict):
        return False, "result missing"

    disconnected = result.get("disconnected")
    if isinstance(disconnected, bool):
        return disconnected, f"result.disconnected={disconnected}"
    return False, "result.disconnected missing"


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    gateway_exe = Path(args.gateway_exe)
    port, port_is_requested = _resolve_runtime_port(args.host, args.port)
    results: list[ChainResult] = []
    temp_config_dir = tempfile.TemporaryDirectory(prefix="tcp-estop-chain-")
    temp_config = _prepare_mock_config(Path(temp_config_dir.name), CANONICAL_CONFIG)

    if not gateway_exe.exists():
        status = "skipped" if args.allow_skip_on_missing_gateway else "known_failure"
        results.append(
            ChainResult(
                check_id="S8-bootstrap",
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
            "port": port,
            "overall_status": _compute_overall_status(results),
            "results": [asdict(item) for item in results],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp estop chain complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        if status == "skipped":
            return SKIPPED_EXIT_CODE
        return KNOWN_FAILURE_EXIT_CODE

    if port_is_requested and _is_tcp_endpoint_reachable(args.host, port):
        results.append(
            ChainResult(
                check_id="S8-bootstrap",
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
            "host": args.host,
            "port": port,
            "overall_status": _compute_overall_status(results),
            "results": [asdict(item) for item in results],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp estop chain complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return 1

    process = subprocess.Popen(
        [str(gateway_exe), "--config", str(temp_config), "--port", str(port)],
        cwd=str(ROOT),
        env=_build_process_env(gateway_exe),
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
                ChainResult(
                    check_id="S8-bootstrap",
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
                "host": args.host,
                "port": port,
                "overall_status": _compute_overall_status(results),
                "results": [asdict(item) for item in results],
            }
            json_path, md_path = _write_report(report_dir, report)
            print(f"tcp estop chain complete: status={report['overall_status']}")
            print(f"json report: {json_path}")
            print(f"markdown report: {md_path}")
            if ready_status == "known_failure":
                return KNOWN_FAILURE_EXIT_CODE
            return 1

        client.connect(timeout_seconds=3.0)

        connect_response = client.send_request("connect", {}, timeout_seconds=5.0)
        if "error" in connect_response:
            results.append(
                ChainResult(
                    check_id="S8-connect",
                    status="failed",
                    expected="connect success",
                    actual=_truncate_json(connect_response),
                    note="connect failed",
                    response=_truncate_json(connect_response),
                )
            )
        else:
            results.append(
                ChainResult(
                    check_id="S8-connect",
                    status="passed",
                    expected="connect success",
                    actual="success",
                    response=_truncate_json(connect_response),
                )
            )

            baseline_status = client.send_request("status", {}, timeout_seconds=8.0)
            baseline_estop, baseline_desc = _is_estop_state(baseline_status)

            estop_response = client.send_request("estop", {}, timeout_seconds=8.0)
            if "error" in estop_response:
                results.append(
                    ChainResult(
                        check_id="S8-trigger-estop",
                        status="failed",
                        expected="estop success",
                        actual=_truncate_json(estop_response),
                        note="estop command returned error",
                        response=_truncate_json(estop_response),
                    )
                )
            else:
                results.append(
                    ChainResult(
                        check_id="S8-trigger-estop",
                        status="passed",
                        expected="estop success",
                        actual="success",
                        response=_truncate_json(estop_response),
                    )
                )

                status_after = client.send_request("status", {}, timeout_seconds=8.0)
                estop_asserted, estop_desc = _is_estop_state(status_after)
                if estop_asserted:
                    note = f"baseline={baseline_desc}; after_estop={estop_desc}"
                    if baseline_estop:
                        note = "baseline already estop-active; " + note
                    results.append(
                        ChainResult(
                            check_id="S8-estop-lock-visible",
                            status="passed",
                            expected="status reflects estop lock",
                            actual=estop_desc,
                            note=note,
                            response=_truncate_json(status_after),
                        )
                    )
                else:
                    results.append(
                        ChainResult(
                            check_id="S8-estop-lock-visible",
                            status="failed",
                            expected="status reflects estop lock",
                            actual=estop_desc,
                            note="status does not expose estop lock after estop command",
                            response=_truncate_json(status_after),
                        )
                    )

                reset_response = client.send_request("estop.reset", {}, timeout_seconds=8.0)
                if "error" in reset_response:
                    results.append(
                        ChainResult(
                            check_id="S8-reset-estop",
                            status="failed",
                            expected="estop.reset success",
                            actual=_truncate_json(reset_response),
                            note="estop.reset returned error",
                            response=_truncate_json(reset_response),
                        )
                    )
                else:
                    reset_flag = bool(reset_response.get("result", {}).get("reset", False))
                    results.append(
                        ChainResult(
                            check_id="S8-reset-estop",
                            status="passed" if reset_flag else "failed",
                            expected="estop.reset success",
                            actual=f"reset={reset_flag}",
                            response=_truncate_json(reset_response),
                        )
                    )

                    status_after_reset = client.send_request("status", {}, timeout_seconds=8.0)
                    estop_cleared, reset_desc = _is_estop_state(status_after_reset)
                    results.append(
                        ChainResult(
                            check_id="S8-estop-clear-visible",
                            status="passed" if not estop_cleared else "failed",
                            expected="status clears estop after estop.reset",
                            actual=reset_desc,
                            response=_truncate_json(status_after_reset),
                        )
                    )

            disconnect_response = client.send_request("disconnect", {}, timeout_seconds=5.0)
            if "error" in disconnect_response:
                results.append(
                    ChainResult(
                        check_id="S8-terminate-path",
                        status="failed",
                        expected="disconnect success",
                        actual=_truncate_json(disconnect_response),
                        note="disconnect failed after estop flow",
                        response=_truncate_json(disconnect_response),
                    )
                )
            else:
                disconnected, disconnect_desc = _is_disconnect_ack(disconnect_response)
                if disconnected:
                    results.append(
                        ChainResult(
                            check_id="S8-terminate-path",
                            status="passed",
                            expected="disconnect success with disconnected=true",
                            actual=disconnect_desc,
                            response=_truncate_json(disconnect_response),
                        )
                    )
                else:
                    results.append(
                        ChainResult(
                            check_id="S8-terminate-path",
                            status="failed",
                            expected="disconnect success with disconnected=true",
                            actual=disconnect_desc,
                            note="disconnect response did not confirm disconnected=true",
                            response=_truncate_json(disconnect_response),
                        )
                    )

    except Exception as exc:  # pragma: no cover - runtime path
        text = str(exc)
        status = "known_failure" if _matches_known_failure(text) else "failed"
        results.append(
            ChainResult(
                check_id="S8-runtime",
                status=status,
                expected="estop chain execution completed",
                actual=text,
                note="unhandled exception during estop chain",
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
        temp_config_dir.cleanup()

    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "gateway_exe": str(gateway_exe),
        "host": args.host,
        "port": port,
        "overall_status": _compute_overall_status(results),
        "results": [asdict(item) for item in results],
    }
    json_path, md_path = _write_report(report_dir, report)
    print(f"tcp estop chain complete: status={report['overall_status']}")
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
