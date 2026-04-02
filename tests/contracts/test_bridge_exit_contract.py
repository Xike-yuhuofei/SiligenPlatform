from __future__ import annotations

import json
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


def _read(path: Path | str) -> str:
    resolved = path if isinstance(path, Path) else WORKSPACE_ROOT / path
    return resolved.read_text(encoding="utf-8-sig", errors="ignore")


def _source_files(*roots: str) -> list[Path]:
    files: list[Path] = []
    for root in roots:
        base = WORKSPACE_ROOT / root
        for pattern in ("*.h", "*.cpp"):
            files.extend(base.rglob(pattern))
    return files


class BridgeExitContractTest(unittest.TestCase):
    def test_legacy_fact_catalog_has_no_active_exceptions(self) -> None:
        active = [item.exception_id for item in LEGACY_FACT_CATALOG.active_exceptions()]
        self.assertFalse(active, msg=f"active legacy exceptions are not allowed after closeout: {active}")

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
        self.assertIn("siligen_runtime_execution_application_public", workflow_application)
        self.assertNotIn("../../runtime-execution/application/include", workflow_application)

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


if __name__ == "__main__":
    unittest.main()
