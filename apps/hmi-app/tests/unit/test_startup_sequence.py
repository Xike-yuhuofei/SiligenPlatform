import sys
import unittest
import os
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.client.startup_sequence import (
    LaunchResult,
    launch_result_from_snapshot,
    load_supervisor_policy_from_env,
    normalize_launch_mode,
    run_launch_sequence,
    run_recovery_action,
)
from hmi_client.client.launch_supervision_session import SupervisorPolicy
from hmi_client.client.launch_supervision_contract import SessionSnapshot, snapshot_timestamp


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

    def wait_ready(self):
        self.wait_calls += 1
        return self.wait_result

    def stop(self):
        self.stop_calls += 1


class _FakeClient:
    def __init__(self, connect_result=True) -> None:
        self.connect_result = connect_result
        self.connect_calls = 0
        self.disconnect_calls = 0

    def connect(self):
        self.connect_calls += 1
        return self.connect_result

    def disconnect(self):
        self.disconnect_calls += 1


class _FakeProtocol:
    def __init__(self, hardware_result=(True, "hardware ready")) -> None:
        self.hardware_result = hardware_result
        self.hardware_calls = 0

    def connect_hardware(self):
        self.hardware_calls += 1
        return self.hardware_result


class StartupSequenceContractTest(unittest.TestCase):
    @staticmethod
    def _failed_snapshot(*, recoverable: bool = True, stage: str = "tcp_ready") -> SessionSnapshot:
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

    def test_normalize_launch_mode_rejects_unknown_value(self) -> None:
        with self.assertRaisesRegex(ValueError, "Unsupported launch mode"):
            normalize_launch_mode("auto")

    def test_load_supervisor_policy_from_env_overrides_defaults(self) -> None:
        previous = {
            "SILIGEN_SUP_BACKEND_READY_TIMEOUT_S": os.getenv("SILIGEN_SUP_BACKEND_READY_TIMEOUT_S"),
            "SILIGEN_SUP_TCP_CONNECT_TIMEOUT_S": os.getenv("SILIGEN_SUP_TCP_CONNECT_TIMEOUT_S"),
            "SILIGEN_SUP_HARDWARE_PROBE_TIMEOUT_S": os.getenv("SILIGEN_SUP_HARDWARE_PROBE_TIMEOUT_S"),
        }
        try:
            os.environ["SILIGEN_SUP_BACKEND_READY_TIMEOUT_S"] = "8.5"
            os.environ["SILIGEN_SUP_TCP_CONNECT_TIMEOUT_S"] = "4.2"
            os.environ["SILIGEN_SUP_HARDWARE_PROBE_TIMEOUT_S"] = "19.3"
            policy = load_supervisor_policy_from_env()
        finally:
            for key, value in previous.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value

        self.assertEqual(policy, SupervisorPolicy(8.5, 4.2, 19.3))

    def test_launch_result_with_snapshot_forces_field_consistency(self) -> None:
        snapshot = self._failed_snapshot(recoverable=True, stage="tcp_ready")
        result = LaunchResult(
            requested_mode="online",
            effective_mode="offline",
            phase="offline",
            success=True,
            backend_started=False,
            tcp_connected=True,
            hardware_ready=True,
            user_message="",
            failure_code=None,
            failure_stage=None,
            recoverable=False,
            session_state="idle",
            session_snapshot=snapshot,
        )

        self.assertEqual(result.effective_mode, "online")
        self.assertEqual(result.phase, "tcp")
        self.assertFalse(result.success)
        self.assertTrue(result.backend_started)
        self.assertFalse(result.tcp_connected)
        self.assertFalse(result.hardware_ready)
        self.assertEqual(result.failure_code, "SUP_TCP_CONNECT_FAILED")
        self.assertEqual(result.failure_stage, "tcp_ready")
        self.assertTrue(result.recoverable)
        self.assertEqual(result.session_state, "failed")

    def test_launch_result_from_snapshot_helper_derives_online_ready(self) -> None:
        ready_snapshot = SessionSnapshot(
            mode="online",
            session_state="ready",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="ready",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="System ready",
            updated_at=snapshot_timestamp(),
        )
        result = launch_result_from_snapshot("online", ready_snapshot)
        self.assertTrue(result.success)
        self.assertTrue(result.online_ready)
        self.assertEqual(result.phase, "ready")

    def test_offline_launch_skips_backend_tcp_and_hardware(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient()
        protocol = _FakeProtocol()
        progress_events = []

        result = run_launch_sequence(
            "offline",
            backend,
            client,
            protocol,
            progress_callback=lambda message, percent: progress_events.append((message, percent)),
        )

        self.assertTrue(result.success)
        self.assertEqual(result.requested_mode, "offline")
        self.assertEqual(result.effective_mode, "offline")
        self.assertEqual(result.phase, "offline")
        self.assertFalse(result.backend_started)
        self.assertFalse(result.tcp_connected)
        self.assertFalse(result.hardware_ready)
        self.assertFalse(result.online_ready)
        self.assertIsNotNone(result.session_snapshot)
        self.assertEqual(result.session_state, "idle")
        self.assertEqual(backend.start_calls, 0)
        self.assertEqual(backend.wait_calls, 0)
        self.assertEqual(client.connect_calls, 0)
        self.assertEqual(protocol.hardware_calls, 0)
        self.assertEqual(progress_events, [("Offline mode: startup skipped", 100)])

    def test_online_launch_reports_backend_failure(self) -> None:
        backend = _FakeBackend(start_result=(False, "missing gateway"))
        client = _FakeClient()
        protocol = _FakeProtocol()

        result = run_launch_sequence("online", backend, client, protocol)

        self.assertFalse(result.success)
        self.assertEqual(result.phase, "backend")
        self.assertIn("missing gateway", result.user_message)
        self.assertEqual(result.failure_stage, "backend_starting")
        self.assertEqual(result.failure_code, "SUP_BACKEND_START_FAILED")
        self.assertEqual(backend.start_calls, 1)
        self.assertEqual(backend.wait_calls, 0)
        self.assertEqual(client.connect_calls, 0)
        self.assertEqual(protocol.hardware_calls, 0)

    def test_online_launch_reports_ready_when_all_stages_succeed(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient(connect_result=True)
        protocol = _FakeProtocol(hardware_result=(True, "ready"))
        progress_events = []
        snapshots = []
        stage_events = []

        result = run_launch_sequence(
            "online",
            backend,
            client,
            protocol,
            progress_callback=lambda message, percent: progress_events.append((message, percent)),
            snapshot_callback=lambda snapshot: snapshots.append(snapshot),
            event_callback=lambda event: stage_events.append(event),
        )

        self.assertTrue(result.success)
        self.assertEqual(result.phase, "ready")
        self.assertTrue(result.backend_started)
        self.assertTrue(result.tcp_connected)
        self.assertTrue(result.hardware_ready)
        self.assertTrue(result.online_ready)
        self.assertIsNotNone(result.session_snapshot)
        self.assertIsNone(result.failure_code)
        self.assertIsNone(result.failure_stage)
        self.assertEqual(backend.start_calls, 1)
        self.assertEqual(backend.wait_calls, 1)
        self.assertEqual(client.connect_calls, 1)
        self.assertEqual(protocol.hardware_calls, 1)
        self.assertGreaterEqual(len(snapshots), 2)
        self.assertTrue(any(event.event_type == "stage_entered" for event in stage_events))
        self.assertTrue(any(event.event_type == "stage_succeeded" and event.stage == "online_ready" for event in stage_events))
        self.assertEqual(
            progress_events,
            [
                ("Starting backend...", 10),
                ("Waiting for backend...", 20),
                ("Backend ready", 30),
                ("Connecting TCP...", 40),
                ("TCP connected", 60),
                ("Initializing hardware...", 70),
                ("System ready", 100),
            ],
        )

    def test_recovery_retry_stage_restores_online_ready(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient(connect_result=True)
        protocol = _FakeProtocol(hardware_result=(True, "ready"))

        result = run_recovery_action(
            action="retry_stage",
            snapshot=self._failed_snapshot(recoverable=True, stage="tcp_ready"),
            backend=backend,
            client=client,
            protocol=protocol,
        )

        self.assertTrue(result.success)
        self.assertTrue(result.online_ready)
        self.assertEqual(result.phase, "ready")
        self.assertIsNone(result.failure_code)
        self.assertIsNone(result.failure_stage)
        self.assertGreaterEqual(client.connect_calls, 1)
        self.assertGreaterEqual(protocol.hardware_calls, 1)

    def test_recovery_retry_stage_returns_input_when_not_recoverable(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient(connect_result=True)
        protocol = _FakeProtocol(hardware_result=(True, "ready"))
        original = self._failed_snapshot(recoverable=False, stage="backend_starting")

        with self.assertRaisesRegex(ValueError, "recoverable failed session snapshot"):
            run_recovery_action(
                action="retry_stage",
                snapshot=original,
                backend=backend,
                client=client,
                protocol=protocol,
            )

        self.assertEqual(backend.start_calls, 0)
        self.assertEqual(client.connect_calls, 0)
        self.assertEqual(protocol.hardware_calls, 0)

    def test_recovery_restart_session_rejects_ready_snapshot(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient(connect_result=True)
        protocol = _FakeProtocol(hardware_result=(True, "ready"))
        ready_snapshot = SessionSnapshot(
            mode="online",
            session_state="ready",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="ready",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="System ready",
            updated_at=snapshot_timestamp(),
        )

        with self.assertRaisesRegex(ValueError, "failed online session snapshot"):
            run_recovery_action(
                action="restart_session",
                snapshot=ready_snapshot,
                backend=backend,
                client=client,
                protocol=protocol,
            )

        self.assertEqual(backend.stop_calls, 0)
        self.assertEqual(backend.start_calls, 0)
        self.assertEqual(client.disconnect_calls, 0)
        self.assertEqual(client.connect_calls, 0)
        self.assertEqual(protocol.hardware_calls, 0)

    def test_recovery_stop_session_transitions_to_idle(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient(connect_result=True)
        protocol = _FakeProtocol(hardware_result=(True, "ready"))
        ready_snapshot = SessionSnapshot(
            mode="online",
            session_state="ready",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="ready",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="System ready",
            updated_at=snapshot_timestamp(),
        )

        result = run_recovery_action(
            action="stop_session",
            snapshot=ready_snapshot,
            backend=backend,
            client=client,
            protocol=protocol,
        )

        self.assertFalse(result.success)
        self.assertEqual(result.session_state, "idle")
        self.assertEqual(result.phase, "backend")
        self.assertEqual(client.disconnect_calls, 1)
        self.assertEqual(backend.stop_calls, 1)


if __name__ == "__main__":
    unittest.main()
