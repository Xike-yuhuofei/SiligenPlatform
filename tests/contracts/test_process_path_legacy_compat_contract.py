from __future__ import annotations

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
    for path in _iter_live_source_files(root):
        if needle in _read(path):
            hits.append(path)
    return hits


class ProcessPathContractSurfaceTest(unittest.TestCase):
    def test_closeout_decision_record_exists(self) -> None:
        decision = WORKSPACE_ROOT / "docs" / "decisions" / "process-path-legacy-compat-freeze.md"
        self.assertTrue(decision.exists(), msg="missing process-path closeout decision record")
        text = _read(decision)
        self.assertIn("Process-Path Legacy Compat Closeout", text)
        self.assertIn("deleted", text)
        self.assertIn("migrated", text)
        self.assertIn("Siligen::ProcessPath::Contracts", text)

    def test_deleted_process_path_bridge_and_dxf_legacy_headers_are_absent(self) -> None:
        deleted_paths = [
            WORKSPACE_ROOT / "modules" / "process-path" / "contracts" / "include" / "process_path" / "contracts" / "IDXFPathSourcePort.h",
            WORKSPACE_ROOT / "modules" / "process-path" / "domain" / "trajectory" / "ports" / "IPathSourcePort.h",
            WORKSPACE_ROOT / "modules" / "process-path" / "domain" / "trajectory" / "ports" / "IDXFPathSourcePort.h",
        ]
        for path in deleted_paths:
            self.assertFalse(path.exists(), msg=f"deleted legacy surface still present: {path}")

    def test_canonical_i_path_source_contract_uses_process_path_namespace(self) -> None:
        header = WORKSPACE_ROOT / "modules" / "process-path" / "contracts" / "include" / "process_path" / "contracts" / "IPathSourcePort.h"
        text = _read(header)
        self.assertIn("namespace Siligen::ProcessPath::Contracts", text)
        self.assertNotIn("namespace Siligen::Domain::Trajectory::Ports", text)

    def test_live_source_no_longer_references_old_process_path_seam(self) -> None:
        scan_roots = [
            WORKSPACE_ROOT / "apps" / "runtime-service",
            WORKSPACE_ROOT / "modules" / "process-path",
            WORKSPACE_ROOT / "modules" / "workflow",
            WORKSPACE_ROOT / "modules" / "dispense-packaging",
            WORKSPACE_ROOT / "modules" / "dxf-geometry",
            WORKSPACE_ROOT / "modules" / "motion-planning",
        ]
        forbidden_needles = [
            "Siligen::Domain::Trajectory::Ports::",
            '#include "domain/trajectory/ports/IPathSourcePort.h"',
            '#include "domain/trajectory/ports/IDXFPathSourcePort.h"',
            '#include "process_path/contracts/IDXFPathSourcePort.h"',
            "topology_repair.enable",
        ]
        for root in scan_roots:
            for needle in forbidden_needles:
                self.assertEqual(
                    _matches(root, needle),
                    [],
                    msg=f"{root} still references forbidden process-path legacy seam: {needle}",
                )

    def test_runtime_service_resolves_canonical_path_source_port(self) -> None:
        bootstrap_cpp = _read(WORKSPACE_ROOT / "apps" / "runtime-service" / "bootstrap" / "ContainerBootstrap.cpp")
        container_cpp = _read(
            WORKSPACE_ROOT / "apps" / "runtime-service" / "container" / "ApplicationContainer.Dispensing.cpp"
        )
        self.assertIn("Siligen::ProcessPath::Contracts::IPathSourcePort", bootstrap_cpp)
        self.assertIn("ResolvePort<ProcessPath::Contracts::IPathSourcePort>()", container_cpp)

    def test_process_path_docs_do_not_admit_legacy_live_surface(self) -> None:
        readme = _read(WORKSPACE_ROOT / "modules" / "process-path" / "README.md")
        contracts_readme = _read(WORKSPACE_ROOT / "modules" / "process-path" / "contracts" / "README.md")
        self.assertNotIn("IDXFPathSourcePort", readme)
        self.assertNotIn("兼容桥接", readme)
        self.assertNotIn("IDXFPathSourcePort", contracts_readme)
        self.assertNotIn("兼容桥接", contracts_readme)

    def test_module_boundary_audit_models_canonical_process_path_surface(self) -> None:
        boundary_model = _read(
            WORKSPACE_ROOT / "scripts" / "validation" / "boundaries" / "module-boundaries.json"
        )
        policy = _read(
            WORKSPACE_ROOT / "scripts" / "validation" / "boundaries" / "boundary-policy.json"
        )
        runner = _read(
            WORKSPACE_ROOT / "scripts" / "validation" / "invoke-module-boundary-audit.ps1"
        )
        self.assertIn("process_path/contracts/IPathSourcePort.h", boundary_model)
        self.assertIn("modules/process-path/contracts/include/process_path/contracts/IDXFPathSourcePort.h", policy)
        self.assertIn("module_boundary_audit.py", runner)
        self.assertNotIn("process_path/contracts/IDXFPathSourcePort.h instead of", boundary_model + policy)


if __name__ == "__main__":
    unittest.main()
