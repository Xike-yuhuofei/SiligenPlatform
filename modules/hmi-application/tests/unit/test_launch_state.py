import sys
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(WORKSPACE_ROOT / "modules" / "hmi-application" / "application"))

from hmi_application.launch_state import (
    build_recovery_action_decision,
    build_launch_ui_state,
    build_runtime_degradation_result,
    current_effective_mode,
    detect_runtime_degradation_result,
)
from hmi_application.startup import launch_result_from_snapshot
from hmi_application.supervisor_contract import SessionSnapshot, SessionStageEvent, snapshot_timestamp


def _snapshot(
    *,
    mode: str = "online",
    session_state: str = "ready",
    backend_state: str = "ready",
    tcp_state: str = "ready",
    hardware_state: str = "ready",
    failure_code=None,
    failure_stage=None,
    recoverable: bool = True,
    last_error_message: str | None = "System ready",
) -> SessionSnapshot:
    return SessionSnapshot(
        mode=mode,
        session_state=session_state,
        backend_state=backend_state,
        tcp_state=tcp_state,
        hardware_state=hardware_state,
        failure_code=failure_code,
        failure_stage=failure_stage,
        recoverable=recoverable,
        last_error_message=last_error_message,
        updated_at=snapshot_timestamp(),
    )


def _stage_event(stage: str = "online_ready") -> SessionStageEvent:
    return SessionStageEvent(
        event_type="stage_succeeded",
        session_id="session-1",
        stage=stage,
        timestamp=snapshot_timestamp(),
    )


class LaunchStateOwnerTest(unittest.TestCase):
    def test_current_effective_mode_prefers_explicit_session_snapshot(self) -> None:
        offline_result = launch_result_from_snapshot("offline", _snapshot(mode="offline", session_state="idle"))
        online_snapshot = _snapshot(mode="online", session_state="starting", tcp_state="connecting", hardware_state="unavailable")

        mode = current_effective_mode("online", offline_result, online_snapshot)

        self.assertEqual(mode, "online")

    def test_offline_ui_state_keeps_preview_resync_pending_when_plan_exists(self) -> None:
        state = build_launch_ui_state(
            "offline",
            None,
            None,
            previous_connected=True,
            has_current_plan=True,
            preview_resync_pending=False,
            session_operation_running=False,
        )

        self.assertEqual(state.effective_mode, "offline")
        self.assertTrue(state.preview_resync_pending)
        self.assertFalse(state.allow_online_actions)
        self.assertFalse(state.system_panel_enabled)
        self.assertEqual(state.launch_mode_label, "Offline")
        self.assertEqual(state.operation_status_text, "离线模式")

    def test_starting_ui_state_blocks_online_actions_before_snapshot_ready(self) -> None:
        state = build_launch_ui_state(
            "online",
            None,
            None,
            previous_connected=False,
            has_current_plan=False,
            preview_resync_pending=False,
            session_operation_running=False,
        )

        self.assertEqual(state.effective_mode, "online")
        self.assertEqual(state.state_label_text, "Starting")
        self.assertEqual(state.operation_status_text, "启动中")
        self.assertFalse(state.allow_online_actions)
        self.assertFalse(state.stop_enabled)

    def test_ready_ui_state_enables_runtime_controls(self) -> None:
        ready_snapshot = _snapshot()
        launch_result = launch_result_from_snapshot("online", ready_snapshot)

        state = build_launch_ui_state(
            "online",
            launch_result,
            ready_snapshot,
            previous_connected=False,
            has_current_plan=True,
            preview_resync_pending=False,
            session_operation_running=False,
        )

        self.assertTrue(state.connected)
        self.assertTrue(state.hardware_connected)
        self.assertTrue(state.allow_online_actions)
        self.assertTrue(state.stop_enabled)
        self.assertTrue(state.hw_connect_enabled)
        self.assertTrue(state.global_estop_enabled)
        self.assertTrue(state.recovery_controls.stop_enabled)

    def test_failed_ui_state_exposes_recovery_controls(self) -> None:
        failed_snapshot = _snapshot(
            session_state="failed",
            tcp_state="failed",
            hardware_state="unavailable",
            failure_code="SUP_TCP_CONNECT_FAILED",
            failure_stage="tcp_ready",
            recoverable=True,
            last_error_message="tcp dropped",
        )
        launch_result = launch_result_from_snapshot("online", failed_snapshot)

        state = build_launch_ui_state(
            "online",
            launch_result,
            failed_snapshot,
            previous_connected=True,
            has_current_plan=True,
            preview_resync_pending=False,
            session_operation_running=False,
        )

        self.assertEqual(state.state_label_text, "Launch Failed")
        self.assertEqual(state.operation_status_text, "启动失败")
        self.assertFalse(state.allow_online_actions)
        self.assertTrue(state.recovery_controls.retry_enabled)
        self.assertTrue(state.recovery_controls.restart_enabled)
        self.assertTrue(state.recovery_controls.stop_enabled)

    def test_stop_session_decision_rejects_starting_snapshot(self) -> None:
        decision = build_recovery_action_decision(
            "stop_session",
            _snapshot(
                session_state="starting",
                backend_state="starting",
                tcp_state="disconnected",
                hardware_state="unavailable",
                last_error_message="Starting backend...",
            ),
            effective_mode="online",
            session_operation_running=False,
        )

        self.assertFalse(decision.allowed)
        self.assertIn("可停止态", decision.message)

    def test_restart_session_decision_rejects_ready_snapshot(self) -> None:
        decision = build_recovery_action_decision(
            "restart_session",
            _snapshot(),
            effective_mode="online",
            session_operation_running=False,
        )

        self.assertFalse(decision.allowed)
        self.assertIn("未处于失败态", decision.message)

    def test_build_runtime_degradation_result_marks_hardware_loss_as_failed_launch(self) -> None:
        ready_snapshot = _snapshot()
        launch_result = launch_result_from_snapshot("online", ready_snapshot)

        result = build_runtime_degradation_result(
            launch_result,
            ready_snapshot,
            [_stage_event()],
            failure_code="SUP_HARDWARE_CONNECT_FAILED",
            failure_stage="hardware_ready",
            message="运行中硬件掉线",
            hardware_state="failed",
        )

        self.assertIsNotNone(result)
        assert result is not None
        self.assertEqual(result.degraded_snapshot.session_state, "failed")
        self.assertEqual(result.degraded_snapshot.failure_code, "SUP_HARDWARE_CONNECT_FAILED")
        self.assertEqual(result.degraded_snapshot.failure_stage, "hardware_ready")
        self.assertEqual(result.launch_result.session_state, "failed")
        self.assertEqual(result.stage_event.event_type, "stage_failed")

    def test_detect_runtime_degradation_result_reports_tcp_disconnect(self) -> None:
        ready_snapshot = _snapshot()
        launch_result = launch_result_from_snapshot("online", ready_snapshot)

        result = detect_runtime_degradation_result(
            launch_result,
            ready_snapshot,
            [_stage_event()],
            tcp_connected=False,
            error_message="socket closed",
        )

        self.assertIsNotNone(result)
        assert result is not None
        self.assertEqual(result.degraded_snapshot.failure_code, "SUP_TCP_CONNECT_FAILED")
        self.assertEqual(result.degraded_snapshot.failure_stage, "tcp_ready")
        self.assertEqual(result.stage_event.event_type, "stage_failed")
        self.assertIn("socket closed", result.status_message)


if __name__ == "__main__":
    unittest.main()
