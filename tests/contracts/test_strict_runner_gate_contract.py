import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
STRICT_HIL_WORKFLOW = ROOT / ".github" / "workflows" / "strict-hil-gate.yml"
STRICT_NATIVE_WORKFLOW = ROOT / ".github" / "workflows" / "strict-native-gate.yml"
STRICT_PR_WORKFLOW = ROOT / ".github" / "workflows" / "strict-pr-gate.yml"
STRICT_NATIVE_CACHE_SCRIPT = ROOT / "scripts" / "validation" / "prepare-strict-native-build-cache.ps1"
TCP_PRECONDITION_MATRIX = ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_precondition_matrix.py"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


class StrictRunnerGateContractTest(unittest.TestCase):
    def test_strict_hil_gate_reclassifies_on_label_changes(self) -> None:
        workflow = _read(STRICT_HIL_WORKFLOW)

        self.assertIn("types:", workflow)
        self.assertIn("- labeled", workflow)
        self.assertIn("- unlabeled", workflow)
        self.assertIn("label hardware-sensitive", workflow)

    def test_strict_native_gate_reclassifies_on_label_changes(self) -> None:
        workflow = _read(STRICT_NATIVE_WORKFLOW)

        self.assertIn("types:", workflow)
        self.assertIn("- labeled", workflow)
        self.assertIn("- unlabeled", workflow)
        self.assertIn("label native-sensitive", workflow)

    def test_strict_hil_gate_fail_closed_before_self_hosted_runner_for_untrusted_pr(self) -> None:
        workflow = _read(STRICT_HIL_WORKFLOW)

        self.assertIn("trusted_hil_runner", workflow)
        self.assertIn("forked pull request cannot run self-hosted HIL", workflow)
        self.assertIn(
            "needs.classify-hil-requirement.outputs.requires_hil == 'true' && "
            "needs.classify-hil-requirement.outputs.trusted_hil_runner == 'true'",
            workflow,
        )
        self.assertIn("Strict HIL Gate required but self-hosted HIL runner is not trusted", workflow)

    def test_strict_hil_gate_reuses_same_workflow_offline_prereq_with_provenance_guard(self) -> None:
        workflow = _read(STRICT_HIL_WORKFLOW)

        self.assertIn("offline-prereq:", workflow)
        self.assertIn("Run HIL Offline Prerequisites", workflow)
        self.assertIn("strict-hil-offline-prereq-evidence", workflow)
        self.assertIn("actions/download-artifact@v4", workflow)
        self.assertIn("-OfflinePrereqReport _strict-hil-offline-prereq\\workspace-validation.json", workflow)
        self.assertIn("needs.offline-prereq.result == 'success'", workflow)
        self.assertIn("Strict HIL Gate required but offline prerequisites result was", workflow)

    def test_strict_native_gate_fail_closed_before_self_hosted_runner_for_untrusted_pr(self) -> None:
        workflow = _read(STRICT_NATIVE_WORKFLOW)

        self.assertIn("trusted_native_runner", workflow)
        self.assertIn("forked pull request cannot run self-hosted native build", workflow)
        self.assertIn(
            "needs.classify-native-requirement.outputs.requires_native == 'true' && "
            "needs.classify-native-requirement.outputs.trusted_native_runner == 'true'",
            workflow,
        )
        self.assertIn("Strict Native Gate required but self-hosted native build runner is not trusted", workflow)

    def test_strict_native_gate_uses_guarded_persistent_build_cache(self) -> None:
        workflow = _read(STRICT_NATIVE_WORKFLOW)

        self.assertIn("clean: false", workflow)
        self.assertIn("Prepare strict native build cache", workflow)
        self.assertIn(".\\scripts\\validation\\prepare-strict-native-build-cache.ps1", workflow)

    def test_strict_native_cache_guard_uses_scoped_safe_directory_for_runner_ownership(self) -> None:
        script = _read(STRICT_NATIVE_CACHE_SCRIPT)

        self.assertIn("safe.directory=$resolvedWorkspaceRoot", script)
        self.assertIn("git @gitBaseArgs status --porcelain --untracked-files=no", script)
        self.assertIn("git @gitBaseArgs status --porcelain --untracked-files=all", script)

    def test_strict_pr_gate_parallelizes_sub_gates_and_keeps_required_summary_name(self) -> None:
        workflow = _read(STRICT_PR_WORKFLOW)

        for job_id in ("hmi-gateway-contract:", "legacy-exit:", "semgrep:", "import-linter:"):
            self.assertIn(job_id, workflow)
        self.assertIn("strict-pr-gate:", workflow)
        self.assertIn("name: Strict PR Gate", workflow)
        self.assertIn("needs:", workflow)
        self.assertIn("- hmi-gateway-contract", workflow)
        self.assertIn("- legacy-exit", workflow)
        self.assertIn("- semgrep", workflow)
        self.assertIn("- import-linter", workflow)
        self.assertIn("if: always()", workflow)
        self.assertIn("finished with result", workflow)

    def test_tcp_precondition_matrix_default_runtime_workspace_is_not_under_report_tree(self) -> None:
        scratch_root = ROOT / "build" / "contract-scratch" / "strict-runner-gate"
        if scratch_root.exists():
            shutil.rmtree(scratch_root)
        scratch_root.parent.mkdir(parents=True, exist_ok=True)

        with tempfile.TemporaryDirectory(dir=scratch_root.parent) as temp_dir:
            report_dir = Path(temp_dir) / ("very-long-report-root-" + ("x" * 80)) / "offline-prereq"
            missing_gateway = Path(temp_dir) / "missing-runtime-gateway.exe"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(TCP_PRECONDITION_MATRIX),
                    "--report-dir",
                    str(report_dir),
                    "--gateway-exe",
                    str(missing_gateway),
                    "--allow-skip-on-missing-gateway",
                ],
                cwd=str(ROOT),
                capture_output=True,
                text=True,
            )

            output = f"{completed.stdout}\n{completed.stderr}"
            self.assertEqual(completed.returncode, 11, msg=output)

            report = json.loads((report_dir / "tcp-precondition-matrix.json").read_text(encoding="utf-8"))
            gateway_cwd = Path(report["gateway_cwd"])

            self.assertFalse(
                str(gateway_cwd).startswith(str(report_dir.resolve())),
                msg=f"gateway_cwd must not be nested under report_dir: {gateway_cwd}",
            )
            self.assertTrue(
                str(gateway_cwd).startswith(str((ROOT / "build" / "runtime-workspaces" / "tcp-precondition-matrix").resolve())),
                msg=f"gateway_cwd must use the canonical runtime workspace root: {gateway_cwd}",
            )
            self.assertTrue((gateway_cwd / "uploads" / "dxf").is_dir())
            projected_artifact_path = gateway_cwd / "uploads" / "dxf" / (("x" * 80) + "_rect_diag.dxf")
            self.assertLess(len(str(projected_artifact_path)), 240)

            shutil.rmtree(gateway_cwd, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
