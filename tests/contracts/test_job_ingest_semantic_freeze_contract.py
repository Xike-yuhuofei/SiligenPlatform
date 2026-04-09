from __future__ import annotations

import re
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
LIVE_SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx", ".py", ".ps1", ".cmake"}


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="ignore")


def _iter_live_source_files(root: Path):
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if path.name == "CMakeLists.txt" or path.suffix in LIVE_SOURCE_SUFFIXES:
            yield path


def _matches(root: Path, needle: str) -> list[Path]:
    hits: list[Path] = []
    current_test_file = Path(__file__).resolve()
    for path in _iter_live_source_files(root):
        if path.resolve() == current_test_file:
            continue
        if needle in _read(path):
            hits.append(path)
    return hits


def _extract_allowed_dependencies(module_yaml_text: str) -> list[str]:
    dependencies: list[str] = []
    in_section = False
    for line in module_yaml_text.splitlines():
        if line.startswith("allowed_dependencies:"):
            in_section = True
            continue
        if not in_section:
            continue
        if line and not line.startswith(" ") and not line.startswith("\t"):
            break
        match = re.match(r"^\s*-\s+(.+?)\s*$", line)
        if match:
            dependencies.append(match.group(1))
    return dependencies


class JobIngestSemanticFreezeContractTest(unittest.TestCase):
    def test_decision_record_exists_and_freezes_upload_centric_owner(self) -> None:
        decision = WORKSPACE_ROOT / "docs" / "decisions" / "job-ingest-upload-semantic-freeze.md"
        self.assertTrue(decision.exists(), msg="missing job-ingest semantic freeze decision record")
        text = _read(decision)
        self.assertIn("Job-Ingest Upload Semantic Freeze", text)
        self.assertIn("UploadResponse", text)
        self.assertIn("Siligen::JobIngest::Contracts", text)
        self.assertIn("IUploadStoragePort", text)
        self.assertIn("IUploadPreparationPort", text)

    def test_module_metadata_and_docs_freeze_upload_response_authority(self) -> None:
        module_yaml = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "module.yaml")
        self.assertIn("owner_artifact: UploadResponse", module_yaml)
        self.assertEqual(["shared"], _extract_allowed_dependencies(module_yaml))
        self.assertIn("authority fact", module_yaml)
        self.assertIn("IUploadStoragePort", module_yaml)
        self.assertIn("IUploadPreparationPort", module_yaml)

        readme = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "README.md")
        self.assertIn("authority artifact", readme)
        self.assertIn("`UploadResponse`", readme)
        self.assertIn("`UploadRequest` 是 ingress 输入 DTO", readme)

        contracts_readme = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "contracts" / "README.md")
        self.assertIn("authority artifact 固定为 `UploadResponse`", contracts_readme)
        self.assertIn("Siligen::JobIngest::Contracts", contracts_readme)

        application_readme = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "application" / "README.md")
        self.assertIn("`UploadResponse` 是本模块 authority artifact", application_readme)
        self.assertIn("IUploadStoragePort", application_readme)
        self.assertIn("IUploadPreparationPort", application_readme)

        tests_readme = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "tests" / "README.md")
        self.assertIn("tests/contracts/test_job_ingest_semantic_freeze_contract.py", tests_readme)

    def test_public_contract_and_application_surface_are_canonical(self) -> None:
        contracts_header = _read(
            WORKSPACE_ROOT
            / "modules"
            / "job-ingest"
            / "contracts"
            / "include"
            / "job_ingest"
            / "contracts"
            / "dispensing"
            / "UploadContracts.h"
        )
        self.assertIn("namespace Siligen::JobIngest::Contracts", contracts_header)
        self.assertNotIn("namespace Siligen::Application::UseCases::Dispensing", contracts_header)

        ports_header = _read(
            WORKSPACE_ROOT
            / "modules"
            / "job-ingest"
            / "application"
            / "include"
            / "job_ingest"
            / "application"
            / "ports"
            / "dispensing"
            / "UploadPorts.h"
        )
        self.assertIn("class IUploadStoragePort", ports_header)
        self.assertIn("class IUploadPreparationPort", ports_header)

        use_case_header = _read(
            WORKSPACE_ROOT
            / "modules"
            / "job-ingest"
            / "application"
            / "include"
            / "job_ingest"
            / "application"
            / "usecases"
            / "dispensing"
            / "UploadFileUseCase.h"
        )
        self.assertIn("IUploadStoragePort", use_case_header)
        self.assertIn("IUploadPreparationPort", use_case_header)
        for forbidden in ("IConfigurationPort", "IFileStoragePort", "DxfPbPreparationService"):
            self.assertNotIn(forbidden, use_case_header)

    def test_job_ingest_build_graph_is_shared_only_and_keeps_canonical_roots(self) -> None:
        cmake = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "CMakeLists.txt")
        self.assertIn("job_ingest/application", cmake)
        self.assertIn("siligen_job_ingest_contracts", cmake)
        self.assertIn(
            'SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/job-ingest/application;modules/job-ingest/tests"',
            cmake,
        )
        for forbidden in (
            "siligen_process_planning_domain_configuration",
            "siligen_process_planning_contracts_public",
            "siligen_dxf_geometry_application_public",
        ):
            self.assertNotIn(forbidden, cmake)

        contracts_cmake = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "contracts" / "CMakeLists.txt")
        self.assertIn("siligen_job_ingest_contracts", contracts_cmake)

    def test_legacy_wrapper_and_duplicate_storage_contract_are_absent(self) -> None:
        legacy_wrapper = (
            WORKSPACE_ROOT
            / "modules"
            / "job-ingest"
            / "application"
            / "include"
            / "application"
            / "usecases"
            / "dispensing"
            / "UploadFileUseCase.h"
        )
        duplicate_storage_contract = (
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
        self.assertFalse(legacy_wrapper.exists(), msg="legacy UploadFileUseCase wrapper must stay deleted")
        self.assertFalse(duplicate_storage_contract.exists(), msg="duplicate job-ingest storage contract must stay deleted")

    def test_runtime_service_keeps_foreign_adaptation_only_at_composition_root(self) -> None:
        container = _read(
            WORKSPACE_ROOT / "apps" / "runtime-service" / "container" / "ApplicationContainer.Dispensing.cpp"
        )
        self.assertIn("class RuntimeUploadStorageAdapter final : public IUploadStoragePort", container)
        self.assertIn("class DxfPreparationAdapter final : public IUploadPreparationPort", container)
        self.assertIn("std::make_shared<RuntimeUploadStorageAdapter>", container)
        self.assertIn("std::make_shared<DxfPreparationAdapter>", container)
        self.assertIn("Resolve<IUploadFilePort>()", container)
        self.assertIn("service_.CleanupPreparedInput(source_path)", container)

    def test_storage_owner_migrates_to_runtime_bootstrap_quarantine_surface(self) -> None:
        runtime_storage_header = (
            WORKSPACE_ROOT
            / "apps"
            / "runtime-service"
            / "include"
            / "runtime_process_bootstrap"
            / "storage"
            / "ports"
            / "IFileStoragePort.h"
        )
        self.assertTrue(runtime_storage_header.exists(), msg="runtime bootstrap storage seam must exist")
        header_text = _read(runtime_storage_header)
        self.assertIn("class IFileStoragePort", header_text)
        self.assertIn("runtime-service bootstrap app-local", header_text)

        self.assertFalse(
            (
                WORKSPACE_ROOT
                / "modules"
                / "process-planning"
                / "domain"
                / "configuration"
                / "ports"
                / "IFileStoragePort.h"
            ).exists(),
            msg="process-planning storage seam should stay removed",
        )
        self.assertFalse(
            (
                WORKSPACE_ROOT
                / "modules"
                / "workflow"
                / "domain"
                / "include"
                / "domain"
                / "configuration"
                / "ports"
                / "IFileStoragePort.h"
            ).exists(),
            msg="workflow stale storage seam should stay removed",
        )

        runtime_readme = _read(WORKSPACE_ROOT / "apps" / "runtime-service" / "README.md")
        self.assertIn("runtime_process_bootstrap/storage/ports/IFileStoragePort.h", runtime_readme)

    def test_direct_consumers_use_canonical_job_ingest_contracts(self) -> None:
        workflow_header = _read(
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "application"
            / "include"
            / "application"
            / "phase-control"
            / "DispensingWorkflowUseCase.h"
        )
        self.assertIn("using Siligen::JobIngest::Contracts::IUploadFilePort;", workflow_header)
        self.assertIn("using Siligen::JobIngest::Contracts::UploadRequest;", workflow_header)
        self.assertIn("using Siligen::JobIngest::Contracts::UploadResponse;", workflow_header)

        gateway_header = _read(
            WORKSPACE_ROOT
            / "apps"
            / "runtime-gateway"
            / "transport-gateway"
            / "src"
            / "facades"
            / "tcp"
            / "TcpDispensingFacade.h"
        )
        self.assertIn("using Siligen::JobIngest::Contracts::IUploadFilePort;", gateway_header)
        self.assertIn("using Siligen::JobIngest::Contracts::UploadRequest;", gateway_header)
        self.assertIn("using Siligen::JobIngest::Contracts::UploadResponse;", gateway_header)

        planner_cli = _read(WORKSPACE_ROOT / "apps" / "planner-cli" / "CommandHandlers.Dxf.cpp")
        self.assertIn("Siligen::JobIngest::Contracts::UploadRequest upload_request;", planner_cli)

    def test_live_source_no_longer_references_legacy_upload_seam(self) -> None:
        scan_roots = [
            WORKSPACE_ROOT / "apps" / "runtime-service",
            WORKSPACE_ROOT / "apps" / "runtime-gateway",
            WORKSPACE_ROOT / "apps" / "planner-cli",
            WORKSPACE_ROOT / "modules" / "job-ingest",
            WORKSPACE_ROOT / "modules" / "workflow",
            WORKSPACE_ROOT / "tests",
        ]
        forbidden_needles = [
            "Siligen::Application::UseCases::Dispensing::UploadRequest",
            "Siligen::Application::UseCases::Dispensing::UploadResponse",
            "Siligen::Application::UseCases::Dispensing::IUploadFilePort",
            '#include "application/usecases/dispensing/UploadFileUseCase.h"',
            '#include "job_ingest/contracts/storage/IFileStoragePort.h"',
            '#include "ports/IFileStoragePort.h"',
        ]
        for root in scan_roots:
            for needle in forbidden_needles:
                self.assertEqual(
                    _matches(root, needle),
                    [],
                    msg=f"{root} still references forbidden legacy upload seam: {needle}",
                )

        runtime_storage_hits = _matches(
            WORKSPACE_ROOT / "apps" / "runtime-service",
            '#include "runtime_process_bootstrap/storage/ports/IFileStoragePort.h"',
        )
        self.assertNotEqual(runtime_storage_hits, [], msg="runtime-service should consume runtime bootstrap storage seam")

    def test_regression_lane_remains_skeleton_only(self) -> None:
        tests_cmake = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "tests" / "CMakeLists.txt")
        tests_readme = _read(WORKSPACE_ROOT / "modules" / "job-ingest" / "tests" / "README.md")
        regression_readme = _read(
            WORKSPACE_ROOT / "modules" / "job-ingest" / "tests" / "regression" / "README.md"
        )

        self.assertNotIn("regression", tests_cmake)
        self.assertIn("当前不进入 live suite", tests_readme)
        self.assertIn("当前不承载 source-bearing 测试", regression_readme)
        self.assertIn("当前不注册 `job-ingest` regression target", regression_readme)


if __name__ == "__main__":
    unittest.main()
