from __future__ import annotations

import configparser
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11
RECIPE_CONTEXT_SOURCE_CLI_EXPLICIT = "cli_explicit"

ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.control_apps_build import preferred_control_apps_build_root, valid_control_apps_build_roots

CANONICAL_CONFIG = ROOT / "config" / "machine" / "machine_config.ini"
CONTROL_APPS_BUILD_ROOT = Path(
    preferred_control_apps_build_root(
        ROOT,
        explicit_build_root=os.getenv("SILIGEN_CONTROL_APPS_BUILD_ROOT"),
    ).root
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


@dataclass
class TcpAdmissionResult:
    status: str
    note: str
    connect_params: dict[str, str]
    connect_response: dict[str, Any] | None = None
    status_response: dict[str, Any] | None = None


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


def resolve_default_exe(*file_names: str) -> Path:
    valid_roots = valid_control_apps_build_roots(
        ROOT,
        explicit_build_root=os.getenv("SILIGEN_CONTROL_APPS_BUILD_ROOT"),
    )
    prioritized_roots: list[Path] = []
    seen_roots: set[Path] = set()
    for root in (CONTROL_APPS_BUILD_ROOT, *valid_roots, ROOT / "build" / "hmi-home-fix"):
        resolved_root = root.resolve()
        if resolved_root in seen_roots:
            continue
        seen_roots.add(resolved_root)
        prioritized_roots.append(resolved_root)

    candidates: list[Path] = []
    for file_name in file_names:
        for build_root in prioritized_roots:
            candidates.extend(
                (
                    build_root / "bin" / file_name,
                    build_root / "bin" / "Debug" / file_name,
                    build_root / "bin" / "Release" / file_name,
                    build_root / "bin" / "RelWithDebInfo" / file_name,
                )
            )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def build_process_env(gateway_exe: Path) -> dict[str, str]:
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


def matches_known_failure(text: str) -> bool:
    lowered = text.lower()
    return any(pattern.lower() in lowered for pattern in KNOWN_FAILURE_PATTERNS)


def truncate_json(payload: dict[str, Any] | None, max_chars: int = 1600) -> str:
    if not payload:
        return ""
    text = json.dumps(payload, ensure_ascii=True)
    if len(text) <= max_chars:
        return text
    return text[:max_chars] + "...[truncated]"


def load_connection_params(config_path: Path) -> dict[str, str]:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    with config_path.open("r", encoding="utf-8") as handle:
        parser.read_file(handle)

    card_ip = parser.get("Network", "control_card_ip", fallback=parser.get("Network", "card_ip", fallback="")).strip()
    local_ip = parser.get("Network", "local_ip", fallback="").strip()

    params: dict[str, str] = {}
    if card_ip:
        params["card_ip"] = card_ip
    if local_ip:
        params["local_ip"] = local_ip
    return params


def require_explicit_recipe_context(recipe_id: str, version_id: str) -> tuple[str, str]:
    normalized_recipe_id = str(recipe_id or "").strip()
    normalized_version_id = str(version_id or "").strip()
    missing: list[str] = []
    if not normalized_recipe_id:
        missing.append("recipe_id")
    if not normalized_version_id:
        missing.append("version_id")
    if missing:
        raise ValueError("missing explicit recipe context: " + ",".join(missing))
    return normalized_recipe_id, normalized_version_id


def recipe_context_metadata(
    recipe_id: str,
    version_id: str,
    *,
    source: str = RECIPE_CONTEXT_SOURCE_CLI_EXPLICIT,
) -> dict[str, str]:
    normalized_recipe_id, normalized_version_id = require_explicit_recipe_context(recipe_id, version_id)
    return {
        "recipe_id": normalized_recipe_id,
        "version_id": normalized_version_id,
        "recipe_context_source": str(source or RECIPE_CONTEXT_SOURCE_CLI_EXPLICIT).strip()
        or RECIPE_CONTEXT_SOURCE_CLI_EXPLICIT,
    }


def result_payload(payload: dict[str, Any] | None) -> dict[str, Any]:
    if not isinstance(payload, dict):
        return {}
    result = payload.get("result", {})
    return result if isinstance(result, dict) else {}


def ensure_published_recipe_version(
    client: Any,
    *,
    recipe_id: str,
    version_id: str,
    timeout_seconds: float = 10.0,
) -> dict[str, Any]:
    metadata = recipe_context_metadata(recipe_id, version_id)
    versions_response = client.send_request(
        "recipe.versions",
        {"recipeId": metadata["recipe_id"]},
        timeout_seconds=timeout_seconds,
    )
    if "error" in versions_response:
        raise RuntimeError("recipe.versions failed: " + truncate_json(versions_response))

    versions_payload = result_payload(versions_response).get("versions", [])
    if not isinstance(versions_payload, list):
        raise RuntimeError("recipe.versions response missing versions list")

    matched_version: dict[str, Any] | None = None
    for item in versions_payload:
        if not isinstance(item, dict):
            continue
        if str(item.get("id", "")).strip() != metadata["version_id"]:
            continue
        matched_version = item
        break

    if matched_version is None:
        raise RuntimeError(
            "recipe.versions missing explicit version: "
            f"recipe_id={metadata['recipe_id']} version_id={metadata['version_id']}"
        )

    matched_recipe_id = str(matched_version.get("recipeId", "")).strip()
    if matched_recipe_id and matched_recipe_id != metadata["recipe_id"]:
        raise RuntimeError(
            "recipe.versions returned mismatched recipe: "
            f"expected={metadata['recipe_id']} actual={matched_recipe_id}"
        )

    version_status = str(matched_version.get("status", "")).strip().lower()
    if version_status != "published":
        raise RuntimeError(
            "recipe version is not published: "
            f"recipe_id={metadata['recipe_id']} version_id={metadata['version_id']} status={version_status or 'unknown'}"
        )

    return {
        **metadata,
        "version_status": version_status,
    }


def tcp_connect_and_ensure_ready(
    client: Any,
    *,
    config_path: Path,
    connect_timeout_seconds: float,
    status_timeout_seconds: float = 5.0,
    ready_timeout_seconds: float = 3.0,
    poll_interval_seconds: float = 0.2,
) -> TcpAdmissionResult:
    connect_params = load_connection_params(config_path)

    try:
        connect_response = client.send_request("connect", connect_params, timeout_seconds=connect_timeout_seconds)
    except Exception as exc:  # pragma: no cover - runtime path
        text = str(exc)
        status = "known_failure" if matches_known_failure(text) else "failed"
        return TcpAdmissionResult(
            status=status,
            note=f"connect request exception: {text}",
            connect_params=connect_params,
        )

    if "error" in connect_response:
        message = str(connect_response.get("error", {}).get("message", "unknown tcp error"))
        combined = f"{message}\n{truncate_json(connect_response)}"
        status = "known_failure" if matches_known_failure(combined) else "failed"
        return TcpAdmissionResult(
            status=status,
            note=f"connect returned error: {message}",
            connect_params=connect_params,
            connect_response=connect_response,
        )

    connect_result = result_payload(connect_response)
    if not bool(connect_result.get("connected", False)):
        return TcpAdmissionResult(
            status="failed",
            note="connect completed but result.connected=false",
            connect_params=connect_params,
            connect_response=connect_response,
        )

    last_status_response: dict[str, Any] | None = None
    deadline = time.perf_counter() + max(ready_timeout_seconds, poll_interval_seconds)
    while time.perf_counter() < deadline:
        try:
            last_status_response = client.send_request("status", {}, timeout_seconds=status_timeout_seconds)
        except Exception as exc:  # pragma: no cover - runtime path
            text = str(exc)
            status = "known_failure" if matches_known_failure(text) else "failed"
            return TcpAdmissionResult(
                status=status,
                note=f"status request exception after connect: {text}",
                connect_params=connect_params,
                connect_response=connect_response,
            )

        if last_status_response is not None and "error" in last_status_response:
            message = str(last_status_response.get("error", {}).get("message", "unknown tcp error"))
            combined = f"{message}\n{truncate_json(last_status_response)}"
            status = "known_failure" if matches_known_failure(combined) else "failed"
            return TcpAdmissionResult(
                status=status,
                note=f"status returned error after connect: {message}",
                connect_params=connect_params,
                connect_response=connect_response,
                status_response=last_status_response,
            )

        if bool(result_payload(last_status_response).get("connected", False)):
            return TcpAdmissionResult(
                status="passed",
                note="connect completed and status.connected=true",
                connect_params=connect_params,
                connect_response=connect_response,
                status_response=last_status_response,
            )
        time.sleep(max(0.05, poll_interval_seconds))

    return TcpAdmissionResult(
        status="failed",
        note="status.connected did not become true after connect",
        connect_params=connect_params,
        connect_response=connect_response,
        status_response=last_status_response,
    )


def read_process_output(process: subprocess.Popen[str]) -> tuple[str, str]:
    try:
        stdout, stderr = process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate(timeout=5)
    return stdout or "", stderr or ""


def rewrite_mock_config(source_text: str, door_input: int | None = None) -> str:
    lines = source_text.splitlines()
    in_hardware = False
    in_interlock = False
    mode_replaced = False
    door_replaced = door_input is None

    for index, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            section = stripped.lower()
            in_hardware = section == "[hardware]"
            in_interlock = section == "[interlock]"
            continue

        if in_hardware and stripped.lower().startswith("mode="):
            indent = line[: len(line) - len(line.lstrip())]
            lines[index] = f"{indent}mode=Mock"
            mode_replaced = True
            continue

        if door_input is not None and in_interlock and stripped.lower().startswith("safety_door_input="):
            indent = line[: len(line) - len(line.lstrip())]
            lines[index] = f"{indent}safety_door_input={door_input}"
            door_replaced = True

    if not mode_replaced:
        raise ValueError("failed to locate [Hardware] mode entry in machine config")
    if not door_replaced:
        raise ValueError("failed to locate [Interlock] safety_door_input entry in machine config")
    return "\n".join(lines) + "\n"


def prepare_mock_config(prefix: str, door_input: int | None = None) -> tuple[tempfile.TemporaryDirectory[str], Path]:
    temp_dir = tempfile.TemporaryDirectory(prefix=prefix)
    rewritten = rewrite_mock_config(CANONICAL_CONFIG.read_text(encoding="utf-8"), door_input=door_input)
    target = Path(temp_dir.name) / "machine_config.mock.ini"
    target.write_text(rewritten, encoding="utf-8")
    return temp_dir, target


def wait_gateway_ready(process: subprocess.Popen[str], host: str, port: int, timeout_seconds: float) -> tuple[str, str]:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        if process.poll() is not None:
            stdout, stderr = read_process_output(process)
            combined = "\n".join(item for item in (stdout, stderr) if item)
            if matches_known_failure(combined):
                return "known_failure", combined or "gateway exited with known failure pattern"
            return "failed", combined or "gateway exited before TCP endpoint became ready"
        try:
            with socket.create_connection((host, port), timeout=1.0):
                return "passed", "TCP endpoint is reachable"
        except OSError:
            time.sleep(0.2)
    return "failed", f"gateway readiness timeout: {host}:{port}"


def extract_axes(status_response: dict[str, Any]) -> dict[str, dict[str, Any]]:
    result = status_response.get("result", {})
    axes_payload = result.get("axes", {})
    if not isinstance(axes_payload, dict):
        return {}
    output: dict[str, dict[str, Any]] = {}
    for axis_name, axis_payload in axes_payload.items():
        if isinstance(axis_payload, dict):
            output[str(axis_name)] = axis_payload
    return output


def extract_position(status_response: dict[str, Any]) -> tuple[float, float]:
    result = status_response.get("result", {})
    position = result.get("position", {})
    if not isinstance(position, dict):
        return 0.0, 0.0
    return float(position.get("x", 0.0)), float(position.get("y", 0.0))


def extract_io(status_response: dict[str, Any]) -> dict[str, Any]:
    result = status_response.get("result", {})
    io_payload = result.get("io", {})
    if not isinstance(io_payload, dict):
        return {}
    return io_payload


def extract_effective_interlocks(status_response: dict[str, Any]) -> dict[str, Any]:
    result = status_response.get("result", {})
    interlocks_payload = result.get("effective_interlocks", {})
    if not isinstance(interlocks_payload, dict):
        return {}
    return interlocks_payload


def extract_supervision(status_response: dict[str, Any]) -> dict[str, Any]:
    result = status_response.get("result", {})
    supervision_payload = result.get("supervision", {})
    if not isinstance(supervision_payload, dict):
        return {}
    return supervision_payload


def extract_supply_open(status_response: dict[str, Any]) -> bool:
    result = status_response.get("result", {})
    dispenser = result.get("dispenser", {})
    if not isinstance(dispenser, dict):
        return False
    return bool(dispenser.get("supply_open", False))


def all_axes_homed_consistently(status_response: dict[str, Any]) -> tuple[bool, list[str], list[str]]:
    axes = extract_axes(status_response)
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


def home_succeeded(home_response: dict[str, Any]) -> bool:
    if "error" in home_response:
        return False
    result = home_response.get("result", {})
    if not isinstance(result, dict):
        return False
    return bool(result.get("completed", False))


def disconnect_ack(disconnect_response: dict[str, Any]) -> tuple[bool, str]:
    result = disconnect_response.get("result", {})
    if not isinstance(result, dict):
        return False, "result missing"
    disconnected = result.get("disconnected")
    if isinstance(disconnected, bool):
        return disconnected, f"result.disconnected={disconnected}"
    return False, "result.disconnected missing"


def compute_overall_status(results: list[CheckResult]) -> str:
    statuses = {item.status for item in results}
    if "failed" in statuses:
        return "failed"
    if "known_failure" in statuses:
        return "known_failure"
    if statuses == {"skipped"}:
        return "skipped"
    return "passed"
