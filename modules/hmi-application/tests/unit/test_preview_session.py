import sys
import types
import unittest
from dataclasses import dataclass
from pathlib import Path
from unittest.mock import patch


WORKSPACE_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(WORKSPACE_ROOT / "modules" / "hmi-application" / "application"))

from hmi_application.preview_gate import PreviewGateState, StartBlockReason
from hmi_application.preview_session import PreflightBlockReason, PreviewSessionOwner, PreviewSnapshotWorker


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
    preview_kind: str = "glue_points",
    dry_run: bool = False,
    glue_point_count: int = 3,
    execution_source_point_count: int = 10,
    include_motion_preview: bool = True,
) -> dict:
    glue_points = [
        {"x": 0.0, "y": 0.0},
        {"x": 3.0, "y": 0.0},
        {"x": 6.0, "y": 0.0},
    ]
    if glue_point_count <= 0:
        glue_points = []
    elif glue_point_count > len(glue_points):
        glue_points.extend({"x": float(index), "y": 0.0} for index in range(len(glue_points), glue_point_count))
    payload = {
        "snapshot_id": "snapshot-1",
        "snapshot_hash": "hash-1",
        "plan_id": "plan-1",
        "preview_source": preview_source,
        "preview_kind": preview_kind,
        "segment_count": 2,
        "glue_point_count": glue_point_count,
        "glue_points": glue_points,
        "execution_polyline_source_point_count": execution_source_point_count,
        "execution_polyline_point_count": 2,
        "execution_polyline": [
            {"x": 0.0, "y": 0.0},
            {"x": 6.0, "y": 0.0},
        ],
        "total_length_mm": 6.0,
        "estimated_time_s": 1.0,
        "generated_at": "2026-03-28T00:00:00Z",
        "dry_run": dry_run,
    }
    if include_motion_preview:
        payload["motion_preview"] = {
            "source": "execution_trajectory_snapshot",
            "kind": "polyline",
            "source_point_count": execution_source_point_count,
            "point_count": 2,
            "is_sampled": execution_source_point_count > 2,
            "sampling_strategy": "fixed_spacing_corner_preserving",
            "polyline": [
                {"x": 0.0, "y": 0.0},
                {"x": 6.0, "y": 0.0},
            ],
        }
    return payload


class _WorkerFakeClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 9527) -> None:
        self.host = host
        self.port = port
        self.connected = False

    def connect(self) -> bool:
        self.connected = True
        return True

    def disconnect(self) -> None:
        self.connected = False


class _WorkerFakeProtocol:
    calls: list[tuple] = []

    def __init__(self, client) -> None:
        self.client = client

    def dxf_prepare_plan(
        self,
        artifact_id: str,
        speed_mm_s: float,
        dry_run: bool = False,
        dry_run_speed_mm_s: float = 0.0,
        timeout: float = 15.0,
    ) -> tuple:
        type(self).calls.append(
            ("dxf.plan.prepare", artifact_id, speed_mm_s, dry_run, dry_run_speed_mm_s, timeout)
        )
        return True, {
            "plan_id": "plan-1",
            "plan_fingerprint": "fp-1",
            "performance_profile": {
                "authority_cache_hit": False,
                "authority_joined_inflight": True,
                "prepare_total_ms": 123,
            },
        }, ""

    def dxf_preview_snapshot(
        self,
        plan_id: str,
        max_polyline_points: int = 4000,
        max_glue_points: int = 5000,
        timeout: float = 15.0,
    ) -> tuple:
        type(self).calls.append(("dxf.preview.snapshot", plan_id, max_polyline_points, max_glue_points, timeout))
        return True, {"snapshot_id": "snapshot-1", "preview_source": "planned_glue_snapshot", "preview_kind": "glue_points"}, ""


def _worker_import_modules() -> dict[str, object]:
    hmi_client_package = types.ModuleType("hmi_client")
    hmi_client_package.__path__ = []  # type: ignore[attr-defined]
    hmi_client_client_package = types.ModuleType("hmi_client.client")
    hmi_client_client_package.__path__ = []  # type: ignore[attr-defined]
    client_package = types.ModuleType("client")
    client_package.__path__ = []  # type: ignore[attr-defined]
    tcp_module = types.ModuleType("client.tcp_client")
    tcp_module.TcpClient = _WorkerFakeClient
    protocol_module = types.ModuleType("client.protocol")
    protocol_module.CommandProtocol = _WorkerFakeProtocol
    return {
        "hmi_client": hmi_client_package,
        "hmi_client.client": hmi_client_client_package,
        "hmi_client.client.tcp_client": tcp_module,
        "hmi_client.client.protocol": protocol_module,
        "client": client_package,
        "client.tcp_client": tcp_module,
        "client.protocol": protocol_module,
    }


