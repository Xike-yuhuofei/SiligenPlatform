from __future__ import annotations

import importlib
import sys
import tempfile
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
PACKAGE_ROOT = WORKSPACE_ROOT / "packages" / "engineering-data"
CONTRACTS_ROOT = WORKSPACE_ROOT / "packages" / "engineering-contracts"
FIXTURE_ROOT = CONTRACTS_ROOT / "fixtures" / "cases" / "rect_diag"

sys.path.insert(0, str(PACKAGE_ROOT / "src"))

PreviewRequest = importlib.import_module("dxf_pipeline.contracts.preview").PreviewRequest  # noqa: E402
load_path_bundle = importlib.import_module("dxf_pipeline.contracts.simulation_input").load_path_bundle  # noqa: E402
generate_preview = importlib.import_module("dxf_pipeline.preview.html_preview").generate_preview  # noqa: E402
DXFPreprocessingService = importlib.import_module("dxf_pipeline.services.dxf_preprocessing").DXFPreprocessingService  # noqa: E402
legacy_pb = importlib.import_module("proto.dxf_primitives_pb2")  # noqa: E402


class DxfPipelineLegacyShimsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.pb_fixture_path = FIXTURE_ROOT / "rect_diag.pb"
        cls.dxf_fixture_path = FIXTURE_ROOT / "rect_diag.dxf"

    def test_legacy_proto_package_parses_fixture(self) -> None:
        bundle = legacy_pb.PathBundle()
        bundle.ParseFromString(self.pb_fixture_path.read_bytes())

        self.assertEqual(bundle.header.schema_version, 1)
        self.assertGreater(len(bundle.primitives), 0)

    def test_legacy_preview_import_forwards_to_canonical_generation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            artifact = generate_preview(
                PreviewRequest(
                    input_path=self.dxf_fixture_path,
                    output_dir=Path(tmp_dir),
                    title="rect_diag",
                    speed_mm_s=10.0,
                )
            )

        self.assertTrue(artifact.preview_path.endswith(".html"))
        self.assertGreater(artifact.segment_count, 0)

    def test_legacy_service_shell_forwards_to_canonical_dxf_export(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            output_path = Path(tmp_dir) / "rect_diag.pb"
            result = DXFPreprocessingService().convert_to_r12(
                str(self.dxf_fixture_path),
                str(output_path),
            )

            self.assertTrue(result.success, msg=result.error_message)
            self.assertTrue(output_path.exists())

            bundle = load_path_bundle(output_path)
            self.assertGreater(len(bundle.primitives), 0)


if __name__ == "__main__":
    unittest.main()
