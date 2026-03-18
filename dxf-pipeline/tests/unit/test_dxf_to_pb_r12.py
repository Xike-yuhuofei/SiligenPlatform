import io
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

import ezdxf

from dxf_pipeline.cli import dxf_to_pb
from dxf_pipeline.contracts.simulation_input import load_path_bundle
from proto import dxf_primitives_pb2 as pb


class DxfToPbR12Test(unittest.TestCase):
    def _run_cli(self, input_path: Path, output_path: Path, *extra_args: str):
        stdout = io.StringIO()
        stderr = io.StringIO()
        argv = ["--input", str(input_path), "--output", str(output_path), *extra_args]
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = dxf_to_pb.main(argv)
        return code, stdout.getvalue(), stderr.getvalue()

    def test_strict_r12_rejects_newer_dxf_versions(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = Path(tmp_dir)
            input_path = tmp_path / "line-r2000.dxf"
            output_path = tmp_path / "line.pb"

            doc = ezdxf.new("R2000")
            doc.modelspace().add_line((0.0, 0.0), (10.0, 0.0))
            doc.saveas(input_path)

            code, _, stderr = self._run_cli(input_path, output_path)

        self.assertEqual(code, 4)
        self.assertIn("Expected DXF R12 (AC1009)", stderr)
        self.assertFalse(output_path.exists())

    def test_no_strict_r12_allows_fallback_lwpolyline(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = Path(tmp_dir)
            input_path = tmp_path / "shape-r2000.dxf"
            output_path = tmp_path / "shape.pb"

            doc = ezdxf.new("R2000")
            doc.modelspace().add_lwpolyline([(0.0, 0.0), (10.0, 0.0), (10.0, 5.0)], close=False)
            doc.saveas(input_path)

            code, stdout, stderr = self._run_cli(input_path, output_path, "--no-strict-r12")
            bundle = load_path_bundle(output_path)

        self.assertEqual(code, 0)
        self.assertIn("Exported 1 primitives", stdout)
        self.assertIn("Proceeding because --no-strict-r12 was set.", stderr)
        self.assertIn("compatibility handlers will be used", stderr)
        self.assertEqual(len(bundle.primitives), 1)
        self.assertEqual(bundle.primitives[0].type, pb.PRIMITIVE_CONTOUR)

    def test_strict_r12_accepts_old_style_polyline(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = Path(tmp_dir)
            input_path = tmp_path / "shape-r12.dxf"
            output_path = tmp_path / "shape.pb"

            doc = ezdxf.new("R12")
            doc.modelspace().add_polyline2d([(0.0, 0.0), (10.0, 0.0), (10.0, 5.0)], close=False)
            doc.saveas(input_path)

            code, stdout, stderr = self._run_cli(input_path, output_path)
            bundle = load_path_bundle(output_path)

        self.assertEqual(code, 0)
        self.assertIn("Exported 1 primitives", stdout)
        self.assertEqual(stderr, "")
        self.assertEqual(len(bundle.primitives), 1)
        self.assertEqual(bundle.primitives[0].type, pb.PRIMITIVE_CONTOUR)


if __name__ == "__main__":
    unittest.main()