class PreviewSessionOwnerTest(unittest.TestCase):
    def setUp(self) -> None:
        self.owner = PreviewSessionOwner()

    def test_process_snapshot_payload_rejects_missing_glue_points(self) -> None:
        result = self.owner.process_snapshot_payload(_valid_payload(glue_point_count=0), current_dry_run=False)

        self.assertFalse(result.ok)
        self.assertEqual(result.title, "胶点预览生成失败")
        self.assertEqual(self.owner.gate.state, PreviewGateState.FAILED)
        self.assertEqual(
            self.owner.gate.last_error_message,
            "当前预览缺少非空 glue_points，不能作为真实在线预览通过依据",
        )
        self.assertIn("缺少非空 glue_points", result.detail)

    def test_process_snapshot_payload_rejects_non_authoritative_preview_source(self) -> None:
        result = self.owner.process_snapshot_payload(
            _valid_payload(preview_source="mock_synthetic"),
            current_dry_run=False,
        )

        self.assertFalse(result.ok)
        self.assertEqual(self.owner.gate.state, PreviewGateState.FAILED)
        self.assertEqual(
            self.owner.gate.last_error_message,
            "当前预览来源为 mock_synthetic，不能作为真实在线预览通过依据",
        )
        self.assertIn("preview_source=mock_synthetic", result.detail)

    def test_process_snapshot_payload_rejects_non_glue_points_preview_kind(self) -> None:
        result = self.owner.process_snapshot_payload(
            _valid_payload(preview_kind="trajectory_polyline"),
            current_dry_run=False,
        )

        self.assertFalse(result.ok)
        self.assertEqual(self.owner.gate.state, PreviewGateState.FAILED)
        self.assertEqual(
            self.owner.gate.last_error_message,
            "当前预览语义为 trajectory_polyline，不是 glue_points，不能作为真实在线预览通过依据",
        )
        self.assertIn("preview_kind=trajectory_polyline", result.detail)

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
        self.assertEqual(self.owner.gate.last_error_message, "旧版 runtime_snapshot 预览契约仍在返回")
        self.assertIn("trajectory_polyline/runtime_snapshot", result.detail)
        self.assertIn("plan_id=plan-legacy", result.detail)

    def test_process_snapshot_payload_reports_sampling_warning_from_source_counts(self) -> None:
        result = self.owner.process_snapshot_payload(
            _valid_payload(glue_point_count=950, execution_source_point_count=1000),
            current_dry_run=False,
        )

        self.assertTrue(result.ok)
        self.assertEqual(result.execution_polyline_source_point_count, 1000)
        self.assertIn("胶点预览疑似退化为轨迹采样点", result.preview_warning)
        self.assertIsNotNone(result.motion_preview_meta)
        self.assertEqual(self.owner.state.motion_preview_source, "execution_trajectory_snapshot")
        self.assertEqual(self.owner.state.motion_preview_source_point_count, 1000)
        self.assertTrue(self.owner.state.motion_preview_is_sampled)
        self.assertEqual(self.owner.state.current_plan_id, "plan-1")
        self.assertEqual(self.owner.gate.state, PreviewGateState.READY_UNSIGNED)

    def test_process_snapshot_payload_prefers_nested_motion_preview_contract(self) -> None:
        payload = _valid_payload()
        payload["motion_preview"] = {
            "source": "execution_trajectory_snapshot",
            "kind": "polyline",
            "source_point_count": 12,
            "point_count": 3,
            "is_sampled": True,
            "sampling_strategy": "fixed_spacing_corner_preserving",
            "polyline": [
                {"x": 1.0, "y": 1.0},
                {"x": 4.0, "y": 2.0},
                {"x": 6.0, "y": 3.0},
            ],
        }

        result = self.owner.process_snapshot_payload(payload, current_dry_run=False)

        self.assertTrue(result.ok)
        self.assertEqual(result.motion_preview[0], (1.0, 1.0))
        self.assertEqual(result.motion_preview[-1], (6.0, 3.0))
        self.assertEqual(self.owner.state.motion_preview_source, "execution_trajectory_snapshot")
        self.assertEqual(self.owner.state.motion_preview_point_count, 3)
        self.assertEqual(self.owner.state.motion_preview_sampling_strategy, "fixed_spacing_corner_preserving")
        self.assertEqual(result.motion_preview_warning, "")

    def test_process_snapshot_payload_falls_back_to_execution_polyline_when_motion_preview_missing(self) -> None:
        result = self.owner.process_snapshot_payload(
            _valid_payload(include_motion_preview=False),
            current_dry_run=False,
        )

        self.assertTrue(result.ok)
        self.assertEqual(result.motion_preview, result.execution_polyline)
        self.assertIn("回退到 execution_polyline", result.motion_preview_warning)
        self.assertEqual(self.owner.state.motion_preview_source, "legacy_execution_polyline")
        self.assertEqual(self.owner.state.motion_preview_kind, "polyline")

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
        payload_result = self.owner.process_snapshot_payload(_valid_payload(), current_dry_run=False)
        self.assertTrue(payload_result.ok)
        self.owner.gate.confirm_current_snapshot()
        self.owner.state.preview_source = "mock_synthetic"

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

    def test_build_preflight_decision_rejects_non_glue_points_preview_kind(self) -> None:
        payload_result = self.owner.process_snapshot_payload(_valid_payload(), current_dry_run=False)
        self.assertTrue(payload_result.ok)
        self.owner.gate.confirm_current_snapshot()
        self.owner.state.preview_kind = "trajectory_polyline"

        decision = self.owner.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=_FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.INVALID_KIND)
        self.assertIn("trajectory_polyline", decision.message)

    def test_build_preflight_decision_rejects_empty_glue_points(self) -> None:
        payload_result = self.owner.process_snapshot_payload(_valid_payload(), current_dry_run=False)
        self.assertTrue(payload_result.ok)
        self.owner.gate.confirm_current_snapshot()
        self.owner.state.glue_point_count = 0

        decision = self.owner.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=_FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.EMPTY_GLUE_POINTS)
        self.assertIn("非空 glue_points", decision.message)

    def test_build_preflight_decision_rejects_authority_hash_mismatch(self) -> None:
        payload_result = self.owner.process_snapshot_payload(_valid_payload(), current_dry_run=False)
        self.assertTrue(payload_result.ok)
        self.owner.gate.confirm_current_snapshot()
        self.owner.state.current_plan_fingerprint = "hash-2"

        decision = self.owner.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=_FakeStatus(),
            dry_run=False,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.HASH_MISMATCH)
        self.assertIn("执行快照不一致", decision.message)

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


