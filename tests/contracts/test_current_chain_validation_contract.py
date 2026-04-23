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
    def test_active_integration_case_catalog_keeps_current_cases(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            cases = build_cases(
                "local",
                ["integration"],
                lane_ref="full-offline-gate",
                report_dir=Path(temp_dir),
            )

        case_names = [case.name for case in cases]
        self.assertIn("tcp-precondition-matrix", case_names)
        self.assertIn("preview-flow-integration", case_names)

    def test_root_test_entry_builds_control_apps_before_workspace_validation(self) -> None:
        text = (WORKSPACE_ROOT / "scripts" / "validation" / "invoke-workspace-tests.ps1").read_text(encoding="utf-8")
        self.assertIn("Resolve-BuildPrerequisiteSuites", text)
        self.assertIn('CanonicalRelativePath "build.ps1"', text)
        self.assertIn("root build prerequisite", text)

    def test_tcp_precondition_matrix_uses_runtime_owned_baseline_for_plan_prepare(self) -> None:
        text = (
            WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_precondition_matrix.py"
        ).read_text(encoding="utf-8")
        self.assertNotIn("source_schema_root", text)
        self.assertIn("production_baseline_metadata(", text)
        self.assertIn('"dispensing_speed_mm_s": 10.0', text)


if __name__ == "__main__":
    unittest.main()
