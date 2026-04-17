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

from ui.offline_preview_builder import (
    _CONTROL_APPS_BUILD_ROOT_ENV,
    _resolve_planner_cli_executable,
    build_offline_preview_payload,
)


class OfflinePreviewBuilderTest(unittest.TestCase):
    def test_build_offline_preview_payload_invokes_planner_cli_same_source_snapshot(self) -> None:
        captured = {}
        sample_path = PROJECT_ROOT.parent.parent / "samples" / "dxf" / "rect_diag.dxf"

        def _fake_run(
            command,
            *,
            cwd=None,
            capture_output=False,
            text=False,
            encoding=None,
            errors=None,
            check=False,
        ):
            captured["command"] = list(command)
            captured["kwargs"] = {
                "cwd": cwd,
                "capture_output": capture_output,
                "text": text,
                "encoding": encoding,
                "errors": errors,
                "check": check,
            }
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

        with patch(
            "ui.offline_preview_builder._resolve_planner_cli_executable",
            autospec=True,
            return_value=Path("C:/tmp/siligen_planner_cli.exe"),
        ):
            with patch("ui.offline_preview_builder.subprocess.run", autospec=True, side_effect=_fake_run):
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
        self.assertEqual(Path(str(captured["kwargs"]["cwd"])).resolve(), PROJECT_ROOT.parent.parent.resolve())

    def test_build_offline_preview_payload_raises_on_nonzero_exit(self) -> None:
        sample_path = PROJECT_ROOT.parent.parent / "samples" / "dxf" / "rect_diag.dxf"

        with patch(
            "ui.offline_preview_builder._resolve_planner_cli_executable",
            autospec=True,
            return_value=Path("C:/tmp/siligen_planner_cli.exe"),
        ):
            with patch(
                "ui.offline_preview_builder.subprocess.run",
                autospec=True,
                return_value=subprocess.CompletedProcess(["planner"], 1, stdout="", stderr="planner failed"),
            ):
                with self.assertRaisesRegex(RuntimeError, "planner failed"):
                    build_offline_preview_payload(sample_path, speed_mm_s=20.0, dry_run=False)

    def test_build_offline_preview_payload_raises_on_invalid_json(self) -> None:
        sample_path = PROJECT_ROOT.parent.parent / "samples" / "dxf" / "rect_diag.dxf"

        with patch(
            "ui.offline_preview_builder._resolve_planner_cli_executable",
            autospec=True,
            return_value=Path("C:/tmp/siligen_planner_cli.exe"),
        ):
            with patch(
                "ui.offline_preview_builder.subprocess.run",
                autospec=True,
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

    def test_resolve_planner_cli_executable_rejects_explicit_override_outside_workspace_build(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            workspace_root = temp_root / "workspace"
            workspace_root.mkdir()
            override_root = temp_root / "shared-build"
            override_bin = override_root / "bin" / "Debug"
            override_bin.mkdir(parents=True)

            override_cli = override_bin / "siligen_planner_cli.exe"
            override_cli.write_text("", encoding="utf-8")
            other_workspace = temp_root / "other-workspace"
            other_workspace.mkdir()
            (override_root / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={other_workspace}\n",
                encoding="utf-8",
            )

            with patch.dict(
                os.environ,
                {_CONTROL_APPS_BUILD_ROOT_ENV: str(override_root)},
                clear=False,
            ):
                with self.assertRaisesRegex(FileNotFoundError, "siligen_planner_cli.exe"):
                    _resolve_planner_cli_executable(workspace_root)

    def test_resolve_planner_cli_executable_supports_workspace_ca_build_root(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            workspace_root = temp_root / "workspace"
            workspace_root.mkdir()
            build_root = workspace_root / "build" / "ca"
            build_bin = build_root / "bin" / "Debug"
            build_bin.mkdir(parents=True)

            cli_path = build_bin / "siligen_planner_cli.exe"
            cli_path.write_text("", encoding="utf-8")
            (build_root / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={workspace_root}\n",
                encoding="utf-8",
            )

            resolved = _resolve_planner_cli_executable(workspace_root)

        self.assertEqual(resolved, cli_path.resolve())

    def test_resolve_planner_cli_executable_ignores_workspace_token_build_root(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            workspace_root = temp_root / "workspace"
            workspace_root.mkdir()
            local_app_data = temp_root / "localappdata"
            ss_root = local_app_data / "SS"
            token = "1234567890ab"
            token_build_root = ss_root / f"cab-{token}"
            token_bin = token_build_root / "bin" / "Debug"
            token_bin.mkdir(parents=True)

            token_cli = token_bin / "siligen_planner_cli.exe"
            token_cli.write_text("", encoding="utf-8")
            (token_build_root / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={workspace_root}\n",
                encoding="utf-8",
            )

            with patch.dict(os.environ, {"LOCALAPPDATA": str(local_app_data)}, clear=False):
                with self.assertRaisesRegex(FileNotFoundError, "siligen_planner_cli.exe"):
                    _resolve_planner_cli_executable(workspace_root)


if __name__ == "__main__":
    unittest.main()
