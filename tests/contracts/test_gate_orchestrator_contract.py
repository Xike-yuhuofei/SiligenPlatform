from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GATES_PATH = ROOT / "scripts" / "validation" / "gates" / "gates.json"
CLASSIFICATION_PATH = ROOT / "scripts" / "validation" / "gates" / "change-classification.json"
INVOKE_GATE = ROOT / "scripts" / "validation" / "invoke-gate.ps1"
INVOKE_PRE_PUSH_GATE = ROOT / "scripts" / "validation" / "invoke-pre-push-gate.ps1"
CLASSIFY_CHANGE = ROOT / "scripts" / "validation" / "classify-change.ps1"
STRICT_NATIVE_WORKFLOW = ROOT / ".github" / "workflows" / "strict-native-gate.yml"
STRICT_HIL_WORKFLOW = ROOT / ".github" / "workflows" / "strict-hil-gate.yml"
STRICT_PR_WORKFLOW = ROOT / ".github" / "workflows" / "strict-pr-gate.yml"
INVOKE_SEMGREP = ROOT / "scripts" / "validation" / "invoke-semgrep.ps1"
INVOKE_IMPORT_LINTER = ROOT / "scripts" / "validation" / "invoke-import-linter.ps1"
INVOKE_CPPCHECK = ROOT / "scripts" / "validation" / "invoke-cppcheck.ps1"
INVOKE_DEPENDENCY_GRAPH = ROOT / "scripts" / "validation" / "invoke-dependency-graph-export.ps1"
INVOKE_MODULE_BOUNDARY_AUDIT = ROOT / "scripts" / "validation" / "invoke-module-boundary-audit.ps1"
RUN_LOCAL_VALIDATION_GATE = ROOT / "scripts" / "validation" / "run-local-validation-gate.ps1"
BUILD_VALIDATION = ROOT / "scripts" / "build" / "build-validation.ps1"
PREPARE_STRICT_NATIVE_BUILD_CACHE = ROOT / "scripts" / "validation" / "prepare-strict-native-build-cache.ps1"
ROOT_README = ROOT / "README.md"
VALIDATION_README = ROOT / "docs" / "validation" / "README.md"
AGENTS = ROOT / "AGENTS.md"
GATE_ORCHESTRATOR_DOC = ROOT / "docs" / "validation" / "gate-orchestrator.md"


