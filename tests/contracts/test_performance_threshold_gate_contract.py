from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
PERFORMANCE_ROOT = WORKSPACE_ROOT / "tests" / "performance"
for candidate in (TEST_KIT_SRC, PERFORMANCE_ROOT):
    if str(candidate) not in sys.path:
        sys.path.insert(0, str(candidate))

from collect_dxf_preview_profiles import evaluate_threshold_gate, resolve_default_gateway_executable


def _payload() -> dict[str, object]:
    return {
        "results": {
            "small": {
                "cold": {
                    "status": "ok",
                    "preview_summary": {
                        "artifact_ms": {"p95_ms": 140.0},
                        "prepare_total_ms": {"p95_ms": 40.0},
                    },
                },
                "hot": {
                    "status": "ok",
                    "preview_summary": {
                        "artifact_ms": {"p95_ms": 120.0},
                        "prepare_total_ms": {"p95_ms": 180.0},
                    },
                },
                "singleflight": {
                    "status": "ok",
                    "summary": {
                        "prepare_total_ms": {"p95_ms": 210.0},
                    },
                },
            },
            "medium": {
                "cold": {
                    "status": "ok",
                    "preview_summary": {
                        "artifact_ms": {"p95_ms": 310.0},
                        "prepare_total_ms": {"p95_ms": 85.0},
                    },
                },
                "hot": {
                    "status": "ok",
                    "preview_summary": {
                        "prepare_total_ms": {"p95_ms": 55.0},
                    },
                    "execution": {
                        "summary": {
                            "execution_total_ms": {"p95_ms": 510.0},
                        }
                    },
                },
                "singleflight": {
                    "status": "ok",
                    "summary": {
                        "prepare_total_ms": {"p95_ms": 70.0},
                    },
                },
            },
            "large": {
                "cold": {
                    "status": "ok",
                    "preview_summary": {
                        "artifact_ms": {"p95_ms": 620.0},
                        "prepare_total_ms": {"p95_ms": 160.0},
                    },
                },
                "hot": {
                    "status": "ok",
                    "preview_summary": {
                        "prepare_total_ms": {"p95_ms": 110.0},
                    },
                    "execution": {
                        "summary": {
                            "execution_total_ms": {"p95_ms": 920.0},
                        }
                    },
                },
                "singleflight": {
                    "status": "ok",
                    "summary": {
                        "prepare_total_ms": {"p95_ms": 135.0},
                    },
                },
            },
        }
    }


class PerformanceThresholdGateContractTest(unittest.TestCase):
    def test_default_gateway_executable_falls_back_to_control_apps_build_root(self) -> None:
        with tempfile.TemporaryDirectory() as workspace_dir, tempfile.TemporaryDirectory() as build_root_dir:
            workspace_root = Path(workspace_dir)
            control_apps_build_root = Path(build_root_dir)
            expected_path = control_apps_build_root / "bin" / "Debug" / "siligen_runtime_gateway.exe"
            expected_path.parent.mkdir(parents=True, exist_ok=True)
            expected_path.write_text("stub", encoding="utf-8")

            actual_path = resolve_default_gateway_executable(
                workspace_root=workspace_root,
                control_apps_build_root=control_apps_build_root,
            )

        self.assertEqual(actual_path, expected_path)

    def test_missing_threshold_config_fails_nightly_gate(self) -> None:
        gate = evaluate_threshold_gate(_payload(), "nightly-performance", "missing-threshold-config.json")
        self.assertEqual(gate["status"], "failed")

    def test_threshold_config_passes_when_payload_satisfies_requirements(self) -> None:
        config = {
            "schema_version": "dxf-preview-profile-thresholds.v1",
            "required_samples": ["small", "medium", "large"],
            "required_scenarios": [
                {"sample_label": "small", "scenario": "cold", "expected_status": "ok"},
                {"sample_label": "small", "scenario": "hot", "expected_status": "ok"},
                {"sample_label": "small", "scenario": "singleflight", "expected_status": "ok"},
                {"sample_label": "medium", "scenario": "cold", "expected_status": "ok"},
                {"sample_label": "medium", "scenario": "hot", "expected_status": "ok"},
                {"sample_label": "medium", "scenario": "singleflight", "expected_status": "ok"},
                {"sample_label": "large", "scenario": "cold", "expected_status": "ok"},
                {"sample_label": "large", "scenario": "hot", "expected_status": "ok"},
                {"sample_label": "large", "scenario": "singleflight", "expected_status": "ok"},
            ],
            "numeric_thresholds": [
                {
                    "name": "small.cold.artifact_ms.p95_ms",
                    "path": "results.small.cold.preview_summary.artifact_ms.p95_ms",
                    "max": 500.0,
                },
                {
                    "name": "small.singleflight.prepare_total_ms.p95_ms",
                    "path": "results.small.singleflight.summary.prepare_total_ms.p95_ms",
                    "max": 500.0,
                },
                {
                    "name": "medium.hot.execution_total_ms.p95_ms",
                    "path": "results.medium.hot.execution.summary.execution_total_ms.p95_ms",
                    "max": 600.0,
                },
                {
                    "name": "large.singleflight.prepare_total_ms.p95_ms",
                    "path": "results.large.singleflight.summary.prepare_total_ms.p95_ms",
                    "max": 200.0,
                },
            ],
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "thresholds.json"
            config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
            gate = evaluate_threshold_gate(_payload(), "nightly-performance", str(config_path))
        self.assertEqual(gate["status"], "passed")


if __name__ == "__main__":
    unittest.main()
