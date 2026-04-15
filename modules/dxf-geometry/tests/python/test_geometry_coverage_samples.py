from __future__ import annotations

from collections import Counter
from pathlib import Path
import subprocess
import sys
import tempfile

import pytest

TEST_ROOT = Path(__file__).resolve().parent
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from support import WORKSPACE_ROOT, pythonpath_env

sys.path.insert(0, str(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "application"))

from engineering_data.contracts.simulation_input import load_path_bundle  # noqa: E402
from engineering_data.proto import dxf_primitives_pb2 as pb  # noqa: E402


SAMPLES_ROOT = WORKSPACE_ROOT / "samples" / "dxf"


def _export_sample(sample_name: str):
    sample_path = SAMPLES_ROOT / sample_name
    assert sample_path.exists(), f"missing DXF sample: {sample_path}"
    with tempfile.TemporaryDirectory() as tmp_dir:
        output_path = Path(tmp_dir) / f"{sample_path.stem}.pb"
        result = subprocess.run(
            [
                sys.executable,
                "-m",
                "engineering_data.cli.dxf_to_pb",
                "--input",
                str(sample_path),
                "--output",
                str(output_path),
            ],
            capture_output=True,
            text=True,
            env=pythonpath_env(),
            check=False,
        )

        assert result.returncode == 0, (
            f"dxf_to_pb failed for {sample_name}\n"
            f"stdout={result.stdout}\n"
            f"stderr={result.stderr}"
        )
        bundle = load_path_bundle(output_path)
    return bundle, result


def _primitive_counts(bundle) -> Counter[int]:
    return Counter(primitive.type for primitive in bundle.primitives)


def _entity_counts(bundle) -> Counter[int]:
    return Counter(meta.entity_type for meta in bundle.metadata)


def test_bra_sample_exports_closed_lwpolyline_contour() -> None:
    bundle, result = _export_sample("bra.dxf")

    assert _primitive_counts(bundle) == Counter({pb.PRIMITIVE_CONTOUR: 1})
    assert _entity_counts(bundle) == Counter({pb.DXF_ENTITY_LWPOLYLINE: 1})
    assert bundle.primitives[0].contour.closed is True
    assert len(bundle.primitives[0].contour.elements) > 0
    assert "Expected DXF R12" in result.stderr
    assert "LWPOLYLINE=1" in result.stderr


def test_demo_sample_exports_mixed_entities_and_contour_arcs() -> None:
    bundle, result = _export_sample("Demo.dxf")

    primitive_counts = _primitive_counts(bundle)
    entity_counts = _entity_counts(bundle)

    assert primitive_counts[pb.PRIMITIVE_LINE] > 0
    assert primitive_counts[pb.PRIMITIVE_CONTOUR] > 0
    assert primitive_counts[pb.PRIMITIVE_POINT] > 0
    assert entity_counts[pb.DXF_ENTITY_LINE] > 0
    assert entity_counts[pb.DXF_ENTITY_LWPOLYLINE] > 0
    assert entity_counts[pb.DXF_ENTITY_POINT] > 0

    contour_arc_count = sum(
        1
        for primitive in bundle.primitives
        if primitive.type == pb.PRIMITIVE_CONTOUR
        for element in primitive.contour.elements
        if element.type == pb.CONTOUR_ARC
    )
    assert contour_arc_count > 0
    assert "Expected DXF R12" in result.stderr
    assert "LWPOLYLINE=31" in result.stderr


def test_arc_circle_quadrants_sample_exports_native_arc_primitives() -> None:
    bundle, result = _export_sample("arc_circle_quadrants.dxf")

    assert _primitive_counts(bundle) == Counter({pb.PRIMITIVE_ARC: 4})
    assert _entity_counts(bundle) == Counter({pb.DXF_ENTITY_ARC: 4})
    assert "Warning:" not in result.stderr


def test_geometry_zoo_sample_covers_supported_importer_primitives() -> None:
    bundle, result = _export_sample("geometry_zoo.dxf")

    assert _primitive_counts(bundle) == Counter(
        {
            pb.PRIMITIVE_LINE: 1,
            pb.PRIMITIVE_ARC: 1,
            pb.PRIMITIVE_CIRCLE: 1,
            pb.PRIMITIVE_POINT: 1,
            pb.PRIMITIVE_CONTOUR: 2,
            pb.PRIMITIVE_SPLINE: 1,
            pb.PRIMITIVE_ELLIPSE: 1,
        }
    )
    assert _entity_counts(bundle) == Counter(
        {
            pb.DXF_ENTITY_LINE: 1,
            pb.DXF_ENTITY_ARC: 1,
            pb.DXF_ENTITY_CIRCLE: 1,
            pb.DXF_ENTITY_POINT: 1,
            pb.DXF_ENTITY_LWPOLYLINE: 2,
            pb.DXF_ENTITY_SPLINE: 1,
            pb.DXF_ENTITY_ELLIPSE: 1,
        }
    )
    assert "Expected DXF R12" in result.stderr
    assert "ELLIPSE=1" in result.stderr
    assert "LWPOLYLINE=1" in result.stderr
    assert "SPLINE=1" in result.stderr