def _json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _git_lines(*args: str) -> list[str]:
    output = subprocess.check_output(["git", *args], cwd=str(ROOT), text=True)
    return [line.strip() for line in output.splitlines() if line.strip()]


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
        self.assertIn('($changedFiles -join ",")', orchestrator)
        self.assertIn('($changedScopes -join ",")', orchestrator)
        self.assertIn("Get-ChangedScopeFromGitRange", orchestrator)
        self.assertIn("Ensure-GitCommitAvailable", orchestrator)
        self.assertIn('safe.directory=$workspaceRoot', orchestrator)
        self.assertIn("git @gitBaseArgs cat-file -e", orchestrator)
        self.assertIn("git @gitBaseArgs fetch --no-tags --no-recurse-submodules origin $Revision", orchestrator)
        self.assertIn("$gitExitCode = $LASTEXITCODE", orchestrator)
        self.assertIn("Install CMake and ensure cmake is on PATH.", orchestrator)
        self.assertIn("Install Ninja and ensure ninja is on PATH.", orchestrator)
        self.assertIn("Install LLVM and ensure clang-cl is on PATH.", orchestrator)
        self.assertIn("Install LLVM and ensure lld-link is on PATH.", orchestrator)
        self.assertIn("Unable to derive changed scope from BaseSha/HeadSha", orchestrator)
        self.assertIn("$global:LASTEXITCODE = 0", orchestrator)

        classifier = _read(CLASSIFY_CHANGE)
        self.assertIn("$ghExitCode = $LASTEXITCODE", classifier)
        self.assertIn("gh pr diff exited with $ghExitCode", classifier)
        self.assertIn('$result.source = "git-diff-base-head"', classifier)
        self.assertIn("$global:LASTEXITCODE = 0", classifier)

        for gate_id in self.gates:
            gate = self._resolved_gate(gate_id)
            for step in gate["steps"]:
                with self.subTest(gate=gate_id, step=step.get("id")):
                    self.assertTrue(step.get("id"))
                    self.assertIsInstance(step.get("blocking"), bool)
                    self.assertIsInstance(step.get("requiredArtifacts"), list)
                    self.assertIsInstance(step.get("requiresTool"), list)
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
        self.assertNotIn("release-check", step_ids)

    def test_pr_gate_contains_host_quick_checks_and_excludes_heavy_gates(self) -> None:
        step_ids = {step["id"] for step in self._resolved_gate("pr")["steps"]}

        expected_quick_checks = {
            "hmi-formal-gateway-contract",
            "legacy-exit",
            "semgrep",
            "import-linter",
            "module-boundary-audit",
            "pyright-static",
            "contracts-quick",
            "hmi-runtime-gateway-protocol-compat",
            "recipe-config-schema-quick",
            "state-machine-interlock-contract",
            "offline-simulation-smoke",
            "validation-system-contract",
            "classification-evidence",
        }
        self.assertTrue(expected_quick_checks.issubset(step_ids))
        self.assertNotIn("module-boundary-bridges", step_ids)

        forbidden_heavy_steps = {
            "build-ci",
            "test-ci",
            "cppcheck",
            "dependency-graphs",
            "local-validation-gate",
            "offline-prerequisite",
            "controlled-hil",
            "native-full-offline",
            "performance",
            "long-running-simulation",
            "release-check",
        }
        self.assertTrue(forbidden_heavy_steps.isdisjoint(step_ids))

        workflow = _read(STRICT_PR_WORKFLOW)
        self.assertIn("invoke-gate.ps1", workflow)
        self.assertIn("-Gate pr", workflow)
        self.assertIn("-BaseSha", workflow)
        self.assertIn("-HeadSha", workflow)
        self.assertIn("fetch-depth: 0", workflow)

        quick_check = _read(ROOT / "scripts" / "validation" / "invoke-pre-push-quick-check.ps1")
        self.assertIn("$markerScanFiles", quick_check)
        self.assertIn("offline execution surfaces", quick_check)
        self.assertIn("scripts/(?!validation/)", quick_check)

        pr_steps = {step["id"]: step for step in self._resolved_gate("pr")["steps"]}
        boundary_step = pr_steps["module-boundary-audit"]
        boundary_command = " ".join(boundary_step["command"])
        self.assertIn("invoke-module-boundary-audit.ps1", boundary_command)
        self.assertIn("-Mode pr", boundary_command)
        self.assertIn("{changedFileArgs}", boundary_step["command"])
        self.assertIn("{stepReportDir}/module-boundary-audit.json", boundary_step["requiredArtifacts"])
        self.assertIn("{stepReportDir}/module-boundary-coverage.json", boundary_step["requiredArtifacts"])
        self.assertIn("{stepReportDir}/bridge-registry-status.json", boundary_step["requiredArtifacts"])
        self.assertIn("{stepReportDir}/logs/dependency-extractor.log", boundary_step["requiredArtifacts"])
        self.assertNotIn("assert-module-boundary-bridges.ps1", boundary_command)

        self.assertNotIn("assert-module-boundary-bridges.ps1", _read(GATES_PATH))
        self.assertNotIn("module-boundary-bridges", _read(GATES_PATH))
        self.assertNotIn("assert-module-boundary-bridges.ps1", _read(RUN_LOCAL_VALIDATION_GATE))
        self.assertNotIn("module-boundary-bridges", _read(RUN_LOCAL_VALIDATION_GATE))
        self.assertNotIn("assert-module-boundary-bridges.ps1", _read(INVOKE_MODULE_BOUNDARY_AUDIT))

    def test_pre_push_default_wrapper_uses_quick_semantics(self) -> None:
        wrapper = _read(INVOKE_PRE_PUSH_GATE)
        orchestrator = _read(INVOKE_GATE)

        self.assertIn('[string]$Gate = "pre-push"', wrapper)
        self.assertIn('[string]$Lane = "quick-gate"', wrapper)
        self.assertIn('[string]$DesiredDepth = "quick"', wrapper)
        self.assertNotIn('[string]$Lane = "full-offline-gate"', wrapper)
        self.assertNotIn('[string]$RiskProfile = "high"', wrapper)
        self.assertNotIn('[string]$DesiredDepth = "full-offline"', wrapper)
        self.assertIn("selected_pre_push_steps", orchestrator)

    def test_pre_push_tool_readiness_matches_possible_selected_steps(self) -> None:
        pre_push = self._resolved_gate("pre-push")
        steps = {step["id"]: step for step in pre_push["steps"]}
        tool_readiness = steps["tool-readiness"]
        readiness_tool_ids = {tool["id"] for tool in tool_readiness.get("requiresTool", [])}

        self.assertEqual(readiness_tool_ids, {"git", "python", "powershell"})
        self.assertNotIn("pydeps", readiness_tool_ids)
        self.assertNotIn("cppcheck", readiness_tool_ids)
        self.assertNotIn("semgrep", readiness_tool_ids)
        self.assertNotIn("import-linter", readiness_tool_ids)
        self.assertIn("invoke-pre-push-tool-readiness.ps1", " ".join(tool_readiness["command"]))
        self.assertIn("{classificationPathArgs}", tool_readiness["command"])
        self.assertIn("{changedFileArgs}", tool_readiness["command"])
        self.assertIn("{stepReportDir}/pre-push-tool-readiness.json", tool_readiness["requiredArtifacts"])
        delete_safety = steps["remote-branch-delete-safety"]
        self.assertIn("invoke-pre-push-remote-branch-delete-safety.ps1", " ".join(delete_safety["command"]))
        self.assertIn(
            "{stepReportDir}/remote-branch-delete-safety.json",
            delete_safety["requiredArtifacts"],
        )
        self.assertIn(
            "{stepReportDir}/remote-branch-delete-safety.md",
            delete_safety["requiredArtifacts"],
        )

        self.assertEqual(
            {tool["id"] for tool in steps["pyright-static"].get("requiresTool", [])},
            {"pyright"},
        )
        contracts_quick_command = " ".join(steps["contracts-quick"]["command"])
        self.assertIn("invoke-pre-push-quick-check.ps1", contracts_quick_command)
        self.assertIn("contracts-quick", contracts_quick_command)
        self.assertEqual(
            {tool["id"] for tool in steps["contracts-quick"].get("requiresTool", [])},
            {"powershell", "python", "pytest"},
        )
        self.assertNotIn("python -m pytest", contracts_quick_command)
        self.assertNotIn("test.ps1", contracts_quick_command)
        self.assertNotIn("build.ps1", contracts_quick_command)
        self.assertNotIn("MSBuild", contracts_quick_command)
        pyright_runner = _read(ROOT / "scripts" / "validation" / "run-pyright-gate.ps1")
        self.assertIn("$command = @(Resolve-PyrightCommand)", pyright_runner)
        self.assertNotIn("--yes", pyright_runner)
        self.assertIn("pre-push does not use npx fallback", pyright_runner)
        install_deps = _read(ROOT / "scripts" / "validation" / "install-python-deps.ps1")
        self.assertIn("PyYAML pyright", install_deps)
        self.assertNotIn("npm install -g pyright", install_deps)
        self.assertNotIn("npx", install_deps)

        readiness_script = _read(ROOT / "scripts" / "validation" / "invoke-pre-push-tool-readiness.ps1")
        self.assertIn("selected_pre_push_steps", readiness_script)
        self.assertIn('-ModuleName "pytest"', readiness_script)
        self.assertIn('-ModuleName "jsonschema"', readiness_script)
        self.assertIn("python-module:yaml-parser", readiness_script)

    def test_pre_push_categories_route_to_required_quick_steps(self) -> None:
        pre_push = self.classification["pre_push"]
        categories = pre_push["categories"]
        expected_categories = {
            "docs-only",
            "low-risk",
            "hmi-ui",
            "hmi-command-chain",
            "runtime-gateway-protocol",
            "recipe-config-schema",
            "motion-or-device-control",
            "validation-or-build-system",
            "native-sensitive",
            "hil-sensitive",
        }
        self.assertEqual(set(categories), expected_categories)
        self.assertIn("hmi-formal-gateway-contract", categories["hmi-command-chain"]["steps"])
        self.assertIn("hmi-runtime-gateway-protocol-compat", categories["runtime-gateway-protocol"]["steps"])
        self.assertIn("recipe-config-schema-quick", categories["recipe-config-schema"]["steps"])
        self.assertIn("state-machine-interlock-contract", categories["motion-or-device-control"]["steps"])
        self.assertIn("offline-simulation-smoke", categories["motion-or-device-control"]["steps"])
        self.assertEqual(categories["native-sensitive"]["steps"], ["native-followup-notice"])
        self.assertEqual(categories["hil-sensitive"]["steps"], ["hil-followup-notice"])
        self.assertIn("^modules/runtime-execution/", categories["hil-sensitive"]["patterns"])
        self.assertNotIn(".*(ui|view|screen|panel|widget|operator).*\\.(py|ps1|json|yaml|yml|md)$", categories["hmi-ui"]["patterns"])

    def test_recipe_schema_and_offline_smoke_use_formal_quick_checks(self) -> None:
        quick_check = _read(ROOT / "scripts" / "validation" / "invoke-pre-push-quick-check.ps1")

        self.assertIn("jsonschema.validators.validator_for(payload).check_schema(payload)", quick_check)
        self.assertIn("configparser.ConfigParser(strict=True)", quick_check)
        self.assertIn("schema compatibility registry missing", quick_check)
        self.assertIn("test_real_dxf_machine_dryrun_observation_contract.py", quick_check)
        self.assertIn("Offline dry-run observation contract passed", quick_check)

    def test_classify_change_outputs_pre_push_contract_shape_for_representative_paths(self) -> None:
        cases = {
            "docs-only": ["docs/validation/gate-orchestrator.md"],
            "hmi-command-chain": ["apps/hmi-app/client/command_gateway.py"],
            "runtime-gateway-protocol": ["apps/runtime-gateway/transport/protocol.py"],
            "recipe-config-schema": ["config/machine/runtime-machine.json"],
            "motion-or-device-control": ["modules/runtime-execution/application/interlock_state.cpp"],
            "validation-or-build-system": ["scripts/validation/invoke-gate.ps1"],
        }

        with tempfile.TemporaryDirectory() as temp_dir:
            for category, files in cases.items():
                output_path = Path(temp_dir) / f"{category}.json"
                completed = subprocess.run(
                    [
                        "powershell",
                        "-NoProfile",
                        "-ExecutionPolicy",
                        "Bypass",
                        "-File",
                        str(CLASSIFY_CHANGE),
                        "-Mode",
                        "pre-push",
                        "-OutputPath",
                        str(output_path),
                        "-ChangedFile",
                        ",".join(files),
                    ],
                    cwd=str(ROOT),
                    capture_output=True,
                    text=True,
                )
                self.assertEqual(completed.returncode, 0, msg=completed.stdout + completed.stderr)
                payload = json.loads(output_path.read_text(encoding="utf-8-sig"))
                self.assertIn(category, payload["categories"])
                if category == "motion-or-device-control":
                    self.assertIn("hil-sensitive", payload["categories"])
                    self.assertIn("hil-followup-notice", payload["selected_pre_push_steps"])
                if category == "recipe-config-schema":
                    self.assertIn("native-sensitive", payload["categories"])
                    self.assertIn("hil-sensitive", payload["categories"])
                    self.assertIn("native-followup-notice", payload["selected_pre_push_steps"])
                    self.assertIn("hil-followup-notice", payload["selected_pre_push_steps"])
                self.assertIn("changed_files", payload)
                self.assertIn("categories", payload)
                self.assertIn("reasons", payload)
                self.assertIn("requires_native_followup", payload)
                self.assertIn("requires_hil_followup", payload)
                self.assertIn("selected_pre_push_steps", payload)

    def test_native_and_full_offline_keep_cppcheck_and_dependency_graphs_blocking(self) -> None:
        for gate_id in ("native", "full-offline"):
            with self.subTest(gate_id=gate_id):
                steps = {step["id"]: step for step in self._resolved_gate(gate_id)["steps"]}

                for required in (
                    "install-python-deps",
                    "hmi-formal-gateway-contract",
                    "legacy-exit",
                    "semgrep",
                    "import-linter",
                    "build-ci",
                    "test-ci",
                    "local-validation-gate",
                ):
                    self.assertIn(required, steps)

                build_tools = {tool["id"] for tool in steps["build-ci"].get("requiresTool", [])}
                self.assertTrue({"powershell", "cmake", "ninja", "clang-cl", "lld-link"}.issubset(build_tools))
                self.assertIn(
                    "build/ca/compile_commands.json",
                    steps["build-ci"].get("requiredArtifacts", []),
                )
                self.assertTrue(steps["cppcheck"]["blocking"])
                self.assertIn("-FailOnIssues", steps["cppcheck"]["command"])
                self.assertIn("-Scope", steps["cppcheck"]["command"])
                self.assertIn("Project", steps["cppcheck"]["command"])
                self.assertNotIn("ChangedFiles", steps["cppcheck"]["command"])
                self.assertNotIn("{changedFileArgs}", steps["cppcheck"]["command"])
                self.assertGreaterEqual(steps["cppcheck"]["timeoutSeconds"], 1800)
                self.assertTrue(steps["dependency-graphs"]["blocking"])
                self.assertNotIn("-SoftFail", steps["dependency-graphs"]["command"])

        cppcheck_runner = _read(INVOKE_CPPCHECK)
        self.assertIn('"--quiet"', cppcheck_runner)
        self.assertIn('[ValidateSet("ChangedFiles", "Project")]', cppcheck_runner)
        self.assertIn("[string[]]$ChangedFile", cppcheck_runner)
        self.assertIn("skipped-no-cpp-files", cppcheck_runner)
        self.assertIn("compile-db-missing", cppcheck_runner)
        self.assertIn('Project scope requires compile_commands.json', cppcheck_runner)
        self.assertIn("--file-filter=*/apps/*", cppcheck_runner)
        self.assertIn("--file-filter=*/modules/*", cppcheck_runner)
        self.assertIn("--file-filter=*/shared/*", cppcheck_runner)
        self.assertIn("--suppress=*:*/third_party/*", cppcheck_runner)
        self.assertIn("[System.Diagnostics.ProcessStartInfo]::new()", cppcheck_runner)
        self.assertIn("RedirectStandardError = $true", cppcheck_runner)
        self.assertNotIn('Join-Path $workspaceRoot "apps"', cppcheck_runner)
        self.assertNotIn('Join-Path $workspaceRoot "modules"', cppcheck_runner)
        self.assertNotIn('Join-Path $workspaceRoot "shared"', cppcheck_runner)
        orchestrator = _read(INVOKE_GATE)
        self.assertIn('$localPattern = $Pattern.Replace', orchestrator)
        self.assertIn('$firstWildcard = $localPattern.IndexOfAny', orchestrator)
        self.assertIn('$root = Resolve-WorkspacePath -PathValue $staticRoot', orchestrator)
        build_runner = _read(BUILD_VALIDATION)
        self.assertIn('"-G", "Ninja"', build_runner)
        self.assertIn('"-DCMAKE_BUILD_TYPE=Debug"', build_runner)
        self.assertIn('"-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"', build_runner)
        self.assertIn("compile_commands.json", build_runner)
        self.assertNotIn("--config Debug", build_runner)
        prepare_runner = _read(PREPARE_STRICT_NATIVE_BUILD_CACHE)
        self.assertIn("CMAKE_GENERATOR:INTERNAL", prepare_runner)
        self.assertIn("expected_generator=Ninja", prepare_runner)
        self.assertIn("stale generator detected", prepare_runner)

    def test_hil_gate_contains_offline_prerequisite_and_controlled_hil(self) -> None:
        steps = {step["id"]: step for step in self._resolved_gate("hil")["steps"]}

        self.assertIn("offline-prerequisite", steps)
        self.assertTrue(steps["offline-prerequisite"]["blocking"])
        self.assertIn("controlled-hil", steps)
        self.assertTrue(steps["controlled-hil"]["blocking"])
        self.assertIn("case-matrix-summary.json", " ".join(steps["controlled-hil"]["requiredArtifacts"]))
        self.assertIn("hil-controlled-release-summary.md", " ".join(steps["controlled-hil"]["requiredArtifacts"]))

    def test_nightly_gate_declares_observability_and_baseline_evidence(self) -> None:
        steps = {step["id"]: step for step in self._resolved_gate("nightly")["steps"]}

        self.assertIn("native-full-offline", steps)
        self.assertIn("performance", steps)
        self.assertIn("long-running-simulation", steps)
        self.assertIn("baseline-drift-check", steps)
        self.assertIn("performance-baseline-evidence", steps)
        self.assertFalse(steps["python-coverage"]["blocking"])
        self.assertFalse(steps["cpp-coverage"]["blocking"])

    def test_classification_rules_are_configured_once_outside_workflows(self) -> None:
        self.assertIn("native-sensitive", self.classification["native"]["labels"])
        self.assertIn("hardware-sensitive", self.classification["hil"]["labels"])
        self.assertIn("pre_push", self.classification)
        self.assertTrue(self.classification["native"]["patterns"])
        self.assertTrue(self.classification["hil"]["patterns"])

        native_workflow = _read(STRICT_NATIVE_WORKFLOW)
        hil_workflow = _read(STRICT_HIL_WORKFLOW)
        pr_workflow = _read(STRICT_PR_WORKFLOW)
        self.assertIn("classify-change.ps1", native_workflow)
        self.assertIn("classify-change.ps1", hil_workflow)
        self.assertNotIn("classify-change.ps1", pr_workflow)
        self.assertIn("-Mode native", native_workflow)
        self.assertIn("-Mode hil", hil_workflow)
        self.assertNotIn("$classificationArgs", native_workflow)
        self.assertNotIn("$classificationArgs", hil_workflow)
        self.assertNotIn("$nativePatterns", native_workflow)
        self.assertNotIn("$hardwarePatterns", hil_workflow)
        self.assertNotIn("pre_push", native_workflow)
        self.assertNotIn("pre_push", hil_workflow)
        self.assertNotIn("path $normalized matches", native_workflow)
        self.assertNotIn("path $normalized matches", hil_workflow)

    def test_windows_powershell_validation_scripts_are_ascii_safe(self) -> None:
        semgrep_runner = _read(INVOKE_SEMGREP)
        self.assertTrue(semgrep_runner.isascii())
        import_linter_runner = _read(INVOKE_IMPORT_LINTER)
        self.assertTrue(import_linter_runner.isascii())
        cppcheck_runner = _read(INVOKE_CPPCHECK)
        self.assertTrue(cppcheck_runner.isascii())
        dependency_graph_runner = _read(INVOKE_DEPENDENCY_GRAPH)
        self.assertTrue(dependency_graph_runner.isascii())

    def test_native_and_hil_sensitive_pre_push_are_followup_only(self) -> None:
        pre_push_steps = {step["id"] for step in self._resolved_gate("pre-push")["steps"]}
        self.assertIn("native-followup-notice", pre_push_steps)
        self.assertIn("hil-followup-notice", pre_push_steps)
        self.assertNotIn("build-ci", pre_push_steps)
        self.assertNotIn("offline-prerequisite", pre_push_steps)
        self.assertNotIn("controlled-hil", pre_push_steps)

        native_category = self.classification["pre_push"]["categories"]["native-sensitive"]
        hil_category = self.classification["pre_push"]["categories"]["hil-sensitive"]
        self.assertEqual(native_category["steps"], ["native-followup-notice"])
        self.assertEqual(hil_category["steps"], ["hil-followup-notice"])

    def test_dirty_worktree_strategy_is_documented_and_manifested(self) -> None:
        wrapper = _read(INVOKE_PRE_PUSH_GATE)
        doc = _read(GATE_ORCHESTRATOR_DOC)
        orchestrator = _read(INVOKE_GATE)

        self.assertIn("dirty_worktree", wrapper)
        self.assertIn("dirty_files", wrapper)
        self.assertIn("dirty_policy", wrapper)
        self.assertIn("dirty_worktree", orchestrator)
        self.assertIn("dirty_policy", orchestrator)
        self.assertIn("dirty worktree", doc.lower())
        self.assertIn("待 push commit", doc)

    def test_simulated_hook_stdin_records_pre_push_hook_range_source(self) -> None:
        skip_steps = ",".join(
            [
                "git-hygiene",
                "tool-readiness",
                "pre-push-classification",
                "changed-file-parse",
                "pyright-static",
                "hmi-formal-gateway-contract",
                "hmi-runtime-gateway-protocol-compat",
                "validation-system-contract",
                "contracts-quick",
                "native-followup-notice",
                "hil-followup-notice",
            ]
        )

        with tempfile.TemporaryDirectory(prefix="pre-push-hook-contract-") as temp_dir:
            temp_root = Path(temp_dir)
            report_root = ROOT / "tests" / "reports" / "tmp" / "pre-push-hook-contract"
            index_path = temp_root / "index"
            smoke_file = temp_root / "pre-push-contract-smoke.md"
            index_env = dict(os.environ)
            index_env["GIT_INDEX_FILE"] = str(index_path)
            try:
                base_sha = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=str(ROOT), text=True).strip()
                smoke_file.write_text("# pre-push contract smoke\n", encoding="utf-8")
                subprocess.run(["git", "read-tree", base_sha], cwd=str(ROOT), env=index_env, check=True)
                blob_sha = subprocess.check_output(
                    ["git", "hash-object", "-w", str(smoke_file)], cwd=str(ROOT), text=True
                ).strip()
                subprocess.run(
                    [
                        "git",
                        "update-index",
                        "--add",
                        "--cacheinfo",
                        "100644",
                        blob_sha,
                        "docs/validation/pre-push-contract-smoke.md",
                    ],
                    cwd=str(ROOT),
                    env=index_env,
                    check=True,
                )
                tree_sha = subprocess.check_output(["git", "write-tree"], cwd=str(ROOT), env=index_env, text=True).strip()
                head_sha = subprocess.check_output(
                    ["git", "commit-tree", tree_sha, "-p", base_sha, "-m", "test: synthesize pre-push hook range"],
                    cwd=str(ROOT),
                    text=True,
                ).strip()
                hook_line = f"refs/heads/contract-smoke {head_sha} refs/heads/contract-smoke {base_sha}\n"

                completed = subprocess.run(
                    [
                        "powershell",
                        "-NoProfile",
                        "-ExecutionPolicy",
                        "Bypass",
                        "-File",
                        str(INVOKE_PRE_PUSH_GATE),
                        "-RemoteName",
                        "origin",
                        "-ReportRoot",
                        "tests\\reports\\tmp\\pre-push-hook-contract",
                        "-SkipStep",
                        skip_steps,
                        "-SkipJustification",
                        "contract smoke verifies hook range metadata only",
                    ],
                    cwd=str(ROOT),
                    input=hook_line,
                    capture_output=True,
                    text=True,
                )
                self.assertEqual(completed.returncode, 0, msg=completed.stdout + completed.stderr)
                manifests = sorted(report_root.glob("*/pre-push-gate-manifest.json"))
                self.assertTrue(manifests, msg=completed.stdout + completed.stderr)
                manifest = json.loads(manifests[-1].read_text(encoding="utf-8-sig"))

                self.assertEqual(manifest["range_source"], "pre-push-hook")
                self.assertEqual(manifest["base_sha"], base_sha)
                self.assertEqual(manifest["head_sha"], head_sha)
                self.assertEqual(manifest["changed_files"], ["docs/validation/pre-push-contract-smoke.md"])
            finally:
                shutil.rmtree(report_root, ignore_errors=True)

    def test_new_remote_branch_hook_uses_main_merge_base(self) -> None:
        wrapper = _read(INVOKE_PRE_PUSH_GATE)

        self.assertIn("git merge-base $defaultBaseRef $localSha", wrapper)
        self.assertIn('"origin/main"', wrapper)
        self.assertIn("rev-parse --verify --quiet $tracking", wrapper)

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
