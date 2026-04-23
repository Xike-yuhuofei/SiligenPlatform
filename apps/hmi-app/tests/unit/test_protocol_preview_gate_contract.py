import sys
import unittest
from pathlib import Path
from unittest.mock import patch


PROJECT_ROOT = Path(__file__).resolve().parents[2]
WORKSPACE_ROOT = PROJECT_ROOT.parents[1]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
sys.path.insert(0, str(PROJECT_ROOT / "src"))
sys.path.insert(0, str(TEST_KIT_SRC))

from hmi_client.client.gateway_launch import MachineConnectionConfig
from hmi_client.client.protocol import CommandProtocol, JobTransitionResult
from test_kit.preview_snapshot_fixture import build_preview_snapshot_success_result


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
            autospec=True,
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
            autospec=True,
            return_value=MachineConnectionConfig(card_ip="192.168.0.1", local_ip="192.168.0.200"),
        ):
            ok, message = protocol.connect_hardware(card_ip="172.16.0.10", local_ip="172.16.0.20", timeout=5.0)

        self.assertTrue(ok)
        self.assertEqual(message, "ok")
        self.assertEqual(
            client.calls[0],
            ("connect", {"card_ip": "172.16.0.10", "local_ip": "172.16.0.20"}, 5.0),
        )

    def test_dispenser_start_returns_success_details(self) -> None:
        client = _FakeClient([{"result": {"dispensing": True}}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.dispenser_start(count=20, interval_ms=40, duration_ms=40)

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertIsNone(error_code)
        self.assertEqual(
            client.calls[0],
            ("dispenser.start", {"count": 20, "interval_ms": 40, "duration_ms": 40}, None),
        )

    def test_dispenser_start_returns_backend_error_details(self) -> None:
        client = _FakeClient([{"error": {"code": 2701, "message": "supply not open"}}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.dispenser_start(count=10, interval_ms=1000, duration_ms=15)

        self.assertFalse(ok)
        self.assertEqual(error, "supply not open")
        self.assertEqual(error_code, 2701)

    def test_dispenser_stop_returns_backend_error_without_code(self) -> None:
        client = _FakeClient([{"error": {"message": "stop timeout"}}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.dispenser_stop()

        self.assertFalse(ok)
        self.assertEqual(error, "stop timeout")
        self.assertIsNone(error_code)

    def test_dispenser_pause_returns_backend_error_details(self) -> None:
        client = _FakeClient([{"error": {"code": 2811, "message": "manual dispenser pause is forbidden while DXF job is active"}}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.dispenser_pause()

        self.assertFalse(ok)
        self.assertEqual(error, "manual dispenser pause is forbidden while DXF job is active")
        self.assertEqual(error_code, 2811)

    def test_dispenser_resume_returns_success_contract(self) -> None:
        client = _FakeClient([{"result": {"resumed": True}}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.dispenser_resume()

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertIsNone(error_code)

    def test_supply_open_returns_backend_error_details(self) -> None:
        client = _FakeClient([{"error": {"code": 2842, "message": "door interlock active"}}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.supply_open()

        self.assertFalse(ok)
        self.assertEqual(error, "door interlock active")
        self.assertEqual(error_code, 2842)

    def test_supply_close_returns_missing_result_fallback(self) -> None:
        client = _FakeClient([{"success": True, "version": "1.0"}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.supply_close()

        self.assertFalse(ok)
        self.assertEqual(error, "响应缺少 result 字段")
        self.assertIsNone(error_code)

    def test_move_to_returns_gateway_success_contract(self) -> None:
        client = _FakeClient([{"result": {"moving": True}}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.move_to(1.0, 1.5, 8.0)

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertIsNone(error_code)
        self.assertEqual(client.calls[0], ("move", {"x": 1.0, "y": 1.5, "speed": 8.0}, None))

    def test_move_to_returns_backend_error_details(self) -> None:
        client = _FakeClient([{"error": {"code": 2401, "message": "motion_not_ready"}}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.move_to(1.0, 1.5, 8.0)

        self.assertFalse(ok)
        self.assertEqual(error, "motion_not_ready")
        self.assertEqual(error_code, 2401)

    def test_move_to_handles_completion_style_result_contract(self) -> None:
        client = _FakeClient([{"result": {"motion_completed": False, "status_message": "Move rejected"}}])
        protocol = CommandProtocol(client)

        ok, error, error_code = protocol.move_to(1.0, 1.5, 8.0)

        self.assertFalse(ok)
        self.assertEqual(error, "Move rejected")
        self.assertIsNone(error_code)

    def test_get_status_reads_backend_interlock_fields(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": True,
                        "connection_state": "connected",
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
                        "safety_boundary": {
                            "state": "blocked",
                            "motion_permitted": False,
                            "process_output_permitted": False,
                            "estop_active": True,
                            "estop_known": True,
                            "door_open_active": True,
                            "door_open_known": True,
                            "interlock_latched": True,
                            "blocking_reasons": ["estop_active", "door_open_active", "interlock_latched"],
                        },
                        "action_capabilities": {
                            "motion_commands_permitted": False,
                            "manual_output_commands_permitted": False,
                            "manual_dispenser_pause_permitted": False,
                            "manual_dispenser_resume_permitted": False,
                            "active_job_present": False,
                            "estop_reset_permitted": True,
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
                        "job_execution": {
                            "job_id": "",
                            "plan_id": "",
                            "plan_fingerprint": "",
                            "state": "idle",
                            "target_count": 0,
                            "completed_count": 0,
                            "current_cycle": 0,
                            "current_segment": 0,
                            "total_segments": 0,
                            "cycle_progress_percent": 0,
                            "overall_progress_percent": 0,
                            "elapsed_seconds": 0.0,
                            "error_message": "",
                            "dry_run": False,
                        },
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
        self.assertEqual(status.device_mode, "production")
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
        self.assertEqual(status.job_execution.state, "idle")
        self.assertEqual(status.safety_boundary.state, "blocked")
        self.assertFalse(status.safety_boundary.motion_permitted)
        self.assertFalse(status.safety_boundary.process_output_permitted)
        self.assertFalse(status.action_capabilities.motion_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_output_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_dispenser_pause_permitted)
        self.assertFalse(status.action_capabilities.manual_dispenser_resume_permitted)
        self.assertFalse(status.action_capabilities.active_job_present)
        self.assertTrue(status.action_capabilities.estop_reset_permitted)
        self.assertEqual(
            status.safety_boundary.blocking_reasons,
            ["estop_active", "door_open_active", "interlock_latched"],
        )

    def test_get_status_prefers_supervision_for_runtime_state(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": True,
                        "connection_state": "connected",
                        "supervision": {
                            "current_state": "Running",
                            "requested_state": "Running",
                            "state_change_in_process": False,
                            "state_reason": "job_running",
                            "failure_code": "",
                            "failure_stage": "",
                            "recoverable": True,
                            "updated_at": "2026-04-01T00:00:00Z",
                        },
                        "effective_interlocks": {
                            "estop_active": False,
                            "estop_known": True,
                            "door_open_active": False,
                            "door_open_known": True,
                            "home_boundary_x_active": False,
                            "home_boundary_y_active": False,
                            "positive_escape_only_axes": [],
                            "sources": {},
                        },
                        "interlock_latched": False,
                        "job_execution": {
                            "job_id": "job-1",
                            "plan_id": "plan-1",
                            "plan_fingerprint": "hash-1",
                            "state": "running",
                            "target_count": 2,
                            "completed_count": 0,
                            "current_cycle": 1,
                            "current_segment": 10,
                            "total_segments": 100,
                            "cycle_progress_percent": 10,
                            "overall_progress_percent": 5,
                            "elapsed_seconds": 1.5,
                            "error_message": "",
                            "dry_run": False,
                        },
                        "axes": {},
                        "io": {"estop": False, "estop_known": True, "door": False, "door_known": True},
                        "dispenser": {"valve_open": False, "supply_open": False},
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        status = protocol.get_status()

        self.assertEqual(status.device_mode, "production")
        self.assertEqual(status.runtime_state, "Running")
        self.assertEqual(status.runtime_state_reason, "job_running")
        self.assertEqual(status.job_execution.job_id, "job-1")
        self.assertEqual(status.job_execution.state, "running")
        self.assertTrue(status.action_capabilities.motion_commands_permitted)
        self.assertTrue(status.action_capabilities.manual_output_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_dispenser_pause_permitted)
        self.assertFalse(status.action_capabilities.manual_dispenser_resume_permitted)
        self.assertTrue(status.action_capabilities.active_job_present)
        self.assertFalse(status.action_capabilities.estop_reset_permitted)
        self.assertEqual(status.runtime_state, "Running")
        self.assertEqual(status.runtime_state_reason, "job_running")

    def test_get_status_preserves_degraded_connection_state(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": False,
                        "connection_state": "degraded",
                        "supervision": {
                            "current_state": "Degraded",
                            "requested_state": "Degraded",
                            "state_change_in_process": False,
                            "state_reason": "heartbeat_degraded",
                            "failure_code": "heartbeat_degraded",
                            "failure_stage": "runtime_status",
                            "recoverable": True,
                            "updated_at": "2026-04-01T00:00:00Z",
                        },
                        "job_execution": {
                            "job_id": "",
                            "plan_id": "",
                            "plan_fingerprint": "",
                            "state": "idle",
                            "target_count": 0,
                            "completed_count": 0,
                            "current_cycle": 0,
                            "current_segment": 0,
                            "total_segments": 0,
                            "cycle_progress_percent": 0,
                            "overall_progress_percent": 0,
                            "elapsed_seconds": 0.0,
                            "error_message": "",
                            "dry_run": False,
                        },
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
        self.assertEqual(status.runtime_state, "Degraded")
        self.assertEqual(status.runtime_state_reason, "heartbeat_degraded")
        self.assertFalse(status.action_capabilities.motion_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_output_commands_permitted)
        self.assertFalse(status.action_capabilities.active_job_present)

    def test_get_status_marks_estop_unknown_when_disconnected_and_no_authoritative_signal(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": False,
                        "connection_state": "disconnected",
                        "supervision": {
                            "current_state": "Disconnected",
                            "requested_state": "Disconnected",
                            "state_change_in_process": False,
                            "state_reason": "hardware_disconnected",
                            "failure_code": "",
                            "failure_stage": "",
                            "recoverable": True,
                            "updated_at": "2026-04-01T00:00:00Z",
                        },
                        "effective_interlocks": {
                            "estop_active": False,
                            "estop_known": False,
                            "door_open_active": False,
                            "door_open_known": False,
                            "home_boundary_x_active": False,
                            "home_boundary_y_active": False,
                            "positive_escape_only_axes": [],
                            "sources": {
                                "estop": "unknown",
                                "door_open": "unknown",
                                "home_boundary_x": "none",
                                "home_boundary_y": "none",
                            },
                        },
                        "interlock_latched": False,
                        "job_execution": {
                            "job_id": "",
                            "plan_id": "",
                            "plan_fingerprint": "",
                            "state": "idle",
                            "target_count": 0,
                            "completed_count": 0,
                            "current_cycle": 0,
                            "current_segment": 0,
                            "total_segments": 0,
                            "cycle_progress_percent": 0,
                            "overall_progress_percent": 0,
                            "elapsed_seconds": 0.0,
                            "error_message": "",
                            "dry_run": False,
                        },
                        "axes": {},
                        "io": {"estop": False, "estop_known": False, "door": False, "door_known": False},
                        "dispenser": {"valve_open": False, "supply_open": False},
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        status = protocol.get_status()

        self.assertFalse(status.connected)
        self.assertEqual(status.runtime_state, "Disconnected")
        self.assertEqual(status.runtime_state_reason, "hardware_disconnected")
        self.assertFalse(status.io.estop_known)
        self.assertFalse(status.effective_interlocks.estop_known)
        self.assertFalse(status.gate_estop_known())
        self.assertFalse(status.gate_estop_active())
        self.assertEqual(status.effective_interlocks.sources["estop"], "unknown")
        self.assertEqual(status.safety_boundary.state, "unknown")
        self.assertFalse(status.safety_boundary.motion_permitted)
        self.assertFalse(status.safety_boundary.process_output_permitted)
        self.assertFalse(status.action_capabilities.motion_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_output_commands_permitted)
        self.assertFalse(status.action_capabilities.active_job_present)
        self.assertEqual(status.safety_boundary.blocking_reasons, ["estop_unknown", "door_unknown"])

    def test_get_status_returns_unknown_when_supervision_is_missing_or_empty(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": True,
                        "connection_state": "connected",
                        "axes": {},
                        "io": {},
                        "dispenser": {"valve_open": False, "supply_open": False},
                    }
                },
                {
                    "result": {
                        "connected": True,
                        "connection_state": "connected",
                        "supervision": {},
                        "axes": {},
                        "io": {},
                        "dispenser": {"valve_open": False, "supply_open": False},
                    }
                },
            ]
        )
        protocol = CommandProtocol(client)

        fallback_status = protocol.get_status()
        explicit_supervision_status = protocol.get_status()

        self.assertEqual(fallback_status.runtime_state, "Unknown")
        self.assertEqual(fallback_status.runtime_state_reason, "unknown")
        self.assertEqual(fallback_status.device_mode, "production")
        self.assertEqual(fallback_status.safety_boundary.state, "unknown")
        self.assertFalse(fallback_status.action_capabilities.motion_commands_permitted)
        self.assertFalse(fallback_status.action_capabilities.manual_output_commands_permitted)
        self.assertFalse(fallback_status.action_capabilities.active_job_present)
        self.assertFalse(fallback_status.action_capabilities.estop_reset_permitted)
        self.assertEqual(fallback_status.safety_boundary.blocking_reasons, ["estop_unknown", "door_unknown"])
        self.assertEqual(explicit_supervision_status.runtime_state, "Unknown")
        self.assertEqual(explicit_supervision_status.runtime_state_reason, "unknown")
        self.assertEqual(explicit_supervision_status.device_mode, "production")
        self.assertEqual(explicit_supervision_status.safety_boundary.state, "unknown")
        self.assertFalse(explicit_supervision_status.action_capabilities.motion_commands_permitted)
        self.assertFalse(explicit_supervision_status.action_capabilities.manual_output_commands_permitted)
        self.assertFalse(explicit_supervision_status.action_capabilities.active_job_present)
        self.assertFalse(explicit_supervision_status.action_capabilities.estop_reset_permitted)
        self.assertEqual(
            explicit_supervision_status.safety_boundary.blocking_reasons,
            ["estop_unknown", "door_unknown"],
        )

    def test_get_status_falls_back_to_dry_run_for_device_mode_when_field_missing(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": True,
                        "connection_state": "connected",
                        "supervision": {
                            "current_state": "Running",
                            "requested_state": "Running",
                            "state_change_in_process": False,
                            "state_reason": "job_running",
                            "failure_code": "",
                            "failure_stage": "",
                            "recoverable": True,
                            "updated_at": "2026-04-01T00:00:00Z",
                        },
                        "effective_interlocks": {
                            "estop_active": False,
                            "estop_known": True,
                            "door_open_active": False,
                            "door_open_known": True,
                            "home_boundary_x_active": False,
                            "home_boundary_y_active": False,
                            "positive_escape_only_axes": [],
                            "sources": {},
                        },
                        "job_execution": {
                            "job_id": "job-dry-run",
                            "plan_id": "plan-dry-run",
                            "plan_fingerprint": "hash-dry-run",
                            "state": "running",
                            "target_count": 1,
                            "completed_count": 0,
                            "current_cycle": 1,
                            "current_segment": 10,
                            "total_segments": 100,
                            "cycle_progress_percent": 10,
                            "overall_progress_percent": 10,
                            "elapsed_seconds": 1.0,
                            "error_message": "",
                            "dry_run": True,
                        },
                        "axes": {},
                        "io": {"estop": False, "estop_known": True, "door": False, "door_known": True},
                        "dispenser": {"valve_open": False, "supply_open": False},
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        status = protocol.get_status()

        self.assertEqual(status.device_mode, "test")
        self.assertEqual(status.safety_boundary.state, "safe")
        self.assertTrue(status.safety_boundary.motion_permitted)
        self.assertFalse(status.safety_boundary.process_output_permitted)
        self.assertTrue(status.action_capabilities.motion_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_output_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_dispenser_pause_permitted)
        self.assertFalse(status.action_capabilities.manual_dispenser_resume_permitted)
        self.assertTrue(status.action_capabilities.active_job_present)
        self.assertFalse(status.action_capabilities.estop_reset_permitted)
        self.assertEqual(status.safety_boundary.blocking_reasons, [])

    def test_get_status_derives_estop_reset_capability_from_estop_supervision_state(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": True,
                        "connection_state": "connected",
                        "device_mode": "production",
                        "interlock_latched": False,
                        "supervision": {
                            "current_state": "Estop",
                            "requested_state": "Estop",
                            "state_change_in_process": False,
                            "state_reason": "interlock_estop",
                            "updated_at": "2026-04-20T12:00:00Z",
                        },
                        "job_execution": {"job_id": "", "state": "idle"},
                        "effective_interlocks": {
                            "estop_active": True,
                            "estop_known": True,
                            "door_open_active": False,
                            "door_open_known": True,
                            "home_boundary_x_active": False,
                            "home_boundary_y_active": False,
                            "positive_escape_only_axes": [],
                            "sources": {},
                        },
                        "io": {"estop": True, "estop_known": True, "door": False, "door_known": True},
                        "axes": {},
                        "dispenser": {"valve_open": False, "supply_open": False},
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        status = protocol.get_status()

        self.assertFalse(status.action_capabilities.motion_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_output_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_dispenser_pause_permitted)
        self.assertFalse(status.action_capabilities.manual_dispenser_resume_permitted)
        self.assertFalse(status.action_capabilities.active_job_present)
        self.assertTrue(status.action_capabilities.estop_reset_permitted)

    def test_get_status_falls_back_to_derived_safety_boundary_when_field_missing(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "connected": True,
                        "connection_state": "connected",
                        "supervision": {
                            "current_state": "Idle",
                            "requested_state": "Idle",
                            "state_change_in_process": False,
                            "state_reason": "idle",
                            "failure_code": "",
                            "failure_stage": "",
                            "recoverable": True,
                            "updated_at": "2026-04-01T00:00:00Z",
                        },
                        "effective_interlocks": {
                            "estop_active": False,
                            "estop_known": True,
                            "door_open_active": False,
                            "door_open_known": True,
                            "home_boundary_x_active": False,
                            "home_boundary_y_active": False,
                            "positive_escape_only_axes": [],
                            "sources": {},
                        },
                        "interlock_latched": True,
                        "job_execution": {
                            "job_id": "",
                            "plan_id": "",
                            "plan_fingerprint": "",
                            "state": "idle",
                            "target_count": 0,
                            "completed_count": 0,
                            "current_cycle": 0,
                            "current_segment": 0,
                            "total_segments": 0,
                            "cycle_progress_percent": 0,
                            "overall_progress_percent": 0,
                            "elapsed_seconds": 0.0,
                            "error_message": "",
                            "dry_run": False,
                        },
                        "axes": {},
                        "io": {"estop": False, "estop_known": True, "door": False, "door_known": True},
                        "dispenser": {"valve_open": False, "supply_open": False},
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        status = protocol.get_status()

        self.assertEqual(status.safety_boundary.state, "blocked")
        self.assertFalse(status.safety_boundary.motion_permitted)
        self.assertFalse(status.safety_boundary.process_output_permitted)
        self.assertFalse(status.action_capabilities.motion_commands_permitted)
        self.assertFalse(status.action_capabilities.manual_output_commands_permitted)
        self.assertFalse(status.action_capabilities.active_job_present)
        self.assertEqual(status.safety_boundary.blocking_reasons, ["interlock_latched"])

    def test_get_motion_coord_status_parses_gateway_payload(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "coord_sys": 1,
                        "state": 1,
                        "is_moving": True,
                        "remaining_segments": 3,
                        "current_velocity": 12.5,
                        "raw_status_word": 17,
                        "raw_segment": 3,
                        "mc_status_ret": 0,
                        "axes": {
                            "X": {
                                "position": 1.5,
                                "velocity": 2.5,
                                "state": 1,
                                "enabled": True,
                                "homed": True,
                                "in_position": False,
                                "has_error": False,
                                "error_code": None,
                                "servo_alarm": False,
                                "following_error": False,
                                "home_failed": False,
                            },
                            "Y": {
                                "position": 0.0,
                                "velocity": 0.0,
                                "state": 0,
                                "enabled": True,
                                "homed": True,
                                "in_position": True,
                                "has_error": False,
                                "error_code": None,
                                "servo_alarm": False,
                                "following_error": False,
                                "home_failed": False,
                            },
                        },
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        status = protocol.get_motion_coord_status()

        self.assertTrue(status.available)
        self.assertEqual(status.coord_sys, 1)
        self.assertEqual(status.state, 1)
        self.assertEqual(status.state_label, "moving")
        self.assertTrue(status.is_moving)
        self.assertEqual(status.remaining_segments, 3)
        self.assertAlmostEqual(status.current_velocity, 12.5)
        self.assertIn("X", status.axes)
        self.assertAlmostEqual(status.axes["X"].velocity, 2.5)
        self.assertEqual(client.calls[0], ("motion.coord.status", {"coord_sys": 1}, None))

    def test_get_motion_coord_status_preserves_backend_error_details(self) -> None:
        client = _FakeClient([{"error": {"code": 2201, "message": "coord status unavailable"}}])
        protocol = CommandProtocol(client)

        status = protocol.get_motion_coord_status(coord_sys=2)

        self.assertFalse(status.available)
        self.assertEqual(status.coord_sys, 2)
        self.assertEqual(status.error_code, 2201)
        self.assertEqual(status.error_message, "coord status unavailable")
        self.assertEqual(client.calls[0], ("motion.coord.status", {"coord_sys": 2}, None))

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

    def test_home_uses_extended_transport_timeout(self) -> None:
        client = _FakeClient([{"result": {"completed": True, "message": "Homing completed"}}])
        protocol = CommandProtocol(client)

        ok, message = protocol.home()

        self.assertTrue(ok)
        self.assertEqual(message, "Homing completed")
        self.assertEqual(client.calls[0], ("home", {}, 90.0))

    def test_home_auto_extends_transport_timeout_from_requested_timeout(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "accepted": True,
                        "summary_state": "completed",
                        "message": "Axes ready at zero",
                        "axis_results": [],
                        "total_time_ms": 123,
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, message = protocol.home_auto(["X"], timeout_ms=120000)

        self.assertTrue(ok)
        self.assertEqual(message, "Axes ready at zero")
        self.assertEqual(client.calls[0], ("home.auto", {"axes": ["X"], "timeout_ms": 120000}, 130.0))

    def test_preview_snapshot_success_contract(self) -> None:
        preview_result = build_preview_snapshot_success_result(
            snapshot_id="s1",
            snapshot_hash="h1",
            plan_id="plan-1",
            segment_count=12,
            glue_points=[
                {"x": 0.0, "y": 0.0},
                {"x": 10.0, "y": 2.0},
            ],
            motion_preview=[
                {"x": 0.0, "y": 0.0},
                {"x": 10.0, "y": 2.0},
            ],
            motion_preview_source_point_count=48,
            execution_point_count=48,
            total_length_mm=120.5,
            estimated_time_s=6.0,
            generated_at="2026-03-21T10:00:00Z",
            preview_binding_layout_id="layout-contract",
        )
        client = _FakeClient(
            [
                {
                    "result": preview_result
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
        self.assertEqual(payload["glue_reveal_lengths_mm"], [0.0, 10.198039])
        self.assertEqual(payload["preview_binding"]["status"], "ready")
        self.assertEqual(payload["preview_binding"]["display_reveal_lengths_mm"], [0.0, 10.198039])
        self.assertEqual(payload["motion_preview"]["source"], "execution_trajectory_snapshot")
        self.assertEqual(len(payload["motion_preview"]["polyline"]), 2)
        self.assertEqual(client.calls[0][0], "dxf.preview.snapshot")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-1")

    def test_preview_snapshot_rejects_non_authoritative_source_error(self) -> None:
        client = _FakeClient(
            [
                {
                    "error": {
                        "code": 3014,
                        "message": "Preview source must be planned_glue_snapshot",
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, payload, error, error_code = protocol.dxf_preview_snapshot_with_error_details(plan_id="plan-1")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertEqual(error_code, 3014)
        self.assertIn("planned_glue_snapshot", error)
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-1")

    def test_preview_snapshot_rejects_non_glue_points_error(self) -> None:
        client = _FakeClient(
            [
                {
                    "error": {
                        "code": 3014,
                        "message": "Preview kind must be glue_points",
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, payload, error, error_code = protocol.dxf_preview_snapshot_with_error_details(plan_id="plan-1")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertEqual(error_code, 3014)
        self.assertIn("glue_points", error)
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-1")

    def test_preview_snapshot_rejects_empty_authority_payload(self) -> None:
        client = _FakeClient(
            [
                {
                    "error": {
                        "code": 3014,
                        "message": "Preview glue points are empty",
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, payload, error, error_code = protocol.dxf_preview_snapshot_with_error_details(plan_id="plan-1")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertEqual(error_code, 3014)
        self.assertIn("glue points are empty", error)
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-1")

    def test_preview_snapshot_rejects_authority_plan_mismatch_error(self) -> None:
        client = _FakeClient(
            [
                {
                    "error": {
                        "code": 3012,
                        "message": "Preview plan_id mismatch",
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, payload, error, error_code = protocol.dxf_preview_snapshot_with_error_details(plan_id="plan-1")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertEqual(error_code, 3012)
        self.assertIn("plan_id mismatch", error)
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-1")

    def test_preview_snapshot_error_contract(self) -> None:
        client = _FakeClient([{"error": {"code": 3201, "message": "DXF not loaded"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_preview_snapshot(plan_id="plan-1")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertIn("DXF not loaded", error)

    def test_preview_snapshot_accepts_max_polyline_points(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": build_preview_snapshot_success_result(
                        snapshot_id="s2",
                        snapshot_hash="h2",
                        plan_id="plan-2",
                        preview_binding_layout_id="layout-contract-plan-2",
                    )
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, payload, _ = protocol.dxf_preview_snapshot(plan_id="plan-2", max_polyline_points=128)

        self.assertTrue(ok)
        self.assertEqual(payload["snapshot_hash"], "h2")
        self.assertEqual(payload["preview_source"], "planned_glue_snapshot")
        self.assertEqual(payload["preview_kind"], "glue_points")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-2")
        self.assertEqual(client.calls[0][1]["max_polyline_points"], 128)

    def test_preview_snapshot_accepts_timeout_override(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": build_preview_snapshot_success_result(
                        snapshot_id="s-timeout",
                        snapshot_hash="h-timeout",
                        plan_id="plan-1",
                        preview_binding_layout_id="layout-timeout",
                    )
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_preview_snapshot(plan_id="plan-1", timeout=300.0)

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["snapshot_hash"], "h-timeout")
        self.assertEqual(client.calls[0], ("dxf.preview.snapshot", {"plan_id": "plan-1",}, 300.0))

    def test_preview_snapshot_rejects_unknown_kwarg_immediately(self) -> None:
        client = _FakeClient([])
        protocol = CommandProtocol(client)

        with self.assertRaisesRegex(TypeError, "unexpected keyword argument"):
            protocol.dxf_preview_snapshot(plan_id="plan-1", unknown_kwarg=True)  # type: ignore[call-arg]

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
        self.assertEqual(client.calls[0][2], 15.0)

    def test_preview_snapshot_binding_failure_error_details_contract(self) -> None:
        client = _FakeClient([{"error": {"code": 3012, "message": "authority trigger binding unavailable"}}])
        protocol = CommandProtocol(client)

        ok, payload, error, error_code = protocol.dxf_preview_snapshot_with_error_details(plan_id="plan-binding")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertEqual(error, "authority trigger binding unavailable")
        self.assertEqual(error_code, 3012)
        self.assertEqual(client.calls[0][0], "dxf.preview.snapshot")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-binding")
        self.assertEqual(client.calls[0][2], 15.0)

    def test_preview_snapshot_error_details_accepts_timeout_override(self) -> None:
        client = _FakeClient([{"error": {"code": 3012, "message": "plan not found"}}])
        protocol = CommandProtocol(client)

        ok, payload, error, error_code = protocol.dxf_preview_snapshot_with_error_details(
            plan_id="plan-x",
            timeout=300.0,
        )

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertEqual(error, "plan not found")
        self.assertEqual(error_code, 3012)
        self.assertEqual(client.calls[0], ("dxf.preview.snapshot", {"plan_id": "plan-x"}, 300.0))

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
        self.assertEqual(client.calls[0][2], 15.0)

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
        assert isinstance(payload, dict)
        self.assertTrue(payload["door"])
        self.assertTrue(payload["limit_x_neg"])
        self.assertEqual(client.calls[0][0], "mock.io.set")
        self.assertEqual(client.calls[0][1], {"door": True, "limit_x_neg": True})

    def test_dxf_create_artifact_encodes_file_and_calls_new_contract(self) -> None:
        client = _FakeClient([{"result": {"artifact_id": "artifact-1"}}])
        protocol = CommandProtocol(client)
        sample_path = PROJECT_ROOT / "sample.dxf"
        with patch.object(Path, "read_bytes", autospec=True, return_value=b"0\nSECTION\n"):
            ok, payload, error = protocol.dxf_create_artifact(str(sample_path))

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["artifact_id"], "artifact-1")
        self.assertEqual(client.calls[0][0], "dxf.artifact.create")
        self.assertEqual(client.calls[0][1]["filename"], "sample.dxf")
        self.assertTrue(client.calls[0][1]["file_content_b64"])

    def test_dxf_prepare_plan_contract(self) -> None:
        client = _FakeClient(
            [
                {
                    "result": {
                        "plan_id": "plan-1",
                        "plan_fingerprint": "fp-1",
                        "production_baseline": {
                            "baseline_id": "baseline-1",
                            "baseline_fingerprint": "baseline-fp-1",
                        },
                    }
                }
            ]
        )
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_prepare_plan(
            "artifact-1",
            speed_mm_s=20.0,
        )

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["plan_id"], "plan-1")
        self.assertEqual(client.calls[0][0], "dxf.plan.prepare")
        self.assertEqual(client.calls[0][1]["artifact_id"], "artifact-1")
        self.assertTrue(client.calls[0][1]["optimize_path"])
        self.assertTrue(client.calls[0][1]["use_interpolation_planner"])
        self.assertEqual(client.calls[0][1]["interpolation_algorithm"], 0)
        self.assertEqual(payload["production_baseline"]["baseline_id"], "baseline-1")
        self.assertEqual(client.calls[0][2], 15.0)

    def test_dxf_prepare_plan_accepts_timeout_override(self) -> None:
        client = _FakeClient([{"result": {"plan_id": "plan-1", "plan_fingerprint": "fp-1"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_prepare_plan(
            "artifact-1",
            speed_mm_s=20.0,
            timeout=300.0,
        )

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["plan_id"], "plan-1")
        self.assertEqual(client.calls[0][0], "dxf.plan.prepare")
        self.assertEqual(client.calls[0][2], 300.0)

    def test_dxf_start_job_contract(self) -> None:
        client = _FakeClient([
            {
                "result": {
                    "job_id": "job-1",
                    "production_baseline": {
                        "baseline_id": "baseline-1",
                        "baseline_fingerprint": "baseline-fp-1",
                    },
                    "performance_profile": {
                        "execution_cache_hit": False,
                        "execution_joined_inflight": True,
                        "execution_wait_ms": 12,
                        "motion_plan_ms": 30,
                        "assembly_ms": 40,
                        "export_ms": 5,
                        "execution_total_ms": 75,
                    },
                }
            }
        ])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_start_job("plan-1", target_count=3, plan_fingerprint="fp-1")

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["job_id"], "job-1")
        self.assertEqual(client.calls[0][0], "dxf.job.start")
        self.assertEqual(client.calls[0][1]["plan_id"], "plan-1")
        self.assertEqual(client.calls[0][1]["target_count"], 3)
        self.assertEqual(client.calls[0][1]["plan_fingerprint"], "fp-1")
        self.assertNotIn("auto_continue", client.calls[0][1])
        self.assertEqual(payload["production_baseline"]["baseline_fingerprint"], "baseline-fp-1")
        self.assertEqual(payload["performance_profile"]["execution_total_ms"], 75)
        self.assertEqual(client.calls[0][2], 15.0)

    def test_dxf_start_job_accepts_manual_continue_override(self) -> None:
        client = _FakeClient([{"result": {"job_id": "job-3"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_start_job(
            "plan-3",
            target_count=2,
            plan_fingerprint="fp-3",
            auto_continue=False,
        )

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["job_id"], "job-3")
        self.assertFalse(client.calls[0][1]["auto_continue"])

    def test_dxf_start_job_accepts_auto_continue_override(self) -> None:
        client = _FakeClient([{"result": {"job_id": "job-2"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_start_job(
            "plan-2",
            target_count=2,
            plan_fingerprint="fp-2",
            auto_continue=True,
        )

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(payload["job_id"], "job-2")
        self.assertTrue(client.calls[0][1]["auto_continue"])

    def test_dxf_start_job_returns_backend_error(self) -> None:
        client = _FakeClient([{"error": {"code": 2901, "message": "safety door open"}}])
        protocol = CommandProtocol(client)

        ok, payload, error = protocol.dxf_start_job("plan-1", target_count=1, plan_fingerprint="fp-1")

        self.assertFalse(ok)
        self.assertEqual(payload, {})
        self.assertIn("safety door open", error)

    def test_dxf_job_pause_contract(self) -> None:
        client = _FakeClient([{"result": {"paused": True, "job_id": "job-1"}}])
        protocol = CommandProtocol(client)

        ok, error = protocol.dxf_job_pause("job-1")

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(client.calls[0], ("dxf.job.pause", {"job_id": "job-1"}, 15.0))

    def test_dxf_job_pause_returns_backend_error(self) -> None:
        client = _FakeClient([{"error": {"code": 2911, "message": "job not running"}}])
        protocol = CommandProtocol(client)

        ok, error = protocol.dxf_job_pause("job-1")

        self.assertFalse(ok)
        self.assertEqual(error, "job not running")

    def test_dxf_job_resume_contract(self) -> None:
        client = _FakeClient([{"result": {"resumed": True, "job_id": "job-1"}}])
        protocol = CommandProtocol(client)

        ok, error = protocol.dxf_job_resume("job-1")

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(client.calls[0], ("dxf.job.resume", {"job_id": "job-1"}, 15.0))

    def test_dxf_job_resume_returns_backend_error(self) -> None:
        client = _FakeClient([{"error": {"code": 2912, "message": "interlock active"}}])
        protocol = CommandProtocol(client)

        ok, error = protocol.dxf_job_resume("job-1")

        self.assertFalse(ok)
        self.assertEqual(error, "interlock active")

    def test_dxf_job_continue_contract(self) -> None:
        client = _FakeClient([{"result": {"continued": True, "job_id": "job-1"}}])
        protocol = CommandProtocol(client)

        ok, error = protocol.dxf_job_continue("job-1")

        self.assertTrue(ok)
        self.assertEqual(error, "")
        self.assertEqual(client.calls[0], ("dxf.job.continue", {"job_id": "job-1"}, 15.0))

    def test_dxf_job_continue_returns_backend_error(self) -> None:
        client = _FakeClient([{"error": {"code": 2919, "message": "job is not waiting for continue"}}])
        protocol = CommandProtocol(client)

        ok, error = protocol.dxf_job_continue("job-1")

        self.assertFalse(ok)
        self.assertEqual(error, "job is not waiting for continue")

    def test_dxf_job_stop_contract(self) -> None:
        client = _FakeClient([{"result": {"stopped": True, "job_id": "job-1", "transition_state": "stopping"}}])
        protocol = CommandProtocol(client)

        result = protocol.dxf_job_stop("job-1")

        self.assertIsInstance(result, JobTransitionResult)
        self.assertTrue(result.accepted)
        self.assertEqual(result.transition_state, "stopping")
        self.assertEqual(result.job_id, "job-1")
        self.assertEqual(result.message, "")
        self.assertEqual(client.calls[0], ("dxf.job.stop", {"job_id": "job-1"}, 15.0))

    def test_dxf_job_stop_returns_backend_error(self) -> None:
        client = _FakeClient([{"error": {"code": 2913, "message": "stop timeout"}}])
        protocol = CommandProtocol(client)

        result = protocol.dxf_job_stop("job-1")

        self.assertFalse(result.accepted)
        self.assertEqual(result.message, "stop timeout")
        self.assertEqual(result.error_code, 2913)

    def test_dxf_job_cancel_contract(self) -> None:
        client = _FakeClient([{"result": {"cancelled": True, "job_id": "job-2", "transition_state": "canceling"}}])
        protocol = CommandProtocol(client)

        result = protocol.dxf_job_cancel("job-2")

        self.assertIsInstance(result, JobTransitionResult)
        self.assertTrue(result.accepted)
        self.assertEqual(result.transition_state, "canceling")
        self.assertEqual(result.job_id, "job-2")
        self.assertEqual(client.calls[0], ("dxf.job.cancel", {"job_id": "job-2"}, 15.0))

    def test_dxf_job_cancel_returns_backend_error(self) -> None:
        client = _FakeClient([{"error": {"code": 2914, "message": "cancel timeout"}}])
        protocol = CommandProtocol(client)

        result = protocol.dxf_job_cancel("job-2")

        self.assertFalse(result.accepted)
        self.assertEqual(result.message, "cancel timeout")
        self.assertEqual(result.error_code, 2914)


if __name__ == "__main__":
    unittest.main()
