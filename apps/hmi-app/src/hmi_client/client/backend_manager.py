"""Gateway process manager backed by an explicit launch contract."""
import atexit
import os
import socket
import subprocess
import time
from pathlib import Path
from typing import Optional, Tuple

from .gateway_launch import GatewayLaunchSpec, load_gateway_launch_spec


class BackendManager:
    """Manage the lifecycle of a transport-gateway executable."""

    DEFAULT_HOST = os.getenv("SILIGEN_TCP_SERVER_HOST", "127.0.0.1")
    DEFAULT_PORT = int(os.getenv("SILIGEN_TCP_SERVER_PORT", "9527"))
    AUTO_START_ENV = "SILIGEN_GATEWAY_AUTOSTART"
    STARTUP_TIMEOUT = 5.0  # seconds
    HEALTH_CHECK_INTERVAL = 0.2  # seconds

    def __init__(
        self,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_PORT,
        launch_spec: GatewayLaunchSpec | None = None,
    ):
        self._host = host
        self._port = port
        self._launch_spec = launch_spec if launch_spec is not None else load_gateway_launch_spec()
        self._process: Optional[subprocess.Popen] = None
        auto_start = os.getenv(self.AUTO_START_ENV, "1").strip().lower()
        self._auto_start_enabled = auto_start not in ("0", "false", "no", "off")

        # Register cleanup on exit
        atexit.register(self.stop)

    @property
    def is_running(self) -> bool:
        """Check if the backend process is running."""
        return self._process is not None and self._process.poll() is None

    @property
    def exe_path(self) -> Path:
        """Get the executable path."""
        if self._launch_spec is None:
            return Path()
        return self._launch_spec.executable

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
        # Check if port is already in use (another instance may be running)
        if self._is_port_in_use():
            return True, "Backend already running"

        if not self._auto_start_enabled:
            return False, "Gateway auto-start 已禁用，请先手动启动 transport-gateway。"

        if self._launch_spec is None:
            return (
                False,
                "未配置 gateway 启动契约。请设置 SILIGEN_GATEWAY_EXE 或 "
                "SILIGEN_GATEWAY_LAUNCH_SPEC。",
            )

        if not self._launch_spec.executable.exists():
            return False, f"Gateway executable not found: {self._launch_spec.executable}"

        try:
            env = os.environ.copy()
            env.update(self._launch_spec.env)
            path_entries = [
                str(path)
                for path in self._launch_spec.path_entries
                if path.exists()
            ]
            existing_path = env.get("PATH", "").strip()
            if path_entries:
                env["PATH"] = os.pathsep.join(
                    path_entries + ([existing_path] if existing_path else [])
                )
            self._process = subprocess.Popen(
                self._launch_spec.command(),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                cwd=str(self._launch_spec.cwd or self._launch_spec.executable.parent),
                env=env,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
        except FileNotFoundError:
            return False, f"Gateway executable not found: {self._launch_spec.executable}"
        except PermissionError:
            return False, f"Permission denied: {self._launch_spec.executable}"
        except Exception as e:
            return False, f"Failed to start gateway: {e}"

        return True, "Gateway process started"

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
