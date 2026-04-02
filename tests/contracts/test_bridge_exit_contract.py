from __future__ import annotations

import re
import sys
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.legacy_fact_catalog import load_legacy_fact_catalog


LEGACY_FACT_CATALOG = load_legacy_fact_catalog(WORKSPACE_ROOT)
BRIDGE_ROOTS = LEGACY_FACT_CATALOG.root_paths_for_rule("bridge-root-presence")
FORBIDDEN_MODULE_YAML_SNIPPETS = LEGACY_FACT_CATALOG.snippets_for_rule(
    "bridge-metadata",
    target_glob="modules/**/module.yaml",
)
FORBIDDEN_CMAKE_SNIPPETS = LEGACY_FACT_CATALOG.snippets_for_rule(
    "bridge-metadata",
    target_glob="modules/**/CMakeLists.txt",
)


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore")


class BridgeExitContractTest(unittest.TestCase):
    def test_legacy_fact_catalog_has_no_active_exceptions(self) -> None:
        active = [item.exception_id for item in LEGACY_FACT_CATALOG.active_exceptions()]
        self.assertFalse(active, msg=f"active legacy exceptions are not allowed after closeout: {active}")

    def test_legacy_fact_catalog_tracks_removed_and_migration_roots(self) -> None:
        self.assertEqual(LEGACY_FACT_CATALOG.root_registry["ROOT-017"].classification, "migration-source")
        self.assertEqual(LEGACY_FACT_CATALOG.root_registry["ROOT-101"].classification, "removed")

    def test_bridge_roots_are_physically_absent(self) -> None:
        present = [relative for relative in BRIDGE_ROOTS if (WORKSPACE_ROOT / relative).exists()]
        self.assertFalse(present, msg=f"bridge roots still present: {present}")

    def test_module_yaml_is_zero_bridge(self) -> None:
        hits: list[str] = []
        for path in (WORKSPACE_ROOT / "modules").rglob("module.yaml"):
            text = _read(path)
            for snippet in FORBIDDEN_MODULE_YAML_SNIPPETS:
                if snippet in text:
                    hits.append(f"{path.relative_to(WORKSPACE_ROOT).as_posix()} -> {snippet}")
        self.assertFalse(hits, msg=f"module bridge metadata still present: {hits}")

    def test_module_cmake_is_zero_bridge(self) -> None:
        hits: list[str] = []
        for path in (WORKSPACE_ROOT / "modules").rglob("CMakeLists.txt"):
            text = _read(path)
            for snippet in FORBIDDEN_CMAKE_SNIPPETS:
                if snippet in text:
                    hits.append(f"{path.relative_to(WORKSPACE_ROOT).as_posix()} -> {snippet}")
        self.assertFalse(hits, msg=f"module bridge CMake metadata still present: {hits}")

    def test_runtime_motion_compat_headers_are_physically_absent(self) -> None:
        removed_headers = [
            "modules/motion-planning/domain/motion/ports/IIOControlPort.h",
            "modules/motion-planning/domain/motion/ports/IMotionRuntimePort.h",
            "modules/motion-planning/domain/motion/domain-services/HomingProcess.h",
            "modules/motion-planning/domain/motion/domain-services/JogController.h",
            "modules/motion-planning/domain/motion/domain-services/MotionBufferController.h",
            "modules/motion-planning/domain/motion/domain-services/ReadyZeroDecisionService.h",
            "modules/motion-planning/domain/motion/domain-services/MotionControlServiceImpl.h",
            "modules/motion-planning/domain/motion/domain-services/MotionStatusServiceImpl.h",
            "modules/workflow/domain/include/domain/motion/ports/IIOControlPort.h",
            "modules/workflow/domain/include/domain/motion/ports/IMotionRuntimePort.h",
            "modules/workflow/domain/domain/motion/ports/IIOControlPort.h",
            "modules/workflow/domain/domain/motion/ports/IMotionRuntimePort.h",
        ]
        present = [relative for relative in removed_headers if (WORKSPACE_ROOT / relative).exists()]
        self.assertFalse(present, msg=f"legacy runtime motion compat headers still present: {present}")

    def test_configuration_residual_compat_headers_are_physically_absent(self) -> None:
        removed_headers = [
            "modules/process-planning/contracts/legacy-bridge/include/domain/configuration/ports/IConfigurationPort.h",
            "modules/process-planning/domain/configuration/ports/IConfigurationPort.h",
            "modules/process-planning/domain/configuration/ports/IFileStoragePort.h",
            "modules/process-planning/domain/configuration/ports/ValveConfig.h",
            "modules/process-planning/domain/configuration/services/ReadyZeroSpeedResolver.h",
            "modules/process-planning/domain/configuration/value-objects/ConfigTypes.h",
            "modules/workflow/domain/include/domain/configuration/ports/IConfigurationPort.h",
            "modules/workflow/domain/include/domain/configuration/ports/IFileStoragePort.h",
            "modules/workflow/domain/include/domain/configuration/ports/ValveConfig.h",
            "modules/workflow/domain/include/domain/configuration/services/ReadyZeroSpeedResolver.h",
            "modules/workflow/domain/domain/configuration/ports/IConfigurationPort.h",
            "modules/workflow/domain/domain/configuration/ports/IFileStoragePort.h",
            "modules/workflow/domain/domain/configuration/ports/ValveConfig.h",
            "modules/workflow/domain/domain/configuration/value-objects/ConfigTypes.h",
        ]
        present = [relative for relative in removed_headers if (WORKSPACE_ROOT / relative).exists()]
        self.assertFalse(present, msg=f"residual configuration compat headers still present: {present}")

    def test_runtime_process_bootstrap_forwarders_are_physically_absent(self) -> None:
        removed_headers = [
            "modules/runtime-execution/runtime/host/ContainerBootstrap.h",
            "modules/runtime-execution/runtime/host/runtime/configuration/WorkspaceAssetPaths.h",
        ]
        present = [relative for relative in removed_headers if (WORKSPACE_ROOT / relative).exists()]
        self.assertFalse(present, msg=f"runtime process bootstrap forwarders still present: {present}")

    def test_shared_contracts_resolve_runtime_device_contracts_from_canonical_root(self) -> None:
        shared_contracts = _read(WORKSPACE_ROOT / "shared" / "contracts" / "CMakeLists.txt")
        self.assertIn('set(SILIGEN_DEVICE_CONTRACTS_CANONICAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/device")', shared_contracts)
        self.assertNotIn("modules/runtime-execution/device-contracts", shared_contracts)
        self.assertNotIn("modules/runtime-execution/contracts/device", shared_contracts)

    def test_shared_device_contracts_have_canonical_build_entry(self) -> None:
        canonical_device_contracts = WORKSPACE_ROOT / "shared" / "contracts" / "device" / "CMakeLists.txt"
        self.assertTrue(canonical_device_contracts.exists(), msg="shared/contracts/device/CMakeLists.txt is required")
        device_cmake = _read(canonical_device_contracts)
        self.assertIn("siligen_device_contracts", device_cmake)
        self.assertIn('SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/contracts/device"', device_cmake)

    def test_application_owner_wiring_has_no_reverse_mutations(self) -> None:
        workflow_application = _read(WORKSPACE_ROOT / "modules" / "workflow" / "application" / "CMakeLists.txt")
        self.assertIn("siligen_job_ingest_application_public", workflow_application)
        self.assertIn("siligen_dxf_geometry_application_public", workflow_application)
        self.assertIn("siligen_runtime_execution_runtime_contracts", workflow_application)
        self.assertNotIn("../../runtime-execution/application/include", workflow_application)
        motion_target_block = re.search(
            r"target_link_libraries\(siligen_application_motion PUBLIC(?P<body>[\s\S]*?)target_compile_features\(siligen_application_motion",
            workflow_application,
        )
        self.assertIsNotNone(motion_target_block)
        self.assertNotIn("siligen_runtime_execution_application_public", motion_target_block.group("body"))
        headers_target_block = re.search(
            r"target_link_libraries\(siligen_workflow_application_headers INTERFACE(?P<body>[\s\S]*?)target_compile_features\(siligen_workflow_application_headers",
            workflow_application,
        )
        self.assertIsNotNone(headers_target_block)
        self.assertNotIn("siligen_runtime_execution_application_public", headers_target_block.group("body"))

        workflow_module = _read(WORKSPACE_ROOT / "modules" / "workflow" / "CMakeLists.txt")
        workflow_domain_root = _read(WORKSPACE_ROOT / "modules" / "workflow" / "domain" / "CMakeLists.txt")
        workflow_domain = _read(WORKSPACE_ROOT / "modules" / "workflow" / "domain" / "domain" / "CMakeLists.txt")
        self.assertNotIn("siligen_workflow_adapters_public", workflow_module)
        self.assertNotIn("siligen_workflow_runtime_consumer_public", workflow_module)
        self.assertNotIn("siligen_process_runtime_core_interface", workflow_module)
        self.assertNotIn("siligen_workflow_dispensing_planning_compat", workflow_domain)
        self.assertNotIn("siligen_motion_execution_services", workflow_domain)
        self.assertNotIn("domain_machine", workflow_domain_root)

        for relative, forbidden in (
            ("modules/job-ingest/CMakeLists.txt", "siligen_workflow_application_public"),
            ("modules/job-ingest/CMakeLists.txt", "siligen_process_runtime_core_application"),
            ("modules/job-ingest/CMakeLists.txt", "siligen_application_dispensing"),
            ("modules/dxf-geometry/CMakeLists.txt", "siligen_workflow_application_public"),
            ("modules/dxf-geometry/CMakeLists.txt", "siligen_process_runtime_core_application"),
            ("modules/dxf-geometry/CMakeLists.txt", "siligen_application_dispensing"),
            ("modules/runtime-execution/application/CMakeLists.txt", "siligen_workflow_application_headers"),
            ("modules/runtime-execution/application/CMakeLists.txt", "siligen_process_runtime_core_application"),
            ("modules/runtime-execution/application/CMakeLists.txt", "siligen_application_dispensing"),
        ):
            self.assertNotIn(forbidden, _read(WORKSPACE_ROOT / relative), msg=f"{relative} must not reference {forbidden}")

    def test_workflow_dxf_wrapper_uses_owner_public_header(self) -> None:
        wrapper = _read(
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "application"
            / "include"
            / "application"
            / "services"
            / "dxf"
            / "DxfPbPreparationService.h"
        )
        self.assertIn("dxf_geometry/application/services/dxf/DxfPbPreparationService.h", wrapper)
        self.assertNotIn("../../../../../../dxf-geometry/application/include/application/services/dxf/DxfPbPreparationService.h", wrapper)

    def test_configuration_contracts_resolve_from_canonical_headers(self) -> None:
        process_planning_contracts = _read(
            WORKSPACE_ROOT
            / "modules"
            / "process-planning"
            / "contracts"
            / "include"
            / "process_planning"
            / "contracts"
            / "ConfigurationContracts.h"
        )
        self.assertIn("process_planning/contracts/configuration/ConfigTypes.h", process_planning_contracts)
        self.assertIn("process_planning/contracts/configuration/IConfigurationPort.h", process_planning_contracts)
        self.assertIn("process_planning/contracts/configuration/ReadyZeroSpeedResolver.h", process_planning_contracts)
        self.assertIn("process_planning/contracts/configuration/ValveConfig.h", process_planning_contracts)
        self.assertNotIn("legacy-bridge/include/domain/configuration/ports/IConfigurationPort.h", process_planning_contracts)

        canonical_config_types = _read(
            WORKSPACE_ROOT
            / "modules"
            / "process-planning"
            / "contracts"
            / "include"
            / "process_planning"
            / "contracts"
            / "configuration"
            / "ConfigTypes.h"
        )
        self.assertIn("struct Position3D", canonical_config_types)
        self.assertIn("struct InterlockConfig", canonical_config_types)
        self.assertIn("struct SystemConfiguration", canonical_config_types)
        self.assertNotIn("domain/motion/value-objects/MotionTypes.h", canonical_config_types)
        self.assertNotIn("domain/safety/value-objects/InterlockTypes.h", canonical_config_types)
        self.assertNotIn("domain/configuration/value-objects/ConfigTypes.h", canonical_config_types)

        runtime_config_contract = _read(
            WORKSPACE_ROOT
            / "modules"
            / "runtime-execution"
            / "contracts"
            / "runtime"
            / "include"
            / "runtime_execution"
            / "contracts"
            / "configuration"
            / "IConfigurationPort.h"
        )
        self.assertIn("process_planning/contracts/configuration/IConfigurationPort.h", runtime_config_contract)
        self.assertNotIn('domain/configuration/ports/IConfigurationPort.h"', runtime_config_contract)

    def test_file_storage_contracts_resolve_from_job_ingest_canonical_headers(self) -> None:
        canonical_file_storage_contract = _read(
            WORKSPACE_ROOT
            / "modules"
            / "job-ingest"
            / "contracts"
            / "include"
            / "job_ingest"
            / "contracts"
            / "storage"
            / "IFileStoragePort.h"
        )
        self.assertIn("namespace Siligen::JobIngest::Contracts::Storage", canonical_file_storage_contract)
        self.assertIn("struct FileData", canonical_file_storage_contract)
        self.assertIn("class IFileStoragePort", canonical_file_storage_contract)
        self.assertFalse(
            (WORKSPACE_ROOT / "modules" / "workflow" / "domain" / "include" / "domain" / "configuration" / "ports" / "IFileStoragePort.h").exists(),
            msg="workflow file-storage compatibility wrapper must be removed",
        )

    def test_live_sources_do_not_use_legacy_file_storage_contracts(self) -> None:
        live_source_files = (
            "modules/job-ingest/application/include/application/usecases/dispensing/UploadFileUseCase.h",
            "modules/job-ingest/application/usecases/dispensing/UploadFileUseCase.cpp",
            "modules/job-ingest/tests/unit/dispensing/UploadFileUseCaseTest.cpp",
            "modules/workflow/application/include/application/usecases/dispensing/CleanupFilesUseCase.h",
            "modules/workflow/application/usecases/dispensing/CleanupFilesUseCase.h",
            "modules/workflow/application/usecases/dispensing/CleanupFilesUseCase.cpp",
            "modules/workflow/tests/unit/dispensing/CleanupFilesUseCaseTest.cpp",
            "apps/runtime-service/bootstrap/InfrastructureBindings.h",
            "apps/runtime-service/bootstrap/ContainerBootstrap.cpp",
            "apps/runtime-service/container/ApplicationContainerFwd.h",
            "apps/runtime-service/container/ApplicationContainer.h",
            "apps/runtime-service/runtime/storage/files/LocalFileStorageAdapter.h",
            "apps/runtime-service/runtime/storage/files/LocalFileStorageAdapter.cpp",
        )
        for relative in live_source_files:
            text = _read(WORKSPACE_ROOT / relative)
            self.assertNotIn(
                '#include "domain/configuration/ports/IFileStoragePort.h"',
                text,
                msg=f"{relative} must not include the legacy file-storage header",
            )
            self.assertNotIn(
                "Domain::Configuration::Ports::IFileStoragePort",
                text,
                msg=f"{relative} must not reference the legacy file-storage port namespace",
            )
            self.assertNotIn(
                "Domain::Configuration::Ports::FileData",
                text,
                msg=f"{relative} must not reference the legacy file-storage data namespace",
            )

    def test_live_targets_do_not_link_configuration_legacy_bridge(self) -> None:
        live_cmake_files = (
            "modules/workflow/application/CMakeLists.txt",
            "modules/workflow/domain/CMakeLists.txt",
            "modules/workflow/domain/domain/CMakeLists.txt",
            "modules/job-ingest/CMakeLists.txt",
            "modules/dxf-geometry/CMakeLists.txt",
            "modules/dispense-packaging/domain/dispensing/CMakeLists.txt",
            "modules/runtime-execution/contracts/runtime/CMakeLists.txt",
            "modules/runtime-execution/application/CMakeLists.txt",
            "modules/runtime-execution/adapters/device/CMakeLists.txt",
            "apps/runtime-service/CMakeLists.txt",
            "apps/planner-cli/CMakeLists.txt",
            "apps/runtime-gateway/CMakeLists.txt",
            "apps/runtime-gateway/transport-gateway/CMakeLists.txt",
        )
        for relative in live_cmake_files:
            text = _read(WORKSPACE_ROOT / relative)
            self.assertNotIn(
                "siligen_process_planning_legacy_configuration_bridge_public",
                text,
                msg=f"{relative} must not reference the deprecated configuration legacy bridge",
            )
            self.assertIn(
                "siligen_process_planning_contracts_public",
                text,
                msg=f"{relative} must reference the canonical process-planning contracts target",
            )

        process_planning_contracts = _read(WORKSPACE_ROOT / "modules" / "process-planning" / "contracts" / "CMakeLists.txt")
        self.assertNotIn("SILIGEN_PROCESS_PLANNING_LEGACY_BRIDGE_INCLUDE_DIR", process_planning_contracts)
        self.assertNotIn("siligen_process_planning_legacy_configuration_bridge_public", process_planning_contracts)

    def test_job_ingest_targets_do_not_link_process_planning_domain_configuration(self) -> None:
        job_ingest_module = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "CMakeLists.txt")
        self.assertNotIn("siligen_process_planning_domain_configuration", job_ingest_module)
        self.assertIn("siligen_process_planning_contracts_public", job_ingest_module)

        job_ingest_tests = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "tests" / "CMakeLists.txt")
        self.assertNotIn("siligen_process_planning_domain_configuration", job_ingest_tests)

    def test_process_planning_configuration_targets_do_not_link_domain_configuration(self) -> None:
        process_planning_module = _read(WORKSPACE_ROOT / "modules" / "process-planning" / "CMakeLists.txt")
        self.assertIn("siligen_process_planning_application_public", process_planning_module)
        self.assertNotIn(
            "target_link_libraries(siligen_module_process_planning INTERFACE\n        siligen_process_planning_domain_configuration",
            process_planning_module,
        )

        process_planning_contracts = _read(WORKSPACE_ROOT / "modules" / "process-planning" / "contracts" / "CMakeLists.txt")
        self.assertNotIn("siligen_process_planning_domain_configuration", process_planning_contracts)
        self.assertNotIn("SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR", process_planning_contracts)
        self.assertNotIn("SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR", process_planning_contracts)

        process_planning_application = _read(
            WORKSPACE_ROOT / "modules" / "process-planning" / "application" / "CMakeLists.txt"
        )
        self.assertNotIn("siligen_process_planning_domain_configuration", process_planning_application)
        self.assertIn("siligen_process_planning_contracts_public", process_planning_application)

        dxf_geometry_tests = _read(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "tests" / "CMakeLists.txt")
        self.assertNotIn("siligen_process_planning_domain_configuration", dxf_geometry_tests)
        self.assertIn("siligen_process_planning_contracts_public", dxf_geometry_tests)

    def test_workflow_live_sources_do_not_use_residual_config_types(self) -> None:
        live_source_files = (
            "modules/workflow/domain/domain/safety/domain-services/SoftLimitValidator.h",
            "modules/workflow/tests/unit/domain/safety/SoftLimitValidatorTest.cpp",
        )
        for relative in live_source_files:
            text = _read(WORKSPACE_ROOT / relative)
            self.assertNotIn(
                '#include "domain/configuration/value-objects/ConfigTypes.h"',
                text,
                msg=f"{relative} must not include the residual workflow ConfigTypes wrapper",
            )
            self.assertNotIn(
                '#include "../../configuration/value-objects/ConfigTypes.h"',
                text,
                msg=f"{relative} must not include the residual workflow ConfigTypes wrapper",
            )
            self.assertIn(
                "process_planning/contracts/configuration/ConfigTypes.h",
                text,
                msg=f"{relative} must include the canonical process-planning ConfigTypes contract",
            )

    def test_workflow_unit_tests_are_canonical_and_legacy_root_removed(self) -> None:
        workflow_tests_root = _read(WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "CMakeLists.txt")
        workflow_unit_tests = _read(WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "unit" / "CMakeLists.txt")
        workflow_module = _read(WORKSPACE_ROOT / "modules" / "workflow" / "CMakeLists.txt")

        self.assertIn('"${CMAKE_CURRENT_SOURCE_DIR}/unit/CMakeLists.txt"', workflow_tests_root)
        self.assertNotIn("process-runtime-core", workflow_tests_root)
        self.assertNotIn("process_runtime_core_", workflow_unit_tests)
        self.assertNotIn("PROCESS_RUNTIME_CORE_", workflow_module)
        self.assertFalse(
            (WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "process-runtime-core").exists(),
            msg="workflow legacy process-runtime-core tests root must be removed",
        )

    def test_workflow_regression_and_integration_roots_match_post_cutover_state(self) -> None:
        workflow_regression_cmake = _read(WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "regression" / "CMakeLists.txt")
        workflow_regression_readme = _read(WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "regression" / "README.md")
        workflow_integration_cmake = _read(WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "integration" / "CMakeLists.txt")
        workflow_integration_readme = _read(WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "integration" / "README.md")

        for expected in (
            "workflow_regression_deterministic_path_execution_smoke",
            "workflow_regression_boundary_cutover_smoke",
            "workflow_regression_planning_ingress_smoke",
        ):
            self.assertIn(expected, workflow_regression_cmake)
            self.assertIn(expected, workflow_regression_readme)

        self.assertNotIn("workflow_integration_motion_runtime_assembly_smoke", workflow_integration_cmake)
        self.assertNotIn("workflow_integration_motion_runtime_assembly_smoke", workflow_integration_readme)

    def test_root_entries_run_bridge_exit_gate(self) -> None:
        build_validation = _read(WORKSPACE_ROOT / "scripts" / "build" / "build-validation.ps1")
        local_gate = _read(WORKSPACE_ROOT / "scripts" / "validation" / "run-local-validation-gate.ps1")
        ci_entry = _read(WORKSPACE_ROOT / "ci.ps1")

        self.assertIn("validate_workspace_layout.py", build_validation)
        self.assertIn("legacy-exit-checks.py", local_gate)
        self.assertIn("legacy-exit-checks.py", ci_entry)


if __name__ == "__main__":
    unittest.main()
