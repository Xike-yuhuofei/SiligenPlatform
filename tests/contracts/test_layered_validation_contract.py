from __future__ import annotations

import sys
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.validation_layers import EXECUTION_LANES, VALIDATION_LAYERS, build_request, route_validation_request


class LayeredValidationContractTest(unittest.TestCase):
    def test_layer_and_lane_catalogs_are_frozen(self) -> None:
        self.assertEqual(
            list(VALIDATION_LAYERS.keys()),
            [
                "L0",
                "L1",
                "L2",
                "L3",
                "L4",
                "L5",
                "L6",
            ],
        )
        self.assertEqual(
            list(EXECUTION_LANES.keys()),
            ["quick-gate", "full-offline-gate", "nightly-performance", "limited-hil"],
        )

    def test_quick_request_routes_to_quick_gate(self) -> None:
        request = build_request(
            requested_suites=["apps", "contracts", "protocol-compatibility"],
            changed_scopes=["shared/testing", "tests/integration"],
            risk_profile="medium",
            desired_depth="quick",
        )
        routed = route_validation_request(request)
        self.assertEqual(routed.selected_lane_ref, "quick-gate")
        self.assertEqual(
            routed.selected_layer_refs,
            ("L0", "L1", "L2"),
        )
        self.assertEqual(routed.skipped_layer_refs, ())

    def test_hil_request_routes_to_limited_hil_with_offline_prereq(self) -> None:
        request = build_request(
            requested_suites=["e2e"],
            changed_scopes=["runtime-execution"],
            risk_profile="hardware-sensitive",
            desired_depth="hil",
            include_hardware_smoke=True,
            include_hil_closed_loop=True,
        )
        routed = route_validation_request(request)
        self.assertEqual(routed.selected_lane_ref, "limited-hil")
        self.assertIn("L2", routed.selected_layer_refs)
        self.assertIn("L3", routed.selected_layer_refs)
        self.assertIn("L4", routed.selected_layer_refs)
        self.assertIn("L5", routed.selected_layer_refs)
        self.assertEqual(routed.upgrade_recommendation, "limited-hil")

    def test_skip_layer_requires_justification(self) -> None:
        request = build_request(
            requested_suites=["contracts"],
            changed_scopes=["shared/testing"],
            skip_layers=["L2"],
        )
        with self.assertRaises(ValueError):
            route_validation_request(request)

    def test_root_entries_surface_lane_flags(self) -> None:
        for relative in ("build.ps1", "test.ps1", "ci.ps1", "scripts/validation/invoke-workspace-tests.ps1"):
            text = (WORKSPACE_ROOT / relative).read_text(encoding="utf-8")
            self.assertIn("Lane", text, msg=f"{relative} must expose lane parameter")
            self.assertTrue(
                ("RiskProfile" in text) and ("DesiredDepth" in text),
                msg=f"{relative} must expose risk/depth parameter",
            )


if __name__ == "__main__":
    unittest.main()
