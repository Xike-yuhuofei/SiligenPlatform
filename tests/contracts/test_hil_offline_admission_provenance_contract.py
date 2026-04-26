import json
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))


def _load_hil_module(module_name: str, filename: str):
    spec = importlib.util.spec_from_file_location(module_name, HIL_DIR / filename)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load {filename}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


run_hil_closed_loop = _load_hil_module("run_hil_closed_loop_contract", "run_hil_closed_loop.py")


def _workspace_report(*, head_sha: str = "abc123", lane: str = "full-offline-gate") -> dict:
    return {
        "metadata": {
            "validation_provenance": {
                "report_schema_version": "workspace-validation.v1",
                "head_sha": head_sha,
                "base_sha": "",
                "workflow_run_id": "",
                "workflow_run_attempt": "",
                "github_event_name": "",
                "github_ref": "",
                "github_repository": "",
                "lane": lane,
                "suite_set": "apps,contracts,protocol-compatibility,integration,e2e",
            }
        },
        "counts": {"passed": 3, "failed": 0, "known_failure": 0, "skipped": 0},
        "results": [
            {"name": "protocol-compatibility", "status": "passed"},
            {"name": "engineering-regression", "status": "passed"},
            {"name": "simulated-line", "status": "passed"},
        ],
    }


class HilOfflineAdmissionProvenanceContractTest(unittest.TestCase):
    def _write_report(self, payload: dict) -> Path:
        temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(temp_dir.cleanup)
        path = Path(temp_dir.name) / "workspace-validation.json"
        path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        return path

    def test_matching_provenance_admits_hil(self) -> None:
        report_path = self._write_report(_workspace_report())

        admission = run_hil_closed_loop._evaluate_offline_admission(
            offline_prereq_report=str(report_path),
            operator_override_reason="",
            expected_offline_head_sha="abc123",
            expected_offline_lane="full-offline-gate",
        )

        self.assertEqual(admission["admission_decision"], "admitted")
        self.assertTrue(admission["offline_prerequisites_passed"])

    def test_missing_provenance_blocks_hil(self) -> None:
        payload = _workspace_report()
        payload["metadata"] = {}
        report_path = self._write_report(payload)

        admission = run_hil_closed_loop._evaluate_offline_admission(
            offline_prereq_report=str(report_path),
            operator_override_reason="",
            expected_offline_head_sha="abc123",
            expected_offline_lane="full-offline-gate",
        )

        self.assertEqual(admission["admission_decision"], "blocked")
        self.assertFalse(admission["offline_prerequisites_passed"])
        self.assertIn("offline-prereq-provenance-schema", {item["name"] for item in admission["checks"]})

    def test_mismatched_head_sha_blocks_hil(self) -> None:
        report_path = self._write_report(_workspace_report(head_sha="old-sha"))

        admission = run_hil_closed_loop._evaluate_offline_admission(
            offline_prereq_report=str(report_path),
            operator_override_reason="",
            expected_offline_head_sha="new-sha",
            expected_offline_lane="full-offline-gate",
        )

        self.assertEqual(admission["admission_decision"], "blocked")
        self.assertFalse(admission["offline_prerequisites_passed"])
        head_check = next(item for item in admission["checks"] if item["name"] == "offline-prereq-head-sha")
        self.assertEqual(head_check["status"], "failed")


if __name__ == "__main__":
    unittest.main()
