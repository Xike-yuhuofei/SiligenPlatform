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

ROOT = Path(__file__).resolve().parents[2]
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)
DEFAULT_DXF_FILE = ROOT / "examples" / "dxf" / "rect_diag.dxf"

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


def _resolve_default_exe(file_name: str) -> Path:
    candidates = (
        CONTROL_APPS_BUILD_ROOT / "bin" / file_name,
        CONTROL_APPS_BUILD_ROOT / "bin" / "Debug" / file_name,
        CONTROL_APPS_BUILD_ROOT / "bin" / "Release" / file_name,
        CONTROL_APPS_BUILD_ROOT / "bin" / "RelWithDebInfo" / file_name,
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


def _truncate_json(payload: dict[str, Any] | None, max_chars: int = 1500) -> str:
    if not payload:
        return ""
    text = json.dumps(payload, ensure_ascii=True)
    if len(text) <= max_chars:
        return text
    return text[:max_chars] + "...[truncated]"


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
    parser.add_argument("--report-dir", default=str(ROOT / "integration" / "reports" / "first-layer" / "s2-s4-precondition"))
    parser.add_argument("--host", default=os.getenv("SILIGEN_HIL_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.getenv("SILIGEN_HIL_PORT", "9527")))
    parser.add_argument("--gateway-exe", default=os.getenv("SILIGEN_HIL_GATEWAY_EXE", str(_resolve_default_exe("siligen_tcp_server.exe"))))
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DXF_FILE)
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    parser.add_argument("--allow-skip-on-active-recipe", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    gateway_exe = Path(args.gateway_exe)
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
            "host": args.host,
            "port": args.port,
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
            "host": args.host,
            "port": args.port,
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
                "host": args.host,
                "port": args.port,
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

        # S3: 未回零时 home.go 阻断
        s3_response = client.send_request("home.go", {}, timeout_seconds=10.0)
        results.append(
            _expect_error_code(
                scenario_id="S3-home-go-without-homing",
                response=s3_response,
                expected_code=2414,
                expected_desc="error_code=2414",
            )
        )

        # S4: 未加载 DXF 时 dxf.execute 阻断
        s4_response = client.send_request(
            "dxf.execute",
            {"dispensing_speed_mm_s": 10.0, "dry_run": False},
            timeout_seconds=10.0,
        )
        results.append(
            _expect_error_code(
                scenario_id="S4-dxf-execute-without-dxf-load",
                response=s4_response,
                expected_code=3001,
                expected_desc="error_code=3001",
            )
        )

        # S2: require_active_recipe=true 且无激活配方时阻断
        load_response = client.send_request(
            "dxf.load",
            {"filepath": str(args.dxf_file)},
            timeout_seconds=30.0,
        )
        if "error" in load_response:
            results.append(
                ScenarioResult(
                    scenario_id="S2-require-active-recipe",
                    status="failed",
                    expected="dxf.load success before require_active_recipe gate",
                    actual=_truncate_json(load_response),
                    note="dxf.load failed; cannot evaluate require_active_recipe gate",
                    response=_truncate_json(load_response),
                )
            )
        else:
            s2_response = client.send_request(
                "dxf.execute",
                {
                    "dispensing_speed_mm_s": 10.0,
                    "dry_run": False,
                    "require_active_recipe": True,
                },
                timeout_seconds=15.0,
            )

            s2_result = _expect_error_code(
                scenario_id="S2-dxf-execute-require-active-recipe",
                response=s2_response,
                expected_code=3004,
                expected_desc="error_code=3004",
            )

            if s2_result.status == "failed" and s2_result.note == "response has no error payload":
                recipe_list_response = client.send_request("recipe.list", {}, timeout_seconds=15.0)
                active_count = 0
                if "error" not in recipe_list_response:
                    recipes = recipe_list_response.get("result", {}).get("recipes", [])
                    if isinstance(recipes, list):
                        active_count = sum(
                            1
                            for item in recipes
                            if isinstance(item, dict) and bool(str(item.get("activeVersionId", "")).strip())
                        )
                if active_count > 0 and args.allow_skip_on_active_recipe:
                    s2_result.status = "skipped"
                    s2_result.note = (
                        "environment already has active recipe; "
                        "require_active_recipe gate cannot be proven in this run"
                    )
                    s2_result.actual = f"active_recipes={active_count}"
                    s2_result.response = _truncate_json(s2_response)
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
        "host": args.host,
        "port": args.port,
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
