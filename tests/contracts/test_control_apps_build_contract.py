from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.control_apps_build import (  # noqa: E402
    control_apps_build_root_probes,
    probe_control_apps_build_readiness,
    valid_control_apps_build_roots,
)


def _write_matching_cmake_cache(build_root: Path, workspace_root: Path) -> None:
    (build_root / "CMakeCache.txt").write_text(
        f"CMAKE_HOME_DIRECTORY:INTERNAL={workspace_root}\n",
        encoding="utf-8",
    )


class ControlAppsBuildContractTest(unittest.TestCase):
    def test_control_apps_build_root_probes_only_include_explicit_override_and_workspace_build_ca(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace_root = Path(temp_dir) / "workspace"
            workspace_root.mkdir()
            explicit_root = Path(temp_dir) / "explicit-build-root"
            probes = control_apps_build_root_probes(
                workspace_root,
                explicit_build_root=str(explicit_root),
            )

        self.assertEqual(
            tuple(probe.source for probe in probes),
            ("env", "workspace-build-ca"),
        )
        self.assertEqual(
            tuple(probe.root for probe in probes),
            (
                explicit_root.resolve(),
                (workspace_root / "build" / "ca").resolve(),
            ),
        )

    def test_valid_control_apps_build_roots_reject_workspace_root_without_matching_cache(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace_root = Path(temp_dir) / "workspace"
            build_root = workspace_root / "build" / "ca"
            build_root.mkdir(parents=True)

            with patch.dict(os.environ, {"LOCALAPPDATA": str(Path(temp_dir) / "localappdata")}, clear=False):
                valid_roots = valid_control_apps_build_roots(workspace_root)
                probes = control_apps_build_root_probes(workspace_root)

        self.assertEqual(valid_roots, ())
        first_probe = probes[0]
        self.assertEqual(first_probe.root, build_root.resolve())
        self.assertFalse(first_probe.accepted)
        self.assertEqual(first_probe.reason, "missing-cmake-cache")

    def test_readiness_reports_missing_artifacts_for_empty_matching_workspace_build(self) -> None:
        required_artifacts = ("siligen_runtime_gateway.exe", "siligen_runtime_service.exe")
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace_root = Path(temp_dir) / "workspace"
            build_root = workspace_root / "build" / "ca"
            build_root.mkdir(parents=True)
            _write_matching_cmake_cache(build_root, workspace_root)

            readiness = probe_control_apps_build_readiness(
                workspace_root,
                required_artifacts=required_artifacts,
            )

        self.assertFalse(readiness.ready)
        selected_probe = readiness.selected_probe
        self.assertIsNotNone(selected_probe)
        assert selected_probe is not None
        self.assertEqual(selected_probe.root, build_root.resolve())
        self.assertEqual(readiness.reason, "missing-required-artifacts")
        self.assertEqual(readiness.missing_artifacts, required_artifacts)

    def test_readiness_accepts_explicit_build_root_with_required_artifacts(self) -> None:
        required_artifacts = ("siligen_runtime_gateway.exe", "siligen_runtime_service.exe")
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace_root = Path(temp_dir) / "workspace"
            workspace_root.mkdir()
            canonical_root = workspace_root / "build" / "ca"
            canonical_root.mkdir(parents=True)
            _write_matching_cmake_cache(canonical_root, workspace_root)
            explicit_root = Path(temp_dir) / "explicit-build-root"
            for artifact_name in required_artifacts:
                artifact_path = explicit_root / "bin" / "Debug" / artifact_name
                artifact_path.parent.mkdir(parents=True, exist_ok=True)
                artifact_path.write_text("", encoding="utf-8")

            readiness = probe_control_apps_build_readiness(
                workspace_root,
                required_artifacts=required_artifacts,
                explicit_build_root=str(explicit_root),
            )

        self.assertTrue(readiness.ready)
        selected_probe = readiness.selected_probe
        self.assertIsNotNone(selected_probe)
        assert selected_probe is not None
        self.assertEqual(selected_probe.root, explicit_root.resolve())
        self.assertEqual(selected_probe.source, "env")
        self.assertEqual(readiness.missing_artifacts, ())


if __name__ == "__main__":
    unittest.main()
