from __future__ import annotations

import sys
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.validation_layers import (
    EXECUTION_LANES,
    SUITE_TAXONOMY,
    build_request,
    lane_policy_metadata,
    route_validation_request,
)


class LayeredValidationLanePolicyContractTest(unittest.TestCase):
    def test_lane_policy_matrix_is_frozen(self) -> None:
        self.assertEqual(lane_policy_metadata("quick-gate")["gate_decision"], "blocking")
        self.assertEqual(lane_policy_metadata("quick-gate")["default_fail_policy"], "fail-fast")
        self.assertEqual(lane_policy_metadata("quick-gate")["fail_fast_case_limit"], 1)

        self.assertEqual(lane_policy_metadata("full-offline-gate")["gate_decision"], "blocking")
        self.assertEqual(lane_policy_metadata("full-offline-gate")["retry_budget"], 1)

        self.assertEqual(lane_policy_metadata("nightly-performance")["gate_decision"], "blocking")
        self.assertEqual(lane_policy_metadata("nightly-performance")["default_fail_policy"], "collect-and-report")

        self.assertEqual(lane_policy_metadata("limited-hil")["gate_decision"], "blocking")
        self.assertTrue(lane_policy_metadata("limited-hil")["human_approval_required"])

    def test_suite_taxonomy_is_frozen(self) -> None:
        self.assertEqual(
            list(SUITE_TAXONOMY.keys()),
            ["apps", "contracts", "protocol-compatibility", "e2e", "performance"],
        )
        self.assertEqual(SUITE_TAXONOMY["contracts"].default_size_label, "small")
        self.assertEqual(SUITE_TAXONOMY["protocol-compatibility"].default_size_label, "medium")
        self.assertEqual(SUITE_TAXONOMY["e2e"].default_size_label, "large")
        self.assertIn("suite:performance", SUITE_TAXONOMY["performance"].label_refs)

    def test_routed_metadata_surfaces_lane_policy_and_suite_labels(self) -> None:
        nightly_request = build_request(
            requested_suites=["performance"],
            changed_scopes=["tests/performance"],
            risk_profile="medium",
            desired_depth="nightly",
        )
        nightly_routed = route_validation_request(nightly_request)
        nightly_metadata = nightly_routed.to_metadata()
        self.assertEqual(nightly_metadata["selected_lane_ref"], "nightly-performance")
        self.assertEqual(nightly_metadata["selected_lane_gate_decision"], "blocking")
        self.assertEqual(nightly_metadata["selected_lane_retry_budget"], 1)
        self.assertIn("suite:performance", nightly_metadata["requested_suite_label_refs"])
        self.assertIn("size:large", nightly_metadata["requested_suite_label_refs"])

        quick_request = build_request(
            requested_suites=["contracts", "protocol-compatibility"],
            changed_scopes=["shared/testing"],
            risk_profile="medium",
            desired_depth="quick",
        )
        quick_routed = route_validation_request(quick_request)
        self.assertEqual(quick_routed.to_metadata()["selected_lane_default_fail_policy"], "fail-fast")

    def test_root_surfaces_include_lane_policy_terms(self) -> None:
        for relative in (
            "ci.ps1",
            "test.ps1",
            "scripts/validation/invoke-workspace-tests.ps1",
            "scripts/validation/run-local-validation-gate.ps1",
        ):
            text = (WORKSPACE_ROOT / relative).read_text(encoding="utf-8")
            self.assertIn("GateDecision", text, msg=f"{relative} must surface gate decision handling")
            self.assertIn("TimeoutBudgetSeconds", text, msg=f"{relative} must surface timeout budget")
            self.assertIn("RetryBudget", text, msg=f"{relative} must surface retry budget")


if __name__ == "__main__":
    unittest.main()
