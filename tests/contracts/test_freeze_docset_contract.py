from __future__ import annotations

import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
FREEZE_ROOT = WORKSPACE_ROOT / "docs" / "architecture" / "dsp-e2e-spec"
FREEZE_REQUIRED_DOCS = (
    FREEZE_ROOT / "README.md",
    FREEZE_ROOT / "dsp-e2e-spec-s05-module-boundary-interface-contract.md",
    FREEZE_ROOT / "dsp-e2e-spec-s06-repo-structure-guide.md",
    FREEZE_ROOT / "dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md",
    FREEZE_ROOT / "dsp-e2e-spec-s10-frozen-directory-index.md",
)


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore")


class FreezeDocsetContractTest(unittest.TestCase):
    def test_formal_freeze_docset_exists(self) -> None:
        self.assertTrue(FREEZE_ROOT.exists(), msg=f"formal freeze root missing: {FREEZE_ROOT}")
        for path in FREEZE_REQUIRED_DOCS:
            self.assertTrue(path.exists(), msg=f"formal freeze document missing: {path}")

    def test_baseline_and_acceptance_report_point_to_same_freeze_root(self) -> None:
        workspace_baseline = _read(WORKSPACE_ROOT / "docs" / "architecture" / "workspace-baseline.md")
        acceptance_report = _read(WORKSPACE_ROOT / "docs" / "architecture" / "system-acceptance-report.md")

        self.assertIn("docs/architecture/dsp-e2e-spec/", workspace_baseline)
        self.assertIn("唯一正式冻结基线入口", workspace_baseline)
        self.assertIn("docs/architecture/dsp-e2e-spec/", acceptance_report)

    def test_root_gates_publish_freeze_docset_as_hard_requirement(self) -> None:
        validation_runner = _read(
            WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src" / "test_kit" / "workspace_validation.py"
        )
        local_gate = _read(WORKSPACE_ROOT / "scripts" / "validation" / "run-local-validation-gate.ps1")
        ci_entry = _read(WORKSPACE_ROOT / "ci.ps1")

        self.assertIn('name="dsp-e2e-spec-docset"', validation_runner)
        self.assertIn("validate_dsp_e2e_spec_docset.py", local_gate)
        self.assertIn("dsp-e2e-spec-docset", local_gate)
        self.assertIn("dsp-e2e-spec-docset", ci_entry)


if __name__ == "__main__":
    unittest.main()
