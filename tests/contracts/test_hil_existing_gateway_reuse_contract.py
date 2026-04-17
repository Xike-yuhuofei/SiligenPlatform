from __future__ import annotations

import importlib.util
import socket
import subprocess
import sys
import threading
import uuid
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
PYTHON_HIL_RUNNERS = (
    HIL_DIR / "run_hardware_smoke.py",
    HIL_DIR / "run_hil_closed_loop.py",
    HIL_DIR / "run_case_matrix.py",
)
HIL_CONTROLLED_RUNNER = HIL_DIR / "run_hil_controlled_test.ps1"


def _load_module(script_path: Path):
    module_name = f"codex_{script_path.stem}_{uuid.uuid4().hex}"
    if str(script_path.parent) not in sys.path:
        sys.path.insert(0, str(script_path.parent))
    spec = importlib.util.spec_from_file_location(module_name, script_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    finally:
        sys.modules.pop(module_name, None)
    return module


def _start_accepting_socket_server() -> tuple[socket.socket, threading.Event, threading.Thread]:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen()
    listener.settimeout(0.1)

    stop_event = threading.Event()

    def _serve() -> None:
        while not stop_event.is_set():
            try:
                conn, _ = listener.accept()
            except (OSError, TimeoutError):
                continue
            with conn:
                pass

    thread = threading.Thread(target=_serve, daemon=True)
    thread.start()
    return listener, stop_event, thread


@pytest.mark.parametrize("script_path", PYTHON_HIL_RUNNERS)
def test_python_hil_runners_expose_reuse_existing_gateway_flag(script_path: Path) -> None:
    completed = subprocess.run(
        [sys.executable, str(script_path), "--help"],
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    assert completed.returncode == 0, completed.stderr
    assert "--reuse-existing-gateway" in completed.stdout


def test_wait_gateway_ready_accepts_existing_endpoint_without_spawned_process() -> None:
    module = _load_module(HIL_DIR / "run_hil_closed_loop.py")
    listener, stop_event, thread = _start_accepting_socket_server()
    host, port = listener.getsockname()
    try:
        status, note = module._wait_gateway_ready(None, host, port, timeout_seconds=1.0)
    finally:
        stop_event.set()
        listener.close()
        thread.join(timeout=1.0)

    assert status == "passed"
    assert "reachable" in note.lower()


def test_hil_controlled_runner_forwards_reuse_existing_gateway_to_child_runners() -> None:
    script_text = HIL_CONTROLLED_RUNNER.read_text(encoding="utf-8")
    assert "[switch]$ReuseExistingGateway" in script_text
    assert 'Write-Output "  reuse_existing_gateway=$([bool]$ReuseExistingGateway)"' in script_text
    assert '$hardwareSmokeArgs += "--reuse-existing-gateway"' in script_text
    assert '$hilClosedLoopArgs += "--reuse-existing-gateway"' in script_text
    assert '$hilCaseMatrixArgs += "--reuse-existing-gateway"' in script_text


def test_hil_controlled_runner_supports_reusing_existing_offline_prereq_report() -> None:
    script_text = HIL_CONTROLLED_RUNNER.read_text(encoding="utf-8")
    assert '[string]$OfflinePrereqReport = ""' in script_text
    assert 'Assert-PathArgumentNotBooleanLiteral -ArgumentName "OfflinePrereqReport"' in script_text
    assert 'Write-Output "  offline_prereq_report=$resolvedOfflinePrereqReport"' in script_text
    assert 'Write-Output "hil controlled test: reuse offline prerequisites report"' in script_text
    assert 'Copy-PathIfPresent -Source $resolvedOfflinePrereqReport -Destination $offlinePrereqJsonPath' in script_text
