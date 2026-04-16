"""TCP Client for Siligen Motion Controller."""
import socket
import threading
import json
import logging
from pathlib import Path
from typing import Any, Callable, Mapping, Optional, cast
from queue import Queue, Empty


_LOGGER = logging.getLogger("hmi.tcp_client")
if not _LOGGER.handlers:
    log_dir = Path("logs")
    log_dir.mkdir(parents=True, exist_ok=True)
    handler = logging.FileHandler(log_dir / "hmi_tcp_client.log", encoding="utf-8")
    formatter = logging.Formatter("%(asctime)s [%(levelname)s] %(message)s")
    handler.setFormatter(formatter)
    _LOGGER.addHandler(handler)
    _LOGGER.setLevel(logging.INFO)
    _LOGGER.propagate = False


JsonDict = dict[str, Any]


class TcpClient:
    """Async TCP client with callback-based message handling."""

    def __init__(self, host: str = "127.0.0.1", port: int = 9527):
        self.host = host
        self.port = port
        self._socket: Optional[socket.socket] = None
        self._recv_thread: Optional[threading.Thread] = None
        self._running = False
        self.last_connect_error: str = ""
        self._request_id = 0
        self._pending: dict[str, Queue[JsonDict]] = {}
        self._on_event: Optional[Callable[[JsonDict], None]] = None
        self._lock = threading.Lock()

    def connect(self, timeout: float = 3.0) -> bool:
        try:
            self.last_connect_error = ""
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._socket.settimeout(timeout)
            self._socket.connect((self.host, self.port))
            self._socket.settimeout(None)
            self._running = True
            self._recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
            self._recv_thread.start()
            return True
        except Exception as e:
            self.last_connect_error = str(e) or "Unknown connection error"
            print(f"Connection failed: {e}")
            return False

    def disconnect(self):
        self._running = False
        if self._socket:
            try:
                self._socket.close()
            except:
                pass
            self._socket = None
        self._fail_pending_requests("TCP disconnected")

    def is_connected(self) -> bool:
        return self._running and self._socket is not None

    def set_event_handler(self, handler: Callable[[JsonDict], None]) -> None:
        self._on_event = handler

    def send_request(self, method: str, params: Mapping[str, object] | None = None, timeout: float = 5.0) -> JsonDict:
        if not self.is_connected():
            return {"error": {"code": -1, "message": "TCP未连接"}}
        with self._lock:
            self._request_id += 1
            req_id = str(self._request_id)

        request: JsonDict = {"id": req_id, "method": method}
        if params:
            request["params"] = dict(params)

        response_queue: Queue[JsonDict] = Queue()
        with self._lock:
            self._pending[req_id] = response_queue

        try:
            msg = json.dumps(request) + "\n"
            if method in ("home", "home.go", "home.auto"):
                _LOGGER.info("TX %s id=%s payload=%s", method, req_id, msg.strip())
            sock = self._socket
            if sock is None:
                return {"error": {"code": -1, "message": "TCP未连接"}}
            sock.sendall(msg.encode("utf-8"))
            response = response_queue.get(timeout=timeout)
            if method in ("home", "home.go", "home.auto"):
                _LOGGER.info(
                    "RX %s id=%s payload=%s",
                    method,
                    req_id,
                    json.dumps(response, ensure_ascii=True)
                )
            return response
        except Empty:
            return {"error": {"code": -1, "message": f"Request timed out ({timeout}s)"}}
        except Exception as e:
            msg = str(e) or "Unknown socket error"
            return {"error": {"code": -1, "message": msg}}
        finally:
            with self._lock:
                self._pending.pop(req_id, None)

    def _recv_loop(self) -> None:
        buffer = ""
        while self._running:
            try:
                sock = self._socket
                if sock is None:
                    break
                data = sock.recv(4096)
                if not data:
                    break
                buffer += data.decode("utf-8")
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    if line.strip():
                        self._handle_message(line)
            except Exception as e:
                if self._running:
                    print(f"Recv error: {e}")
                break
        self._running = False

    def _handle_message(self, line: str) -> None:
        try:
            msg_raw = json.loads(line)
            if not isinstance(msg_raw, dict):
                return
            msg = cast(JsonDict, msg_raw)
            msg_id = msg.get("id")
            queue = None
            if msg_id:
                with self._lock:
                    queue = self._pending.get(msg_id)
            if queue is not None:
                queue.put(msg)
            elif self._on_event:
                self._on_event(msg)
        except json.JSONDecodeError as e:
            print(f"JSON parse error: {e}")

    def _fail_pending_requests(self, message: str) -> None:
        with self._lock:
            pending_queues = list(self._pending.values())
        for response_queue in pending_queues:
            try:
                response_queue.put_nowait({"error": {"code": -1, "message": message}})
            except Exception:
                pass
