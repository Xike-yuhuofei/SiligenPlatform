import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src" / "hmi_client"))

from ui.offline_preview_builder import _resolve_planner_cli_executable, build_offline_preview_payload


class OfflinePreviewBuilderTest(unittest.TestCase):
    def test_build_offline_preview_payload_invokes_planner_cli_same_source_snapshot(self) -> None:
        captured = {}
        sample_path = PROJECT_ROOT.parent.parent / "samples" / "dxf" / "rect_diag.dxf"

        def _fake_run(command, **kwargs):
            captured["command"] = list(command)
            captured["kwargs"] = dict(kwargs)
            return subprocess.CompletedProcess(
                command,
                0,
                stdout=json.dumps(
                    {
                        "snapshot_id": "snapshot-1",
                        "snapshot_hash": "hash-1",
                        "plan_id": "plan-1",
                        "preview_source": "planned_glue_snapshot",
                        "preview_kind": "glue_points",
                        "glue_points": [{"x": 0.0, "y": 0.0}],
                    }
                ),
                stderr="",
            )

        with patch("ui.offline_preview_builder._resolve_planner_cli_executable", return_value=Path("C:/tmp/siligen_planner_cli.exe")):
            with patch("ui.offline_preview_builder.subprocess.run", side_effect=_fake_run):
                payload = build_offline_preview_payload(sample_path, speed_mm_s=18.5, dry_run=True)

        self.assertEqual(payload["plan_id"], "plan-1")
        self.assertTrue(str(captured["command"][0]).endswith("siligen_planner_cli.exe"))
        self.assertIn("dxf-preview-snapshot", captured["command"])
        self.assertIn("--json", captured["command"])
        self.assertIn("--no-optimize-path", captured["command"])
        self.assertIn("--use-interpolation-planner", captured["command"])
        self.assertIn("--interpolation-algorithm", captured["command"])
        self.assertIn("--dry-run", captured["command"])
        self.assertIn("--dry-run-speed", captured["command"])
        self.assertEqual(captured["kwargs"]["check"], False)
        self.assertTrue(str(captured["kwargs"]["cwd"]).endswith("wt-t150-fix"))

    def test_build_offline_preview_payload_raises_on_nonzero_exit(self) -> None:
        sample_path = PROJECT_ROOT.parent.parent / "samples" / "dxf" / "rect_diag.dxf"

        with patch("ui.offline_preview_builder._resolve_planner_cli_executable", return_value=Path("C:/tmp/siligen_planner_cli.exe")):
            with patch(
                "ui.offline_preview_builder.subprocess.run",
                return_value=subprocess.CompletedProcess(["planner"], 1, stdout="", stderr="planner failed"),
            ):
                with self.assertRaisesRegex(RuntimeError, "planner failed"):
                    build_offline_preview_payload(sample_path, speed_mm_s=20.0, dry_run=False)

    def test_build_offline_preview_payload_raises_on_invalid_json(self) -> None:
        sample_path = PROJECT_ROOT.parent.parent / "samples" / "dxf" / "rect_diag.dxf"

        with patch("ui.offline_preview_builder._resolve_planner_cli_executable", return_value=Path("C:/tmp/siligen_planner_cli.exe")):
            with patch(
                "ui.offline_preview_builder.subprocess.run",
                return_value=subprocess.CompletedProcess(["planner"], 0, stdout="not-json", stderr=""),
            ):
                with self.assertRaisesRegex(RuntimeError, "非 JSON"):
                    build_offline_preview_payload(sample_path, speed_mm_s=20.0, dry_run=False)

    def test_resolve_planner_cli_executable_prefers_workspace_build_root_over_localappdata(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            workspace_root = temp_root / "workspace"
            workspace_root.mkdir()
            local_app_data = temp_root / "localappdata"
            local_app_data.mkdir()

            workspace_build_root = workspace_root / "build" / "control-apps"
            localappdata_build_root = local_app_data / "SiligenSuite" / "control-apps-build"
            workspace_bin = workspace_build_root / "bin" / "Debug"
            localappdata_bin = localappdata_build_root / "bin" / "Debug"
            workspace_bin.mkdir(parents=True)
            localappdata_bin.mkdir(parents=True)

            workspace_cli = workspace_bin / "siligen_planner_cli.exe"
            localappdata_cli = localappdata_bin / "siligen_planner_cli.exe"
            workspace_cli.write_text("", encoding="utf-8")
            localappdata_cli.write_text("", encoding="utf-8")

            cache_line = f"CMAKE_HOME_DIRECTORY:INTERNAL={workspace_root}\n"
            (workspace_build_root / "CMakeCache.txt").write_text(cache_line, encoding="utf-8")
            (localappdata_build_root / "CMakeCache.txt").write_text(cache_line, encoding="utf-8")

            with patch.dict(os.environ, {"LOCALAPPDATA": str(local_app_data)}, clear=False):
                resolved = _resolve_planner_cli_executable(workspace_root)

        self.assertEqual(resolved, workspace_cli.resolve())


if __name__ == "__main__":
    unittest.main()
