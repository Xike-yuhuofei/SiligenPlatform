from __future__ import annotations

import argparse
import json
import sys
import tempfile
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
from run_online_runtime_action_matrix import (  # noqa: E402
    DEFAULT_OPERATOR_PREVIEW_DXF,
    OPERATOR_PREVIEW_REQUIRED_STAGES,
    RUNTIME_ACTION_CASES,
    build_command,
    summarize_operator_context,
)
from run_online_soak_profiles import (  # noqa: E402
    DEFAULT_SOAK_PROFILE_IDS,
    SoakProfileResult,
    _write_report as write_soak_report,
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

    def test_hmi_runtime_action_matrix_freezes_operator_preview_authority(self) -> None:
        self.assertEqual(
            tuple((case.case_id, case.profile) for case in RUNTIME_ACTION_CASES),
            (
                ("operator-preview", "operator_preview"),
                ("runtime-home-move", "home_move"),
                ("runtime-jog", "jog"),
                ("runtime-supply-dispenser", "supply_dispenser"),
                ("runtime-estop-reset", "estop_reset"),
                ("runtime-door-interlock", "door_interlock"),
            ),
        )

    def test_operator_preview_contract_requires_runtime_owned_stage_sequence(self) -> None:
        output = "\n".join(
            [
                "OPERATOR_CONTEXT stage=preview-ready artifact_id=artifact-001 plan_id=plan-001 preview_source=planned_glue_snapshot snapshot_hash=hash-001 snapshot_ready=true",
                "OPERATOR_CONTEXT stage=preview-refreshed artifact_id=artifact-001 plan_id=plan-001 preview_source=planned_glue_snapshot snapshot_hash=hash-001 snapshot_ready=true",
            ]
        )

        summary = summarize_operator_context(output)

        self.assertEqual(tuple(summary["operator_context_stages"]), OPERATOR_PREVIEW_REQUIRED_STAGES)
        self.assertTrue(summary["preview_ready_observed"])
        self.assertTrue(summary["preview_refresh_observed"])
        self.assertTrue(summary["stage_sequence_ok"])
        self.assertTrue(summary["contract_ok"])
        self.assertEqual(summary["artifact_id"], "artifact-001")
        self.assertEqual(summary["plan_id"], "plan-001")
        self.assertEqual(summary["snapshot_hash"], "hash-001")
        self.assertEqual(summary["preview_source"], "planned_glue_snapshot")
        self.assertTrue(summary["snapshot_ready"])

    def test_operator_preview_case_passes_explicit_dxf_browse_path_to_online_smoke(self) -> None:
        args = argparse.Namespace(
            report_root=ROOT / "tests" / "reports" / "adhoc" / "hmi-runtime-action-matrix",
            python_exe=sys.executable,
            timeout_ms=20000,
            gateway_exe=None,
            gateway_config=None,
            use_mock_gateway_config=False,
        )
        case = next(item for item in RUNTIME_ACTION_CASES if item.profile == "operator_preview")
        command = build_command(
            args=args,
            case=case,
            screenshot_path=ROOT / "tests" / "reports" / "adhoc" / "operator-preview.png",
        )

        self.assertIn("-DxfBrowsePath", command)
        self.assertIn(str(DEFAULT_OPERATOR_PREVIEW_DXF), command)

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

    def test_soak_summary_persists_production_baseline(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            report_dir = Path(temp_dir)
            json_path, md_path = write_soak_report(
                report_dir=report_dir,
                profile_results=[
                    SoakProfileResult(
                        profile_id="soak-30m-matrix",
                        status="passed",
                        exit_code=0,
                        long_run_minutes=30.0,
                        sample_labels=("small", "medium", "large"),
                        gate_mode="adhoc",
                        command=["python", "tests/performance/collect_dxf_preview_profiles.py"],
                        report_dir=str(report_dir / "profiles" / "soak-30m-matrix"),
                        latest_json=str(report_dir / "profiles" / "soak-30m-matrix" / "latest.json"),
                        latest_markdown=str(report_dir / "profiles" / "soak-30m-matrix" / "latest.md"),
                        threshold_gate_status="passed",
                    )
                ],
                selected_profile_ids=("soak-30m-matrix",),
                production_baseline={
                    "baseline_id": "baseline-001",
                    "baseline_fingerprint": "baseline-fp-001",
                    "production_baseline_source": "runtime_owned",
                },
            )

            payload = json.loads(json_path.read_text(encoding="utf-8"))
            markdown = md_path.read_text(encoding="utf-8")

        self.assertEqual(
            payload["production_baseline"],
            {
                "baseline_id": "baseline-001",
                "baseline_fingerprint": "baseline-fp-001",
                "production_baseline_source": "runtime_owned",
            },
        )
        self.assertIn("- baseline_id: `baseline-001`", markdown)
        self.assertIn("- baseline_fingerprint: `baseline-fp-001`", markdown)
        self.assertIn("- production_baseline_source: `runtime_owned`", markdown)


if __name__ == "__main__":
    unittest.main()
