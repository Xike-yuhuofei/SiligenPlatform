import sys
import unittest
from pathlib import Path
from types import SimpleNamespace


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.client.backend_manager import BackendStepResult
from hmi_client.client.launch_supervision_contract import RuntimeIdentity, SessionSnapshot
from hmi_client.client.launch_supervision_session import SupervisorPolicy, SupervisorSession
from hmi_client.client.protocol import RuntimeIdentityStatus, StatusQueryResult


EXPECTED_RUNTIME_IDENTITY = RuntimeIdentity(
    executable_path="C:\\runtime\\siligen_runtime_gateway.exe",
    working_directory="C:\\runtime",
)


class _FakeBackend:
    def __init__(self, start_result=(True, "started"), ready_result=(True, "ready")) -> None:
        self.start_result = start_result
        self.ready_result = ready_result
        self.start_calls = 0
        self.ready_calls = 0
        self.stop_calls = 0
        self.exe_path = EXPECTED_RUNTIME_IDENTITY.executable_path
        self.working_directory = EXPECTED_RUNTIME_IDENTITY.working_directory

    def start(self):
        self.start_calls += 1
        return self.start_result

    def wait_ready(self, timeout: float = 5.0):
        self.ready_calls += 1
        return self.ready_result

    def stop(self):
        self.stop_calls += 1


class _FakeClient:
    def __init__(self, connect_result=True, connect_error="") -> None:
        self.connect_result = connect_result
        self.connect_calls = 0
        self.disconnect_calls = 0
        self.connect_timeouts = []
        self._connected = False
        self.last_connect_error = connect_error

    def connect(self, timeout=3.0):
        self.connect_calls += 1
        self.connect_timeouts.append(timeout)
        self._connected = bool(self.connect_result)
        return self.connect_result

    def disconnect(self):
        self.disconnect_calls += 1
        self._connected = False

    def is_connected(self):
        return self._connected


class _FakeProtocol:
    def __init__(self, hardware_result=(True, "ready")) -> None:
        self.hardware_result = hardware_result
        self.hardware_calls = 0

    def connect_hardware(self, card_ip: str = "", local_ip: str = "", timeout: float = 15.0):
        self.hardware_calls += 1
        return self.hardware_result


class _FakeRuntimeProbe:
    def __init__(self, status_result: StatusQueryResult | None = None) -> None:
        self.status_result = status_result or StatusQueryResult(
            ok=True,
            status=SimpleNamespace(
                runtime_identity=RuntimeIdentityStatus(
                    executable_path=EXPECTED_RUNTIME_IDENTITY.executable_path,
                    working_directory=EXPECTED_RUNTIME_IDENTITY.working_directory,
                    protocol_version=EXPECTED_RUNTIME_IDENTITY.protocol_version,
                    preview_snapshot_contract=EXPECTED_RUNTIME_IDENTITY.preview_snapshot_contract,
                )
            ),
        )
        self.calls = 0
        self.timeouts: list[float] = []

    def get_status_detailed(self, timeout: float = 5.0):
        self.calls += 1
        self.timeouts.append(timeout)
        return self.status_result


class _TimeoutAwareBackend(_FakeBackend):
    def __init__(self, start_result=(True, "started"), ready_result=(True, "ready")) -> None:
        super().__init__(start_result=start_result, ready_result=ready_result)
        self.ready_timeouts = []

    def wait_ready(self, timeout=5.0):
        self.ready_calls += 1
        self.ready_timeouts.append(timeout)
        return self.ready_result


class _TimeoutAwareProtocol(_FakeProtocol):
    def __init__(self, hardware_result=(True, "ready")) -> None:
        super().__init__(hardware_result=hardware_result)
        self.hardware_timeouts = []

    def connect_hardware(self, card_ip: str = "", local_ip: str = "", timeout: float = 15.0):
        self.hardware_calls += 1
        self.hardware_timeouts.append(timeout)
        return self.hardware_result


class _DetailedBackend:
    def __init__(self, start_detail: BackendStepResult, ready_detail: BackendStepResult) -> None:
        self.start_detail = start_detail
        self.ready_detail = ready_detail
        self.start_calls = 0
        self.ready_calls = 0
        self.exe_path = EXPECTED_RUNTIME_IDENTITY.executable_path
        self.working_directory = EXPECTED_RUNTIME_IDENTITY.working_directory

    def start_detailed(self):
        self.start_calls += 1
        return self.start_detail

    def wait_ready_detailed(self, timeout: float = 5.0):
        self.ready_calls += 1
        return self.ready_detail

    def stop(self):
        pass


