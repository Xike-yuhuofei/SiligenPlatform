from __future__ import annotations

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


if __name__ == "__main__":
    unittest.main()
