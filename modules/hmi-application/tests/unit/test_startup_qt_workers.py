import unittest

from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.adapters.qt_workers import RecoveryWorker as ReexportRecoveryWorker
from hmi_application.adapters.qt_workers import StartupWorker as ReexportStartupWorker
from hmi_application.adapters.startup_qt_workers import RecoveryWorker, StartupWorker
from hmi_application.contracts.launch_supervision_contract import SessionSnapshot, snapshot_timestamp


class _FakeBackend:
    def start(self):
        return True, "started"

    def wait_ready(self, timeout: float = 5.0):
        return True, "ready"

    def stop(self):
        return None


class _FakeClient:
    def __init__(self, connect_result=True) -> None:
        self.connect_result = connect_result

    def connect(self, timeout: float = 3.0):
        return self.connect_result

    def disconnect(self):
        return None


class _FakeProtocol:
    def __init__(self, hardware_result=(True, "hardware ready")) -> None:
        self.hardware_result = hardware_result

    def connect_hardware(self, card_ip: str = "", local_ip: str = "", timeout: float = 15.0):
        return self.hardware_result


class StartupQtWorkersTest(unittest.TestCase):
    def test_qt_workers_module_reexports_split_startup_workers(self) -> None:
        self.assertIs(ReexportStartupWorker, StartupWorker)
        self.assertIs(ReexportRecoveryWorker, RecoveryWorker)

    def test_startup_worker_emits_launch_result(self) -> None:
        worker = StartupWorker(
            backend=_FakeBackend(),
            client=_FakeClient(connect_result=True),
            protocol=_FakeProtocol(hardware_result=(True, "ready")),
            launch_mode="online",
        )
        finished = []
        worker.finished.connect(lambda result: finished.append(result))

        worker.run()

        self.assertEqual(len(finished), 1)
        self.assertTrue(finished[0].success)
        self.assertTrue(finished[0].online_ready)

    def test_recovery_worker_emits_recovery_result(self) -> None:
        worker = RecoveryWorker(
            action="retry_stage",
            recovery_snapshot=SessionSnapshot(
                mode="online",
                session_state="failed",
                backend_state="ready",
                tcp_state="failed",
                hardware_state="unavailable",
                failure_code="SUP_TCP_CONNECT_FAILED",
                failure_stage="tcp_ready",
                recoverable=True,
                last_error_message="failure",
                updated_at=snapshot_timestamp(),
            ),
            backend=_FakeBackend(),
            client=_FakeClient(connect_result=True),
            protocol=_FakeProtocol(hardware_result=(True, "ready")),
        )
        finished = []
        worker.finished.connect(lambda result: finished.append(result))

        worker.run()

        self.assertEqual(len(finished), 1)
        self.assertTrue(finished[0].success)
        self.assertTrue(finished[0].online_ready)


if __name__ == "__main__":
    unittest.main()
