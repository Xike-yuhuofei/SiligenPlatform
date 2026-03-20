import importlib.util
import os
import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
WORKSPACE_ROOT = PROJECT_ROOT.parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))
MODULE_PATH = PROJECT_ROOT / "src" / "hmi_client" / "ui" / "dxf_default_paths.py"
MODULE_SPEC = importlib.util.spec_from_file_location("hmi_client.ui.dxf_default_paths", MODULE_PATH)
assert MODULE_SPEC is not None
assert MODULE_SPEC.loader is not None
DXF_DEFAULT_PATHS = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(DXF_DEFAULT_PATHS)
build_default_dxf_candidates = DXF_DEFAULT_PATHS.build_default_dxf_candidates


class _EnvGuard:
    def __init__(self, **updates: str | None) -> None:
        self._updates = updates
        self._original: dict[str, str | None] = {}

    def __enter__(self):
        for key, value in self._updates.items():
            self._original[key] = os.environ.get(key)
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value
        return self

    def __exit__(self, exc_type, exc, tb):
        for key, value in self._original.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


class DefaultDxfCandidateTest(unittest.TestCase):
    def test_uses_workspace_canonical_candidates_only(self) -> None:
        legacy_dxf_root = (WORKSPACE_ROOT / ("control" + "-core") / "uploads" / "dxf").resolve()
        with _EnvGuard(
            SILIGEN_WORKSPACE_ROOT=None,
            SILIGEN_DXF_DEFAULT_DIR=None,
        ):
            candidates = build_default_dxf_candidates(
                PROJECT_ROOT / "src" / "hmi_client" / "ui" / "main_window.py"
            )

        self.assertEqual(
            candidates,
            [
                (WORKSPACE_ROOT / "uploads" / "dxf").resolve(),
                (
                    WORKSPACE_ROOT
                    / "packages"
                    / "engineering-contracts"
                    / "fixtures"
                    / "cases"
                    / "rect_diag"
                ).resolve(),
            ],
        )
        self.assertNotIn(legacy_dxf_root, candidates)

    def test_explicit_default_dir_override_has_highest_priority(self) -> None:
        override = (PROJECT_ROOT / "tmp" / "dxf-override").resolve()
        with _EnvGuard(
            SILIGEN_WORKSPACE_ROOT=None,
            SILIGEN_DXF_DEFAULT_DIR=str(override),
        ):
            candidates = build_default_dxf_candidates(
                PROJECT_ROOT / "src" / "hmi_client" / "ui" / "main_window.py"
            )

        self.assertGreaterEqual(len(candidates), 1)
        self.assertEqual(candidates[0], override)
