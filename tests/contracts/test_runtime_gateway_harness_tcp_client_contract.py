from __future__ import annotations

import importlib.util
import json
import socket
import sys
import threading
import uuid
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
HARNESS_PATH = HIL_DIR / "runtime_gateway_harness.py"


def _load_harness_module():
    module_name = f"codex_runtime_gateway_harness_{uuid.uuid4().hex}"
    if str(HIL_DIR) not in sys.path:
        sys.path.insert(0, str(HIL_DIR))
    spec = importlib.util.spec_from_file_location(module_name, HARNESS_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    finally:
        sys.modules.pop(module_name, None)
    return module


def _start_json_server(handler):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen()
    listener.settimeout(0.2)
    stop_event = threading.Event()

    def _serve() -> None:
        while not stop_event.is_set():
            try:
                conn, _ = listener.accept()
            except (OSError, TimeoutError):
                continue
            with conn:
                handler(conn)
            return

    thread = threading.Thread(target=_serve, daemon=True)
    thread.start()
    return listener, stop_event, thread


def test_tcp_json_client_preserves_out_of_order_responses_by_request_id() -> None:
    harness = _load_harness_module()
    received_request_ids: list[str] = []

    def _handler(conn: socket.socket) -> None:
        reader = conn.makefile("rb")
        first_request = json.loads(reader.readline().decode("utf-8"))
        received_request_ids.append(str(first_request["id"]))
        conn.sendall(
            (
                json.dumps({"id": "2", "result": {"request": "second"}}, ensure_ascii=True) + "\n"
                + json.dumps({"id": "1", "result": {"request": "first"}}, ensure_ascii=True) + "\n"
            ).encode("utf-8")
        )
        second_request = json.loads(reader.readline().decode("utf-8"))
        received_request_ids.append(str(second_request["id"]))

    listener, stop_event, thread = _start_json_server(_handler)
    host, port = listener.getsockname()
    client = harness.TcpJsonClient(host, port)
    try:
        client.connect(timeout_seconds=1.0)
        first_response = client.send_request("first", None, timeout_seconds=1.0)
        second_response = client.send_request("second", None, timeout_seconds=1.0)
    finally:
        client.close()
        stop_event.set()
        listener.close()
        thread.join(timeout=1.0)

    assert first_response["result"]["request"] == "first"
    assert second_response["result"]["request"] == "second"
    assert received_request_ids == ["1", "2"]


def test_tcp_json_client_timeout_error_exposes_request_metadata() -> None:
    harness = _load_harness_module()

    def _handler(conn: socket.socket) -> None:
        reader = conn.makefile("rb")
        _ = reader.readline()
        threading.Event().wait(0.5)

    listener, stop_event, thread = _start_json_server(_handler)
    host, port = listener.getsockname()
    client = harness.TcpJsonClient(host, port)
    try:
        client.connect(timeout_seconds=1.0)
        with pytest.raises(harness.TcpRequestTimeoutError) as exc_info:
            client.send_request("status", None, timeout_seconds=0.1)
    finally:
        client.close()
        stop_event.set()
        listener.close()
        thread.join(timeout=1.0)

    exc = exc_info.value
    assert exc.method == "status"
    assert exc.request_id == "1"
    assert exc.elapsed_ms >= 100.0
    assert "method=status" in str(exc)
    assert "request_id=1" in str(exc)
