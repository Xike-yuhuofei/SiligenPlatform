from __future__ import annotations

import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
HMI_SCRIPTS_DIR = ROOT / "apps" / "hmi-app" / "scripts"
PERFORMANCE_DIR = ROOT / "tests" / "performance"
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
for candidate in (HIL_DIR, HMI_SCRIPTS_DIR, PERFORMANCE_DIR, TEST_KIT_SRC):
    if str(candidate) not in sys.path:
        sys.path.insert(0, str(candidate))

from run_full_online_hmi_suite import FULL_ONLINE_HMI_SCENARIOS, scenario_ids  # noqa: E402
from run_online_soak_profiles import (  # noqa: E402
    DEFAULT_SOAK_PROFILE_IDS,
    evaluate_collect_payload_status,
    resolve_profiles,
)
from run_real_dxf_machine_dryrun_suite import DEFAULT_CASE_IDS as DEFAULT_DRYRUN_CASE_IDS  # noqa: E402
from run_real_dxf_machine_dryrun_suite import evaluate_dryrun_case_status, resolve_suite_cases as resolve_dryrun_suite_cases  # noqa: E402
from run_real_dxf_preview_suite import DEFAULT_CASE_IDS as DEFAULT_PREVIEW_CASE_IDS  # noqa: E402
from run_real_dxf_preview_suite import evaluate_preview_case_status, resolve_suite_cases as resolve_preview_suite_cases  # noqa: E402
from test_kit.dxf_truth_matrix import full_chain_case_ids  # noqa: E402


class OnlineTestMatrixRunnerContractTest(unittest.TestCase):
    def test_preview_and_dryrun_suites_default_to_full_chain_truth_matrix(self) -> None:
        expected_case_ids = full_chain_case_ids(ROOT)
        self.assertEqual(DEFAULT_PREVIEW_CASE_IDS, expected_case_ids)
        self.assertEqual(DEFAULT_DRYRUN_CASE_IDS, expected_case_ids)
        self.assertEqual(
            tuple(case.case_id for case in resolve_preview_suite_cases(())),
            expected_case_ids,
        )
        self.assertEqual(
            tuple(case.case_id for case in resolve_dryrun_suite_cases(())),
            expected_case_ids,
        )

    def test_hmi_full_suite_freezes_required_online_scenarios(self) -> None:
        self.assertEqual(
            scenario_ids(),
            (
                "online-smoke",
                "online-ready-timeout",
                "online-recovery-loop",
                "runtime-action-matrix",
            ),
        )
        self.assertEqual(len(FULL_ONLINE_HMI_SCENARIOS), 4)

    def test_soak_profiles_cover_formal_and_planned_blockers(self) -> None:
        profiles = resolve_profiles(())
        self.assertEqual(DEFAULT_SOAK_PROFILE_IDS, tuple(profile.profile_id for profile in profiles))
        self.assertEqual(
            tuple((profile.profile_id, profile.long_run_minutes, profile.sample_labels) for profile in profiles),
            (
                ("soak-30m-matrix", 30.0, ("small", "medium", "large")),
                ("soak-2h-small", 120.0, ("small",)),
                ("soak-8h-small", 480.0, ("small",)),
                ("soak-24h-small", 1440.0, ("small",)),
            ),
        )

    def test_runner_status_evaluators_keep_authority_rules_stable(self) -> None:
        self.assertEqual(
            evaluate_preview_case_status(exit_code=0, child_overall_status="passed", preview_verdict="passed"),
            "passed",
        )
        self.assertEqual(
            evaluate_preview_case_status(exit_code=0, child_overall_status="passed", preview_verdict="mismatch"),
            "failed",
        )
        self.assertEqual(
            evaluate_dryrun_case_status(exit_code=0, child_overall_status="passed", verdict_kind="completed"),
            "passed",
        )
        self.assertEqual(
            evaluate_dryrun_case_status(exit_code=0, child_overall_status="known_failure", verdict_kind="completed"),
            "known_failure",
        )

    def test_soak_payload_status_fails_on_any_non_ok_profile_block(self) -> None:
        passing_payload = {
            "results": {
                "small": {
                    "cold": {"status": "ok"},
                    "hot": {"status": "ok"},
                    "singleflight": {"status": "ok"},
                    "control_cycle": {"status": "ok"},
                    "long_run": {"status": "ok"},
                }
            },
            "threshold_gate": {"status": "not-applicable"},
        }
        failing_payload = {
            "results": {
                "small": {
                    "cold": {"status": "ok"},
                    "hot": {"status": "ok"},
                    "singleflight": {"status": "ok"},
                    "control_cycle": {"status": "failed"},
                    "long_run": {"status": "ok"},
                }
            },
            "threshold_gate": {"status": "not-applicable"},
        }
        self.assertEqual(evaluate_collect_payload_status(passing_payload), "passed")
        self.assertEqual(evaluate_collect_payload_status(failing_payload), "failed")


if __name__ == "__main__":
    unittest.main()
