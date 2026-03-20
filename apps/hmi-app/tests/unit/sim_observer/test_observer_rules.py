import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.contracts.observer_rules import (  # noqa: E402
    ContextWindowRef,
    GroupMinimizationResult,
    PrimaryAssociationResult,
    SequenceRangeRef,
    SpatialRef,
    TimeMappingResult,
    ensure_reason,
)
from hmi_client.features.sim_observer.contracts.observer_status import (  # noqa: E402
    ObserverState,
    REASON_DIRECT_OBJECT_ANCHOR,
)
from hmi_client.features.sim_observer.contracts.observer_types import (  # noqa: E402
    BBox2D,
    GroupBasis,
    ObjectId,
)


class ObserverRulesTest(unittest.TestCase):
    def test_primary_association_requires_object_for_single_object(self) -> None:
        with self.assertRaises(ValueError):
            PrimaryAssociationResult(association_level="single_object")

    def test_group_minimization_validates_cardinality(self) -> None:
        with self.assertRaises(ValueError):
            GroupMinimizationResult(
                group_id=ObjectId("group:1"),
                group_basis=GroupBasis.ISSUE_RELATED,
                member_object_ids=(ObjectId("segment:1"),),
                group_cardinality=2,
                is_minimized=False,
            )

    def test_spatial_ref_requires_geometry_or_bbox_when_resolved(self) -> None:
        with self.assertRaises(ValueError):
            SpatialRef(mapping_state=ObserverState.RESOLVED)
        SpatialRef(
            geometry_refs=("segment:1",),
            bbox=BBox2D(0.0, 0.0, 1.0, 1.0),
            mapping_state=ObserverState.RESOLVED,
            mapped_object_ids=(ObjectId("segment:1"),),
        )

    def test_time_mapping_result_requires_sequence_range_when_level_is_sequence_range(self) -> None:
        with self.assertRaises(ValueError):
            TimeMappingResult(
                association_level="sequence_range",
                mapping_state=ObserverState.RESOLVED,
            )

        seq = SequenceRangeRef(
            start_segment_id=ObjectId("segment:1"),
            end_segment_id=ObjectId("segment:2"),
            member_segment_ids=(ObjectId("segment:1"), ObjectId("segment:2")),
            continuity_state="continuous",
        )
        TimeMappingResult(
            association_level="sequence_range",
            sequence_range_ref=seq,
            mapping_state=ObserverState.RESOLVED,
        )

    def test_context_window_ref_validates_resolved_window(self) -> None:
        with self.assertRaises(ValueError):
            ContextWindowRef(
                window_start=None,
                window_end=None,
                anchor_time=1.0,
                window_basis="explicit_window",
                window_state=ObserverState.RESOLVED,
            )

    def test_ensure_reason_constructs_reason_info(self) -> None:
        reason = ensure_reason(REASON_DIRECT_OBJECT_ANCHOR, "selected direct object")
        self.assertEqual(reason.reason_code, REASON_DIRECT_OBJECT_ANCHOR)


if __name__ == "__main__":
    unittest.main()
