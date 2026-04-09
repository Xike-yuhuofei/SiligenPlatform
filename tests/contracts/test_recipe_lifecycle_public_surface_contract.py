from __future__ import annotations

import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="ignore")


class RecipeLifecyclePublicSurfaceContractTest(unittest.TestCase):
    def test_recipe_lifecycle_live_cmake_targets_do_not_expose_shared_compat_root(self) -> None:
        for relative in (
            "modules/recipe-lifecycle/CMakeLists.txt",
            "modules/recipe-lifecycle/domain/CMakeLists.txt",
            "modules/recipe-lifecycle/application/CMakeLists.txt",
            "modules/recipe-lifecycle/adapters/CMakeLists.txt",
        ):
            self.assertNotIn(
                "SILIGEN_SHARED_COMPAT_INCLUDE_ROOT",
                _read(WORKSPACE_ROOT / relative),
                msg=f"{relative} must not expose shared compat include root in the live recipe-lifecycle surface",
            )

    def test_recipe_lifecycle_public_targets_remain_the_canonical_recipe_surface(self) -> None:
        root_cmake = _read(WORKSPACE_ROOT / "modules/recipe-lifecycle/CMakeLists.txt")
        runtime_service_cmake = _read(WORKSPACE_ROOT / "apps/runtime-service/CMakeLists.txt")
        planner_cli_cmake = _read(WORKSPACE_ROOT / "apps/planner-cli/CMakeLists.txt")
        gateway_cmake = _read(WORKSPACE_ROOT / "apps/runtime-gateway/transport-gateway/CMakeLists.txt")

        self.assertIn("siligen_recipe_lifecycle_domain_public", root_cmake)
        self.assertIn("siligen_recipe_lifecycle_application_public", root_cmake)
        self.assertIn("siligen_recipe_lifecycle_serialization_public", root_cmake)

        self.assertIn("siligen_recipe_lifecycle_domain_public", runtime_service_cmake)
        self.assertIn("siligen_recipe_lifecycle_application_public", runtime_service_cmake)
        self.assertIn("siligen_recipe_lifecycle_serialization_public", runtime_service_cmake)
        self.assertIn("siligen_recipe_lifecycle_application_public", planner_cli_cmake)
        self.assertIn("siligen_recipe_lifecycle_application_public", gateway_cmake)
        self.assertIn("siligen_recipe_lifecycle_serialization_public", gateway_cmake)


if __name__ == "__main__":
    unittest.main()
