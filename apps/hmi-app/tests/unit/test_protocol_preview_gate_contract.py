import sys
import unittest
from pathlib import Path
from unittest.mock import patch


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.client.protocol import CommandProtocol


class _FakeClient:
    def __init__(self, responses):
        self._responses = list(responses)
        self.calls = []

    def send_request(self, method, params=None, timeout=None):
        self.calls.append((method, params, timeout))
        if self._responses:
            return self._responses.pop(0)
        return {"result": {}}


class PreviewGateProtocolContractTest(unittest.TestCase):
    def test_get_status_reads_backend_interlock_fields(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": True,
                        "machine_state": "Idle",
                        "axes": {},
                        "io": {"estop": True, "estop_known": True, "door": True, "door_known": True},
                        "dispenser": {"valve_open": False, "supply_open": False},
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        status = protocol.get_status()

        self.assertTrue(status.connected)
        self.assertTrue(status.io.estop)
        self.assertTrue(status.io.door)
        self.assertTrue(status.io.estop_known)
        self.assertTrue(status.io.door_known)

    def test_preview_snapshot_success_contract(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "snapshot_id": "s1",
                        "snapshot_hash": "h1",
                        "segment_count": 12,
                        "point_count": 36,
                        "trajectory_polyline": [
                            {"x": 0.0, "y": 0.0},
                            {"x": 10.0, "y": 2.0},
                        ],
                        "total_length_mm": 120.5,
                        "estimated_time_s": 6.0,
                        "generated_at": "2026-03-21T10:00:00Z",
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_preview_snapshot(plan_id="plan-1")

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["snapshot_hash"], "h1")
        self.assertEqual(len(payload["trajectory_polyline"]), 2)
        self.assertEqual(client.calls[0][0], "dxf.preview.snapshot")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-1")

    def test_preview_snapshot_error_contract(self) -> None:
        client = _FakeClient([{"error": {"code": 3201, "message": "DXF not loaded"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_preview_snapshot(plan_id="plan-1")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertIn("DXF not loaded", error)

    def test_preview_snapshot_accepts_max_polyline_points(self) -> None:
        client = _FakeClient([{"result": {"snapshot_id": "s2", "snapshot_hash": "h2", "plan_id": "plan-2"}}])
        protocol = CommandProtocol(client)

        ok, payload, _ = protocol.dxf_preview_snapshot(plan_id="plan-2", max_polyline_points=128)

        self.assertTrue(ok)
        self.assertEqual(payload["snapshot_hash"], "h2")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-2")
        self.assertEqual(client.calls[0][1]["max_polyline_points"], 128)

    def test_preview_confirm_contract(self) -> None:
        client = _FakeClient([{"result": {"confirmed": True, "plan_id": "plan-1", "snapshot_hash": "h1"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_preview_confirm("plan-1", "h1")

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertTrue(payload["confirmed"])
        self.assertEqual(client.calls[0][0], "dxf.preview.confirm")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-1")
        self.assertEqual(client.calls[0][1]["snapshot_hash"], "h1")

    def test_dxf_execute_passes_snapshot_hash(self) -> None:
        client = _FakeClient([{"result": {"ok": True}}])
        protocol = CommandProtocol(client)

        started = protocol.dxf_execute(
            20.0,
            dry_run=False,
            dry_run_speed_mm_s=0.0,
            snapshot_hash="hash-abc",
        )

        self.assertTrue(started)
        self.assertEqual(client.calls[0][0], "dxf.execute")
        self.assertEqual(client.calls[0][1]["snapshot_hash"], "hash-abc")

    def test_dxf_create_artifact_encodes_file_and_calls_new_contract(self) -> None:
        client = _FakeClient([{"result": {"artifact_id": "artifact-1"}}])
        protocol = CommandProtocol(client)
        sample_path = PROJECT_ROOT / "sample.dxf"
        with patch.object(Path, "read_bytes", return_value=b"0\nSECTION\n"):
            ok, payload, error = protocol.dxf_create_artifact(str(sample_path))

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["artifact_id"], "artifact-1")
        self.assertEqual(client.calls[0][0], "dxf.artifact.create")
        self.assertEqual(client.calls[0][1]["filename"], "sample.dxf")
        self.assertTrue(client.calls[0][1]["file_content_b64"])

    def test_dxf_prepare_plan_contract(self) -> None:
        client = _FakeClient([{"result": {"plan_id": "plan-1", "plan_fingerprint": "fp-1"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_prepare_plan("artifact-1", speed_mm_s=20.0)

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["plan_id"], "plan-1")
        self.assertEqual(client.calls[0][0], "dxf.plan.prepare")
        self.assertEqual(client.calls[0][1]["artifact_id"], "artifact-1")

    def test_dxf_start_job_contract(self) -> None:
        client = _FakeClient([{"result": {"job_id": "job-1"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_start_job("plan-1", target_count=3, plan_fingerprint="fp-1")

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["job_id"], "job-1")
        self.assertEqual(client.calls[0][0], "dxf.job.start")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-1")
        self.assertEqual(client.calls[0][1]["target_count"], 3)
        self.assertEqual(client.calls[0][1]["plan_fingerprint"], "fp-1")

    def test_dxf_start_job_returns_backend_error(self) -> None:
        client = _FakeClient([{"error": {"code": 2901, "message": "safety door open"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_start_job("plan-1", target_count=1, plan_fingerprint="fp-1")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertIn("safety door open", error)


if __name__ == "__main__":
    unittest.main()
