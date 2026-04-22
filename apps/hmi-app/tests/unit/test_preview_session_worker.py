import sys
import unittest
from pathlib import Path
from unittest.mock import patch


PROJECT_ROOT = Path(__file__).resolve().parents[2]
WORKSPACE_ROOT = PROJECT_ROOT.parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))
sys.path.insert(0, str(WORKSPACE_ROOT / "modules" / "hmi-application" / "application"))

from hmi_application.adapters.qt_workers import DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S, PreviewSnapshotWorker


class _FakeTcpClient:
    instances = []

    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.connected = False
        self.disconnected = False
        self.__class__.instances.append(self)

    def connect(self) -> bool:
        self.connected = True
        return True

    def disconnect(self) -> None:
        self.disconnected = True


class _FakeCommandProtocol:
    instances = []

    def __init__(self, client):
        self.client = client
        self.calls = []
        self.prepare_timeout = None
        self.snapshot_timeout = None
        self.__class__.instances.append(self)

    def dxf_prepare_plan(
        self,
        artifact_id,
        speed_mm_s,
        *,
        dry_run=False,
        dry_run_speed_mm_s=0.0,
        timeout=15.0,
    ):
        self.calls.append(("dxf.plan.prepare", artifact_id))
        self.prepare_timeout = timeout
        return True, {
            "plan_id": "plan-1",
            "plan_fingerprint": "fp-1",
            "production_baseline": {
                "baseline_id": "baseline-production-default",
                "baseline_fingerprint": "baseline-fp-20260421",
            },
            "performance_profile": {
                "authority_cache_hit": True,
                "authority_joined_inflight": False,
                "prepare_total_ms": 42,
            },
        }, ""

    def dxf_preview_snapshot(self, plan_id, max_polyline_points=4000, max_glue_points=5000, timeout=15.0):
        self.calls.append(("dxf.preview.snapshot", plan_id))
        self.snapshot_timeout = timeout
        return True, {
            "snapshot_id": "snapshot-1",
            "snapshot_hash": "fp-1",
            "plan_id": plan_id,
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "glue_point_count": 2,
            "glue_points": [{"x": 0.0, "y": 0.0}, {"x": 10.0, "y": 0.0}],
            "glue_reveal_lengths_mm": [0.0, 10.0],
            "motion_preview": {
                "source": "execution_trajectory_snapshot",
                "kind": "polyline",
                "source_point_count": 2,
                "point_count": 2,
                "is_sampled": False,
                "sampling_strategy": "execution_trajectory_geometry_preserving",
                "polyline": [{"x": 0.0, "y": 0.0}, {"x": 10.0, "y": 0.0}],
            },
        }, ""


class PreviewSnapshotWorkerTimeoutTest(unittest.TestCase):
    def setUp(self) -> None:
        _FakeTcpClient.instances.clear()
        _FakeCommandProtocol.instances.clear()

    def test_worker_uses_extended_timeout_for_plan_and_snapshot(self) -> None:
        worker = PreviewSnapshotWorker(
            host="127.0.0.1",
            port=9527,
            artifact_id="artifact-1",
            speed_mm_s=20.0,
            dry_run=False,
            dry_run_speed_mm_s=20.0,
        )
        completed = []
        worker.completed.connect(lambda ok, payload, error: completed.append((ok, payload, error)))

        with patch(
            "hmi_client.client.tcp_client.TcpClient",
            autospec=True,
            side_effect=lambda host, port: _FakeTcpClient(host, port),
        ), patch(
            "hmi_client.client.protocol.CommandProtocol",
            autospec=True,
            side_effect=lambda client: _FakeCommandProtocol(client),
        ):
            worker.run()

        self.assertEqual(len(_FakeTcpClient.instances), 1)
        self.assertTrue(_FakeTcpClient.instances[0].connected)
        self.assertTrue(_FakeTcpClient.instances[0].disconnected)
        self.assertEqual(len(_FakeCommandProtocol.instances), 1)
        self.assertEqual(_FakeCommandProtocol.instances[0].prepare_timeout, DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S)
        self.assertEqual(_FakeCommandProtocol.instances[0].snapshot_timeout, DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S)
        self.assertEqual(len(completed), 1)
        self.assertTrue(completed[0][0])
        self.assertEqual(completed[0][2], "")
        self.assertEqual(completed[0][1]["plan_id"], "plan-1")
        self.assertEqual(completed[0][1]["snapshot_hash"], "fp-1")
        self.assertEqual(completed[0][1]["performance_profile"]["prepare_total_ms"], 42)
        self.assertIn("worker_profile", completed[0][1])
        protocol = _FakeCommandProtocol.instances[0]
        self.assertEqual(protocol.client, _FakeTcpClient.instances[0])
        self.assertEqual(protocol.prepare_timeout, DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S)
        self.assertEqual(protocol.snapshot_timeout, DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S)
        self.assertEqual(
            protocol.calls,
            [
                ("dxf.plan.prepare", "artifact-1"),
                ("dxf.preview.snapshot", "plan-1"),
            ],
        )

    def test_worker_cancelled_before_run_does_not_emit_result(self) -> None:
        worker = PreviewSnapshotWorker(
            host="127.0.0.1",
            port=9527,
            artifact_id="artifact-1",
            speed_mm_s=20.0,
            dry_run=False,
            dry_run_speed_mm_s=20.0,
        )
        completed = []
        worker.completed.connect(lambda ok, payload, error: completed.append((ok, payload, error)))

        worker.cancel()
        with patch(
            "hmi_client.client.tcp_client.TcpClient",
            autospec=True,
            side_effect=lambda host, port: _FakeTcpClient(host, port),
        ), patch(
            "hmi_client.client.protocol.CommandProtocol",
            autospec=True,
            side_effect=lambda client: _FakeCommandProtocol(client),
        ):
            worker.run()

        self.assertEqual(completed, [])


if __name__ == "__main__":
    unittest.main()
