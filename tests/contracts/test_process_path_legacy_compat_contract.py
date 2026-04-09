from __future__ import annotations

import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="ignore")


class ProcessPathLegacyCompatContractTest(unittest.TestCase):
    def test_decision_record_exists(self) -> None:
        decision = WORKSPACE_ROOT / "docs" / "decisions" / "process-path-legacy-compat-freeze.md"
        self.assertTrue(decision.exists(), msg="missing process-path legacy compat freeze decision record")
        text = _read(decision)
        self.assertIn("IDXFPathSourcePort", text)
        self.assertIn("modules/workflow/adapters/infrastructure/adapters/planning/dxf/", text)

    def test_live_runtime_bootstrap_does_not_register_dxf_compat_port(self) -> None:
        bindings_h = _read(WORKSPACE_ROOT / "apps" / "runtime-service" / "bootstrap" / "InfrastructureBindings.h")
        bindings_cpp = _read(
            WORKSPACE_ROOT / "apps" / "runtime-service" / "bootstrap" / "InfrastructureBindingsBuilder.cpp"
        )
        self.assertNotIn("dxf_path_source_port", bindings_h)
        self.assertNotIn("dxf_path_source_port", bindings_cpp)
        self.assertIn("PbPathSourceAdapter", bindings_cpp)

    def test_workflow_dxf_build_splits_canonical_and_legacy_targets(self) -> None:
        canonical_cmake = _read(
            WORKSPACE_ROOT / "modules" / "dxf-geometry" / "adapters" / "CMakeLists.txt"
        )
        self.assertIn("add_library(siligen_parsing_adapter STATIC", canonical_cmake)
        self.assertIn("dxf/PbPathSourceAdapter.cpp", canonical_cmake)
        self.assertIn("dxf/AutoPathSourceAdapter.cpp", canonical_cmake)
        self.assertIn("dxf/DXFAdapterFactory.cpp", canonical_cmake)

        workflow_bridge_dir = (
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "adapters"
            / "infrastructure"
            / "adapters"
            / "planning"
            / "dxf"
        )
        self.assertFalse(
            workflow_bridge_dir.exists(),
            msg=f"workflow dxf bridge dir should be removed: {workflow_bridge_dir}",
        )

    def test_duplicate_legacy_impl_dirs_are_absent(self) -> None:
        duplicate_dxf_adapter_dir = (
            WORKSPACE_ROOT
            / "modules"
            / "workflow"
            / "adapters"
            / "infrastructure"
            / "adapters"
            / "planning"
            / "geometry"
        )
        orphan_trajectory_dir = WORKSPACE_ROOT / "modules" / "workflow" / "domain" / "domain" / "trajectory"
        self.assertFalse(duplicate_dxf_adapter_dir.exists(), msg=f"workflow contour bridge dir still present: {duplicate_dxf_adapter_dir}")
        self.assertFalse(orphan_trajectory_dir.exists(), msg=f"orphan trajectory dir still present: {orphan_trajectory_dir}")


if __name__ == "__main__":
    unittest.main()
