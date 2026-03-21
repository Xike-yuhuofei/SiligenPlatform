import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.dispense_preview_gate import (
    DispensePreviewGate,
    PreviewSnapshotMeta,
    StartBlockReason,
)


def _snapshot(hash_value: str = "hash-1") -> PreviewSnapshotMeta:
    return PreviewSnapshotMeta(
        snapshot_id="snapshot-1",
        snapshot_hash=hash_value,
        segment_count=10,
        point_count=20,
        total_length_mm=100.0,
        estimated_time_s=5.0,
        generated_at="2026-03-21T10:00:00Z",
    )


class DispensePreviewGateTest(unittest.TestCase):
    def test_initial_state_blocks_start(self) -> None:
        gate = DispensePreviewGate()
        decision = gate.decision_for_start()
        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, StartBlockReason.PREVIEW_MISSING)

    def test_ready_unsigned_requires_confirmation(self) -> None:
        gate = DispensePreviewGate()
        gate.begin_preview()
        gate.preview_ready(_snapshot())

        decision = gate.decision_for_start()
        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, StartBlockReason.CONFIRM_MISSING)

    def test_confirmed_snapshot_allows_start(self) -> None:
        gate = DispensePreviewGate()
        gate.begin_preview()
        gate.preview_ready(_snapshot())
        self.assertTrue(gate.confirm_current_snapshot())

        decision = gate.decision_for_start()
        self.assertTrue(decision.allowed)
        self.assertEqual(decision.reason, StartBlockReason.NONE)
        self.assertEqual(gate.get_confirmed_snapshot_hash(), "hash-1")

    def test_input_change_marks_state_stale_and_blocks(self) -> None:
        gate = DispensePreviewGate()
        gate.begin_preview()
        gate.preview_ready(_snapshot())
        gate.confirm_current_snapshot()

        gate.mark_input_changed()
        decision = gate.decision_for_start()
        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, StartBlockReason.PREVIEW_STALE)

    def test_validate_execution_hash_mismatch(self) -> None:
        gate = DispensePreviewGate()
        gate.begin_preview()
        gate.preview_ready(_snapshot("abc"))
        gate.confirm_current_snapshot()

        decision = gate.validate_execution_snapshot_hash("xyz")
        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, StartBlockReason.HASH_MISMATCH)

    def test_preview_failed_exposes_failure_reason(self) -> None:
        gate = DispensePreviewGate()
        gate.preview_failed("timeout")

        decision = gate.decision_for_start()
        self.assertFalse(decision.allowed)
        self.assertEqual(decision.reason, StartBlockReason.PREVIEW_FAILED)
        self.assertEqual(gate.last_error_message, "timeout")


if __name__ == "__main__":
    unittest.main()

