from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.evidence_bundle import (
    EVIDENCE_BUNDLE_SCHEMA_VERSION,
    REQUIRED_TRACE_FIELDS,
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    bundle_is_compatible,
    default_failure_classification,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)


class ValidationEvidenceBundleContractTest(unittest.TestCase):
    def test_infer_verdict_matches_contract_priority(self) -> None:
        self.assertEqual(infer_verdict(["passed", "passed"]), "passed")
        self.assertEqual(infer_verdict(["passed", "known_failure"]), "known-failure")
        self.assertEqual(infer_verdict(["passed", "failed"]), "failed")
        self.assertEqual(infer_verdict(["skipped"]), "skipped")

    def test_bundle_writer_emits_required_files_and_trace_fields(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            report_root = Path(temp_dir)
            summary_json_path = report_root / "workspace-validation.json"
            summary_md_path = report_root / "workspace-validation.md"
            summary_json_path.write_text(json.dumps({"status": "passed"}, indent=2), encoding="utf-8")
            summary_md_path.write_text("# Summary\n", encoding="utf-8")

            records = [
                EvidenceCaseRecord(
                    case_id="layered-validation-routing-smoke",
                    name="layered-validation-routing-smoke",
                    suite_ref="protocol-compatibility",
                    owner_scope="tests/integration",
                    primary_layer="L2",
                    producer_lane_ref="quick-gate",
                    status="passed",
                    evidence_profile="contract-report",
                    stability_state="stable",
                    size_label="medium",
                    label_refs=("suite:protocol-compatibility", "size:medium", "layer:L2"),
                    required_assets=("sample.dxf.rect_diag",),
                    required_fixtures=("fixture.layered-routing-smoke",),
                    fault_ids=("fault.simulated.invalid-empty-segments",),
                    deterministic_replay={
                        "seed": 0,
                        "clock_profile": "deterministic-monotonic",
                        "repeat_count": 2,
                        "passed": True,
                    },
                    failure_classification=default_failure_classification(status="passed"),
                    trace_fields=trace_fields(
                        stage_id="L2",
                        artifact_id="layered-validation-routing-smoke",
                        module_id="tests/integration",
                        workflow_state="executed",
                        execution_state="passed",
                        event_name="layered-validation-routing-smoke",
                        evidence_path=str(summary_json_path),
                    ),
                ),
                EvidenceCaseRecord(
                    case_id="hil-closed-loop",
                    name="hil-closed-loop",
                    suite_ref="e2e",
                    owner_scope="runtime-execution",
                    primary_layer="L5",
                    producer_lane_ref="limited-hil",
                    status="blocked",
                    evidence_profile="hil-report",
                    stability_state="stable",
                    size_label="large",
                    label_refs=("suite:e2e", "size:large", "layer:L5"),
                    required_assets=("sample.dxf.rect_diag",),
                    required_fixtures=("fixture.hil-closed-loop",),
                    fault_ids=("fault.hil.tcp-disconnect",),
                    deterministic_replay={
                        "seed": 0,
                        "clock_profile": "deterministic-monotonic",
                        "repeat_count": 1,
                        "passed": False,
                    },
                    failure_classification={
                        "category": "validation",
                        "code": "blocked",
                        "blocking": True,
                        "message": "hil admission blocked",
                    },
                    trace_fields=trace_fields(
                        stage_id="L5",
                        artifact_id="hil-closed-loop",
                        module_id="runtime-execution",
                        workflow_state="executed",
                        execution_state="blocked",
                        event_name="hil-closed-loop",
                        failure_code="validation.blocked",
                        evidence_path=str(summary_json_path),
                    ),
                ),
            ]
            bundle = EvidenceBundle(
                bundle_id="bundle-test",
                request_ref="request-test",
                producer_lane_ref="limited-hil",
                report_root=str(report_root),
                summary_file=str(summary_md_path),
                machine_file=str(summary_json_path),
                verdict=infer_verdict([record.status for record in records]),
                skipped_layer_refs=("L6",),
                skip_justification="performance deferred to nightly lane",
                offline_prerequisites=("L0", "L1", "L2", "L3", "L4"),
                metadata={
                    "fault_matrix_id": "fault-matrix.simulated-line.v1",
                    "selected_fault_ids": ["fault.simulated.invalid-empty-segments", "fault.hil.tcp-disconnect"],
                    "deterministic_seed": 0,
                    "clock_profile": "deterministic-monotonic",
                    "double_surface": {"fake_controller": ["readiness", "abort"], "fake_io": ["disconnect"]},
                },
                case_records=records,
            )

            written = write_bundle_artifacts(
                bundle=bundle,
                report_root=report_root,
                summary_json_path=summary_json_path,
                summary_md_path=summary_md_path,
                evidence_links=[
                    EvidenceLink(label="manual.md", path=str(report_root / "manual.md"), role="manual-check"),
                ],
            )

            for key in (
                "case_index_json",
                "evidence_links_md",
                "failure_details_json",
                "report_manifest_json",
                "report_index_json",
                "bundle_json",
            ):
                self.assertTrue(Path(written[key]).exists(), msg=f"missing bundle artifact: {key}")

            payload = json.loads(Path(written["bundle_json"]).read_text(encoding="utf-8"))
            self.assertEqual(payload["schema_version"], EVIDENCE_BUNDLE_SCHEMA_VERSION)
            self.assertEqual(payload["verdict"], "blocked")
            self.assertEqual(payload["producer_lane_ref"], "limited-hil")
            self.assertEqual(payload["skipped_layer_refs"], ["L6"])
            self.assertEqual(payload["metadata"]["fault_matrix_id"], "fault-matrix.simulated-line.v1")
            self.assertTrue(bundle_is_compatible(payload))
            self.assertTrue(payload["report_manifest_file"])
            self.assertTrue(payload["report_index_file"])
            for record in payload["case_records"]:
                self.assertIn("size_label", record)
                self.assertIn("label_refs", record)
                self.assertIn("fault_ids", record)
                self.assertIn("deterministic_replay", record)
                self.assertIn("failure_classification", record)
                self.assertTrue(record["size_label"])
                self.assertTrue(record["label_refs"])
                for field_name in REQUIRED_TRACE_FIELDS:
                    self.assertIn(field_name, record["trace_fields"])
                for field_name in ("category", "code", "blocking"):
                    self.assertIn(field_name, record["failure_classification"])


if __name__ == "__main__":
    unittest.main()
