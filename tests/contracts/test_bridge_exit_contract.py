from __future__ import annotations

import json
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]


def _read(relative: str) -> str:
    return (WORKSPACE_ROOT / relative).read_text(encoding="utf-8-sig", errors="ignore")


def _source_files(*roots: str) -> list[Path]:
    files: list[Path] = []
    for root in roots:
        base = WORKSPACE_ROOT / root
        for pattern in ("*.h", "*.cpp"):
            files.extend(base.rglob(pattern))
    return files


class BridgeExitContractTest(unittest.TestCase):
    def test_configuration_residual_compat_headers_are_physically_absent(self) -> None:
        removed_headers = [
            "modules/process-planning/contracts/legacy-bridge/include/domain/configuration/ports/IConfigurationPort.h",
            "modules/process-planning/domain/configuration/ports/IConfigurationPort.h",
            "modules/process-planning/domain/configuration/ports/IFileStoragePort.h",
            "modules/process-planning/domain/configuration/ports/ValveConfig.h",
            "modules/process-planning/domain/configuration/value-objects/ConfigTypes.h",
            "modules/workflow/domain/domain/configuration/ports/IConfigurationPort.h",
            "modules/workflow/domain/domain/configuration/ports/IFileStoragePort.h",
            "modules/workflow/domain/domain/configuration/ports/ValveConfig.h",
            "modules/workflow/domain/domain/configuration/value-objects/ConfigTypes.h",
            "modules/workflow/domain/include/domain/configuration/ports/IConfigurationPort.h",
            "modules/workflow/domain/include/domain/configuration/ports/IFileStoragePort.h",
            "modules/workflow/domain/include/domain/configuration/ports/ValveConfig.h",
            "modules/workflow/domain/include/domain/configuration/services/ReadyZeroSpeedResolver.h",
        ]
        present = [path for path in removed_headers if (WORKSPACE_ROOT / path).exists()]
        self.assertFalse(present, msg=f"residual compatibility headers still present: {present}")

    def test_configuration_contracts_resolve_from_canonical_headers(self) -> None:
        contracts_cmake = _read("modules/process-planning/contracts/CMakeLists.txt")
        self.assertIn("siligen_process_planning_contracts_public", contracts_cmake)
        self.assertIn(
            "process_planning/contracts/configuration/IConfigurationPort.h",
            contracts_cmake,
        )

        runtime_contract_header = _read(
            "modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/configuration/IConfigurationPort.h"
        )
        self.assertIn("process_planning/contracts/configuration/IConfigurationPort.h", runtime_contract_header)
        self.assertNotIn("domain/configuration/ports/IConfigurationPort.h", runtime_contract_header)

        for relative in (
            "apps/runtime-service/bootstrap/ContainerBootstrap.cpp",
            "apps/runtime-service/runtime/configuration/ConfigFileAdapter.h",
            "apps/runtime-service/runtime/configuration/validators/ConfigValidator.h",
            "modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h",
            "modules/workflow/application/include/application/usecases/dispensing/valve/ValveCommandUseCase.h",
        ):
            text = _read(relative)
            self.assertIn(
                "process_planning/contracts/configuration/IConfigurationPort.h",
                text,
                msg=f"{relative} must include canonical configuration contracts",
            )
            self.assertNotIn(
                "domain/configuration/ports/IConfigurationPort.h",
                text,
                msg=f"{relative} must not include residual configuration compatibility headers",
            )

    def test_file_storage_contracts_resolve_from_job_ingest_canonical_headers(self) -> None:
        contracts_cmake = _read("modules/job-ingest/contracts/CMakeLists.txt")
        self.assertIn("siligen_job_ingest_contracts", contracts_cmake)
        self.assertIn(
            "job_ingest/contracts/storage/IFileStoragePort.h",
            contracts_cmake,
        )

        include_required = (
            "apps/runtime-service/bootstrap/ContainerBootstrap.cpp",
            "apps/runtime-service/container/ApplicationContainer.h",
            "apps/runtime-service/runtime/storage/files/LocalFileStorageAdapter.h",
            "modules/workflow/application/include/application/usecases/dispensing/CleanupFilesUseCase.h",
        )
        for relative in include_required:
            text = _read(relative)
            self.assertIn(
                "job_ingest/contracts/storage/IFileStoragePort.h",
                text,
                msg=f"{relative} must include canonical file-storage contracts",
            )
            self.assertNotIn(
                "domain/configuration/ports/IFileStoragePort.h",
                text,
                msg=f"{relative} must not include residual file-storage compatibility headers",
            )

        upload_use_case_header = _read(
            "modules/job-ingest/application/include/application/usecases/dispensing/UploadFileUseCase.h"
        )
        self.assertIn("JobIngest::Contracts::Storage::IFileStoragePort", upload_use_case_header)
        self.assertNotIn("domain/configuration/ports/IFileStoragePort.h", upload_use_case_header)

    def test_live_sources_do_not_use_legacy_file_storage_contracts(self) -> None:
        hits: list[str] = []
        for path in _source_files(
            "apps/runtime-service",
            "modules/job-ingest",
            "modules/workflow/application",
        ):
            relative = path.relative_to(WORKSPACE_ROOT).as_posix()
            if "/tests/" in relative:
                continue
            text = path.read_text(encoding="utf-8-sig", errors="ignore")
            if "domain/configuration/ports/IFileStoragePort.h" in text:
                hits.append(relative)
        self.assertFalse(hits, msg=f"live sources still include legacy file-storage contracts: {hits}")

    def test_live_targets_do_not_link_configuration_legacy_bridge(self) -> None:
        expected_contract_targets = {
            "apps/planner-cli/CMakeLists.txt",
            "apps/runtime-service/CMakeLists.txt",
            "modules/job-ingest/CMakeLists.txt",
            "modules/runtime-execution/adapters/device/CMakeLists.txt",
            "modules/runtime-execution/application/CMakeLists.txt",
            "modules/runtime-execution/contracts/runtime/CMakeLists.txt",
            "modules/workflow/application/CMakeLists.txt",
            "modules/workflow/domain/CMakeLists.txt",
            "modules/workflow/domain/domain/CMakeLists.txt",
        }
        for relative in sorted(expected_contract_targets):
            text = _read(relative)
            self.assertNotIn(
                "siligen_process_planning_legacy_configuration_bridge_public",
                text,
                msg=f"{relative} must not link the legacy configuration bridge target",
            )
            self.assertIn(
                "siligen_process_planning_contracts_public",
                text,
                msg=f"{relative} must link the canonical configuration contracts target",
            )

    def test_job_ingest_targets_do_not_link_process_planning_domain_configuration(self) -> None:
        hits: list[str] = []
        for path in (WORKSPACE_ROOT / "modules" / "job-ingest").rglob("CMakeLists.txt"):
            relative = path.relative_to(WORKSPACE_ROOT).as_posix()
            text = path.read_text(encoding="utf-8-sig", errors="ignore")
            if "siligen_process_planning_domain_configuration" in text:
                hits.append(relative)
        self.assertFalse(hits, msg=f"job-ingest targets still depend on process-planning domain configuration: {hits}")

    def test_process_planning_configuration_targets_do_not_link_domain_configuration(self) -> None:
        for relative in (
            "modules/process-planning/contracts/CMakeLists.txt",
            "modules/process-planning/application/CMakeLists.txt",
        ):
            text = _read(relative)
            self.assertNotIn(
                "siligen_process_planning_domain_configuration",
                text,
                msg=f"{relative} must not depend on domain/configuration targets",
            )

    def test_workflow_live_sources_do_not_use_residual_config_types(self) -> None:
        validator_header = _read("modules/workflow/domain/domain/safety/domain-services/SoftLimitValidator.h")
        self.assertIn("process_planning/contracts/configuration/ConfigTypes.h", validator_header)
        self.assertNotIn("domain/configuration/value-objects/ConfigTypes.h", validator_header)

    def test_workflow_tests_root_is_canonical(self) -> None:
        workflow_tests_root = _read("modules/workflow/tests/CMakeLists.txt")
        workflow_unit_tests = _read("modules/workflow/tests/unit/CMakeLists.txt")

        self.assertIn('"${CMAKE_CURRENT_SOURCE_DIR}/unit"', workflow_tests_root)
        self.assertIn('"${CMAKE_CURRENT_SOURCE_DIR}/integration"', workflow_tests_root)
        self.assertIn('"${CMAKE_CURRENT_SOURCE_DIR}/regression"', workflow_tests_root)
        self.assertNotIn("process-runtime-core", workflow_tests_root)
        self.assertFalse(
            (WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "process-runtime-core").exists(),
            msg="workflow legacy process-runtime-core tests root must be removed",
        )
        for expected in (
            "siligen_unit_tests",
            "siligen_pr1_tests",
            "siligen_dispensing_semantics_tests",
        ):
            self.assertIn(expected, workflow_unit_tests)

    def test_workflow_regression_and_integration_roots_match_post_cutover_state(self) -> None:
        workflow_regression_cmake = _read("modules/workflow/tests/regression/CMakeLists.txt")
        workflow_regression_readme = _read("modules/workflow/tests/regression/README.md")
        workflow_integration_cmake = _read("modules/workflow/tests/integration/CMakeLists.txt")
        workflow_integration_readme = _read("modules/workflow/tests/integration/README.md")

        for expected in (
            "workflow_regression_deterministic_path_execution_smoke",
            "workflow_regression_boundary_cutover_smoke",
            "workflow_regression_planning_ingress_smoke",
        ):
            self.assertIn(expected, workflow_regression_cmake)
            self.assertIn(expected, workflow_regression_readme)

        self.assertNotIn("workflow_integration_motion_runtime_assembly_smoke", workflow_integration_cmake)
        self.assertNotIn("workflow_integration_motion_runtime_assembly_smoke", workflow_integration_readme)

    def test_build_validation_and_workspace_validation_track_workflow_regression_assets(self) -> None:
        build_validation = _read("scripts/build/build-validation.ps1")
        workspace_validation = _read("shared/testing/test-kit/src/test_kit/workspace_validation.py")

        self.assertIn("function Get-WorkspaceBuildToken", build_validation)
        self.assertIn('Join-Path (Join-Path $env:LOCALAPPDATA "SS") ("cab-" + $workspaceBuildToken)', build_validation)
        self.assertIn("WORKSPACE_BUILD_TOKEN = hashlib.sha256", workspace_validation)
        self.assertIn('/ "SS"', workspace_validation)
        self.assertIn('f"cab-{WORKSPACE_BUILD_TOKEN}"', workspace_validation)

        for expected in (
            "siligen_dispensing_semantics_tests",
            "workflow_regression_deterministic_path_execution_smoke",
            "workflow_regression_boundary_cutover_smoke",
            "workflow_regression_planning_ingress_smoke",
        ):
            self.assertIn(expected, build_validation)
            self.assertIn(expected, workspace_validation)

    def test_layout_validator_enforces_current_workflow_boundary(self) -> None:
        validator = _read("scripts/migration/validate_workspace_layout.py")
        self.assertIn(
            "workflow application motion target must not link: siligen_runtime_execution_application_public",
            validator,
        )
        self.assertIn(
            "workflow application headers must not re-export: siligen_runtime_execution_application_public",
            validator,
        )

    def test_hmi_formal_gateway_contract_file_exists(self) -> None:
        gateway_contract = WORKSPACE_ROOT / "apps" / "hmi-app" / "config" / "gateway-launch.json"
        gateway_sample = WORKSPACE_ROOT / "apps" / "hmi-app" / "config" / "gateway-launch.sample.json"

        self.assertTrue(gateway_contract.exists(), msg="formal gateway launch contract must exist")
        payload = json.loads(gateway_contract.read_text(encoding="utf-8"))
        sample_payload = json.loads(gateway_sample.read_text(encoding="utf-8"))

        self.assertEqual(payload["env"]["SILIGEN_TCP_SERVER_PORT"], "9527")
        self.assertEqual(payload["env"]["SILIGEN_TCP_SERVER_HOST"], "127.0.0.1")
        self.assertEqual(payload["args"], sample_payload["args"])
        self.assertEqual(payload["env"], sample_payload["env"])

    def test_root_entries_run_bridge_exit_gate(self) -> None:
        build_validation = _read("scripts/build/build-validation.ps1")
        local_gate = _read("scripts/validation/run-local-validation-gate.ps1")
        ci_entry = _read("ci.ps1")

        self.assertIn("validate_workspace_layout.py", build_validation)
        self.assertIn("legacy-exit-checks.py", local_gate)
        self.assertIn("legacy-exit-checks.py", ci_entry)


if __name__ == "__main__":
    unittest.main()
