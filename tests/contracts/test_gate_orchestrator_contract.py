from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GATES_PATH = ROOT / "scripts" / "validation" / "gates" / "gates.json"
CLASSIFICATION_PATH = ROOT / "scripts" / "validation" / "gates" / "change-classification.json"
INVOKE_GATE = ROOT / "scripts" / "validation" / "invoke-gate.ps1"
STRICT_NATIVE_WORKFLOW = ROOT / ".github" / "workflows" / "strict-native-gate.yml"
STRICT_HIL_WORKFLOW = ROOT / ".github" / "workflows" / "strict-hil-gate.yml"
ROOT_README = ROOT / "README.md"
VALIDATION_README = ROOT / "docs" / "validation" / "README.md"
AGENTS = ROOT / "AGENTS.md"
GATE_ORCHESTRATOR_DOC = ROOT / "docs" / "validation" / "gate-orchestrator.md"


def _json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


class GateOrchestratorContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.gates = _json(GATES_PATH)["gates"]
        self.classification = _json(CLASSIFICATION_PATH)

    def _resolved_gate(self, gate_id: str) -> dict:
        gate = self.gates[gate_id]
        alias = gate.get("aliasOf")
        if alias:
            resolved = dict(self.gates[alias])
            resolved["defaultReportRoot"] = gate["defaultReportRoot"]
            return resolved
        return gate

    def test_canonical_gate_ids_are_loadable(self) -> None:
        self.assertEqual(
            set(self.gates),
            {"pre-push", "pr", "full-offline", "native", "hil", "nightly", "release"},
        )
        for gate_id in self.gates:
            gate = self._resolved_gate(gate_id)
            self.assertIn("defaultReportRoot", gate)
            self.assertTrue(gate.get("steps"), msg=f"{gate_id} must resolve to executable steps")

    def test_step_contract_requires_explicit_blocking_logs_and_artifact_rules(self) -> None:
        orchestrator = _read(INVOKE_GATE)
        self.assertIn("gate-manifest.json", orchestrator)
        self.assertIn("gate-summary.json", orchestrator)
        self.assertIn("gate-summary.md", orchestrator)
        self.assertIn('"logs"', orchestrator)
        self.assertIn('"$stepId.log"', orchestrator)
        self.assertIn("if (-not $gateFailed)", orchestrator)

        for gate_id in self.gates:
            gate = self._resolved_gate(gate_id)
            for step in gate["steps"]:
                with self.subTest(gate=gate_id, step=step.get("id")):
                    self.assertTrue(step.get("id"))
                    self.assertIsInstance(step.get("blocking"), bool)
                    self.assertIsInstance(step.get("requiredArtifacts"), list)
                    self.assertGreater(step.get("timeoutSeconds", 0), 0)
                    self.assertTrue(step.get("command"), msg="each step must keep existing checks as explicit commands")

    def test_no_implicit_report_only_semantics_remain_in_gate_config(self) -> None:
        config_text = GATES_PATH.read_text(encoding="utf-8")
        self.assertNotIn("report-only", config_text.lower())

        non_blocking_steps = []
        for gate_id in self.gates:
            gate = self._resolved_gate(gate_id)
            for step in gate["steps"]:
                if step["blocking"] is False:
                    non_blocking_steps.append((gate_id, step["id"]))

        self.assertIn(("nightly", "python-coverage"), non_blocking_steps)
        self.assertIn(("nightly", "cpp-coverage"), non_blocking_steps)

    def test_pre_push_gate_stays_fast_and_offline_only(self) -> None:
        step_ids = {step["id"] for step in self._resolved_gate("pre-push")["steps"]}

        self.assertIn("contracts-quick", step_ids)
        self.assertNotIn("build-ci", step_ids)
        self.assertNotIn("test-ci", step_ids)
        self.assertNotIn("controlled-hil", step_ids)
        self.assertNotIn("performance", step_ids)
        self.assertNotIn("native-full-offline", step_ids)

    def test_native_gate_keeps_cppcheck_and_dependency_graphs_blocking(self) -> None:
        steps = {step["id"]: step for step in self._resolved_gate("native")["steps"]}

        self.assertTrue(steps["cppcheck"]["blocking"])
        self.assertIn("-FailOnIssues", steps["cppcheck"]["command"])
        self.assertTrue(steps["dependency-graphs"]["blocking"])
        self.assertNotIn("-SoftFail", steps["dependency-graphs"]["command"])

    def test_full_offline_front_loads_tool_readiness_and_avoids_duplicate_local_validation_artifacts(self) -> None:
        full_offline = self._resolved_gate("full-offline")
        step_ids = [step["id"] for step in full_offline["steps"]]
        tool_readiness = next(step for step in full_offline["steps"] if step["id"] == "tool-readiness")

        self.assertLess(step_ids.index("tool-readiness"), step_ids.index("build-ci"))
        self.assertLess(step_ids.index("tool-readiness"), step_ids.index("test-ci"))
        self.assertTrue(tool_readiness["blocking"])

        required_tools = {tool["id"]: tool for tool in tool_readiness["requiresTool"]}
        self.assertTrue(required_tools["cppcheck"].get("required", True))
        self.assertIn("cppcheck", required_tools)
        self.assertIn("pydeps", required_tools)

        gate_required_artifacts = full_offline.get("requiredArtifacts", [])
        self.assertNotIn(
            "{gateReportDir}/local-validation-gate/*/local-validation-gate-summary.json",
            gate_required_artifacts,
        )

    def test_classification_rules_are_configured_once_outside_workflows(self) -> None:
        self.assertIn("native-sensitive", self.classification["native"]["labels"])
        self.assertIn("hardware-sensitive", self.classification["hil"]["labels"])
        self.assertTrue(self.classification["hil"]["defaultRequiredOnClassificationFailure"])
        self.assertTrue(self.classification["native"]["patterns"])
        self.assertTrue(self.classification["hil"]["patterns"])

        native_workflow = _read(STRICT_NATIVE_WORKFLOW)
        hil_workflow = _read(STRICT_HIL_WORKFLOW)
        self.assertIn("classify-change.ps1", native_workflow)
        self.assertIn("classify-change.ps1", hil_workflow)
        self.assertNotIn("$nativePatterns", native_workflow)
        self.assertNotIn("$hardwarePatterns", hil_workflow)
        self.assertNotIn("path $normalized matches", native_workflow)
        self.assertNotIn("path $normalized matches", hil_workflow)

    def test_hil_gate_reuses_offline_prerequisite_report(self) -> None:
        hil_steps = {step["id"]: step for step in self._resolved_gate("hil")["steps"]}
        controlled_hil_command = hil_steps["controlled-hil"]["command"]

        self.assertIn("-OfflinePrereqReport", controlled_hil_command)
        self.assertIn(
            "{gateReportDir}/offline-prerequisite/workspace-validation.json",
            controlled_hil_command,
        )

    def test_gate_orchestrator_is_published_as_authoritative_developer_doc(self) -> None:
        doc = _read(GATE_ORCHESTRATOR_DOC)
        self.assertIn("权威文档", doc)
        self.assertIn("开发者必须遵守的规则", doc)
        self.assertIn("scripts/validation/gates/gates.json", doc)

        for path in (ROOT_README, VALIDATION_README, AGENTS):
            with self.subTest(path=path):
                text = _read(path)
                self.assertIn("docs/validation/gate-orchestrator.md", text)
                self.assertIn("scripts/validation/gates/gates.json", text)


if __name__ == "__main__":
    unittest.main()
