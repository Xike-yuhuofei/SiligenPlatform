import sys
import time
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.tools.mock_server import MockState


class MockServerInterlockTest(unittest.TestCase):
    def test_motion_coord_status_reports_idle_axes_after_connect(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request("home", {"axes": ["X", "Y"]})

        coord_status = state.handle_request("motion.coord.status", {"coord_sys": 1})

        self.assertIn("result", coord_status)
        self.assertEqual(coord_status["result"]["coord_sys"], 1)
        self.assertEqual(coord_status["result"]["state"], 0)
        self.assertFalse(coord_status["result"]["is_moving"])
        self.assertEqual(coord_status["result"]["remaining_segments"], 0)
        self.assertEqual(coord_status["result"]["current_velocity"], 0.0)
        self.assertTrue(coord_status["result"]["axes"]["X"]["homed"])
        self.assertTrue(coord_status["result"]["axes"]["X"]["in_position"])
        self.assertEqual(coord_status["result"]["axes"]["X"]["state"], 0)

    def test_motion_coord_status_reports_motion_while_axes_are_moving(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request("home", {"axes": ["X", "Y"]})
        state.handle_request("move", {"x": 3.0, "y": 2.0, "speed": 8.0})

        coord_status = state.handle_request("motion.coord.status", {"coord_sys": 1})

        self.assertIn("result", coord_status)
        self.assertEqual(coord_status["result"]["state"], 1)
        self.assertTrue(coord_status["result"]["is_moving"])
        self.assertEqual(coord_status["result"]["remaining_segments"], 1)
        self.assertEqual(coord_status["result"]["current_velocity"], 8.0)
        self.assertEqual(coord_status["result"]["axes"]["X"]["state"], 1)
        self.assertEqual(coord_status["result"]["axes"]["X"]["position"], 3.0)
        self.assertEqual(coord_status["result"]["axes"]["Y"]["position"], 2.0)

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
            {
                "artifact_id": state.dxf.artifact_id,
                "dispensing_speed_mm_s": 12.5,
            },
        )
        self.assertIn("result", plan)
        snapshot = state.handle_request("dxf.preview.snapshot", {"plan_id": plan["result"]["plan_id"]})
        self.assertIn("result", snapshot)
        self.assertEqual(snapshot["result"]["preview_source"], "planned_glue_snapshot")
        self.assertEqual(snapshot["result"]["preview_kind"], "glue_points")
        self.assertEqual(
            snapshot["result"]["preview_binding"]["source"],
            "runtime_authority_preview_binding",
        )
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
            {
                "artifact_id": state.dxf.artifact_id,
                "dispensing_speed_mm_s": 12.5,
            },
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

    def test_job_start_defaults_to_auto_continue_for_multi_cycle_jobs(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request(
            "dxf.artifact.create",
            {"filename": "sample.dxf", "file_content_b64": "MFxOU0VDVElPTgoyXEVOVElUSUVTXDAKRU5EU0VDXDAKRU9GCg=="},
        )
        plan = state.handle_request(
            "dxf.plan.prepare",
            {
                "artifact_id": state.dxf.artifact_id,
                "dispensing_speed_mm_s": 12.5,
            },
        )
        snapshot = state.handle_request("dxf.preview.snapshot", {"plan_id": plan["result"]["plan_id"]})
        state.handle_request(
            "dxf.preview.confirm",
            {
                "plan_id": plan["result"]["plan_id"],
                "snapshot_hash": snapshot["result"]["snapshot_hash"],
            },
        )

        start = state.handle_request(
            "dxf.job.start",
            {
                "plan_id": plan["result"]["plan_id"],
                "plan_fingerprint": plan["result"]["plan_fingerprint"],
                "target_count": 2,
            },
        )
        self.assertIn("result", start)
        self.assertTrue(state.dxf.auto_continue)

        with state._lock:
            state.dxf.progress = 97.5

        deadline = time.time() + 2.0
        status = {"result": {"state": "unknown", "completed_count": 0, "current_cycle": 0}}
        while time.time() < deadline:
            status = state.handle_request("dxf.job.status", {"job_id": start["result"]["job_id"]})
            if status["result"]["completed_count"] >= 1:
                break
            time.sleep(0.05)

        self.assertEqual(status["result"]["state"], "running")
        self.assertEqual(status["result"]["completed_count"], 1)
        self.assertEqual(status["result"]["current_cycle"], 2)

    def test_job_start_can_explicitly_wait_for_continue(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request(
            "dxf.artifact.create",
            {"filename": "sample.dxf", "file_content_b64": "MFxOU0VDVElPTgoyXEVOVElUSUVTXDAKRU5EU0VDXDAKRU9GCg=="},
        )
        plan = state.handle_request(
            "dxf.plan.prepare",
            {
                "artifact_id": state.dxf.artifact_id,
                "dispensing_speed_mm_s": 12.5,
            },
        )
        snapshot = state.handle_request("dxf.preview.snapshot", {"plan_id": plan["result"]["plan_id"]})
        state.handle_request(
            "dxf.preview.confirm",
            {
                "plan_id": plan["result"]["plan_id"],
                "snapshot_hash": snapshot["result"]["snapshot_hash"],
            },
        )

        start = state.handle_request(
            "dxf.job.start",
            {
                "plan_id": plan["result"]["plan_id"],
                "plan_fingerprint": plan["result"]["plan_fingerprint"],
                "target_count": 2,
                "auto_continue": False,
            },
        )
        self.assertIn("result", start)
        self.assertFalse(state.dxf.auto_continue)

        with state._lock:
            state.dxf.progress = 97.5

        deadline = time.time() + 2.0
        status = {"result": {"state": "unknown", "completed_count": 0}}
        while time.time() < deadline:
            status = state.handle_request("dxf.job.status", {"job_id": start["result"]["job_id"]})
            if status["result"]["state"] == "awaiting_continue":
                break
            time.sleep(0.05)

        self.assertEqual(status["result"]["state"], "awaiting_continue")
        self.assertEqual(status["result"]["completed_count"], 1)

        continued = state.handle_request("dxf.job.continue", {"job_id": start["result"]["job_id"]})
        self.assertIn("result", continued)

        with state._lock:
            state.dxf.progress = 97.5

        deadline = time.time() + 2.0
        while time.time() < deadline:
            status = state.handle_request("dxf.job.status", {"job_id": start["result"]["job_id"]})
            if status["result"]["state"] == "completed":
                break
            time.sleep(0.05)

        self.assertEqual(status["result"]["state"], "completed")
        self.assertEqual(status["result"]["completed_count"], 2)

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

    def test_home_go_is_rejected_when_speed_override_is_provided(self) -> None:
        state = MockState(seed_alarms=False)
        state.handle_request("connect", {})
        state.handle_request("home", {"axes": ["X"]})

        resp = state.handle_request("home.go", {"axes": ["X"], "speed": 12.5})

        self.assertIn("error", resp)
        self.assertIn("ready_zero_speed_mm_s", resp["error"]["message"])

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
