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


if __name__ == "__main__":
    unittest.main()
