from __future__ import annotations

from typing import Any
from typing import cast

from PyQt5.QtCore import QThread, pyqtSignal

from ..contracts.launch_supervision_contract import RecoveryAction
from ..services.startup_sequence_service import run_launch_sequence, run_recovery_action


class StartupWorker(QThread):
    """Background worker thread for startup sequence."""

    progress = pyqtSignal(str, int)
    snapshot = pyqtSignal(object)
    stage_event = pyqtSignal(object)
    finished = pyqtSignal(object)

    def __init__(
        self,
        backend: Any,
        client: Any,
        protocol: Any,
        runtime_probe: Any,
        launch_mode: str = "online",
        policy: Any | None = None,
    ):
        super().__init__()
        from ..adapters.launch_supervision_env import load_supervisor_policy_from_env
        from ..domain.launch_supervision_types import normalize_launch_mode

        self._backend = backend
        self._client = client
        self._protocol = protocol
        self._runtime_probe = runtime_probe
        self._launch_mode = normalize_launch_mode(launch_mode)
        self._policy = policy if policy is not None else load_supervisor_policy_from_env()

    def run(self):
        self.finished.emit(
            run_launch_sequence(
                self._launch_mode,
                self._backend,
                self._client,
                self._protocol,
                self._runtime_probe,
                progress_callback=self.progress.emit,
                snapshot_callback=self.snapshot.emit,
                event_callback=self.stage_event.emit,
                policy=self._policy,
            )
        )


class RecoveryWorker(QThread):
    """Background worker for supervisor recovery actions."""

    progress = pyqtSignal(str, int)
    snapshot = pyqtSignal(object)
    stage_event = pyqtSignal(object)
    finished = pyqtSignal(object)

    def __init__(
        self,
        action: str,
        recovery_snapshot: Any,
        backend: Any,
        client: Any,
        protocol: Any,
        runtime_probe: Any,
        policy: Any | None = None,
    ):
        super().__init__()
        from ..adapters.launch_supervision_env import load_supervisor_policy_from_env

        self._action = action
        self._recovery_snapshot = recovery_snapshot
        self._backend = backend
        self._client = client
        self._protocol = protocol
        self._runtime_probe = runtime_probe
        self._policy = policy if policy is not None else load_supervisor_policy_from_env()

    def run(self):
        self.finished.emit(
            run_recovery_action(
                action=cast(RecoveryAction, self._action),
                snapshot=self._recovery_snapshot,
                backend=self._backend,
                client=self._client,
                protocol=self._protocol,
                runtime_probe=self._runtime_probe,
                progress_callback=self.progress.emit,
                snapshot_callback=self.snapshot.emit,
                event_callback=self.stage_event.emit,
                policy=self._policy,
            )
        )


__all__ = [
    "RecoveryWorker",
    "StartupWorker",
]
