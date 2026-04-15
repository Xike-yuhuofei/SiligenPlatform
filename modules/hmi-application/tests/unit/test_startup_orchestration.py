import sys
import unittest
from pathlib import Path

TEST_ROOT = Path(__file__).resolve().parents[1]
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.contracts.launch_supervision_contract import (
    FailureStage,
    SessionSnapshot,
    snapshot_timestamp,
)
from hmi_application.services.startup_orchestration import run_launch_sequence, run_recovery_action


class _FakeBackend:
    def __init__(self, start_result=(True, "started"), wait_result=(True, "ready")) -> None:
        self.start_result = start_result
        self.wait_result = wait_result
        self.start_calls = 0
        self.wait_calls = 0
        self.stop_calls = 0

    def start(self):
        self.start_calls += 1
        return self.start_result

    def wait_ready(self, timeout: float = 5.0):
        self.wait_calls += 1
        return self.wait_result

    def stop(self):
        self.stop_calls += 1


class _FakeClient:
    def __init__(self, connect_result=True) -> None:
        self.connect_result = connect_result
        self.connect_calls = 0
        self.disconnect_calls = 0

    def connect(self, timeout: float = 3.0):
        self.connect_calls += 1
        return self.connect_result

    def disconnect(self):
        self.disconnect_calls += 1


class _FakeProtocol:
    def __init__(self, hardware_result=(True, "hardware ready")) -> None:
        self.hardware_result = hardware_result
        self.hardware_calls = 0

    def connect_hardware(self, card_ip: str = "", local_ip: str = "", timeout: float = 15.0):
        self.hardware_calls += 1
        return self.hardware_result


def _failed_snapshot(*, recoverable: bool = True, stage: FailureStage = "tcp_ready") -> SessionSnapshot:
    return SessionSnapshot(
        mode="online",
        session_state="failed",
        backend_state="ready",
        tcp_state="failed",
        hardware_state="unavailable",
        failure_code="SUP_TCP_CONNECT_FAILED",
        failure_stage=stage,
        recoverable=recoverable,
        last_error_message="failure for recovery test",
        updated_at=snapshot_timestamp(),
    )


class StartupOrchestrationTest(unittest.TestCase):
    def test_run_launch_sequence_logs_ready_path_without_changing_contract(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient(connect_result=True)
        protocol = _FakeProtocol(hardware_result=(True, "ready"))

        result = run_launch_sequence("online", backend, client, protocol)

        self.assertTrue(result.success)
        self.assertEqual(result.phase, "ready")
        self.assertEqual(backend.start_calls, 1)
        self.assertEqual(client.connect_calls, 1)
        self.assertEqual(protocol.hardware_calls, 1)

    def test_run_recovery_action_keeps_online_recovery_contract(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient(connect_result=True)
        protocol = _FakeProtocol(hardware_result=(True, "ready"))

        result = run_recovery_action(
            action="retry_stage",
            snapshot=_failed_snapshot(),
            backend=backend,
            client=client,
            protocol=protocol,
        )

        self.assertTrue(result.success)
        self.assertTrue(result.online_ready)
        self.assertIsNone(result.failure_code)
        self.assertIsNone(result.failure_stage)

    def test_run_recovery_action_rejects_offline_snapshot(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient(connect_result=True)
        protocol = _FakeProtocol(hardware_result=(True, "ready"))
        offline_snapshot = SessionSnapshot(
            mode="offline",
            session_state="idle",
            backend_state="stopped",
            tcp_state="disconnected",
            hardware_state="unavailable",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="offline",
            updated_at=snapshot_timestamp(),
        )

        with self.assertRaisesRegex(ValueError, "online session snapshots"):
            run_recovery_action(
                action="retry_stage",
                snapshot=offline_snapshot,
                backend=backend,
                client=client,
                protocol=protocol,
            )


if __name__ == "__main__":
    unittest.main()
