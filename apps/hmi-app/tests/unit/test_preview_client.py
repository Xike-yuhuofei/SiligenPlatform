import json
import os
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[2]
WORKSPACE_ROOT = PROJECT_ROOT.parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.integrations.dxf_pipeline.preview_client import DxfPipelinePreviewClient


class PreviewClientContractTest(unittest.TestCase):
    def test_generate_preview_invokes_canonical_engineering_data_cli(self) -> None:
        dxf_path = (
            WORKSPACE_ROOT
            / "packages"
            / "engineering-contracts"
            / "fixtures"
            / "cases"
            / "rect_diag"
            / "rect_diag.dxf"
        )
        payload = {
            "preview_path": str(WORKSPACE_ROOT / "tmp" / "preview.html"),
            "entity_count": 5,
            "segment_count": 5,
            "point_count": 10,
            "total_length_mm": 541.4213562373095,
            "estimated_time_s": 27.071067811865476,
        }

        client = DxfPipelinePreviewClient(siligen_root=WORKSPACE_ROOT)

        with mock.patch.dict(
            os.environ,
            {
                "SILIGEN_ENGINEERING_DATA_ROOT": "",
                "SILIGEN_ENGINEERING_DATA_PYTHON": "",
                "SILIGEN_WORKSPACE_ROOT": "",
            },
            clear=False,
        ):
            with mock.patch(
                "hmi_client.integrations.dxf_pipeline.preview_client.subprocess.run"
            ) as run_mock:
                run_mock.return_value = mock.Mock(
                    returncode=0,
                    stdout=json.dumps(payload, ensure_ascii=False),
                    stderr="",
                )
                result = client.generate_preview(str(dxf_path), speed_mm_s=20.0, title="rect_diag")

        self.assertTrue(result.ok)
        args, kwargs = run_mock.call_args
        command = args[0]
        self.assertEqual(command[1:3], ["-m", "engineering_data.cli.generate_preview"])
        self.assertIn("--json", command)
        self.assertIn("--deterministic", command)

        python_path_entries = kwargs["env"]["PYTHONPATH"].split(os.pathsep)
        self.assertIn(
            str(WORKSPACE_ROOT / "packages" / "engineering-data" / "src"),
            python_path_entries,
        )

    def test_generate_preview_returns_timeout_error(self) -> None:
        dxf_path = (
            WORKSPACE_ROOT
            / "packages"
            / "engineering-contracts"
            / "fixtures"
            / "cases"
            / "rect_diag"
            / "rect_diag.dxf"
        )
        client = DxfPipelinePreviewClient(siligen_root=WORKSPACE_ROOT)

        with mock.patch(
            "hmi_client.integrations.dxf_pipeline.preview_client.subprocess.run",
            side_effect=subprocess.TimeoutExpired(cmd=["python"], timeout=1.0),
        ):
            result = client.generate_preview(str(dxf_path), speed_mm_s=20.0, title="rect_diag")

        self.assertFalse(result.ok)
        self.assertIn("超时", result.error_message)

    def test_generate_preview_requires_preview_path_in_payload(self) -> None:
        dxf_path = (
            WORKSPACE_ROOT
            / "packages"
            / "engineering-contracts"
            / "fixtures"
            / "cases"
            / "rect_diag"
            / "rect_diag.dxf"
        )
        client = DxfPipelinePreviewClient(siligen_root=WORKSPACE_ROOT)

        with mock.patch(
            "hmi_client.integrations.dxf_pipeline.preview_client.subprocess.run"
        ) as run_mock:
            run_mock.return_value = mock.Mock(
                returncode=0,
                stdout=json.dumps({"segment_count": 5}, ensure_ascii=False),
                stderr="",
            )
            result = client.generate_preview(str(dxf_path), speed_mm_s=20.0, title="rect_diag")

        self.assertFalse(result.ok)
        self.assertIn("preview_path", result.error_message)


if __name__ == "__main__":
    unittest.main()
