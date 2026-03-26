import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.tools.mock_server import MockState


class MockServerInterlockTest(unittest.TestCase):
    def test_status_reflects_mock_io_switches_and_limit_bits(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})

        resp = state.handle_request(
            "mock.io.set",
            {
                "estop": True,
                "door": True,
                "limit_x_pos": True,
                "limit_x_neg": True,
                "limit_y_pos": True,
                "limit_y_neg": True,
            },
        )
        status = state.handle_request("status", {})

        self.assertTrue(resp["result"]["estop"])
        self.assertTrue(resp["result"]["door"])
        self.assertTrue(resp["result"]["limit_x_pos"])
        self.assertTrue(resp["result"]["limit_x_neg"])
        self.assertTrue(resp["result"]["limit_y_pos"])
        self.assertTrue(resp["result"]["limit_y_neg"])
        self.assertTrue(status["result"]["io"]["estop"])
        self.assertTrue(status["result"]["io"]["door"])
        self.assertTrue(status["result"]["io"]["limit_x_pos"])
        self.assertTrue(status["result"]["io"]["limit_x_neg"])
        self.assertTrue(status["result"]["io"]["limit_y_pos"])
        self.assertTrue(status["result"]["io"]["limit_y_neg"])
        self.assertTrue(status["result"]["effective_interlocks"]["estop_active"])
        self.assertTrue(status["result"]["effective_interlocks"]["door_open_active"])
        self.assertTrue(status["result"]["effective_interlocks"]["home_boundary_x_active"])
        self.assertTrue(status["result"]["effective_interlocks"]["home_boundary_y_active"])
        self.assertEqual(status["result"]["effective_interlocks"]["positive_escape_only_axes"], ["X", "Y"])
        self.assertEqual(status["result"]["supervision"]["current_state"], "Estop")
        self.assertEqual(status["result"]["supervision"]["state_reason"], "interlock_estop")

    def test_job_start_is_rejected_when_safety_door_open(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request(
            "dxf.artifact.create",
            {"filename": "sample.dxf", "file_content_b64": "MFxOU0VDVElPTgoyXEVOVElUSUVTXDAKRU5EU0VDXDAKRU9GCg=="},
        )
        plan = state.handle_request(
            "dxf.plan.prepare",
            {"artifact_id": state.dxf.artifact_id, "dispensing_speed_mm_s": 12.5},
        )
        self.assertIn("result", plan)
        snapshot = state.handle_request("dxf.preview.snapshot", {"plan_id": plan["result"]["plan_id"]})
        self.assertIn("result", snapshot)
        self.assertEqual(snapshot["result"]["preview_source"], "mock_synthetic")
        confirm = state.handle_request(
            "dxf.preview.confirm",
            {
                "plan_id": plan["result"]["plan_id"],
                "snapshot_hash": snapshot["result"]["snapshot_hash"],
            },
        )
        self.assertIn("result", confirm)

        state.handle_request("mock.io.set", {"door": True})
        start = state.handle_request(
            "dxf.job.start",
            {
                "plan_id": plan["result"]["plan_id"],
                "plan_fingerprint": plan["result"]["plan_fingerprint"],
                "target_count": 1,
            },
        )

        self.assertIn("error", start)
        self.assertIn("safety door open", start["error"]["message"])

    def test_job_resume_is_rejected_when_estop_active(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request(
            "dxf.artifact.create",
            {"filename": "sample.dxf", "file_content_b64": "MFxOU0VDVElPTgoyXEVOVElUSUVTXDAKRU5EU0VDXDAKRU9GCg=="},
        )
        plan = state.handle_request(
            "dxf.plan.prepare",
            {"artifact_id": state.dxf.artifact_id, "dispensing_speed_mm_s": 12.5},
        )
        snapshot = state.handle_request("dxf.preview.snapshot", {"plan_id": plan["result"]["plan_id"]})
        confirm = state.handle_request(
            "dxf.preview.confirm",
            {
                "plan_id": plan["result"]["plan_id"],
                "snapshot_hash": snapshot["result"]["snapshot_hash"],
            },
        )
        self.assertIn("result", confirm)
        start = state.handle_request(
            "dxf.job.start",
            {
                "plan_id": plan["result"]["plan_id"],
                "plan_fingerprint": plan["result"]["plan_fingerprint"],
                "target_count": 1,
            },
        )
        self.assertIn("result", start)
        state.handle_request("dxf.job.pause", {})
        state.handle_request("mock.io.set", {"estop": True})

        resume = state.handle_request("dxf.job.resume", {})

        self.assertIn("error", resume)
        self.assertIn("emergency stop active", resume["error"]["message"])

    def test_home_is_rejected_when_interlock_active(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})

        for io_update, expected_message in (
            ({"door": True}, "safety door open"),
            ({"estop": True}, "emergency stop active"),
        ):
            with self.subTest(io_update=io_update):
                state.handle_request("mock.io.set", {"door": False, "estop": False})
                state.handle_request("mock.io.set", io_update)

                resp = state.handle_request("home", {"axes": ["X"]})

                self.assertIn("error", resp)
                self.assertIn(expected_message, resp["error"]["message"])

    def test_home_go_is_rejected_when_axis_not_homed(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})

        resp = state.handle_request("home.go", {"axes": ["X"]})

        self.assertIn("error", resp)
        self.assertIn("axis not homed", resp["error"]["message"])

    def test_home_go_is_rejected_when_interlock_active(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request("home", {"axes": ["X", "Y"]})

        for io_update, expected_message in (
            ({"door": True}, "safety door open"),
            ({"estop": True}, "emergency stop active"),
        ):
            with self.subTest(io_update=io_update):
                state.handle_request("mock.io.set", {"door": False, "estop": False})
                state.handle_request("mock.io.set", io_update)

                resp = state.handle_request("home.go", {"axes": ["X"]})

                self.assertIn("error", resp)
                self.assertIn(expected_message, resp["error"]["message"])

    def test_home_auto_drives_not_homed_and_away_from_zero_axes_to_zero(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.axes["X"].homed = False
        state.axes["X"].position = 7.5
        state.axes["Y"].homed = True
        state.axes["Y"].position = 3.0

        resp = state.handle_request("home.auto", {"axes": ["X", "Y"]})

        self.assertIn("result", resp)
        self.assertTrue(resp["result"]["accepted"])
        self.assertEqual(resp["result"]["summary_state"], "completed")
        self.assertEqual(state.axes["X"].position, 0.0)
        self.assertEqual(state.axes["Y"].position, 0.0)
        self.assertTrue(state.axes["X"].homed)
        self.assertEqual(
            resp["result"]["axis_results"],
            [
                {
                    "axis": "X",
                    "supervisor_state": "not_homed",
                    "planned_action": "home",
                    "executed": True,
                    "success": True,
                    "reason_code": "not_homed",
                    "message": "Homing completed",
                },
                {
                    "axis": "Y",
                    "supervisor_state": "homed_away_from_zero",
                    "planned_action": "go_home",
                    "executed": True,
                    "success": True,
                    "reason_code": "homed_away_from_zero",
                    "message": "Moved to zero",
                },
            ],
        )

    def test_home_auto_is_rejected_when_interlock_active(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})

        for io_update, expected_message in (
            ({"door": True}, "safety door open"),
            ({"estop": True}, "emergency stop active"),
        ):
            with self.subTest(io_update=io_update):
                state.handle_request("mock.io.set", {"door": False, "estop": False})
                state.handle_request("mock.io.set", io_update)

                resp = state.handle_request("home.auto", {"axes": ["X"]})

                self.assertIn("error", resp)
                self.assertIn(expected_message, resp["error"]["message"])

    def test_manual_actions_are_rejected_when_interlock_active(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request("home", {"axes": ["X", "Y"]})
        state.handle_request("supply.open", {})

        scenarios = (
            ("jog", {"axis": "X", "direction": 1, "speed": 5.0}),
            ("move", {"x": 1.0, "y": 2.0, "speed": 5.0}),
            ("supply.open", {}),
            ("dispenser.start", {"count": 1, "interval_ms": 100, "duration_ms": 50}),
        )

        for io_update, expected_message in (
            ({"door": True}, "safety door open"),
            ({"estop": True}, "emergency stop active"),
        ):
            for method, params in scenarios:
                with self.subTest(io_update=io_update, method=method):
                    state.handle_request("mock.io.set", {"door": False, "estop": False})
                    state.handle_request("supply.open", {})
                    state.handle_request("mock.io.set", io_update)

                    resp = state.handle_request(method, params)

                    self.assertIn("error", resp)
                    self.assertIn(expected_message, resp["error"]["message"])

    def test_estop_reset_clears_status(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request("estop", {})

        reset_resp = state.handle_request("estop.reset", {})
        status = state.handle_request("status", {})

        self.assertIn("result", reset_resp)
        self.assertTrue(reset_resp["result"]["reset"])
        self.assertFalse(status["result"]["io"]["estop"])


if __name__ == "__main__":
    unittest.main()
