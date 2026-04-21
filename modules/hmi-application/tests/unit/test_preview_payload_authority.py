import unittest

from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.preview_gate import PreviewGateState
from unit.preview_test_support import build_preview_services, valid_payload


class PreviewPayloadAuthorityServiceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.state, self.playback, self.preflight, self.authority = build_preview_services()

    def test_process_snapshot_payload_rejects_missing_glue_points(self) -> None:
        result = self.authority.process_snapshot_payload(valid_payload(glue_point_count=0), current_dry_run=False)

        self.assertFalse(result.ok)
        self.assertEqual(result.title, "胶点预览生成失败")
        self.assertEqual(self.state.gate.state, PreviewGateState.FAILED)
        self.assertEqual(
            self.state.gate.last_error_message,
            "当前预览缺少非空 glue_points，不能作为真实在线预览通过依据",
        )
        self.assertIn("缺少非空 glue_points", result.detail)

    def test_process_snapshot_payload_rejects_non_authoritative_preview_source(self) -> None:
        result = self.authority.process_snapshot_payload(
            valid_payload(preview_source="mock_synthetic"),
            current_dry_run=False,
        )

        self.assertFalse(result.ok)
        self.assertEqual(self.state.gate.state, PreviewGateState.FAILED)
        self.assertEqual(
            self.state.gate.last_error_message,
            "当前预览来源为 mock_synthetic，不能作为真实在线预览通过依据",
        )
        self.assertIn("preview_source=mock_synthetic", result.detail)

    def test_process_snapshot_payload_rejects_non_glue_points_preview_kind(self) -> None:
        result = self.authority.process_snapshot_payload(
            valid_payload(preview_kind="trajectory_polyline"),
            current_dry_run=False,
        )

        self.assertFalse(result.ok)
        self.assertEqual(self.state.gate.state, PreviewGateState.FAILED)
        self.assertEqual(
            self.state.gate.last_error_message,
            "当前预览语义为 trajectory_polyline，不是 glue_points，不能作为真实在线预览通过依据",
        )
        self.assertIn("preview_kind=trajectory_polyline", result.detail)

    def test_process_snapshot_payload_rejects_legacy_runtime_snapshot_contract(self) -> None:
        result = self.authority.process_snapshot_payload(
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
        self.assertEqual(self.state.gate.last_error_message, "旧版 runtime_snapshot 预览契约仍在返回")
        self.assertIn("trajectory_polyline/runtime_snapshot", result.detail)
        self.assertIn("plan_id=plan-legacy", result.detail)

    def test_process_snapshot_payload_reports_sampling_warning_from_source_counts(self) -> None:
        result = self.authority.process_snapshot_payload(
            valid_payload(glue_point_count=950, motion_source_point_count=1000),
            current_dry_run=False,
        )

        self.assertTrue(result.ok)
        self.assertIn("胶点预览疑似退化为轨迹采样点", result.preview_warning)
        self.assertIsNotNone(result.motion_preview_meta)
        self.assertEqual(self.state.motion_preview_source, "execution_trajectory_snapshot")
        self.assertEqual(self.state.motion_preview_source_point_count, 1000)
        self.assertTrue(self.state.motion_preview_is_sampled)
        self.assertEqual(self.state.current_plan_id, "plan-1")
        self.assertEqual(self.state.gate.state, PreviewGateState.READY_UNSIGNED)

    def test_process_snapshot_payload_prefers_nested_motion_preview_contract(self) -> None:
        payload = valid_payload()
        payload["motion_preview"] = {
            "source": "execution_trajectory_snapshot",
            "kind": "polyline",
            "source_point_count": 12,
            "point_count": 3,
            "is_sampled": True,
            "sampling_strategy": "execution_trajectory_geometry_preserving_clamp",
            "polyline": [
                {"x": 1.0, "y": 1.0},
                {"x": 4.0, "y": 2.0},
                {"x": 6.0, "y": 3.0},
            ],
        }

        result = self.authority.process_snapshot_payload(payload, current_dry_run=False)

        self.assertTrue(result.ok)
        self.assertEqual(result.motion_preview[0], (1.0, 1.0))
        self.assertEqual(result.motion_preview[-1], (6.0, 3.0))
        self.assertEqual(self.state.motion_preview_source, "execution_trajectory_snapshot")
        self.assertEqual(self.state.motion_preview_point_count, 3)
        self.assertEqual(
            self.state.motion_preview_sampling_strategy,
            "execution_trajectory_geometry_preserving_clamp",
        )
        self.assertEqual(result.motion_preview_warning, "")

    def test_process_snapshot_payload_preserves_authority_glue_reveal_lengths(self) -> None:
        payload = valid_payload()
        payload["glue_reveal_lengths_mm"] = [0.0, 3.25, 6.5]

        result = self.authority.process_snapshot_payload(payload, current_dry_run=False)

        self.assertTrue(result.ok)
        self.assertEqual(result.glue_reveal_lengths_mm, (0.0, 3.25, 6.5))

    def test_process_snapshot_payload_fails_when_glue_reveal_lengths_invalid(self) -> None:
        payload = valid_payload()
        payload["glue_reveal_lengths_mm"] = [0.0, 6.5, 3.25]

        result = self.authority.process_snapshot_payload(payload, current_dry_run=False)

        self.assertFalse(result.ok)
        self.assertEqual(result.title, "胶点预览生成失败")
        self.assertEqual(self.state.gate.last_error_message, "运行时快照缺少有效 glue_reveal_lengths_mm")
        self.assertIn("glue_reveal_lengths_mm", result.detail)

    def test_process_snapshot_payload_stores_preview_diagnostic_code(self) -> None:
        payload = valid_payload()
        payload["preview_validation_classification"] = "pass_with_exception"
        payload["preview_exception_reason"] = "span spacing outside configured window but accepted as explicit exception"
        payload["preview_diagnostic_code"] = "process_path_fragmentation"

        result = self.authority.process_snapshot_payload(payload, current_dry_run=False)

        self.assertTrue(result.ok)
        self.assertEqual(self.state.preview_diagnostic_code, "process_path_fragmentation")
        notice = result.preview_diagnostic_notice
        assert notice is not None
        self.assertEqual(notice.title, "路径碎片化提示")
        self.assertIn("按例外规则放行", notice.detail)
        self.assertNotIn("span spacing outside configured window", notice.detail)
        self.assertIn("路径碎片化提示", self.preflight.build_confirmation_summary())
        self.assertIn("按例外规则放行", self.preflight.build_confirmation_summary())

    def test_process_snapshot_payload_builds_local_playback_model_from_motion_preview(self) -> None:
        result = self.authority.process_snapshot_payload(valid_payload(), current_dry_run=False)

        self.assertTrue(result.ok)
        playback = self.playback.status()
        self.assertTrue(playback.available)
        self.assertEqual(playback.state, "idle")
        self.assertEqual(playback.progress, 0.0)
        self.assertEqual(playback.point_count, 2)
        self.assertEqual(playback.duration_s, 1.0)

    def test_process_snapshot_payload_fails_when_motion_preview_missing(self) -> None:
        result = self.authority.process_snapshot_payload(
            valid_payload(include_motion_preview=False),
            current_dry_run=False,
        )

        self.assertFalse(result.ok)
        self.assertEqual(result.title, "胶点预览生成失败")
        self.assertIn("缺少 motion_preview", result.detail)
        self.assertEqual(self.state.gate.last_error_message, "运行时快照缺少 motion_preview")
        self.assertFalse(self.playback.status().available)

    def test_process_snapshot_payload_fails_when_motion_preview_source_is_not_execution_snapshot(self) -> None:
        payload = valid_payload()
        payload["motion_preview"]["source"] = "execution_polyline"
        payload["motion_preview"]["sampling_strategy"] = "legacy_execution_polyline_compat"

        result = self.authority.process_snapshot_payload(payload, current_dry_run=False)

        self.assertFalse(result.ok)
        self.assertEqual(result.title, "胶点预览生成失败")
        self.assertIn("motion_preview.source=execution_polyline", result.detail)
        self.assertEqual(
            self.state.gate.last_error_message,
            "运行时快照返回了非执行真值运动轨迹来源: execution_polyline",
        )

    def test_process_snapshot_payload_invalidates_plan_when_dry_run_mode_changes(self) -> None:
        result = self.authority.process_snapshot_payload(
            valid_payload(dry_run=True),
            current_dry_run=False,
        )

        self.assertTrue(result.ok)
        self.assertEqual(result.status_message, "运行模式已变更，请刷新预览并重新确认")
        self.assertEqual(self.state.gate.state, PreviewGateState.STALE)
        self.assertEqual(self.state.current_plan_id, "")
        self.assertEqual(self.state.preview_source, "")

    def test_handle_resync_error_clears_owner_state_for_unrecoverable_runtime_loss(self) -> None:
        payload_result = self.authority.process_snapshot_payload(valid_payload(), current_dry_run=False)
        self.assertTrue(payload_result.ok)
        self.authority.mark_resync_pending()
        self.state.last_preview_resync_attempt_ts = 10.0

        result = self.authority.handle_resync_error("plan-1", "plan not found", 3012)

        self.assertIsNotNone(result)
        assert result is not None
        self.assertFalse(self.state.preview_state_resync_pending)
        self.assertEqual(self.state.last_preview_resync_attempt_ts, 0.0)
        self.assertEqual(self.state.current_plan_id, "")
        self.assertEqual(self.state.current_plan_fingerprint, "")
        self.assertEqual(self.state.preview_source, "")
        self.assertEqual(self.state.gate.state, PreviewGateState.FAILED)
        self.assertEqual(result.title, "胶点预览已失效")

    def test_should_request_runtime_resync_enforces_retry_interval(self) -> None:
        self.state.current_plan_id = "plan-1"
        self.authority.mark_resync_pending()

        first = self.authority.should_request_runtime_resync(
            offline_mode=False,
            connected=True,
            production_running=False,
            current_job_id="",
            now_monotonic=10.0,
        )
        second = self.authority.should_request_runtime_resync(
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
