from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BOUNDARY_MODEL = ROOT / "scripts" / "validation" / "boundaries" / "module-boundaries.json"
BRIDGE_REGISTRY = ROOT / "scripts" / "validation" / "boundaries" / "bridge-registry.json"
BOUNDARY_POLICY = ROOT / "scripts" / "validation" / "boundaries" / "boundary-policy.json"
GATES = ROOT / "scripts" / "validation" / "gates" / "gates.json"
RUN_LOCAL_VALIDATION_GATE = ROOT / "scripts" / "validation" / "run-local-validation-gate.ps1"
RUNNER = ROOT / "scripts" / "validation" / "invoke-module-boundary-audit.ps1"
EVALUATOR = ROOT / "scripts" / "validation" / "boundary_audit" / "module_boundary_audit.py"


def _json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def _write(root: Path, rel: str, text: str) -> Path:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


class ModuleBoundaryAuditContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.boundary = _json(BOUNDARY_MODEL)
        self.bridges = _json(BRIDGE_REGISTRY)
        self.policy = _json(BOUNDARY_POLICY)

    def test_authoritative_json_models_are_loadable_and_schema_shaped(self) -> None:
        self.assertEqual(self.boundary["schemaVersion"], 1)
        self.assertEqual(self.bridges["schemaVersion"], 1)
        self.assertEqual(self.policy["schemaVersion"], 1)
        self.assertTrue(self.boundary["modules"])
        self.assertTrue(self.bridges["bridges"])
        self.assertEqual({item["mode"] for item in self.policy["policies"]}, {"pr", "full", "nightly"})

        for module in self.boundary["modules"]:
            with self.subTest(module=module.get("id")):
                self.assertTrue(module["id"])
                self.assertTrue(module["owner"])
                self.assertIsInstance(module["roots"], list)
                self.assertIsInstance(module["public_surfaces"], list)
                self.assertIsInstance(module["private_surfaces"], list)
                self.assertIsInstance(module["allowed_dependencies"], list)
                self.assertIsInstance(module["forbidden_dependencies"], list)
                for root in module["roots"]:
                    self.assertTrue((ROOT / root).exists(), msg=f"configured module root missing: {root}")

        for bridge in self.bridges["bridges"]:
            with self.subTest(bridge=bridge.get("id")):
                for key in (
                    "id",
                    "owner",
                    "consumer",
                    "reason",
                    "allowed_files",
                    "allowed_targets",
                    "expiry_condition",
                    "cleanup_ticket",
                ):
                    self.assertTrue(bridge.get(key), msg=f"bridge field missing: {key}")

        for policy in self.policy["policies"]:
            with self.subTest(policy=policy.get("mode")):
                self.assertTrue(policy["blocking_findings"])
                self.assertIsInstance(policy["advisory_findings"], list)
                self.assertIsInstance(policy["coverage_requirements"], dict)

    def test_gate_config_uses_module_boundary_audit_as_only_production_step(self) -> None:
        gates = _json(GATES)["gates"]
        pr_steps = {step["id"]: step for step in gates["pr"]["steps"]}
        self.assertIn("module-boundary-audit", pr_steps)
        self.assertNotIn("module-boundary-bridges", pr_steps)

        production_text = "\n".join(
            [
                _read(GATES),
                _read(RUN_LOCAL_VALIDATION_GATE),
                _read(RUNNER),
                _read(EVALUATOR),
            ]
        )
        self.assertIn("invoke-module-boundary-audit.ps1", production_text)
        self.assertNotIn("assert-module-boundary-bridges.ps1", production_text)

    def test_bridge_registry_contains_formalized_quarantine_rules(self) -> None:
        registry_text = _read(BRIDGE_REGISTRY)
        for needle in (
            "ITestRecordRepository",
            "ITestConfigurationPort",
            "ICMPTestPresetPort",
            "TestDataTypes",
            "siligen_workflow_adapters_public",
            "siligen_workflow_runtime_consumer_public",
        ):
            self.assertIn(needle, registry_text)

    def test_boundary_policy_models_required_owner_forbidden_and_retired_surfaces(self) -> None:
        policy_text = _read(BOUNDARY_POLICY)
        for needle in (
            "runtime-execution-application-public-target",
            "process-path-application-public-target",
            "process-path-contracts-public-target",
            "trace-diagnostics-contracts-public-target",
            "siligen_runtime_execution_workflow_runtime_compat",
            "siligen_runtime_execution_workflow_domain_compat",
            "modules/process-path/contracts/include/process_path/contracts/IDXFPathSourcePort.h",
        ):
            self.assertIn(needle, policy_text)

    def _run_audit(self, workspace: Path, changed_file: str) -> tuple[subprocess.CompletedProcess[str], dict]:
        report_dir = workspace / "reports" / changed_file.replace("/", "_").replace("\\", "_")
        completed = subprocess.run(
            [
                "powershell",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(RUNNER),
                "-Mode",
                "pr",
                "-WorkspaceRoot",
                str(workspace),
                "-ReportDir",
                str(report_dir),
                "-ChangedFile",
                changed_file,
            ],
            cwd=str(ROOT),
            capture_output=True,
            text=True,
        )
        audit_path = report_dir / "module-boundary-audit.json"
        self.assertTrue(audit_path.exists(), msg=completed.stdout + completed.stderr)
        return completed, json.loads(audit_path.read_text(encoding="utf-8-sig"))

    def test_runner_emits_required_artifacts_for_pr_smoke(self) -> None:
        with tempfile.TemporaryDirectory(prefix="module-boundary-audit-smoke-") as temp_dir:
            workspace = Path(temp_dir)
            changed = "scripts/validation/gates/gates.json"
            _write(workspace, changed, "{}\n")
            completed, audit = self._run_audit(workspace, changed)
            self.assertEqual(completed.returncode, 0, msg=completed.stdout + completed.stderr)
            report_dir = Path(audit["reports"]["coverage"]).parent
            self.assertEqual(audit["status"], "passed")
            self.assertEqual(audit["scanned_files"], [])
            actual_report_root = workspace / "reports" / changed.replace("/", "_").replace("\\", "_")
            for rel in (
                "module-boundary-audit.json",
                "module-boundary-audit.md",
                "module-boundary-coverage.json",
                "bridge-registry-status.json",
                "logs/model-loader.log",
                "logs/dependency-extractor.log",
                "logs/policy-evaluator.log",
            ):
                self.assertTrue((actual_report_root / rel).exists(), msg=rel)
            self.assertEqual(str(report_dir), ".")

    def test_unregistered_bridge_blocks_pr_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="module-boundary-audit-unregistered-") as temp_dir:
            workspace = Path(temp_dir)
            changed = "apps/runtime-service/CMakeLists.txt"
            _write(
                workspace,
                changed,
                "add_library(runtime_service INTERFACE)\n"
                "target_link_libraries(runtime_service INTERFACE siligen_new_escape_bridge)\n",
            )
            completed, audit = self._run_audit(workspace, changed)
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("unregistered_bridge", {item["kind"] for item in audit["blocking_findings"]})

    def test_private_surface_access_blocks_pr_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="module-boundary-audit-private-") as temp_dir:
            workspace = Path(temp_dir)
            changed = "modules/dispense-packaging/application/Bad.cpp"
            _write(
                workspace,
                changed,
                '#include "modules/process-path/domain/trajectory/domain-services/TrajectoryShaper.h"\n',
            )
            completed, audit = self._run_audit(workspace, changed)
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("private_surface_access", {item["kind"] for item in audit["blocking_findings"]})

    def test_forbidden_compat_target_blocks_pr_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="module-boundary-audit-compat-") as temp_dir:
            workspace = Path(temp_dir)
            changed = "apps/planner-cli/CMakeLists.txt"
            _write(
                workspace,
                changed,
                "add_library(planner_cli INTERFACE)\n"
                "target_link_libraries(planner_cli INTERFACE siligen_runtime_execution_workflow_runtime_compat)\n",
            )
            completed, audit = self._run_audit(workspace, changed)
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("forbidden_dependency", {item["kind"] for item in audit["blocking_findings"]})

    def test_retired_surface_reintroduction_blocks_pr_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="module-boundary-audit-retired-") as temp_dir:
            workspace = Path(temp_dir)
            changed = "modules/process-path/contracts/include/process_path/contracts/IDXFPathSourcePort.h"
            _write(workspace, changed, "#pragma once\n")
            completed, audit = self._run_audit(workspace, changed)
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("retired_surface_reintroduced", {item["kind"] for item in audit["blocking_findings"]})

    def test_missing_required_owner_target_blocks_pr_audit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="module-boundary-audit-owner-") as temp_dir:
            workspace = Path(temp_dir)
            changed = "modules/runtime-execution/application/CMakeLists.txt"
            _write(workspace, changed, "add_library(not_the_required_target INTERFACE)\n")
            completed, audit = self._run_audit(workspace, changed)
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("missing_required_owner_target", {item["kind"] for item in audit["blocking_findings"]})


if __name__ == "__main__":
    unittest.main()