class SupervisorSessionTest(unittest.TestCase):
    def test_offline_mode_skips_all_online_stages(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="offline",
        )
        progress_events = []
        snapshot = session.start(progress_callback=lambda message, percent: progress_events.append((message, percent)))

        self.assertEqual(snapshot.mode, "offline")
        self.assertEqual(snapshot.session_state, "idle")
        self.assertIsNone(snapshot.failure_code)
        self.assertEqual(progress_events, [("Offline mode: startup skipped", 100)])

    def test_backend_start_failure_maps_failure_code(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(start_result=(False, "missing spec")),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        snapshot = session.start()

        self.assertEqual(snapshot.session_state, "failed")
        self.assertEqual(snapshot.failure_stage, "backend_starting")
        self.assertEqual(snapshot.failure_code, "SUP_BACKEND_START_FAILED")

    def test_backend_detailed_failure_preserves_specific_failure_code(self) -> None:
        session = SupervisorSession(
            backend=_DetailedBackend(
                start_detail=BackendStepResult(
                    ok=False,
                    message="auto-start disabled",
                    failure_code="SUP_BACKEND_AUTOSTART_DISABLED",
                    recoverable=False,
                ),
                ready_detail=BackendStepResult(ok=True, message="ready"),
            ),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        snapshot = session.start()

        self.assertEqual(snapshot.failure_stage, "backend_starting")
        self.assertEqual(snapshot.failure_code, "SUP_BACKEND_AUTOSTART_DISABLED")
        self.assertFalse(snapshot.recoverable)

    def test_backend_ready_timeout_maps_to_expected_code(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(start_result=(True, "ok"), ready_result=(False, "timeout")),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        snapshot = session.start()

        self.assertEqual(snapshot.session_state, "failed")
        self.assertEqual(snapshot.failure_stage, "backend_ready")
        self.assertEqual(snapshot.failure_code, "SUP_BACKEND_READY_TIMEOUT")

    def test_full_online_success_reaches_ready(self) -> None:
        progress_events = []
        session = SupervisorSession(
            backend=_FakeBackend(),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        snapshot = session.start(progress_callback=lambda message, percent: progress_events.append((message, percent)))

        self.assertEqual(snapshot.session_state, "ready")
        self.assertTrue(snapshot.online_ready)
        self.assertEqual(snapshot.failure_code, None)
        self.assertEqual(
            progress_events,
            [
                ("Starting backend...", 10),
                ("Waiting for backend...", 20),
                ("Backend ready", 30),
                ("Connecting TCP...", 40),
                ("TCP connected", 60),
                ("Verifying runtime contract...", 70),
                ("Initializing hardware...", 80),
                ("System ready", 100),
            ],
        )
        self.assertTrue(snapshot.runtime_contract_verified)
        self.assertEqual(snapshot.runtime_identity, EXPECTED_RUNTIME_IDENTITY)

    def test_start_emits_stage_snapshots_before_ready(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        snapshots = []
        final_snapshot = session.start(snapshot_callback=lambda snap: snapshots.append(snap))

        self.assertEqual(final_snapshot.session_state, "ready")
        self.assertGreaterEqual(len(snapshots), 5)
        self.assertEqual(snapshots[0].session_state, "starting")
        self.assertEqual(snapshots[0].backend_state, "starting")
        self.assertEqual(snapshots[0].tcp_state, "disconnected")
        self.assertEqual(snapshots[0].hardware_state, "unavailable")
        self.assertEqual(snapshots[-1].session_state, "ready")
        self.assertTrue(snapshots[-1].online_ready)

    def test_start_emits_structured_stage_events(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        events = []
        session.start(event_callback=lambda event: events.append(event))

        entered_stages = [event.stage for event in events if event.event_type == "stage_entered"]
        self.assertEqual(
            entered_stages,
            [
                "backend_starting",
                "backend_ready",
                "tcp_connecting",
                "tcp_ready",
                "runtime_contract_ready",
                "hardware_probing",
                "hardware_ready",
                "online_ready",
            ],
        )
        self.assertTrue(any(event.event_type == "stage_succeeded" and event.stage == "online_ready" for event in events))

    def test_failure_stage_is_first_failure_point(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(start_result=(True, "ok"), ready_result=(False, "timeout")),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        snapshots = []
        final_snapshot = session.start(snapshot_callback=lambda snap: snapshots.append(snap))

        self.assertEqual(final_snapshot.session_state, "failed")
        self.assertEqual(final_snapshot.failure_stage, "backend_ready")
        self.assertEqual(final_snapshot.failure_code, "SUP_BACKEND_READY_TIMEOUT")
        self.assertEqual(snapshots[-1].failure_stage, "backend_ready")

    def test_tcp_failure_uses_tcp_failure_code(self) -> None:
        client = _FakeClient(connect_result=False, connect_error="connection refused")
        session = SupervisorSession(
            backend=_FakeBackend(),
            client=client,
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        snapshot = session.start()

        self.assertEqual(snapshot.session_state, "failed")
        self.assertEqual(snapshot.failure_stage, "tcp_connecting")
        self.assertEqual(snapshot.failure_code, "SUP_TCP_CONNECT_FAILED")
        self.assertEqual(client.connect_timeouts, [3.0])

    def test_supervisor_policy_overrides_stage_timeouts(self) -> None:
        backend = _TimeoutAwareBackend()
        client = _FakeClient(connect_result=True)
        protocol = _TimeoutAwareProtocol(hardware_result=(True, "ready"))
        policy = SupervisorPolicy(
            backend_ready_timeout_s=7.5,
            tcp_connect_timeout_s=4.5,
            hardware_probe_timeout_s=21.0,
        )
        session = SupervisorSession(
            backend=backend,
            client=client,
            protocol=protocol,
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
            policy=policy,
        )

        snapshot = session.start()
        self.assertTrue(snapshot.online_ready)
        self.assertEqual(backend.ready_timeouts, [7.5])
        self.assertEqual(client.connect_timeouts, [4.5])
        self.assertEqual(protocol.hardware_timeouts, [21.0])

    def test_hardware_failure_uses_hardware_failure_code(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(),
            client=_FakeClient(),
            protocol=_FakeProtocol(hardware_result=(False, "no card")),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        snapshot = session.start()

        self.assertEqual(snapshot.session_state, "failed")
        self.assertEqual(snapshot.failure_stage, "hardware_probing")
        self.assertEqual(snapshot.failure_code, "SUP_HARDWARE_CONNECT_FAILED")

    def test_runtime_degradation_detects_tcp_disconnect(self) -> None:
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
            updated_at="2026-03-20T00:00:00Z",
            runtime_contract_verified=True,
            runtime_identity=EXPECTED_RUNTIME_IDENTITY,
        )
        degraded = SupervisorSession.detect_runtime_degradation(
            ready_snapshot,
            tcp_connected=False,
            error_message="socket closed",
        )
        self.assertIsNotNone(degraded)
        assert degraded is not None
        self.assertEqual(degraded.failure_code, "SUP_TCP_CONNECT_FAILED")
        self.assertEqual(degraded.failure_stage, "tcp_ready")
        self.assertEqual(degraded.session_state, "failed")

    def test_runtime_degradation_detects_hw_unavailable(self) -> None:
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
            updated_at="2026-03-20T00:00:00Z",
            runtime_contract_verified=True,
            runtime_identity=EXPECTED_RUNTIME_IDENTITY,
        )
        degraded = SupervisorSession.detect_runtime_degradation(
            ready_snapshot,
            tcp_connected=True,
            hardware_ready=False,
        )
        self.assertIsNotNone(degraded)
        assert degraded is not None
        self.assertEqual(degraded.failure_code, "SUP_HARDWARE_CONNECT_FAILED")
        self.assertEqual(degraded.failure_stage, "hardware_ready")
        self.assertEqual(degraded.session_state, "failed")

    def test_runtime_degradation_with_event_emits_stage_failed(self) -> None:
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
            updated_at="2026-03-20T00:00:00Z",
            runtime_contract_verified=True,
            runtime_identity=EXPECTED_RUNTIME_IDENTITY,
        )

        result = SupervisorSession.detect_runtime_degradation_with_event(
            ready_snapshot,
            session_id="session-123",
            tcp_connected=False,
            error_message="connection dropped",
        )
        self.assertIsNotNone(result)
        assert result is not None
        degraded, event = result
        self.assertEqual(degraded.failure_code, "SUP_TCP_CONNECT_FAILED")
        self.assertEqual(degraded.failure_stage, "tcp_ready")
        self.assertEqual(event.event_type, "stage_failed")
        self.assertEqual(event.stage, "tcp_ready")
        self.assertEqual(event.failure_code, "SUP_TCP_CONNECT_FAILED")
        self.assertEqual(event.session_id, "session-123")

    def test_retry_stage_retries_current_failure_stage(self) -> None:
        backend = _FakeBackend(start_result=(True, "ok"), ready_result=(False, "timeout"))
        session = SupervisorSession(
            backend=backend,
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        failed = session.start()
        self.assertEqual(failed.failure_stage, "backend_ready")
        self.assertEqual(backend.ready_calls, 1)

        backend.ready_result = (True, "ready")
        retried = session.retry_stage(failed)
        self.assertTrue(retried.online_ready)
        self.assertEqual(backend.start_calls, 1)
        self.assertEqual(backend.ready_calls, 2)

    def test_retry_stage_rejects_unrecoverable_failed_snapshot(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        failed = SessionSnapshot(
            mode="online",
            session_state="failed",
            backend_state="ready",
            tcp_state="failed",
            hardware_state="unavailable",
            failure_code="SUP_TCP_CONNECT_FAILED",
            failure_stage="tcp_ready",
            recoverable=False,
            last_error_message="tcp lost",
            updated_at="2026-03-20T00:00:00Z",
        )

        with self.assertRaisesRegex(ValueError, "recoverable failed session snapshot"):
            session.retry_stage(failed)

    def test_restart_session_invokes_cleanup_then_restarts(self) -> None:
        backend = _FakeBackend(start_result=(False, "boom"), ready_result=(True, "ready"))
        client = _FakeClient()
        session = SupervisorSession(
            backend=backend,
            client=client,
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        failed = session.start()
        self.assertEqual(failed.failure_stage, "backend_starting")

        backend.start_result = (True, "started")
        restarted = session.restart_session(failed)
        self.assertTrue(restarted.online_ready)
        self.assertEqual(backend.stop_calls, 1)
        self.assertEqual(client.disconnect_calls, 1)

    def test_restart_session_rejects_ready_snapshot(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient()
        session = SupervisorSession(
            backend=backend,
            client=client,
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        ready = SessionSnapshot(
            mode="online",
            session_state="ready",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="ready",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="System ready",
            updated_at="2026-03-20T00:00:00Z",
            runtime_contract_verified=True,
            runtime_identity=EXPECTED_RUNTIME_IDENTITY,
        )

        with self.assertRaisesRegex(ValueError, "failed online session snapshot"):
            session.restart_session(ready)

        self.assertEqual(backend.stop_calls, 0)
        self.assertEqual(backend.start_calls, 0)
        self.assertEqual(client.disconnect_calls, 0)

    def test_stop_session_rejects_starting_snapshot(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        starting = SessionSnapshot(
            mode="online",
            session_state="starting",
            backend_state="starting",
            tcp_state="disconnected",
            hardware_state="unavailable",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="Starting backend...",
            updated_at="2026-03-20T00:00:00Z",
        )

        with self.assertRaisesRegex(ValueError, "ready or failed online session snapshot"):
            session.stop_session(starting)

    def test_stop_session_transitions_to_idle(self) -> None:
        session = SupervisorSession(
            backend=_FakeBackend(),
            client=_FakeClient(),
            protocol=_FakeProtocol(),
            runtime_probe=_FakeRuntimeProbe(),
            launch_mode="online",
        )
        ready = session.start()
        snapshots = []
        events = []
        idle = session.stop_session(
            ready,
            snapshot_callback=lambda snap: snapshots.append(snap),
            event_callback=lambda event: events.append(event),
        )

        self.assertEqual(idle.session_state, "idle")
        self.assertEqual(idle.backend_state, "stopped")
        self.assertEqual(idle.tcp_state, "disconnected")
        self.assertEqual(idle.hardware_state, "unavailable")
        self.assertEqual([snap.session_state for snap in snapshots], ["stopping", "idle"])
        self.assertTrue(any(event.event_type == "recovery_invoked" for event in events))


if __name__ == "__main__":
    unittest.main()
