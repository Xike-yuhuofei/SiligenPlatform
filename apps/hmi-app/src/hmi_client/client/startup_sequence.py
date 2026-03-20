"""Startup Sequence Manager - Handles launch-mode-aware startup flows."""
from __future__ import annotations

import logging
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Literal
from PyQt5.QtCore import QThread, pyqtSignal

from .backend_manager import BackendManager
from .tcp_client import TcpClient
from .protocol import CommandProtocol

LaunchMode = Literal["online", "offline"]


_STARTUP_LOGGER = logging.getLogger("hmi.startup")
if not _STARTUP_LOGGER.handlers:
    log_dir = Path("logs")
    log_dir.mkdir(parents=True, exist_ok=True)
    handler = logging.FileHandler(log_dir / "hmi_startup.log", encoding="utf-8")
    formatter = logging.Formatter("%(asctime)s [%(levelname)s] %(message)s")
    handler.setFormatter(formatter)
    _STARTUP_LOGGER.addHandler(handler)
    _STARTUP_LOGGER.setLevel(logging.INFO)
    _STARTUP_LOGGER.propagate = False


def normalize_launch_mode(value: str | None) -> LaunchMode:
    mode = (value or "online").strip().lower()
    if mode not in ("online", "offline"):
        raise ValueError(f"Unsupported launch mode: {value}")
    return mode


@dataclass
class LaunchResult:
    """Unified launch outcome consumed by the UI layer."""

    requested_mode: LaunchMode
    effective_mode: LaunchMode
    phase: str  # "offline" | "backend" | "tcp" | "hardware" | "ready"
    success: bool
    backend_started: bool
    tcp_connected: bool
    hardware_ready: bool
    user_message: str

    @property
    def degraded(self) -> bool:
        return self.requested_mode != self.effective_mode


# Backward compatibility for existing imports while the rest of the app migrates.
StartupResult = LaunchResult


def run_launch_sequence(
    launch_mode: str,
    backend: BackendManager,
    client: TcpClient,
    protocol: CommandProtocol,
    progress_callback: Callable[[str, int], None] | None = None,
) -> LaunchResult:
    mode = normalize_launch_mode(launch_mode)
    _STARTUP_LOGGER.info("Launch mode requested: %s", mode)

    def emit_progress(message: str, percent: int) -> None:
        if progress_callback is not None:
            progress_callback(message, percent)

    if mode == "offline":
        emit_progress("Offline mode: startup skipped", 100)
        result = LaunchResult(
            requested_mode=mode,
            effective_mode="offline",
            phase="offline",
            success=True,
            backend_started=False,
            tcp_connected=False,
            hardware_ready=False,
            user_message="Offline mode active: startup sequence skipped.",
        )
        _STARTUP_LOGGER.info(
            "Launch mode effective: %s; phase: %s",
            result.effective_mode,
            result.phase,
        )
        return result

    emit_progress("Starting backend...", 10)
    ok, msg = backend.start()
    if not ok:
        result = LaunchResult(
            requested_mode=mode,
            effective_mode="online",
            phase="backend",
            success=False,
            backend_started=False,
            tcp_connected=False,
            hardware_ready=False,
            user_message=msg,
        )
        _STARTUP_LOGGER.info(
            "Launch mode effective: %s; phase: %s; success=%s; message=%s",
            result.effective_mode,
            result.phase,
            result.success,
            result.user_message,
        )
        return result

    emit_progress("Waiting for backend...", 20)
    ok, msg = backend.wait_ready()
    if not ok:
        result = LaunchResult(
            requested_mode=mode,
            effective_mode="online",
            phase="backend",
            success=False,
            backend_started=True,
            tcp_connected=False,
            hardware_ready=False,
            user_message=msg,
        )
        _STARTUP_LOGGER.info(
            "Launch mode effective: %s; phase: %s; success=%s; message=%s",
            result.effective_mode,
            result.phase,
            result.success,
            result.user_message,
        )
        return result

    emit_progress("Backend ready", 30)
    emit_progress("Connecting TCP...", 40)
    if not client.connect():
        result = LaunchResult(
            requested_mode=mode,
            effective_mode="online",
            phase="tcp",
            success=False,
            backend_started=True,
            tcp_connected=False,
            hardware_ready=False,
            user_message="TCP connection failed",
        )
        _STARTUP_LOGGER.info(
            "Launch mode effective: %s; phase: %s; success=%s; message=%s",
            result.effective_mode,
            result.phase,
            result.success,
            result.user_message,
        )
        return result

    emit_progress("TCP connected", 60)
    emit_progress("Initializing hardware...", 70)
    ok, msg = protocol.connect_hardware()
    if not ok:
        result = LaunchResult(
            requested_mode=mode,
            effective_mode="online",
            phase="hardware",
            success=False,
            backend_started=True,
            tcp_connected=True,
            hardware_ready=False,
            user_message=msg,
        )
        _STARTUP_LOGGER.info(
            "Launch mode effective: %s; phase: %s; success=%s; message=%s",
            result.effective_mode,
            result.phase,
            result.success,
            result.user_message,
        )
        return result

    emit_progress("System ready", 100)
    result = LaunchResult(
        requested_mode=mode,
        effective_mode="online",
        phase="ready",
        success=True,
        backend_started=True,
        tcp_connected=True,
        hardware_ready=True,
        user_message="System ready",
    )
    _STARTUP_LOGGER.info(
        "Launch mode effective: %s; phase: %s; success=%s; message=%s",
        result.effective_mode,
        result.phase,
        result.success,
        result.user_message,
    )
    return result


class StartupWorker(QThread):
    """Background worker thread for startup sequence."""

    # Signals for progress updates
    progress = pyqtSignal(str, int)  # (message, percent)
    finished = pyqtSignal(object)  # LaunchResult

    def __init__(
        self,
        backend: BackendManager,
        client: TcpClient,
        protocol: CommandProtocol,
        launch_mode: str = "online",
    ):
        super().__init__()
        self._backend = backend
        self._client = client
        self._protocol = protocol
        self._launch_mode = normalize_launch_mode(launch_mode)

    def run(self):
        """Execute the launch sequence based on the requested mode."""
        self.finished.emit(
            run_launch_sequence(
                self._launch_mode,
                self._backend,
                self._client,
                self._protocol,
                progress_callback=self.progress.emit,
            )
        )
