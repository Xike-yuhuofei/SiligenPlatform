from __future__ import annotations

import re
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="ignore")


class TraceDiagnosticsPublicSurfaceContractTest(unittest.TestCase):
    def test_trace_diagnostics_live_cmake_targets_do_not_expose_shared_compat_root(self) -> None:
        for relative in (
            "modules/trace-diagnostics/CMakeLists.txt",
            "modules/trace-diagnostics/application/CMakeLists.txt",
            "modules/trace-diagnostics/contracts/CMakeLists.txt",
        ):
            self.assertNotIn(
                "SILIGEN_SHARED_COMPAT_INCLUDE_ROOT",
                _read(WORKSPACE_ROOT / relative),
                msg=f"{relative} must not expose shared compat include root in the live trace-diagnostics surface",
            )

    def test_trace_diagnostics_module_root_keeps_application_and_contracts_as_public_surface(self) -> None:
        root_cmake = _read(WORKSPACE_ROOT / "modules/trace-diagnostics/CMakeLists.txt")
        application_cmake = _read(WORKSPACE_ROOT / "modules/trace-diagnostics/application/CMakeLists.txt")
        contracts_cmake = _read(WORKSPACE_ROOT / "modules/trace-diagnostics/contracts/CMakeLists.txt")

        self.assertIn("siligen_trace_diagnostics_application_public", root_cmake)
        self.assertIn("siligen_trace_diagnostics_contracts_public", root_cmake)
        self.assertRegex(
            application_cmake,
            r"target_link_libraries\s*\(\s*siligen_trace_diagnostics_application_public\s+INTERFACE\s+\$<LINK_ONLY:siligen_trace_diagnostics_application>",
        )
        self.assertIn("target_include_directories(siligen_trace_diagnostics_contracts_public INTERFACE", contracts_cmake)
        self.assertNotIn("$<LINK_ONLY:siligen_spdlog_adapter>", application_cmake)
        self.assertNotRegex(
            root_cmake,
            r"target_link_libraries\s*\(\s*siligen_module_trace_diagnostics\s+INTERFACE\s+siligen_spdlog_adapter",
        )

    def test_trace_diagnostics_module_metadata_keeps_application_contract_truth(self) -> None:
        module_yaml = _read(WORKSPACE_ROOT / "modules/trace-diagnostics/module.yaml")

        self.assertIn("canonical application/contracts surface", module_yaml)
        self.assertIn("adapter 不对外暴露", module_yaml)
        self.assertNotIn("canonical adapters", module_yaml)


if __name__ == "__main__":
    unittest.main()
