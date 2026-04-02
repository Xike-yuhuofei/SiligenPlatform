from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.evidence_bundle import (
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    bundle_is_compatible,
    default_failure_classification,
    trace_fields,
    write_bundle_artifacts,
)
from test_kit.validation_layers import build_request, lane_policy_metadata, route_validation_request


def _assert_quick_gate_policy() -> dict[str, object]:
    routed = route_validation_request(
        build_request(
            requested_suites=["contracts", "protocol-compatibility"],
            changed_scopes=["shared/testing", "tests/integration"],
            risk_profile="medium",
            desired_depth="quick",
        )
    )
    metadata = routed.to_metadata()
    assert metadata["selected_lane_ref"] == "quick-gate"
    assert metadata["selected_lane_gate_decision"] == "blocking"
    assert metadata["selected_lane_default_fail_policy"] == "fail-fast"
    assert metadata["selected_lane_fail_fast_case_limit"] == 1
    return metadata


def _assert_nightly_policy() -> dict[str, object]:
    routed = route_validation_request(
        build_request(
            requested_suites=["performance"],
            changed_scopes=["tests/performance"],
            risk_profile="medium",
            desired_depth="nightly",
        )
    )
    metadata = routed.to_metadata()
    assert metadata["selected_lane_ref"] == "nightly-performance"
    assert metadata["selected_lane_gate_decision"] == "blocking"
    assert "suite:performance" in metadata["requested_suite_label_refs"]
    return metadata


def _assert_limited_hil_policy() -> dict[str, object]:
    routed = route_validation_request(
        build_request(
            requested_suites=["e2e"],
            changed_scopes=["runtime-execution"],
            risk_profile="hardware-sensitive",
            desired_depth="hil",
            include_hardware_smoke=True,
            include_hil_closed_loop=True,
        )
    )
    metadata = routed.to_metadata()
    assert metadata["selected_lane_ref"] == "limited-hil"
    assert metadata["selected_lane_gate_decision"] == "blocking"
    assert metadata["selected_lane_timeout_budget_seconds"] == lane_policy_metadata("limited-hil")["timeout_budget_seconds"]
    return metadata


def _assert_bundle_manifest_smoke() -> dict[str, object]:
    with tempfile.TemporaryDirectory() as temp_dir:
        report_root = Path(temp_dir)
        summary_json_path = report_root / "summary.json"
        summary_md_path = report_root / "summary.md"
        summary_json_path.write_text(json.dumps({"status": "passed"}, indent=2), encoding="utf-8")
        summary_md_path.write_text("# Summary\n", encoding="utf-8")

        bundle = EvidenceBundle(
            bundle_id="lane-policy-smoke",
            request_ref="nightly-performance",
            producer_lane_ref="nightly-performance",
            report_root=str(report_root),
            summary_file=str(summary_md_path),
            machine_file=str(summary_json_path),
            verdict="passed",
            metadata={"threshold_gate": {"status": "passed"}},
            case_records=[
                EvidenceCaseRecord(
                    case_id="performance-small",
                    name="performance-small",
                    suite_ref="performance",
                    owner_scope="tests/performance",
                    primary_layer="L4-performance",
                    producer_lane_ref="nightly-performance",
                    status="passed",
                    evidence_profile="performance-report",
                    stability_state="stable",
                    size_label="large",
                    label_refs=("suite:performance", "size:large", "layer:L4-performance"),
                    deterministic_replay={"passed": True, "seed": 0, "clock_profile": "deterministic-monotonic", "repeat_count": 1},
                    failure_classification=default_failure_classification(status="passed"),
                    trace_fields=trace_fields(
                        stage_id="L4-performance",
                        artifact_id="performance-small",
                        module_id="tests/performance",
                        workflow_state="executed",
                        execution_state="passed",
                        event_name="performance-small",
                        evidence_path=str(summary_json_path),
                    ),
                )
            ],
        )
        written = write_bundle_artifacts(
            bundle=bundle,
            report_root=report_root,
            summary_json_path=summary_json_path,
            summary_md_path=summary_md_path,
            evidence_links=[EvidenceLink(label="summary.json", path=str(summary_json_path), role="machine-summary")],
        )
        payload = json.loads(Path(written["bundle_json"]).read_text(encoding="utf-8"))
        assert bundle_is_compatible(payload)
        return {
            "bundle_json": written["bundle_json"],
            "report_manifest_json": written["report_manifest_json"],
            "report_index_json": written["report_index_json"],
        }


def main() -> int:
    payload = {
        "quick_gate_policy": lane_policy_metadata("quick-gate"),
        "nightly_policy": lane_policy_metadata("nightly-performance"),
        "limited_hil_policy": lane_policy_metadata("limited-hil"),
        "quick_route": _assert_quick_gate_policy(),
        "nightly_route": _assert_nightly_policy(),
        "limited_hil_route": _assert_limited_hil_policy(),
        "bundle_manifest_smoke": _assert_bundle_manifest_smoke(),
    }
    print(json.dumps(payload, ensure_ascii=True, indent=2))
    print("lane policy smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
