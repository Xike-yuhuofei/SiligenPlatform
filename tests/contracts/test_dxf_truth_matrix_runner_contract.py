import importlib.util
import sys
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.dxf_truth_matrix import full_chain_case_ids


def _load_module(module_name: str, relative_path: str):
    module_path = WORKSPACE_ROOT / relative_path
    spec = importlib.util.spec_from_file_location(module_name, module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"failed to load module: {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


class DxfTruthMatrixRunnerContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.engineering_runner = _load_module(
            "engineering_regression_runner_contract",
            "tests/integration/scenarios/run_engineering_regression.py",
        )
        cls.simulated_line_runner = _load_module(
            "simulated_line_runner_contract",
            "tests/e2e/simulated-line/run_simulated_line.py",
        )
        cls.hil_runner = _load_module(
            "hil_closed_loop_runner_contract",
            "tests/e2e/hardware-in-loop/run_hil_closed_loop.py",
        )

    def test_engineering_regression_uses_full_chain_truth_matrix_order(self) -> None:
        selected = self.engineering_runner._selected_cases([])
        self.assertEqual(
            tuple(case.case_id for case in selected),
            full_chain_case_ids(WORKSPACE_ROOT),
        )

    def test_simulated_line_compat_scenarios_cover_all_full_chain_cases(self) -> None:
        compat_scenarios = self.simulated_line_runner._compat_scenarios()
        self.assertEqual(
            tuple(item.name for item in compat_scenarios),
            ("compat_rect_diag", "compat_bra", "compat_arc_circle_quadrants"),
        )

    def test_hil_dxf_selection_supports_default_and_case_id(self) -> None:
        default_case_id, default_path, default_assets = self.hil_runner._resolve_dxf_selection(None, None)
        self.assertEqual(default_case_id, "rect_diag")
        self.assertEqual(default_path.name, "rect_diag.dxf")
        self.assertEqual(default_assets, ("sample.dxf.rect_diag",))

        bra_case_id, bra_path, bra_assets = self.hil_runner._resolve_dxf_selection("bra", None)
        self.assertEqual(bra_case_id, "bra")
        self.assertEqual(bra_path.name, "bra.dxf")
        self.assertEqual(bra_assets, ("sample.dxf.bra",))

        inferred_case_id, _, inferred_assets = self.hil_runner._resolve_dxf_selection(
            None,
            WORKSPACE_ROOT / "samples" / "dxf" / "arc_circle_quadrants.dxf",
        )
        self.assertEqual(inferred_case_id, "arc_circle_quadrants")
        self.assertEqual(inferred_assets, ("sample.dxf.arc_circle_quadrants",))


if __name__ == "__main__":
    unittest.main()
