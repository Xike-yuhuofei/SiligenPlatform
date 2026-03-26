"""Gateway process manager backed by an explicit launch contract."""
import atexit
import os
import socket
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Tuple

from .gateway_launch import GatewayLaunchSpec, load_gateway_launch_spec


@dataclass(frozen=True)
class BackendStepResult:
    ok: bool
    message: str
    failure_code: str | None = None
    recoverable: bool = True


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

    def start_detailed(self) -> BackendStepResult:
        """
        Start the backend process.

        Returns:
            Structured startup result.
        """
        # Check if port is already in use (another instance may be running)
        if self._is_port_in_use():
            return BackendStepResult(ok=True, message="Backend already running")

        if not self._auto_start_enabled:
            return BackendStepResult(
                ok=False,
                message=(
                    "Gateway auto-start 已禁用"
                    "（SILIGEN_GATEWAY_AUTOSTART=0 或使用了 -DisableGatewayAutostart），"
                    "请先手动启动 transport-gateway。"
                ),
                failure_code="SUP_BACKEND_AUTOSTART_DISABLED",
                recoverable=False,
            )

        if self._launch_spec is None:
            return BackendStepResult(
                ok=False,
                message=(
                    "未配置 gateway 启动契约。请使用 apps/hmi-app/run.ps1，或设置 "
                    "SILIGEN_GATEWAY_LAUNCH_SPEC。若只有 exe 路径，请先通过 run.ps1 -GatewayExe 生成契约。"
                ),
                failure_code="SUP_BACKEND_SPEC_MISSING",
                recoverable=False,
            )

        if not self._launch_spec.executable.exists():
            return BackendStepResult(
                ok=False,
                message=f"Gateway executable not found: {self._launch_spec.executable}",
                failure_code="SUP_BACKEND_EXE_NOT_FOUND",
                recoverable=False,
            )

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
            return BackendStepResult(
                ok=False,
                message=f"Gateway executable not found: {self._launch_spec.executable}",
                failure_code="SUP_BACKEND_EXE_NOT_FOUND",
                recoverable=False,
            )
        except PermissionError:
            return BackendStepResult(
                ok=False,
                message=f"Permission denied: {self._launch_spec.executable}",
                failure_code="SUP_BACKEND_START_FAILED",
                recoverable=True,
            )
        except Exception as e:
            return BackendStepResult(
                ok=False,
                message=f"Failed to start gateway: {e}",
                failure_code="SUP_BACKEND_START_FAILED",
                recoverable=True,
            )

        return BackendStepResult(ok=True, message="Gateway process started")

    def start(self) -> Tuple[bool, str]:
        result = self.start_detailed()
        return result.ok, result.message

    def wait_ready_detailed(self, timeout: float = STARTUP_TIMEOUT) -> BackendStepResult:
        """
        Wait for the backend to be ready (port connectable).

        Args:
            timeout: Timeout in seconds

        Returns:
            Structured readiness result.
        """
        start_time = time.time()

        while time.time() - start_time < timeout:
            # Check if process exited abnormally
            if self._process and self._process.poll() is not None:
                return BackendStepResult(
                    ok=False,
                    message=f"Backend process exited with code: {self._process.returncode}",
                    failure_code="SUP_BACKEND_EXITED",
                    recoverable=True,
                )

            # Try to connect to the port
            if self._is_port_in_use():
                return BackendStepResult(ok=True, message="Backend ready")

            time.sleep(self.HEALTH_CHECK_INTERVAL)

        return BackendStepResult(
            ok=False,
            message=f"Backend startup timeout ({timeout}s)",
            failure_code="SUP_BACKEND_READY_TIMEOUT",
            recoverable=True,
        )

    def wait_ready(self, timeout: float = STARTUP_TIMEOUT) -> Tuple[bool, str]:
        result = self.wait_ready_detailed(timeout=timeout)
        return result.ok, result.message

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
