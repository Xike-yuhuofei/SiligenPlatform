import sys
import unittest
from pathlib import Path
from unittest.mock import patch


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.client.gateway_launch import MachineConnectionConfig
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
    def test_connect_hardware_falls_back_to_gateway_connection_config(self) -> None:
        client = _FakeClient([{"result": {"connected": True, "message": "ok"}}])
        protocol = CommandProtocol(client)

        with patch(
            "hmi_client.client.protocol.load_gateway_connection_config",
            return_value=MachineConnectionConfig(card_ip="192.168.0.1", local_ip="192.168.0.200"),
        ):
            ok, message = protocol.connect_hardware()

        self.assertTrue(ok)
        self.assertEqual(message, "ok")
        self.assertEqual(
            client.calls[0],
            ("connect", {"card_ip": "192.168.0.1", "local_ip": "192.168.0.200"}, 15.0),
        )

    def test_connect_hardware_prefers_explicit_params_over_gateway_config(self) -> None:
        client = _FakeClient([{"result": {"connected": True, "message": "ok"}}])
        protocol = CommandProtocol(client)

        with patch(
            "hmi_client.client.protocol.load_gateway_connection_config",
            return_value=MachineConnectionConfig(card_ip="192.168.0.1", local_ip="192.168.0.200"),
        ):
            ok, message = protocol.connect_hardware(card_ip="172.16.0.10", local_ip="172.16.0.20", timeout=5.0)

        self.assertTrue(ok)
        self.assertEqual(message, "ok")
        self.assertEqual(
            client.calls[0],
            ("connect", {"card_ip": "172.16.0.10", "local_ip": "172.16.0.20"}, 5.0),
        )

    def test_get_status_reads_backend_interlock_fields(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": True,
                        "connection_state": "connected",
                        "machine_state": "Idle",
                        "machine_state_reason": "idle",
                        "supervision": {
                            "current_state": "Idle",
                            "requested_state": "Fault",
                            "state_change_in_process": True,
                            "state_reason": "idle",
                            "failure_code": "",
                            "failure_stage": "",
                            "recoverable": True,
                            "updated_at": "2026-03-26T00:00:00Z",
                        },
                        "effective_interlocks": {
                            "estop_active": True,
                            "estop_known": True,
                            "door_open_active": True,
                            "door_open_known": True,
                            "home_boundary_x_active": True,
                            "home_boundary_y_active": False,
                            "positive_escape_only_axes": ["X"],
                            "sources": {
                                "estop": "system_interlock",
                                "door_open": "dispensing_interlock",
                                "home_boundary_x": "motion_home_signal",
                                "home_boundary_y": "none",
                            },
                        },
                        "interlock_latched": True,
                        "active_job_id": "",
                        "active_job_state": "",
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
        self.assertTrue(status.effective_interlocks.estop_active)
        self.assertTrue(status.effective_interlocks.door_open_active)
        self.assertTrue(status.home_boundary_active("X"))
        self.assertEqual(status.effective_interlocks.positive_escape_only_axes, ["X"])
        self.assertEqual(status.supervision.requested_state, "Fault")
        self.assertTrue(status.supervision.state_change_in_process)
        self.assertTrue(status.gate_estop_active())
        self.assertTrue(status.gate_door_active())

    def test_get_status_preserves_degraded_connection_state(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": False,
                        "connection_state": "degraded",
                        "machine_state": "Degraded",
                        "machine_state_reason": "heartbeat_degraded",
                        "axes": {},
                        "io": {},
                        "dispenser": {"valve_open": False, "supply_open": False},
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        status = protocol.get_status()

        self.assertFalse(status.connected)
        self.assertEqual(status.connection_state, "degraded")
        self.assertEqual(status.machine_state_reason, "heartbeat_degraded")

    def test_home_prefers_axis_level_error_messages(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "completed": False,
                        "message": "Homing completed with errors",
                        "error_messages": ["Hardware not connected", "Axis Y timeout"],
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, message = protocol.home()

        self.assertFalse(ok)
        self.assertEqual(message, "Hardware not connected; Axis Y timeout")

    def test_home_prefers_axis_results_over_legacy_error_messages(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "completed": False,
                        "message": "Homing completed with errors",
                        "error_messages": ["Homing failed", "Homing failed"],
                        "axis_results": [
                            {"axis": "X", "success": False, "message": "HOME switch stuck active"},
                            {"axis": "Y", "success": False, "message": "Timeout waiting for settle"},
                        ],
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, message = protocol.home()

        self.assertFalse(ok)
        self.assertEqual(message, "X: HOME switch stuck active; Y: Timeout waiting for settle")

    def test_home_auto_reports_axis_level_failure_messages(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "accepted": True,
                        "summary_state": "failed",
                        "message": "Axes ready at zero",
                        "axis_results": [
                            {
                                "axis": "X",
                                "supervisor_state": "not_homed",
                                "planned_action": "home",
                                "executed": True,
                                "success": False,
                                "reason_code": "home_failed",
                                "message": "Timeout waiting for settle",
                            }
                        ],
                        "total_time_ms": 123,
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, message = protocol.home_auto(["X"])

        self.assertFalse(ok)
        self.assertEqual(message, "X: Timeout waiting for settle")
        self.assertEqual(client.calls[0][0], "home.auto")

    def test_preview_snapshot_success_contract(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "snapshot_id": "s1",
                        "snapshot_hash": "h1",
                        "preview_source": "planned_glue_snapshot",
                        "preview_kind": "glue_points",
                        "segment_count": 12,
                        "point_count": 36,
                        "glue_point_count": 36,
                        "glue_points": [
                            {"x": 0.0, "y": 0.0},
                            {"x": 10.0, "y": 2.0},
                        ],
                        "execution_point_count": 48,
                        "execution_polyline_source_point_count": 48,
                        "execution_polyline_point_count": 2,
                        "execution_polyline": [
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
        self.assertEqual(payload["preview_source"], "planned_glue_snapshot")
        self.assertEqual(payload["preview_kind"], "glue_points")
        self.assertEqual(len(payload["glue_points"]), 2)
        self.assertEqual(len(payload["execution_polyline"]), 2)
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
        client = _FakeClient([{"result": {"snapshot_id": "s2", "snapshot_hash": "h2", "plan_id": "plan-2", "preview_source": "planned_glue_snapshot", "preview_kind": "glue_points"}}])
        protocol = CommandProtocol(client)

        ok, payload, _ = protocol.dxf_preview_snapshot(plan_id="plan-2", max_polyline_points=128)

        self.assertTrue(ok)
        self.assertEqual(payload["snapshot_hash"], "h2")
        self.assertEqual(payload["preview_source"], "planned_glue_snapshot")
        self.assertEqual(payload["preview_kind"], "glue_points")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-2")
        self.assertEqual(client.calls[0][1]["max_polyline_points"], 128)

    def test_preview_snapshot_error_details_contract(self) -> None:
        client = _FakeClient([{"error": {"code": 3012, "message": "plan not found"}}])
        protocol = CommandProtocol(client)

        ok, payload, error, error_code = protocol.dxf_preview_snapshot_with_error_details(plan_id="plan-x")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertEqual(error, "plan not found")
        self.assertEqual(error_code, 3012)
        self.assertEqual(client.calls[0][0], "dxf.preview.snapshot")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-x")

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

    def test_estop_reset_contract(self) -> None:
        client = _FakeClient([{"result": {"reset": True, "message": "Emergency stop reset"}}])
        protocol = CommandProtocol(client)

        ok, message = protocol.estop_reset()

        self.assertTrue(ok)
        self.assertEqual(message, "Emergency stop reset")
        self.assertEqual(client.calls[0][0], "estop.reset")

    def test_mock_io_set_contract(self) -> None:
        client = _FakeClient([{"result": {"estop": False, "door": True, "limit_x_neg": True, "limit_y_neg": False}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.mock_io_set(door=True, limit_x_neg=True)

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertTrue(payload["door"])
        self.assertTrue(payload["limit_x_neg"])
        self.assertEqual(client.calls[0][0], "mock.io.set")
        self.assertEqual(client.calls[0][1], {"door": True, "limit_x_neg": True})

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