class PreviewSnapshotWorkerTest(unittest.TestCase):
    def setUp(self) -> None:
        _WorkerFakeProtocol.calls = []

    def test_worker_uses_300s_timeout_for_prepare_and_snapshot(self) -> None:
        worker = PreviewSnapshotWorker(
            host="127.0.0.1",
            port=9527,
            artifact_id="artifact-1",
            speed_mm_s=20.0,
            dry_run=False,
            dry_run_speed_mm_s=20.0,
        )
        emitted = []
        worker.completed.connect(lambda ok, payload, error: emitted.append((ok, payload, error)))

        with patch.dict(sys.modules, _worker_import_modules(), clear=False):
            worker.run()

        self.assertEqual(
            _WorkerFakeProtocol.calls,
            [
                ("dxf.plan.prepare", "artifact-1", 20.0, False, 20.0, 300.0),
                ("dxf.preview.snapshot", "plan-1", 4000, 5000, 300.0),
            ],
        )
        self.assertTrue(emitted)
        self.assertTrue(emitted[0][0])
        self.assertEqual(emitted[0][1]["plan_id"], "plan-1")
        self.assertEqual(emitted[0][1]["snapshot_hash"], "fp-1")
        self.assertEqual(emitted[0][1]["performance_profile"]["prepare_total_ms"], 123)
        self.assertEqual(emitted[0][1]["worker_profile"]["plan_prepare_rpc_ms"] >= 0, True)

    def test_worker_cancelled_before_run_does_not_emit_result(self) -> None:
        worker = PreviewSnapshotWorker(
            host="127.0.0.1",
            port=9527,
            artifact_id="artifact-1",
            speed_mm_s=20.0,
            dry_run=False,
            dry_run_speed_mm_s=20.0,
        )
        emitted = []
        worker.completed.connect(lambda ok, payload, error: emitted.append((ok, payload, error)))

        worker.cancel()
        with patch.dict(sys.modules, _worker_import_modules(), clear=False):
            worker.run()

        self.assertEqual(emitted, [])


if __name__ == "__main__":
    unittest.main()
