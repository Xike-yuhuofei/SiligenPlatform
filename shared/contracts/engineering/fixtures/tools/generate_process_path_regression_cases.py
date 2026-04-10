import argparse
from pathlib import Path
import sys


WORKSPACE_ROOT = Path(__file__).resolve().parents[5]
ENGINEERING_DATA_ROOT = WORKSPACE_ROOT / "modules" / "dxf-geometry" / "application"

sys.path.insert(0, str(ENGINEERING_DATA_ROOT))

from engineering_data.proto import dxf_primitives_pb2 as pb  # noqa: E402


FIXTURE_ROOT = WORKSPACE_ROOT / "shared" / "contracts" / "engineering" / "fixtures" / "cases"


def _point(x: float, y: float) -> pb.Point2D:
    return pb.Point2D(x=float(x), y=float(y))


def _line(start: tuple[float, float], end: tuple[float, float]) -> pb.LinePrimitive:
    return pb.LinePrimitive(start=_point(*start), end=_point(*end))


def _add_line(bundle: pb.PathBundle,
              entity_id: int,
              segment_index: int,
              start: tuple[float, float],
              end: tuple[float, float],
              *,
              closed: bool = False,
              layer: str = "0") -> None:
    primitive = bundle.primitives.add()
    primitive.type = pb.PRIMITIVE_LINE
    primitive.line.CopyFrom(_line(start, end))

    meta = bundle.metadata.add()
    meta.entity_id = entity_id
    meta.entity_type = pb.DXF_ENTITY_LINE
    meta.entity_segment_index = segment_index
    meta.entity_closed = closed
    meta.layer = layer
    meta.color = 7


def _make_bundle(case_name: str) -> pb.PathBundle:
    bundle = pb.PathBundle()
    bundle.header.schema_version = 1
    bundle.header.source_path = f"{case_name}.pb"
    bundle.header.units = "mm"
    bundle.header.unit_scale = 1.0
    bundle.header.tolerance.chordal = 0.1
    bundle.header.tolerance.max_seg = 0.1
    bundle.header.tolerance.snap = 0.0
    bundle.header.tolerance.angular = 1.0
    bundle.header.tolerance.min_seg = 0.0
    return bundle


def build_fragmented_chain() -> pb.PathBundle:
    bundle = _make_bundle("fragmented_chain")
    _add_line(bundle, 1, 0, (0.0, 0.0), (10.0, 0.0))
    _add_line(bundle, 2, 0, (0.0, 10.0), (0.0, 0.0))
    _add_line(bundle, 3, 0, (10.0, 0.0), (10.0, 10.0))
    _add_line(bundle, 4, 0, (10.0, 10.0), (0.0, 10.0))
    return bundle


def build_multi_contour_reorder() -> pb.PathBundle:
    bundle = _make_bundle("multi_contour_reorder")
    _add_line(bundle, 100, 0, (0.0, 0.0), (10.0, 0.0), layer="outer-a")
    _add_line(bundle, 100, 1, (10.0, 0.0), (20.0, 0.0), layer="outer-a")
    _add_line(bundle, 200, 0, (20.0, 0.0), (10.0, 0.0), layer="outer-b")
    _add_line(bundle, 300, 0, (10.0, 10.0), (0.0, 10.0), closed=True, layer="inner-loop")
    _add_line(bundle, 300, 1, (0.0, 10.0), (0.0, 0.0), closed=True, layer="inner-loop")
    _add_line(bundle, 300, 2, (0.0, 0.0), (10.0, 0.0), closed=True, layer="inner-loop")
    _add_line(bundle, 300, 3, (10.0, 0.0), (10.0, 10.0), closed=True, layer="inner-loop")
    return bundle


def build_intersection_split() -> pb.PathBundle:
    bundle = _make_bundle("intersection_split")
    _add_line(bundle, 400, 0, (0.0, 0.0), (10.0, 10.0), layer="cross-a")
    _add_line(bundle, 401, 0, (0.0, 10.0), (10.0, 0.0), layer="cross-b")
    _add_line(bundle, 402, 0, (10.0, 0.0), (15.0, 0.0), layer="tail")
    return bundle


CASE_BUILDERS = {
    "fragmented_chain": build_fragmented_chain,
    "multi_contour_reorder": build_multi_contour_reorder,
    "intersection_split": build_intersection_split,
}


def write_case(case_name: str) -> Path:
    bundle = CASE_BUILDERS[case_name]()
    output_dir = FIXTURE_ROOT / case_name
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"{case_name}.pb"
    output_path.write_bytes(bundle.SerializeToString())
    return output_path


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Generate repo-owned process-path regression PB fixtures.")
    parser.add_argument(
        "--case",
        choices=sorted(CASE_BUILDERS.keys()),
        action="append",
        dest="cases",
        help="Only generate the named case. Can be repeated.",
    )
    args = parser.parse_args(argv)

    cases = args.cases or sorted(CASE_BUILDERS.keys())
    for case_name in cases:
        output = write_case(case_name)
        print(f"generated {case_name}: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
