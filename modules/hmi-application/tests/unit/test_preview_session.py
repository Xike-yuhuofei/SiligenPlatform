import sys
import unittest
from pathlib import Path

TEST_ROOT = Path(__file__).resolve().parents[1]
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.preview_gate import PreviewGateState
from hmi_application.preview_session import PreflightBlockReason, PreviewSessionOwner
from unit.preview_test_support import FakeStatus, valid_payload


class PreviewSessionOwnerSeamTest(unittest.TestCase):
    def setUp(self) -> None:
        self.owner = PreviewSessionOwner()

    def test_owner_exposes_gate_and_state_from_same_session(self) -> None:
        self.assertIs(self.owner.gate, self.owner.state.gate)

    def test_owner_build_preflight_decision_invalidates_plan_on_mode_mismatch(self) -> None:
        result = self.owner.process_snapshot_payload(valid_payload(dry_run=False), current_dry_run=False)

        self.assertTrue(result.ok)
        decision = self.owner.build_preflight_decision(
            online_ready=True,
            connected=True,
            hardware_connected=True,
            runtime_status_fault=False,
            status=FakeStatus(),
            dry_run=True,
        )

        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, PreflightBlockReason.PREVIEW_MODE_MISMATCH)
        self.assertEqual(self.owner.gate.state, PreviewGateState.STALE)
        self.assertEqual(self.owner.state.current_plan_id, "")
        self.assertEqual(self.owner.state.preview_source, "")

    def test_owner_static_helper_methods_delegate_to_payload_authority_helpers(self) -> None:
        self.assertEqual(
            PreviewSessionOwner.extract_points(
                {"points": [{"x": 1, "y": 2}, {"x": "bad", "y": 3}]},
                "points",
            ),
            [(1.0, 2.0)],
        )
        self.assertEqual(
            PreviewSessionOwner.extract_float_list({"lengths": [1, 2.5, "3.0"]}, "lengths"),
            [1.0, 2.5, 3.0],
        )
        self.assertTrue(PreviewSessionOwner.is_unrecoverable_resync_error("plan not found", 3012))


if __name__ == "__main__":
    unittest.main()
