import unittest

from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.contracts.launch_supervision_contract import RuntimeIdentity, SessionSnapshot, snapshot_timestamp
from hmi_application.services.launch_result_projection import (
    build_launch_result,
    build_offline_launch_result,
    launch_result_from_snapshot,
)


RUNTIME_IDENTITY = RuntimeIdentity(
    executable_path="C:\\runtime\\siligen_runtime_gateway.exe",
    working_directory="C:\\runtime",
)


def _ready_snapshot() -> SessionSnapshot:
    return SessionSnapshot(
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
        runtime_contract_verified=True,
        runtime_identity=RUNTIME_IDENTITY,
    )


class LaunchResultProjectionTest(unittest.TestCase):
    def test_build_launch_result_projects_ready_snapshot(self) -> None:
        result = build_launch_result("online", _ready_snapshot())

        self.assertTrue(result.success)
        self.assertTrue(result.online_ready)
        self.assertEqual(result.phase, "ready")
        self.assertEqual(result.effective_mode, "online")

    def test_launch_result_from_snapshot_normalizes_requested_mode(self) -> None:
        result = launch_result_from_snapshot("ONLINE", _ready_snapshot())

        self.assertEqual(result.requested_mode, "online")
        self.assertTrue(result.online_ready)

    def test_build_offline_launch_result_uses_offline_snapshot_defaults(self) -> None:
        result = build_offline_launch_result()

        self.assertEqual(result.requested_mode, "offline")
        self.assertEqual(result.effective_mode, "offline")
        self.assertEqual(result.phase, "offline")
        self.assertFalse(result.online_ready)
        self.assertIn("Offline", result.user_message)


if __name__ == "__main__":
    unittest.main()
