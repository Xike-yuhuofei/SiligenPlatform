import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.contracts.observer_types import (  # noqa: E402
    ArcGeometry,
    BBox2D,
    DisplayLabel,
    GroupBasis,
    ObjectAnchor,
    ObjectGroup,
    ObjectId,
    ObjectType,
    PathSegmentObject,
    Point2D,
    SourceRef,
    TimeAnchorKind,
    TimeAnchorObject,
    ensure_source_refs,
)


class ObserverTypesTest(unittest.TestCase):
    def test_bbox_validation(self) -> None:
        self.assertTrue(BBox2D(0.0, 0.0, 1.0, 1.0).is_valid())
        with self.assertRaises(ValueError):
            BBox2D(2.0, 0.0, 1.0, 1.0)

    def test_ensure_source_refs_normalizes_to_tuple(self) -> None:
        ref = SourceRef(source_name="timeline", source_path="timeline[0]")
        self.assertEqual(ensure_source_refs(None), tuple())
        self.assertEqual(ensure_source_refs(ref), (ref,))
        self.assertEqual(ensure_source_refs([ref]), (ref,))

    def test_time_anchor_range_requires_time_end(self) -> None:
        with self.assertRaises(ValueError):
            TimeAnchorObject(
                object_id=ObjectId("time:1"),
                object_type=ObjectType.TIME_ANCHOR,
                display_label=DisplayLabel("Time 1"),
                anchor_kind=TimeAnchorKind.TIME_RANGE,
                time_start=1.0,
                time_end=None,
            )

    def test_object_group_cardinality_must_match_members(self) -> None:
        with self.assertRaises(ValueError):
            ObjectGroup(
                object_id=ObjectId("group:1"),
                object_type=ObjectType.OBJECT_GROUP,
                display_label=DisplayLabel("Group 1"),
                member_object_ids=(ObjectId("segment:1"),),
                group_source="summary",
                group_basis=GroupBasis.ISSUE_RELATED,
                group_cardinality=2,
                is_minimized=False,
            )

    def test_object_anchor_allows_object_and_group_context(self) -> None:
        anchor = ObjectAnchor(object_id=ObjectId("segment:1"), group_id=ObjectId("group:1"))
        self.assertEqual(anchor.object_id, ObjectId("segment:1"))
        self.assertEqual(anchor.group_id, ObjectId("group:1"))

    def test_arc_geometry_requires_positive_radius(self) -> None:
        with self.assertRaises(ValueError):
            ArcGeometry(
                center=Point2D(0.0, 0.0),
                radius=0.0,
                start_angle_rad=0.0,
                end_angle_rad=1.0,
            )

    def test_path_segment_arc_geometry_requires_arc_segment_type(self) -> None:
        with self.assertRaises(ValueError):
            PathSegmentObject(
                object_id=ObjectId("segment:0"),
                object_type=ObjectType.PATH_SEGMENT,
                display_label=DisplayLabel("Segment 0"),
                segment_index=0,
                segment_type="line",
                start_point=Point2D(0.0, 0.0),
                end_point=Point2D(1.0, 0.0),
                bbox=BBox2D(0.0, 0.0, 1.0, 0.0),
                arc_geometry=ArcGeometry(
                    center=Point2D(0.0, 0.0),
                    radius=1.0,
                    start_angle_rad=0.0,
                    end_angle_rad=1.0,
                ),
            )


if __name__ == "__main__":
    unittest.main()
