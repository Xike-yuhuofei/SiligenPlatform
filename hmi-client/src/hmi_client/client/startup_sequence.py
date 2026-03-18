"""Startup Sequence Manager - Handles the three-stage startup process."""
from dataclasses import dataclass
from PyQt5.QtCore import QThread, pyqtSignal

from .backend_manager import BackendManager
from .tcp_client import TcpClient
from .protocol import CommandProtocol


@dataclass
class StartupResult:
    """Result of the startup sequence."""
    success: bool
    stage: str  # "backend" | "tcp" | "hardware" | "complete"
    message: str


class StartupWorker(QThread):
    """Background worker thread for startup sequence."""

    # Signals for progress updates
    progress = pyqtSignal(str, int)  # (message, percent)
    finished = pyqtSignal(object)  # StartupResult

    def __init__(
        self,
        backend: BackendManager,
        client: TcpClient,
        protocol: CommandProtocol
    ):
        super().__init__()
        self._backend = backend
        self._client = client
        self._protocol = protocol

    def run(self):
        """Execute the three-stage startup sequence."""
        # Stage 1: Start backend (0-30%)
        self.progress.emit("Starting backend...", 10)
        ok, msg = self._backend.start()
        if not ok:
            self.finished.emit(StartupResult(False, "backend", msg))
            return

        self.progress.emit("Waiting for backend...", 20)
        ok, msg = self._backend.wait_ready()
        if not ok:
            self.finished.emit(StartupResult(False, "backend", msg))
            return
        self.progress.emit("Backend ready", 30)

        # Stage 2: Connect TCP (30-60%)
        self.progress.emit("Connecting TCP...", 40)
        if not self._client.connect():
            self.finished.emit(StartupResult(False, "tcp", "TCP connection failed"))
            return
        self.progress.emit("TCP connected", 60)

        # Stage 3: Initialize hardware (60-100%)
        self.progress.emit("Initializing hardware...", 70)
        ok, msg = self._protocol.connect_hardware()
        if not ok:
            self.finished.emit(StartupResult(False, "hardware", msg))
            return

        self.progress.emit("System ready", 100)
        self.finished.emit(StartupResult(True, "complete", "Startup complete"))
