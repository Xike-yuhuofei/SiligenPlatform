from __future__ import annotations

import sys
import unittest
from pathlib import Path
import json


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
    return path.read_text(encoding="utf-8-sig", errors="ignore")


def _fixed_string_hits(pattern: str, roots: tuple[str, ...] = ("modules", "apps", "tests")) -> list[str]:
    hits: list[str] = []
    valid_suffixes = {".h", ".hpp", ".cpp", ".cc", ".cxx", ".cmake", ".ps1", ".py"}
    current_test_file = Path(__file__).resolve()
    for root in roots:
        for path in (WORKSPACE_ROOT / root).rglob("*"):
            if not path.is_file():
                continue
            if path.resolve() == current_test_file:
                continue
            if path.name != "CMakeLists.txt" and path.suffix not in valid_suffixes:
                continue
            if pattern in _read(path):
                hits.append(path.relative_to(WORKSPACE_ROOT).as_posix())
    return hits


class BridgeExitContractTest(unittest.TestCase):
    def test_legacy_fact_catalog_active_exceptions_are_scoped_and_documented(self) -> None:
        active = {item.exception_id: item for item in LEGACY_FACT_CATALOG.active_exceptions()}
        self.assertEqual({"EXC-904"}, set(active), msg=f"unexpected active legacy exceptions: {sorted(active)}")

        exc_904 = active["EXC-904"]
        self.assertEqual(
            {
                "modules/dxf-geometry/tests/CMakeLists.txt",
                "modules/dxf-geometry/tests/README.md",
                "modules/job-ingest/tests/CMakeLists.txt",
                "modules/job-ingest/tests/README.md",
                "modules/process-path/tests/CMakeLists.txt",
                "modules/process-path/tests/README.md",
                "modules/topology-feature/tests/CMakeLists.txt",
                "modules/topology-feature/tests/README.md",
            },
            set(exc_904.allowed_locations),
        )
        self.assertEqual("modules/*/tests", exc_904.owner_scope)
        self.assertIn("canonical test directories", exc_904.justification.lower())

    def test_legacy_fact_catalog_tracks_removed_and_migration_roots(self) -> None:
        self.assertEqual(LEGACY_FACT_CATALOG.root_registry["ROOT-017"].classification, "migration-source")
        self.assertEqual(LEGACY_FACT_CATALOG.root_registry["ROOT-101"].classification, "removed")
        self.assertEqual(LEGACY_FACT_CATALOG.root_registry["ROOT-011"].path, "specs")
        self.assertEqual(LEGACY_FACT_CATALOG.root_registry["ROOT-011"].classification, "local-cache")
        self.assertEqual(LEGACY_FACT_CATALOG.root_registry["ROOT-108"].path, ".specify")
        self.assertEqual(LEGACY_FACT_CATALOG.root_registry["ROOT-108"].classification, "local-cache")

    def test_local_cache_roots_are_classified_as_ignored_non_baseline_assets(self) -> None:
        self.assertEqual(LEGACY_FACT_CATALOG.classify_zone("specs/demo/spec.md"), "local-cache-root")
        self.assertEqual(LEGACY_FACT_CATALOG.classify_zone(".specify/init-options.json"), "local-cache-root")
        local_cache_zone = LEGACY_FACT_CATALOG.zone_registry["local-cache-root"]
        self.assertEqual(local_cache_zone.zone_class, "local-cache")
        self.assertEqual(local_cache_zone.default_scan_policy, "exclude")

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
        self.assertIn("siligen_job_ingest_contracts", workflow_application)
        self.assertIn("siligen_dxf_geometry_application_public", workflow_application)
        self.assertIn("siligen_runtime_execution_application_public", workflow_application)
        self.assertNotIn("siligen_runtime_execution_application_headers", workflow_application)

        runtime_execution_application = _read(
            WORKSPACE_ROOT / "modules" / "runtime-execution" / "application" / "CMakeLists.txt"
        )
        self.assertIn("siligen_runtime_execution_safety_services", runtime_execution_application)
        self.assertNotIn("siligen_safety_domain_services", runtime_execution_application)
        self.assertNotIn(
            "target_include_directories(siligen_runtime_execution_runtime_contracts INTERFACE",
            runtime_execution_application,
        )

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
            ("modules/workflow/tests/canonical/CMakeLists.txt", "siligen_workflow_application_public"),
            ("modules/workflow/tests/integration/CMakeLists.txt", "siligen_workflow_application_public"),
        ):
            self.assertNotIn(forbidden, _read(WORKSPACE_ROOT / relative), msg=f"{relative} must not reference {forbidden}")

    def test_workflow_planning_port_adapters_header_is_declaration_only(self) -> None:
        adapters_header = _read(
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "application"
            / "include"
            / "application"
            / "ports"
            / "dispensing"
            / "PlanningPortAdapters.h"
        )
        self.assertIn("AdaptProcessPathFacade(", adapters_header)
        self.assertIn("AdaptMotionPlanningFacade(", adapters_header)
        self.assertIn("AdaptDxfPreparationService(", adapters_header)
        self.assertNotIn('#include "application/services/motion_planning/MotionPlanningFacade.h"', adapters_header)
        self.assertNotIn('#include "application/services/process_path/ProcessPathFacade.h"', adapters_header)
        self.assertNotIn('#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"', adapters_header)
        self.assertNotIn("class ProcessPathFacadePortAdapter final", adapters_header)
        self.assertNotIn("class MotionPlanningFacadePortAdapter final", adapters_header)
        self.assertNotIn("class DxfPreparationPortAdapter final", adapters_header)

        runtime_service_source = _read(
            WORKSPACE_ROOT / "apps" / "runtime-service" / "container" / "ApplicationContainer.Dispensing.cpp"
        )
        self.assertIn(
            '#include "application/services/process_path/ProcessPathFacade.h"',
            runtime_service_source,
            msg="runtime-service must include ProcessPathFacade directly after workflow adapter header stops re-exporting it",
        )

    def test_workflow_module_root_bundle_uses_canonical_surfaces_only(self) -> None:
        workflow_root = _read(WORKSPACE_ROOT / "modules" / "workflow" / "CMakeLists.txt")
        self.assertIn("siligen_module_workflow", workflow_root)
        self.assertIn("siligen_workflow_application_headers", workflow_root)
        self.assertIn("siligen_workflow_adapters_public", workflow_root)
        self.assertIn("siligen_application_dispensing", workflow_root)
        self.assertIn("siligen_application_redundancy", workflow_root)
        self.assertNotIn("siligen_workflow_application_public", workflow_root)

    def test_workflow_bridge_domain_no_longer_compiles_runtime_safety_or_dispensing_concrete(self) -> None:
        workflow_domain_cmake = _read(WORKSPACE_ROOT / "modules" / "workflow" / "domain" / "domain" / "CMakeLists.txt")
        workflow_domain_root_cmake = _read(WORKSPACE_ROOT / "modules" / "workflow" / "domain" / "CMakeLists.txt")
        self.assertNotIn("siligen_safety_domain_services", workflow_domain_cmake)
        self.assertNotIn("siligen_dispensing_execution_services", workflow_domain_cmake)
        self.assertNotIn("siligen_triggering", workflow_domain_cmake)
        self.assertNotIn("PositionTriggerController.cpp", workflow_domain_cmake)
        self.assertNotIn("siligen_trajectory", workflow_domain_cmake)
        self.assertNotIn("siligen_configuration", workflow_domain_cmake)
        self.assertNotIn("domain_machine", workflow_domain_root_cmake)
        self.assertNotIn("siligen_coordinate_alignment_domain_machine", workflow_domain_root_cmake)
        self.assertNotIn('"${CMAKE_CURRENT_SOURCE_DIR}/../../coordinate-alignment/domain/machine"', workflow_domain_root_cmake)
        self.assertNotIn("if(NOT TARGET siligen_triggering)", workflow_domain_root_cmake)

        motion_core_cmake = WORKSPACE_ROOT / "modules" / "workflow" / "domain" / "motion-core" / "CMakeLists.txt"
        self.assertFalse(
            motion_core_cmake.exists(),
            msg="workflow motion-core CMake must stay deleted after safety family closeout",
        )

    def test_planning_boundary_surface_moves_out_of_workflow_public_root(self) -> None:
        legacy_header = (
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "domain"
            / "include"
            / "domain"
            / "planning-boundary"
            / "ports"
            / "ISpatialIndexPort.h"
        )
        self.assertFalse(
            legacy_header.exists(),
            msg="workflow planning-boundary port header should be deleted after canonical move to dispense-packaging",
        )

        canonical_header = (
            WORKSPACE_ROOT
            / "modules"
            / "dispense-packaging"
            / "domain"
            / "dispensing"
            / "planning"
            / "ports"
            / "ISpatialIndexPort.h"
        )
        self.assertTrue(canonical_header.exists(), msg="dispense-packaging must expose the canonical spatial index port")
        self.assertIn("namespace Siligen::Domain::PlanningBoundary::Ports", _read(canonical_header))

        for relative in (
            "modules/dispense-packaging/domain/dispensing/domain-services/PathOptimizationStrategy.h",
            "modules/workflow/domain/domain/dispensing/domain-services/PathOptimizationStrategy.h",
        ):
            self.assertIn(
                '#include "domain/dispensing/planning/ports/ISpatialIndexPort.h"',
                _read(WORKSPACE_ROOT / relative),
                msg=f"{relative} must include the canonical dispense-packaging planning port",
            )

        hits = _fixed_string_hits('#include "domain/planning-boundary/ports/ISpatialIndexPort.h"', roots=("modules", "apps", "tests"))
        self.assertFalse(hits, msg=f"legacy workflow planning-boundary include still used: {hits}")

    def test_workflow_canonical_tests_do_not_register_runtime_safety_cases(self) -> None:
        canonical_cmake = _read(WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "canonical" / "CMakeLists.txt")
        for pattern in (
            "unit/domain/safety/InterlockPolicyTest.cpp",
            "unit/domain/safety/SoftLimitValidatorTest.cpp",
            "unit/domain/safety/SafetyOutputGuardTest.cpp",
        ):
            self.assertNotIn(pattern, canonical_cmake)

        for relative in (
            "modules/workflow/tests/canonical/unit/domain/safety/InterlockPolicyTest.cpp",
            "modules/workflow/tests/canonical/unit/domain/safety/SoftLimitValidatorTest.cpp",
            "modules/workflow/tests/canonical/unit/domain/safety/SafetyOutputGuardTest.cpp",
        ):
            self.assertFalse((WORKSPACE_ROOT / relative).exists(), msg=f"workflow canonical safety test should move out: {relative}")

    def test_workflow_domain_has_no_application_service_dependency(self) -> None:
        workflow_domain_root = WORKSPACE_ROOT / "modules" / "workflow" / "domain"
        hits: list[str] = []
        for path in workflow_domain_root.rglob("*"):
            if path.suffix not in {".h", ".hpp", ".cpp", ".cc", ".cxx"}:
                continue
            text = _read(path)
            if '#include "application/services/' in text:
                hits.append(path.relative_to(WORKSPACE_ROOT).as_posix())
        self.assertFalse(
            hits,
            msg=f"workflow domain must not include application/services headers: {hits}",
        )

    def test_workflow_dxf_wrapper_is_deleted_after_cutover(self) -> None:
        wrapper = (
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
        self.assertFalse(wrapper.exists(), msg="workflow DXF wrapper should be deleted after canonical cutover")

    def test_dxf_geometry_legacy_public_wrapper_is_deleted_after_cutover(self) -> None:
        wrapper = (
            WORKSPACE_ROOT
            / "modules"
            / "dxf-geometry"
            / "application"
            / "include"
            / "application"
            / "services"
            / "dxf"
            / "DxfPbPreparationService.h"
        )
        self.assertFalse(wrapper.exists(), msg="dxf-geometry legacy public wrapper should be deleted after canonical cutover")

        hits = _fixed_string_hits('#include "application/services/dxf/DxfPbPreparationService.h"', roots=("modules", "apps", "tests"))
        self.assertFalse(hits, msg=f"legacy dxf-geometry public include still used: {hits}")

    def test_dxf_geometry_trajectory_shells_are_removed_from_live_surface(self) -> None:
        compat_module = (
            WORKSPACE_ROOT
            / "modules"
            / "dxf-geometry"
            / "application"
            / "engineering_data"
            / "trajectory"
            / "offline_path_to_trajectory.py"
        )
        module_cli = (
            WORKSPACE_ROOT
            / "modules"
            / "dxf-geometry"
            / "application"
            / "engineering_data"
            / "cli"
            / "path_to_trajectory.py"
        )
        self.assertFalse(compat_module.exists(), msg="dxf-geometry trajectory compat module should be deleted after motion-planning cutover")
        self.assertFalse(module_cli.exists(), msg="dxf-geometry module trajectory CLI should be deleted after motion-planning cutover")

        validation_script = _read(WORKSPACE_ROOT / "scripts" / "validation" / "verify-engineering-data-cli.ps1")
        self.assertIn('scripts\\engineering-data\\path_to_trajectory.py', validation_script)
        self.assertNotIn('engineering_data.cli.path_to_trajectory', validation_script)

    def test_dxf_geometry_docs_record_only_simulation_residual_and_external_preview_trajectory_owners(self) -> None:
        readme = _read(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "README.md")
        module_yaml = _read(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "module.yaml")
        tests_readme = _read(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "tests" / "README.md")
        contracts_readme = _read(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "contracts" / "README.md")

        self.assertIn("preview owner 已迁到 `apps/planner-cli/tools/planner_cli_preview/`", readme)
        self.assertIn("trajectory owner 已迁到 `modules/motion-planning/application/motion_planning/trajectory_generation.py`", readme)
        self.assertNotIn("trajectory 只保留兼容 import / CLI 入口", readme)
        self.assertNotIn("compat wrapper", readme)

        self.assertIn("preview owner 已迁到 apps/planner-cli/tools/planner_cli_preview", module_yaml)
        self.assertIn(
            "trajectory owner 已迁到 modules/motion-planning/application/motion_planning/trajectory_generation.py",
            module_yaml,
        )
        self.assertNotIn("trajectory/preview/runtime residual", module_yaml)

        self.assertIn("trajectory compat shell 与模块内旧 CLI 已从 live surface 删除", tests_readme)
        self.assertIn("本地 preview owner gate 已迁到 `apps/planner-cli/tests/python/`", tests_readme)
        self.assertNotIn("trajectory 已切为指向 `motion-planning` owner 的 compat wrapper", tests_readme)

        self.assertIn("simulation runtime concrete", contracts_readme)
        self.assertIn("trajectory owner 已迁到 `motion-planning`", contracts_readme)
        self.assertNotIn("runtime / trajectory residual", contracts_readme)

    def test_workflow_runtime_execution_adapter_header_is_deleted(self) -> None:
        adapter = (
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "application"
            / "include"
            / "application"
            / "ports"
            / "dispensing"
            / "WorkflowExecutionPortAdapters.h"
        )
        self.assertFalse(adapter.exists(), msg="workflow runtime execution adapter header should not remain in public include surface")

    def test_workflow_legacy_event_publisher_shim_is_deleted(self) -> None:
        shim = (
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "domain"
            / "include"
            / "domain"
            / "system"
            / "ports"
            / "IEventPublisherPort.h"
        )
        self.assertFalse(shim.exists(), msg="workflow legacy domain/system event publisher shim should be deleted")

    def test_trace_diagnostics_owns_generic_diagnostics_sink_contract(self) -> None:
        legacy_header = (
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "domain"
            / "include"
            / "domain"
            / "diagnostics"
            / "ports"
            / "IDiagnosticsPort.h"
        )
        self.assertFalse(
            legacy_header.exists(),
            msg="workflow generic diagnostics sink header should be deleted after cutover to trace-diagnostics/contracts",
        )

        canonical_header = (
            WORKSPACE_ROOT
            / "modules"
            / "trace-diagnostics"
            / "contracts"
            / "include"
            / "trace_diagnostics"
            / "contracts"
            / "IDiagnosticsPort.h"
        )
        diagnostic_types = canonical_header.with_name("DiagnosticTypes.h")
        umbrella_header = (
            WORKSPACE_ROOT
            / "modules"
            / "trace-diagnostics"
            / "contracts"
            / "include"
            / "trace_diagnostics"
            / "contracts"
            / "TraceDiagnosticsContracts.h"
        )
        self.assertTrue(canonical_header.exists(), msg="trace-diagnostics contracts must expose IDiagnosticsPort.h")
        self.assertTrue(diagnostic_types.exists(), msg="trace-diagnostics contracts must expose DiagnosticTypes.h")
        self.assertIn('#include "trace_diagnostics/contracts/IDiagnosticsPort.h"', _read(umbrella_header))
        self.assertIn('#include "trace_diagnostics/contracts/DiagnosticTypes.h"', _read(umbrella_header))

        for relative in (
            "modules/workflow/application/CMakeLists.txt",
            "modules/runtime-execution/application/CMakeLists.txt",
            "modules/runtime-execution/runtime/host/CMakeLists.txt",
            "apps/runtime-service/CMakeLists.txt",
            "modules/runtime-execution/adapters/device/CMakeLists.txt",
            "modules/workflow/domain/CMakeLists.txt",
        ):
            self.assertIn(
                "siligen_trace_diagnostics_contracts_public",
                _read(WORKSPACE_ROOT / relative),
                msg=f"{relative} must link trace-diagnostics contracts for the generic sink",
            )

        expected_include = '#include "trace_diagnostics/contracts/IDiagnosticsPort.h"'
        legacy_wrapper = (
            WORKSPACE_ROOT
            / "modules"
            / "runtime-execution"
            / "application"
            / "usecases"
            / "system"
            / "InitializeSystemUseCase.h"
        )
        self.assertFalse(
            legacy_wrapper.exists(),
            msg="runtime-execution system usecase wrapper should be deleted after public include cutover",
        )
        legacy_motion_monitoring_wrapper = (
            WORKSPACE_ROOT
            / "modules"
            / "runtime-execution"
            / "application"
            / "usecases"
            / "motion"
            / "monitoring"
            / "MotionMonitoringUseCase.h"
        )
        self.assertFalse(
            legacy_motion_monitoring_wrapper.exists(),
            msg="runtime-execution motion monitoring wrapper should be deleted after public include cutover",
        )
        for relative in (
            "modules/workflow/application/planning-trigger/PlanningUseCase.cpp",
            "modules/workflow/tests/integration/PlanningFailureSurfaceTest.cpp",
            "modules/runtime-execution/application/include/runtime_execution/application/usecases/system/InitializeSystemUseCase.h",
            "modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h",
            "modules/runtime-execution/runtime/host/runtime/diagnostics/DiagnosticsPortAdapter.h",
            "apps/runtime-service/bootstrap/ContainerBootstrap.cpp",
        ):
            self.assertIn(expected_include, _read(WORKSPACE_ROOT / relative), msg=f"{relative} must include the canonical diagnostics sink contract")

        hits = _fixed_string_hits('#include "domain/diagnostics/ports/IDiagnosticsPort.h"', roots=("modules", "apps"))
        self.assertFalse(hits, msg=f"legacy workflow diagnostics sink include still used: {hits}")

    def test_hardware_test_diagnostics_contracts_move_to_runtime_bootstrap_surface(self) -> None:
        for relative in (
            "apps/runtime-service/include/runtime_process_bootstrap/diagnostics/aggregates/TestConfiguration.h",
            "apps/runtime-service/include/runtime_process_bootstrap/diagnostics/aggregates/TestRecord.h",
            "apps/runtime-service/include/runtime_process_bootstrap/diagnostics/ports/ICMPTestPresetPort.h",
            "apps/runtime-service/include/runtime_process_bootstrap/diagnostics/ports/ITestConfigurationPort.h",
            "apps/runtime-service/include/runtime_process_bootstrap/diagnostics/ports/ITestRecordRepository.h",
            "apps/runtime-service/include/runtime_process_bootstrap/diagnostics/value-objects/TestDataTypes.h",
        ):
            self.assertTrue((WORKSPACE_ROOT / relative).exists(), msg=f"runtime bootstrap diagnostics header missing: {relative}")

        for relative in (
            "modules/workflow/domain/include/domain/diagnostics/aggregates/TestConfiguration.h",
            "modules/workflow/domain/include/domain/diagnostics/aggregates/TestRecord.h",
            "modules/workflow/domain/include/domain/diagnostics/ports/ICMPTestPresetPort.h",
            "modules/workflow/domain/include/domain/diagnostics/ports/ITestConfigurationPort.h",
            "modules/workflow/domain/include/domain/diagnostics/ports/ITestRecordRepository.h",
            "modules/workflow/domain/include/domain/diagnostics/value-objects/TestDataTypes.h",
        ):
            self.assertFalse((WORKSPACE_ROOT / relative).exists(), msg=f"workflow hardware-test diagnostics header should be deleted: {relative}")

        bootstrap = _read(WORKSPACE_ROOT / "apps/runtime-service/bootstrap/ContainerBootstrap.cpp")
        self.assertIn('#include "runtime_process_bootstrap/diagnostics/ports/ICMPTestPresetPort.h"', bootstrap)
        self.assertIn('#include "runtime_process_bootstrap/diagnostics/ports/ITestConfigurationPort.h"', bootstrap)
        self.assertIn('#include "runtime_process_bootstrap/diagnostics/ports/ITestRecordRepository.h"', bootstrap)
        self.assertNotIn('#include "domain/diagnostics/ports/ICMPTestPresetPort.h"', bootstrap)
        self.assertNotIn('#include "domain/diagnostics/ports/ITestConfigurationPort.h"', bootstrap)
        self.assertNotIn('#include "domain/diagnostics/ports/ITestRecordRepository.h"', bootstrap)

        expected_hits = {
            '#include "runtime_process_bootstrap/diagnostics/ports/ITestRecordRepository.h"': {
                "apps/runtime-service/bootstrap/ContainerBootstrap.cpp",
            },
            '#include "runtime_process_bootstrap/diagnostics/ports/ITestConfigurationPort.h"': {
                "apps/runtime-service/bootstrap/ContainerBootstrap.cpp",
            },
            '#include "runtime_process_bootstrap/diagnostics/ports/ICMPTestPresetPort.h"': {
                "apps/runtime-service/bootstrap/ContainerBootstrap.cpp",
            },
            '#include "trace_diagnostics/contracts/DiagnosticTypes.h"': {
                "apps/runtime-service/include/runtime_process_bootstrap/diagnostics/value-objects/TestDataTypes.h",
                "modules/runtime-execution/adapters/device/include/siligen/device/adapters/hardware/HardwareTestAdapter.h",
                "modules/trace-diagnostics/contracts/include/trace_diagnostics/contracts/TraceDiagnosticsContracts.h",
            },
        }
        legacy_wrapper = (
            WORKSPACE_ROOT
            / "modules"
            / "runtime-execution"
            / "adapters"
            / "device"
            / "src"
            / "adapters"
            / "diagnostics"
            / "health"
            / "testing"
            / "HardwareTestAdapter.h"
        )
        self.assertFalse(
            legacy_wrapper.exists(),
            msg="runtime-execution device diagnostics wrapper header should be deleted after public include cutover",
        )
        for pattern, expected in expected_hits.items():
            hits = set(_fixed_string_hits(pattern, roots=("modules", "apps")))
            self.assertEqual(expected, hits, msg=f"hardware-test diagnostics cutover drifted for pattern: {pattern}")

    def test_workflow_recipe_serializer_is_deleted_after_recipe_lifecycle_cutover(self) -> None:
        legacy_header = (
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "domain"
            / "include"
            / "domain"
            / "recipes"
            / "serialization"
            / "RecipeJsonSerializer.h"
        )
        legacy_impl = (
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "domain"
            / "domain"
            / "recipes"
            / "serialization"
            / "RecipeJsonSerializer.cpp"
        )
        self.assertFalse(legacy_header.exists(), msg="workflow recipe serializer header should be deleted after recipe-lifecycle cutover")
        self.assertFalse(legacy_impl.exists(), msg="workflow recipe serializer implementation should be deleted after recipe-lifecycle cutover")

    def test_recipe_lifecycle_owns_recipe_cutover(self) -> None:
        runtime_service_cmake = _read(WORKSPACE_ROOT / "apps" / "runtime-service" / "CMakeLists.txt")
        planner_cli_cmake = _read(WORKSPACE_ROOT / "apps" / "planner-cli" / "CMakeLists.txt")
        gateway_cmake = _read(WORKSPACE_ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "CMakeLists.txt")
        recipe_lifecycle_root_cmake = _read(WORKSPACE_ROOT / "modules" / "recipe-lifecycle" / "CMakeLists.txt")
        recipe_lifecycle_domain_cmake = _read(WORKSPACE_ROOT / "modules" / "recipe-lifecycle" / "domain" / "CMakeLists.txt")
        recipe_lifecycle_application_cmake = _read(
            WORKSPACE_ROOT / "modules" / "recipe-lifecycle" / "application" / "CMakeLists.txt"
        )
        recipe_lifecycle_adapters_cmake = _read(WORKSPACE_ROOT / "modules" / "recipe-lifecycle" / "adapters" / "CMakeLists.txt")

        self.assertIn("siligen_recipe_lifecycle_domain_public", runtime_service_cmake)
        self.assertIn("siligen_recipe_lifecycle_application_public", runtime_service_cmake)
        self.assertIn("siligen_recipe_lifecycle_serialization_public", runtime_service_cmake)
        self.assertIn("siligen_recipe_lifecycle_application_public", planner_cli_cmake)
        self.assertIn("siligen_recipe_lifecycle_application_public", gateway_cmake)
        self.assertIn("siligen_recipe_lifecycle_serialization_public", gateway_cmake)
        self.assertNotIn("runtime/recipes/RecipeJsonSerializer.cpp", runtime_service_cmake)
        self.assertNotIn("siligen_workflow_recipe_serialization_public", runtime_service_cmake)
        self.assertNotIn("siligen_workflow_recipe_serialization_public", gateway_cmake)
        self.assertNotIn("siligen_workflow_recipe_application_public", runtime_service_cmake)
        self.assertNotIn("siligen_workflow_recipe_application_public", planner_cli_cmake)
        self.assertNotIn("siligen_workflow_recipe_application_public", gateway_cmake)
        self.assertNotIn("SILIGEN_SHARED_COMPAT_INCLUDE_ROOT", recipe_lifecycle_root_cmake)
        self.assertNotIn("SILIGEN_SHARED_COMPAT_INCLUDE_ROOT", recipe_lifecycle_domain_cmake)
        self.assertNotIn("SILIGEN_SHARED_COMPAT_INCLUDE_ROOT", recipe_lifecycle_application_cmake)
        self.assertNotIn("SILIGEN_SHARED_COMPAT_INCLUDE_ROOT", recipe_lifecycle_adapters_cmake)

        serializer_header = (
            WORKSPACE_ROOT
            / "modules"
            / "recipe-lifecycle"
            / "adapters"
            / "include"
            / "recipe_lifecycle"
            / "adapters"
            / "serialization"
            / "RecipeJsonSerializer.h"
        )
        self.assertTrue(serializer_header.exists(), msg="recipe-lifecycle must expose RecipeJsonSerializer from adapters public surface")
        self.assertNotIn("domain/recipes/serialization/RecipeJsonSerializer.h", _read(serializer_header))

        expected_include = '#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"'
        for relative in (
            "apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp",
            "apps/runtime-service/runtime/recipes/RecipeBundleSerializer.h",
            "apps/runtime-service/runtime/recipes/RecipeFileRepository.cpp",
            "apps/runtime-service/runtime/recipes/ParameterSchemaFileProvider.cpp",
            "apps/runtime-service/runtime/recipes/TemplateFileRepository.cpp",
            "apps/runtime-service/runtime/recipes/AuditFileRepository.cpp",
        ):
            self.assertIn(expected_include, _read(WORKSPACE_ROOT / relative), msg=f"{relative} must include recipe-lifecycle serializer header")

    def test_workflow_unit_directory_is_registration_only(self) -> None:
        workflow_tests_unit = WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "unit"
        self.assertFalse(
            (workflow_tests_unit / "MotionOwnerBehaviorTest.cpp").exists(),
            msg="workflow/tests/unit must not keep foreign-owner motion source-bearing tests",
        )
        self.assertFalse(
            (workflow_tests_unit / "PlanningFailureSurfaceTest.cpp").exists(),
            msg="workflow/tests/unit must not keep source-bearing planning integration tests",
        )
        unit_cmake = _read(workflow_tests_unit / "CMakeLists.txt")
        self.assertNotIn("add_executable(", unit_cmake, msg="workflow/tests/unit must remain registration-only")

    def test_workflow_services_and_examples_are_shell_only(self) -> None:
        payload_suffixes = {".h", ".hpp", ".cpp", ".cc", ".cxx", ".py", ".ps1"}
        for relative in ("modules/workflow/services", "modules/workflow/examples"):
            payload = [
                path.relative_to(WORKSPACE_ROOT).as_posix()
                for path in (WORKSPACE_ROOT / relative).rglob("*")
                if path.is_file() and path.suffix in payload_suffixes
            ]
            self.assertFalse(payload, msg=f"{relative} must remain shell-only: {payload}")

    def test_root_entries_run_bridge_exit_gate(self) -> None:
        build_validation = _read(WORKSPACE_ROOT / "scripts" / "build" / "build-validation.ps1")
        local_gate = _read(WORKSPACE_ROOT / "scripts" / "validation" / "run-local-validation-gate.ps1")
        ci_entry = _read(WORKSPACE_ROOT / "ci.ps1")

        self.assertIn("validate_workspace_layout.py", build_validation)
        self.assertIn("legacy-exit-checks.py", local_gate)
        self.assertIn("legacy-exit-checks.py", ci_entry)

    def test_third_party_bootstrap_defaults_to_repo_tracked_bundles(self) -> None:
        bootstrap_script = _read(WORKSPACE_ROOT / "scripts" / "bootstrap" / "bootstrap-third-party.ps1")
        self.assertIn('Join-Path $PSScriptRoot "bundles"', bootstrap_script)

        manifest = json.loads(_read(WORKSPACE_ROOT / "scripts" / "bootstrap" / "third-party-manifest.json"))
        bundle_root = WORKSPACE_ROOT / "scripts" / "bootstrap" / "bundles"
        self.assertTrue(bundle_root.exists(), msg="scripts/bootstrap/bundles is required for fresh clones")
        for package in manifest["packages"]:
            if not package.get("required", False):
                continue
            archive_name = package["archiveFileName"]
            self.assertTrue((bundle_root / archive_name).exists(), msg=f"missing repo-tracked third-party bundle: {archive_name}")

    def test_multicard_vendor_sdk_assets_are_tracked_in_canonical_dir(self) -> None:
        vendor_root = WORKSPACE_ROOT / "modules" / "runtime-execution" / "adapters" / "device" / "vendor" / "multicard"
        self.assertTrue((vendor_root / "MultiCard.dll").exists(), msg="canonical vendor dir must track MultiCard.dll")
        self.assertTrue((vendor_root / "MultiCard.lib").exists(), msg="canonical vendor dir must track MultiCard.lib")


if __name__ == "__main__":
    unittest.main()
