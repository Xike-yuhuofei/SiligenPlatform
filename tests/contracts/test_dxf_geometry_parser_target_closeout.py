from __future__ import annotations

import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="ignore")


class DxfGeometryParserTargetCloseoutTest(unittest.TestCase):
    def test_owner_scoped_parser_target_replaces_legacy_target_definition(self) -> None:
        adapters_cmake = _read(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "adapters" / "CMakeLists.txt")
        module_cmake = _read(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "CMakeLists.txt")

        self.assertIn("siligen_dxf_geometry_pb_path_source_adapter", adapters_cmake)
        self.assertNotIn("siligen_parsing_adapter", adapters_cmake)
        self.assertIn("siligen_dxf_geometry_pb_path_source_adapter", module_cmake)
        self.assertNotIn("siligen_parsing_adapter", module_cmake)

    def test_runtime_service_consumes_module_root_not_legacy_parser_target(self) -> None:
        runtime_service_cmake = _read(WORKSPACE_ROOT / "apps" / "runtime-service" / "CMakeLists.txt")

        self.assertIn("siligen_module_dxf_geometry", runtime_service_cmake)
        self.assertNotIn("siligen_parsing_adapter", runtime_service_cmake)
        self.assertNotIn("siligen_dxf_geometry_application_public", runtime_service_cmake)

    def test_known_test_consumers_no_longer_reference_legacy_parser_target(self) -> None:
        expected_new_target_paths = [
            WORKSPACE_ROOT / "modules" / "dxf-geometry" / "tests" / "CMakeLists.txt",
            WORKSPACE_ROOT / "modules" / "motion-planning" / "tests" / "CMakeLists.txt",
            WORKSPACE_ROOT / "modules" / "process-path" / "tests" / "CMakeLists.txt",
        ]
        no_direct_parser_dependency_paths = [
            WORKSPACE_ROOT / "modules" / "workflow" / "tests" / "integration" / "CMakeLists.txt",
        ]

        for path in expected_new_target_paths:
            text = _read(path)
            self.assertIn(
                "siligen_dxf_geometry_pb_path_source_adapter",
                text,
                msg=f"{path} must link the canonical parser target",
            )
            self.assertNotIn(
                "siligen_parsing_adapter",
                text,
                msg=f"{path} must not reference the deleted parser target",
            )

        for path in no_direct_parser_dependency_paths:
            self.assertNotIn(
                "siligen_parsing_adapter",
                _read(path),
                msg=f"{path} must not retain a legacy parser-target dependency",
            )

    def test_live_docs_admit_only_canonical_parser_target_truth(self) -> None:
        readme = _read(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "README.md")
        module_yaml = _read(WORKSPACE_ROOT / "modules" / "dxf-geometry" / "module.yaml")

        self.assertIn("siligen_dxf_geometry_pb_path_source_adapter", readme)
        self.assertIn("siligen_dxf_geometry_pb_path_source_adapter", module_yaml)
        self.assertNotIn("siligen_parsing_adapter", readme)
        self.assertNotIn("siligen_parsing_adapter", module_yaml)


if __name__ == "__main__":
    unittest.main()
