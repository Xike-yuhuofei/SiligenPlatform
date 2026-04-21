from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = ROOT / "scripts" / "validation" / "run_refactor_guard_fallback.py"
CONFIG_PATH = ROOT / "scripts" / "validation" / "semgrep" / "arch-rules.yml"


class RefactorGuardFallbackContractTest(unittest.TestCase):
    def test_fallback_scanner_enforces_rule_paths_and_exclusions(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace_root = Path(temp_dir) / "workspace"
            report_path = workspace_root / "tests" / "reports" / "semgrep" / "semgrep-results.json"

            (workspace_root / "apps" / "demo").mkdir(parents=True, exist_ok=True)
            (workspace_root / "docs").mkdir(parents=True, exist_ok=True)
            (workspace_root / "scripts" / "validation").mkdir(parents=True, exist_ok=True)
            (workspace_root / "shared" / "kernel").mkdir(parents=True, exist_ok=True)
            (workspace_root / "shared" / "testing").mkdir(parents=True, exist_ok=True)

            (workspace_root / "apps" / "demo" / "run.ps1").write_text(
                "Join-Path $PSScriptRoot 'control-core/start.ps1'\n"
                "Join-Path $PSScriptRoot 'legacy/runtime-path'\n",
                encoding="utf-8",
            )
            (workspace_root / "apps" / "demo" / "main.py").write_text(
                "target = 'modules/sample/internal/private_api.py'\n",
                encoding="utf-8",
            )
            (workspace_root / "shared" / "kernel" / "surface.py").write_text(
                "PUBLIC_OWNER = 'apps/runtime-service/bootstrap'\n",
                encoding="utf-8",
            )
            (workspace_root / "shared" / "testing" / "skip.py").write_text(
                "TEST_HELPER = 'apps/runtime-service/bootstrap'\n",
                encoding="utf-8",
            )
            (workspace_root / "docs" / "skip.md").write_text(
                "pack" "ages/legacy-doc-path\n",
                encoding="utf-8",
            )
            (workspace_root / "build.ps1").write_text(
                "Write-Output 'examp" "les/obsolete-entry'\n",
                encoding="utf-8",
            )

            completed = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT_PATH),
                    "--workspace-root",
                    str(workspace_root),
                    "--config-path",
                    str(CONFIG_PATH),
                    "--report-json",
                    str(report_path),
                    "--scan-target",
                    str(workspace_root / "apps"),
                    "--scan-target",
                    str(workspace_root / "shared"),
                    "--scan-target",
                    str(workspace_root / "docs"),
                    "--scan-target",
                    str(workspace_root / "build.ps1"),
                ],
                cwd=str(ROOT),
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )

            self.assertEqual(completed.returncode, 1, completed.stderr)
            payload = json.loads(report_path.read_text(encoding="utf-8"))
            findings = payload["results"]
            finding_keys = {(item["check_id"], item["path"]) for item in findings}

            self.assertIn(
                ("refactor.guard.no-retired-entry-reference-in-entrypoints", "apps/demo/run.ps1"),
                finding_keys,
            )
            self.assertIn(
                ("refactor.guard.no-legacy-named-runtime-config-paths", "apps/demo/run.ps1"),
                finding_keys,
            )
            self.assertIn(
                ("refactor.guard.apps-no-module-internal-paths", "apps/demo/main.py"),
                finding_keys,
            )
            self.assertIn(
                ("refactor.guard.shared-no-apps-reverse-dependency", "shared/kernel/surface.py"),
                finding_keys,
            )
            self.assertIn(
                ("refactor.guard.no-retired-root-reference-live-files", "build.ps1"),
                finding_keys,
            )
            self.assertNotIn(
                ("refactor.guard.no-retired-root-reference-live-files", "docs/skip.md"),
                finding_keys,
            )
            self.assertNotIn(
                ("refactor.guard.shared-no-apps-reverse-dependency", "shared/testing/skip.py"),
                finding_keys,
            )


if __name__ == "__main__":
    unittest.main()
