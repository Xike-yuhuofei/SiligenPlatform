import unittest

from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.preview_gate import StartBlockReason
from hmi_application.preview_session import PreflightBlockReason
from unit.preview_test_support import FakeStatus, build_preview_services, valid_payload


class PreviewPreflightServiceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.state, self.playback, self.preflight, self.authority = build_preview_services()

    def _ready_payload(self) -> None:
        result = self.authority.process_snapshot_payload(valid_payload(), current_dry_run=False)
        self.assertTrue(result.ok)

    def test_current_preview_diagnostic_notice_prefers_fragmentation_notice_over_generic_exception(self) -> None:
        payload = valid_payload()
        payload["preview_validation_classification"] = "pass_with_exception"
        payload["preview_exception_reason"] = "短闭环按例外保留，仍允许确认与启动。"
        payload["preview_diagnostic_code"] = "process_path_fragmentation"

        result = self.authority.process_snapshot_payload(payload, current_dry_run=False)

        self.assertTrue(result.ok)
        notice = self.preflight.current_preview_diagnostic_notice()
        assert notice is not None
        self.assertEqual(notice.title, "路径碎片化提示")
        self.assertIn("短闭环按例外保留", notice.detail)

    def test_current_preview_diagnostic_notice_normalizes_generic_exception_reason(self) -> None:
        payload = valid_payload()
        payload["preview_validation_classification"] = "pass_with_exception"
        payload["preview_exception_reason"] = "span spacing outside configured window but accepted as explicit exception"

        result = self.authority.process_snapshot_payload(payload, current_dry_run=False)

        self.assertTrue(result.ok)
        notice = self.preflight.current_preview_diagnostic_notice()
        assert notice is not None
        self.assertEqual(notice.title, "可继续提示")
        self.assertIn("已按例外规则放行", notice.detail)
        self.assertNotIn("span spacing outside configured window", notice.detail)

    def test_build_preflight_decision_requires_preview_confirmation(self) -> None:
        self._ready_payload()

        decision = self.preflight.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.PREVIEW_CONFIRM_REQUIRED)
        self.assertEqual(self.state.gate.decision_for_start().reason, StartBlockReason.CONFIRM_MISSING)

    def test_build_preflight_decision_rejects_mock_preview_source(self) -> None:
        self._ready_payload()
        self.state.gate.confirm_current_snapshot()
        self.state.preview_source = "mock_synthetic"

        decision = self.preflight.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.INVALID_SOURCE)
        self.assertIn("mock_synthetic", decision.message)

    def test_build_preflight_decision_rejects_non_glue_points_preview_kind(self) -> None:
        self._ready_payload()
        self.state.gate.confirm_current_snapshot()
        self.state.preview_kind = "trajectory_polyline"

        decision = self.preflight.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.INVALID_KIND)
        self.assertIn("trajectory_polyline", decision.message)

    def test_build_preflight_decision_rejects_empty_glue_points(self) -> None:
        self._ready_payload()
        self.state.gate.confirm_current_snapshot()
        self.state.glue_point_count = 0

        decision = self.preflight.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.EMPTY_GLUE_POINTS)
        self.assertIn("非空 glue_points", decision.message)

    def test_build_preflight_decision_rejects_authority_hash_mismatch(self) -> None:
        self._ready_payload()
        self.state.gate.confirm_current_snapshot()
        self.state.current_plan_fingerprint = "hash-2"

        decision = self.preflight.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.HASH_MISMATCH)
        self.assertIn("执行快照不一致", decision.message)

    def test_apply_confirmation_payload_rejects_hash_mismatch(self) -> None:
        self._ready_payload()

        result = self.preflight.apply_confirmation_payload({"confirmed": True, "snapshot_hash": "other"})

        self.assertFalse(result.ok)
        self.assertEqual(self.state.gate.last_error_message, "确认返回的快照哈希与当前预览不一致")


if __name__ == "__main__":
    unittest.main()
