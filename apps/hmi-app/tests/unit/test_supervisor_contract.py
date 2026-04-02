import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.client.launch_supervision_contract import (
    SessionSnapshot,
    SessionStageEvent,
    is_online_ready,
    snapshot_timestamp,
)


class SupervisorContractTest(unittest.TestCase):
    def test_online_ready_requires_all_ready_fields(self) -> None:
        snapshot = SessionSnapshot(
            mode="online",
            session_state="ready",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="ready",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message=None,
            updated_at="2026-03-20T00:00:00+00:00",
        )

        self.assertTrue(snapshot.online_ready)
        self.assertTrue(is_online_ready(snapshot))

    def test_failure_code_breaks_online_ready(self) -> None:
        with self.assertRaisesRegex(ValueError, "non-failed snapshot"):
            SessionSnapshot(
                mode="online",
                session_state="ready",
                backend_state="ready",
                tcp_state="ready",
                hardware_state="ready",
                failure_code="SUP_TCP_CONNECT_FAILED",
                failure_stage="tcp_connecting",
                recoverable=True,
                last_error_message="connection refused",
                updated_at="2026-03-20T00:00:00+00:00",
            )

    def test_failed_snapshot_requires_failure_fields(self) -> None:
        with self.assertRaisesRegex(ValueError, "failed snapshot requires"):
            SessionSnapshot(
                mode="online",
                session_state="failed",
                backend_state="failed",
                tcp_state="failed",
                hardware_state="unavailable",
                failure_code=None,
                failure_stage=None,
                recoverable=True,
                last_error_message="error",
                updated_at="2026-03-20T00:00:00+00:00",
            )

    def test_failed_snapshot_with_code_stage_is_valid(self) -> None:
        snapshot = SessionSnapshot(
            mode="online",
            session_state="failed",
            backend_state="failed",
            tcp_state="failed",
            hardware_state="unavailable",
            failure_code="SUP_TCP_CONNECT_FAILED",
            failure_stage="tcp_ready",
            recoverable=True,
            last_error_message="connection lost",
            updated_at="2026-03-20T00:00:00+00:00",
        )
        self.assertFalse(snapshot.online_ready)
        self.assertFalse(is_online_ready(snapshot))

    def test_snapshot_timestamp_uses_utc_z_suffix(self) -> None:
        value = snapshot_timestamp()
        self.assertTrue(value.endswith("Z"))

    def test_stage_event_accepts_valid_values(self) -> None:
        event = SessionStageEvent(
            event_type="stage_entered",
            session_id="abc",
            stage="backend_starting",
            timestamp="2026-03-20T00:00:00Z",
        )
        self.assertEqual(event.event_type, "stage_entered")

    def test_stage_event_rejects_invalid_type(self) -> None:
        with self.assertRaisesRegex(ValueError, "Invalid event_type"):
            SessionStageEvent(
                event_type="unknown",
                session_id="abc",
                stage="backend_starting",
                timestamp="2026-03-20T00:00:00Z",
            )


if __name__ == "__main__":
    unittest.main()
