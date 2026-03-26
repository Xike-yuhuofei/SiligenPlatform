from __future__ import annotations

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

    def test_shared_contracts_resolve_runtime_device_contracts_from_canonical_root(self) -> None:
        shared_contracts = _read(WORKSPACE_ROOT / "shared" / "contracts" / "CMakeLists.txt")
        self.assertIn("modules/runtime-execution/contracts/device", shared_contracts)
        self.assertNotIn("modules/runtime-execution/device-contracts", shared_contracts)

    def test_root_entries_run_bridge_exit_gate(self) -> None:
        build_validation = _read(WORKSPACE_ROOT / "scripts" / "build" / "build-validation.ps1")
        local_gate = _read(WORKSPACE_ROOT / "scripts" / "validation" / "run-local-validation-gate.ps1")
        ci_entry = _read(WORKSPACE_ROOT / "ci.ps1")

        self.assertIn("validate_workspace_layout.py", build_validation)
        self.assertIn("legacy-exit-checks.py", local_gate)
        self.assertIn("legacy-exit-checks.py", ci_entry)


if __name__ == "__main__":
    unittest.main()
