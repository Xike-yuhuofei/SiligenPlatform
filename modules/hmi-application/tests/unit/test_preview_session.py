import sys
import unittest
from dataclasses import dataclass
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(WORKSPACE_ROOT / "modules" / "hmi-application" / "application"))

from hmi_application.preview_gate import PreviewGateState, StartBlockReason
from hmi_application.preview_session import PreflightBlockReason, PreviewSessionOwner


@dataclass
class _FakeStatus:
    connected: bool = True
    estop_known: bool = True
    door_known: bool = True
    estop_active: bool = False
    door_active: bool = False

    def gate_estop_known(self) -> bool:
        return self.estop_known

    def gate_door_known(self) -> bool:
        return self.door_known

    def gate_estop_active(self) -> bool:
        return self.estop_active

    def gate_door_active(self) -> bool:
        return self.door_active


def _valid_payload(
    *,
    preview_source: str = "planned_glue_snapshot",
    dry_run: bool = False,
    glue_point_count: int = 3,
    execution_source_point_count: int = 10,
) -> dict:
    glue_points = [
        {"x": 0.0, "y": 0.0},
        {"x": 3.0, "y": 0.0},
        {"x": 6.0, "y": 0.0},
    ]
    if glue_point_count > len(glue_points):
        glue_points.extend({"x": float(index), "y": 0.0} for index in range(len(glue_points), glue_point_count))
    return {
        "snapshot_id": "snapshot-1",
        "snapshot_hash": "hash-1",
        "plan_id": "plan-1",
        "preview_source": preview_source,
        "preview_kind": "glue_points",
        "segment_count": 2,
        "glue_point_count": glue_point_count,
        "glue_points": glue_points,
        "execution_polyline_source_point_count": execution_source_point_count,
        "execution_polyline": [
            {"x": 0.0, "y": 0.0},
            {"x": 6.0, "y": 0.0},
        ],
        "total_length_mm": 6.0,
        "estimated_time_s": 1.0,
        "generated_at": "2026-03-28T00:00:00Z",
        "dry_run": dry_run,
    }


class PreviewSessionOwnerTest(unittest.TestCase):
    def setUp(self) -> None:
        self.owner = PreviewSessionOwner()

    def test_process_snapshot_payload_rejects_missing_glue_points(self) -> None:
        result = self.owner.process_snapshot_payload(
            {
                "snapshot_id": "snapshot-1",
                "snapshot_hash": "hash-1",
                "plan_id": "plan-1",
                "preview_source": "planned_glue_snapshot",
            },
            current_dry_run=False,
        )

        self.assertFalse(result.ok)
        self.assertEqual(result.title, "胶点预览生成失败")
        self.assertEqual(self.owner.gate.state, PreviewGateState.FAILED)
        self.assertIn("缺少 glue_points", result.detail)

    def test_process_snapshot_payload_rejects_legacy_runtime_snapshot_contract(self) -> None:
        result = self.owner.process_snapshot_payload(
            {
                "snapshot_id": "snapshot-legacy",
                "snapshot_hash": "hash-legacy",
                "plan_id": "plan-legacy",
                "preview_source": "runtime_snapshot",
                "trajectory_polyline": [{"x": 0.0, "y": 0.0}],
            },
            current_dry_run=False,
        )

        self.assertFalse(result.ok)
        self.assertEqual(self.owner.gate.last_error_message, "运行时仍返回旧版轨迹预览契约")
        self.assertIn("trajectory_polyline/runtime_snapshot", result.detail)

    def test_process_snapshot_payload_reports_sampling_warning_from_source_counts(self) -> None:
        result = self.owner.process_snapshot_payload(
            _valid_payload(glue_point_count=950, execution_source_point_count=1000),
            current_dry_run=False,
        )

        self.assertTrue(result.ok)
        self.assertEqual(result.execution_polyline_source_point_count, 1000)
        self.assertIn("胶点预览疑似退化为轨迹采样点", result.preview_warning)
        self.assertEqual(self.owner.state.current_plan_id, "plan-1")
        self.assertEqual(self.owner.gate.state, PreviewGateState.READY_UNSIGNED)

    def test_process_snapshot_payload_invalidates_plan_when_dry_run_mode_changes(self) -> None:
        result = self.owner.process_snapshot_payload(
            _valid_payload(dry_run=True),
            current_dry_run=False,
        )

        self.assertTrue(result.ok)
        self.assertEqual(result.status_message, "运行模式已变更，请刷新预览并重新确认")
        self.assertEqual(self.owner.gate.state, PreviewGateState.STALE)
        self.assertEqual(self.owner.state.current_plan_id, "")
        self.assertEqual(self.owner.state.preview_source, "")

    def test_build_preflight_decision_requires_preview_confirmation(self) -> None:
        payload_result = self.owner.process_snapshot_payload(_valid_payload(), current_dry_run=False)
        self.assertTrue(payload_result.ok)

        decision = self.owner.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=_FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.PREVIEW_CONFIRM_REQUIRED)
        self.assertEqual(self.owner.gate.decision_for_start().reason, StartBlockReason.CONFIRM_MISSING)

    def test_build_preflight_decision_rejects_mock_preview_source(self) -> None:
        payload_result = self.owner.process_snapshot_payload(
            _valid_payload(preview_source="mock_synthetic"),
            current_dry_run=False,
        )
        self.assertTrue(payload_result.ok)
        self.owner.gate.confirm_current_snapshot()

        decision = self.owner.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=_FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.INVALID_SOURCE)
        self.assertIn("mock_synthetic", decision.message)

    def test_handle_resync_error_clears_owner_state_for_unrecoverable_runtime_loss(self) -> None:
        payload_result = self.owner.process_snapshot_payload(_valid_payload(), current_dry_run=False)
        self.assertTrue(payload_result.ok)
        self.owner.mark_resync_pending()
        self.owner.state.last_preview_resync_attempt_ts = 10.0

        result = self.owner.handle_resync_error("plan-1", "plan not found", 3012)

        self.assertIsNotNone(result)
        assert result is not None
        self.assertFalse(self.owner.state.preview_state_resync_pending)
        self.assertEqual(self.owner.state.last_preview_resync_attempt_ts, 0.0)
        self.assertEqual(self.owner.state.current_plan_id, "")
        self.assertEqual(self.owner.state.current_plan_fingerprint, "")
        self.assertEqual(self.owner.state.preview_source, "")
        self.assertEqual(self.owner.gate.state, PreviewGateState.FAILED)
        self.assertEqual(result.title, "胶点预览已失效")

    def test_should_request_runtime_resync_enforces_retry_interval(self) -> None:
        self.owner.state.current_plan_id = "plan-1"
        self.owner.mark_resync_pending()

        first = self.owner.should_request_runtime_resync(
            offline_mode=False,
            connected=True,
            production_running=False,
            current_job_id="",
            now_monotonic=10.0,
        )
        second = self.owner.should_request_runtime_resync(
            offline_mode=False,
            connected=True,
            production_running=False,
            current_job_id="",
            now_monotonic=11.0,
        )

        self.assertTrue(first)
        self.assertFalse(second)


if __name__ == "__main__":
    unittest.main()
