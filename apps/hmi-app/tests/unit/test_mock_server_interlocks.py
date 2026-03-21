import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.tools.mock_server import MockState


class MockServerInterlockTest(unittest.TestCase):
    def test_status_reflects_mock_io_switches(self) -> None:
        state = MockState(seed_alarms=False)

        resp = state.handle_request("mock.io.set", {"estop": True, "door": True})
        status = state.handle_request("status", {})

        self.assertTrue(resp["result"]["estop"])
        self.assertTrue(resp["result"]["door"])
        self.assertTrue(status["result"]["io"]["estop"])
        self.assertTrue(status["result"]["io"]["door"])

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


if __name__ == "__main__":
    unittest.main()
