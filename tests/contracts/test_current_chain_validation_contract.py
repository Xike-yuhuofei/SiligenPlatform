from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.workspace_validation import build_cases  # noqa: E402


class CurrentChainValidationContractTest(unittest.TestCase):
    def test_active_integration_case_catalog_excludes_recipe_compatibility_case(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            cases = build_cases(
                "local",
                ["integration"],
                lane_ref="full-offline-gate",
                report_dir=Path(temp_dir),
            )

        case_names = [case.name for case in cases]
        self.assertNotIn("recipe-config-compatibility", case_names)
        self.assertIn("tcp-precondition-matrix", case_names)
        self.assertIn("preview-flow-integration", case_names)

    def test_root_test_entry_builds_control_apps_before_workspace_validation(self) -> None:
        text = (WORKSPACE_ROOT / "scripts" / "validation" / "invoke-workspace-tests.ps1").read_text(encoding="utf-8")
        self.assertIn("Resolve-BuildPrerequisiteSuites", text)
        self.assertIn('CanonicalRelativePath "build.ps1"', text)
        self.assertIn("root build prerequisite", text)

    def test_tcp_precondition_matrix_does_not_resolve_active_recipe_for_plan_prepare(self) -> None:
        text = (
            WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_precondition_matrix.py"
        ).read_text(encoding="utf-8")
        self.assertNotIn("resolve_active_recipe", text)
        self.assertNotIn("recipe_context_metadata(", text)
        self.assertNotIn("ensure_published_recipe_version(", text)
        self.assertNotIn('"recipe_id":', text)
        self.assertNotIn('"version_id":', text)
        self.assertIn("production_baseline_metadata(", text)
        self.assertIn('"dispensing_speed_mm_s": 10.0', text)

    def test_first_layer_rereview_does_not_forward_legacy_recipe_cli(self) -> None:
        text = (
            WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_first_layer_rereview.ps1"
        ).read_text(encoding="utf-8")
        self.assertNotIn("--recipe-id", text)
        self.assertNotIn("--version-id", text)
        self.assertNotIn("AllowSkipOnActiveRecipe", text)


if __name__ == "__main__":
    unittest.main()
