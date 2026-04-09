import unittest
from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.contracts.launch_supervision_contract import (
    SessionSnapshot,
    SessionStageEvent,
    is_online_ready,
    snapshot_timestamp,
)


class LaunchSupervisionContractTest(unittest.TestCase):
    def test_failed_snapshot_requires_failure_fields(self) -> None:
        with self.assertRaises(ValueError):
            SessionSnapshot(
                mode="online",
                session_state="failed",
                backend_state="ready",
                tcp_state="ready",
                hardware_state="failed",
                failure_code=None,
                failure_stage=None,
                recoverable=True,
                last_error_message="hardware lost",
                updated_at=snapshot_timestamp(),
            )

    def test_non_failed_snapshot_rejects_failure_payload(self) -> None:
        with self.assertRaises(ValueError):
            SessionSnapshot(
                mode="online",
                session_state="ready",
                backend_state="ready",
                tcp_state="ready",
                hardware_state="ready",
                failure_code="SUP_TCP_CONNECT_FAILED",
                failure_stage="tcp_ready",
                recoverable=True,
                last_error_message="ready",
                updated_at=snapshot_timestamp(),
            )

    def test_online_ready_requires_all_channels_ready(self) -> None:
        ready_snapshot = SessionSnapshot(
            mode="online",
            session_state="ready",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="ready",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="ready",
            updated_at=snapshot_timestamp(),
        )
        degraded_snapshot = SessionSnapshot(
            mode="online",
            session_state="degraded",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="failed",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="hardware unavailable",
            updated_at=snapshot_timestamp(),
        )

        self.assertTrue(is_online_ready(ready_snapshot))
        self.assertFalse(is_online_ready(degraded_snapshot))

    def test_stage_event_rejects_unknown_event_type(self) -> None:
        with self.assertRaises(ValueError):
            SessionStageEvent(
                event_type="unknown",  # type: ignore[arg-type]
                session_id="session-1",
                stage="online_ready",
                timestamp=snapshot_timestamp(),
            )


if __name__ == "__main__":
    unittest.main()
