"""Backend Process Manager - manages the real control-core TCP server lifecycle."""
import atexit
import os
import socket
import subprocess
import time
from pathlib import Path
from typing import Optional, Tuple


_HMI_PROJECT_ROOT = Path(__file__).resolve().parents[3]
_DEFAULT_CONTROL_CORE_ROOT = _HMI_PROJECT_ROOT.parent / "control-core"


def _resolve_control_core_root() -> Path:
    """Resolve the control-core project root.

    By default the HMI and control-core repos live side by side under the same
    workspace root. An env override keeps the path flexible for other setups.
    """
    override = os.getenv("SILIGEN_CONTROL_CORE_ROOT", "").strip()
    if override:
        return Path(override).expanduser().resolve()
    return _DEFAULT_CONTROL_CORE_ROOT


_CONTROL_CORE_ROOT = _resolve_control_core_root()


def _resolve_default_exe_path() -> Path:
    """Resolve control-core TCP server path across common build layouts."""
    override = os.getenv("SILIGEN_TCP_SERVER_EXE", "").strip()
    if override:
        return Path(override).expanduser().resolve()

    build_bin = _CONTROL_CORE_ROOT / "build" / "bin"
    candidates = [
        build_bin / "siligen_tcp_server.exe",
        build_bin / "Release" / "siligen_tcp_server.exe",
        build_bin / "RelWithDebInfo" / "siligen_tcp_server.exe",
        build_bin / "Debug" / "siligen_tcp_server.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _resolve_runtime_dll_dirs(exe_path: Path) -> list[Path]:
    """Collect common directories that may contain vendor/runtime DLLs."""
    candidates = [
        exe_path.parent,
        _CONTROL_CORE_ROOT / "build" / "bin",
        _CONTROL_CORE_ROOT / "build" / "bin" / "Debug",
        _CONTROL_CORE_ROOT / "build" / "bin" / "Release",
        _CONTROL_CORE_ROOT / "build" / "bin" / "RelWithDebInfo",
        _CONTROL_CORE_ROOT / "build" / "lib" / "Debug",
        _CONTROL_CORE_ROOT / "build" / "lib" / "Release",
        _CONTROL_CORE_ROOT / "build" / "lib" / "RelWithDebInfo",
        _CONTROL_CORE_ROOT / "src" / "infrastructure" / "drivers" / "multicard",
        _CONTROL_CORE_ROOT / "third_party" / "vendor" / "MultiCardDemoVC" / "bin",
    ]

    resolved: list[Path] = []
    seen: set[str] = set()
    for candidate in candidates:
        key = str(candidate).lower()
        if key in seen or not candidate.exists():
            continue
        seen.add(key)
        resolved.append(candidate)
    return resolved


class BackendManager:
    """Manages the lifecycle of the real control-core TCP server process."""

    DEFAULT_EXE_PATH = str(_resolve_default_exe_path())
    DEFAULT_HOST = os.getenv("SILIGEN_TCP_SERVER_HOST", "127.0.0.1")
    DEFAULT_PORT = int(os.getenv("SILIGEN_TCP_SERVER_PORT", "9527"))
    STARTUP_TIMEOUT = 5.0  # seconds
    HEALTH_CHECK_INTERVAL = 0.2  # seconds

    def __init__(
        self,
        exe_path: str = DEFAULT_EXE_PATH,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_PORT
    ):
        self._exe_path = Path(exe_path)
        self._host = host
        self._port = port
        self._process: Optional[subprocess.Popen] = None

        # Register cleanup on exit
        atexit.register(self.stop)

    @property
    def is_running(self) -> bool:
        """Check if the backend process is running."""
        return self._process is not None and self._process.poll() is None

    @property
    def exe_path(self) -> Path:
        """Get the executable path."""
        return self._exe_path

    @property
    def host(self) -> str:
        """Get the host address."""
        return self._host

    @property
    def port(self) -> int:
        """Get the port number."""
        return self._port

    def start(self) -> Tuple[bool, str]:
        """
        Start the backend process.

        Returns:
            Tuple of (success, message)
        """
        # Check if executable exists
        if not self._exe_path.exists():
            return False, f"Backend executable not found: {self._exe_path}"

        # Check if port is already in use (another instance may be running)
        if self._is_port_in_use():
            return True, "Backend already running"

        # Start the real control-core TCP server with its own repo as cwd so
        # relative config/log paths keep working.
        try:
            env = os.environ.copy()
            dll_dirs = [str(path) for path in _resolve_runtime_dll_dirs(self._exe_path)]
            existing_path = env.get("PATH", "")
            env["PATH"] = os.pathsep.join(dll_dirs + ([existing_path] if existing_path else []))
            self._process = subprocess.Popen(
                [str(self._exe_path)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=str(_CONTROL_CORE_ROOT),
                env=env,
                creationflags=subprocess.CREATE_NO_WINDOW  # Windows: no console window
            )
        except FileNotFoundError:
            return False, f"Backend executable not found: {self._exe_path}"
        except PermissionError:
            return False, f"Permission denied: {self._exe_path}"
        except Exception as e:
            return False, f"Failed to start backend: {e}"

        return True, "Backend process started"

    def wait_ready(self, timeout: float = STARTUP_TIMEOUT) -> Tuple[bool, str]:
        """
        Wait for the backend to be ready (port connectable).

        Args:
            timeout: Timeout in seconds

        Returns:
            Tuple of (success, message)
        """
        start_time = time.time()

        while time.time() - start_time < timeout:
            # Check if process exited abnormally
            if self._process and self._process.poll() is not None:
                return False, f"Backend process exited with code: {self._process.returncode}"

            # Try to connect to the port
            if self._is_port_in_use():
                return True, "Backend ready"

            time.sleep(self.HEALTH_CHECK_INTERVAL)

        return False, f"Backend startup timeout ({timeout}s)"

    def stop(self) -> None:
        """Stop the backend process."""
        if self._process:
            try:
                self._process.terminate()
                self._process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                self._process.kill()
                try:
                    self._process.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    pass
            except Exception:
                pass
            finally:
                self._process = None

    def _is_port_in_use(self) -> bool:
        """Check if the port is in use."""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(0.5)
                s.connect((self._host, self._port))
                return True
        except (socket.timeout, ConnectionRefusedError, OSError):
            return False
