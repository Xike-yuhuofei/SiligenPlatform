from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
import unittest
from contextlib import contextmanager
from types import SimpleNamespace
from typing import Any, cast
from unittest.mock import patch
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
PERFORMANCE_ROOT = WORKSPACE_ROOT / "tests" / "performance"
for candidate in (TEST_KIT_SRC, PERFORMANCE_ROOT):
    if str(candidate) not in sys.path:
        sys.path.insert(0, str(candidate))

from collect_dxf_preview_profiles import (
    ControlCycleRecord,
    LongRunIterationRecord,
    LaunchSpecResolution,
    PreviewCycleRecord,
    StartJobCycleRecord,
    collect_long_run_profile,
    collect_process_metrics,
    evaluate_threshold_gate,
    resolve_default_gateway_executable,
    summarize_control_cycle_records,
    summarize_long_run_records,
)


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
                "control_cycle": {
                    "status": "ok",
                    "summary": {
                        "pause_to_running_ms": {"p95_ms": 120.0},
                        "stop_to_idle_ms": {"p95_ms": 125.0},
                        "rerun_total_ms": {"p95_ms": 220.0},
                        "execution_total_ms": {"p95_ms": 180.0},
                        "working_set_mb": {"delta_max": 12.0},
                        "private_memory_mb": {"delta_max": 11.0},
                        "handle_count": {"delta_max": 1.0},
                        "timeout_count": 0,
                    },
                },
                "long_run": {
                    "status": "ok",
                    "summary": {
                        "execution_total_ms": {"p95_ms": 190.0},
                        "working_set_mb": {"delta_max": 14.0},
                        "private_memory_mb": {"delta_max": 13.0},
                        "handle_count": {"delta_max": 1.0},
                        "thread_count": {"max": 11.0},
                        "timeout_count": 0,
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
                "control_cycle": {
                    "status": "ok",
                    "summary": {
                        "pause_to_running_ms": {"p95_ms": 180.0},
                        "stop_to_idle_ms": {"p95_ms": 210.0},
                        "rerun_total_ms": {"p95_ms": 2600.0},
                        "execution_total_ms": {"p95_ms": 2400.0},
                        "working_set_mb": {"delta_max": 18.0},
                        "private_memory_mb": {"delta_max": 16.0},
                        "handle_count": {"delta_max": 2.0},
                        "timeout_count": 0,
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
                "control_cycle": {
                    "status": "ok",
                    "summary": {
                        "pause_to_running_ms": {"p95_ms": 320.0},
                        "stop_to_idle_ms": {"p95_ms": 350.0},
                        "rerun_total_ms": {"p95_ms": 28000.0},
                        "execution_total_ms": {"p95_ms": 26000.0},
                        "working_set_mb": {"delta_max": 28.0},
                        "private_memory_mb": {"delta_max": 24.0},
                        "handle_count": {"delta_max": 3.0},
                        "timeout_count": 0,
                    },
                },
            },
        }
    }


class PerformanceThresholdGateContractTest(unittest.TestCase):
    def test_collect_process_metrics_ignores_sampler_timeout(self) -> None:
        with patch(
            "collect_dxf_preview_profiles.subprocess.run",
            autospec=True,
            side_effect=subprocess.TimeoutExpired(cmd="powershell", timeout=5.0),
        ):
            self.assertEqual(collect_process_metrics(1234), {})

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

    def test_default_gateway_executable_prefers_workspace_build_ca_over_hmi_home_fix(self) -> None:
        with tempfile.TemporaryDirectory() as workspace_dir:
            workspace_root = Path(workspace_dir)
            workspace_ca_exe = workspace_root / "build" / "ca" / "bin" / "Debug" / "siligen_runtime_gateway.exe"
            workspace_hmi_fix_exe = workspace_root / "build" / "hmi-home-fix" / "bin" / "Debug" / "siligen_runtime_gateway.exe"
            workspace_ca_exe.parent.mkdir(parents=True, exist_ok=True)
            workspace_hmi_fix_exe.parent.mkdir(parents=True, exist_ok=True)
            workspace_ca_exe.write_text("ca", encoding="utf-8")
            workspace_hmi_fix_exe.write_text("fix", encoding="utf-8")

            actual_path = resolve_default_gateway_executable(
                workspace_root=workspace_root,
                control_apps_build_root=workspace_root / "build" / "ca",
            )

        self.assertEqual(actual_path, workspace_ca_exe)

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
                {"sample_label": "small", "scenario": "control_cycle", "expected_status": "ok"},
                {"sample_label": "medium", "scenario": "control_cycle", "expected_status": "ok"},
                {"sample_label": "large", "scenario": "control_cycle", "expected_status": "ok"},
                {"sample_label": "small", "scenario": "long_run", "expected_status": "ok"},
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
                {
                    "name": "small.control_cycle.pause_to_running_ms.p95_ms",
                    "path": "results.small.control_cycle.summary.pause_to_running_ms.p95_ms",
                    "max": 500.0,
                },
                {
                    "name": "small.long_run.thread_count.max",
                    "path": "results.small.long_run.summary.thread_count.max",
                    "max": 32.0,
                },
                {
                    "name": "large.control_cycle.execution_total_ms.p95_ms",
                    "path": "results.large.control_cycle.summary.execution_total_ms.p95_ms",
                    "max": 33000.0,
                },
            ],
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "thresholds.json"
            config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
            gate = evaluate_threshold_gate(_payload(), "nightly-performance", str(config_path))
        self.assertEqual(gate["status"], "passed")

    def test_collect_long_run_profile_reuses_single_preview_snapshot(self) -> None:
        args = cast(
            argparse.Namespace,
            SimpleNamespace(
                long_run_minutes=1.0,
                include_start_job=True,
                host="127.0.0.1",
                port=9856,
                reuse_running=False,
                artifact_timeout=120.0,
            ),
        )
        launch_spec = cast(LaunchSpecResolution, SimpleNamespace())
        sample_path = WORKSPACE_ROOT / "samples" / "dxf" / "rect_diag.dxf"
        preview_record = PreviewCycleRecord(
            success=True,
            artifact_id="artifact-1",
            plan_id="plan-1",
            plan_fingerprint="fp-1",
            snapshot_hash="fp-1",
            artifact_ms=12.0,
        )
        execution_record = StartJobCycleRecord(success=True, execution_total_ms=162.0)

        @contextmanager
        def managed_backend_context():
            yield SimpleNamespace(manager=None, mode="started")

        @contextmanager
        def protocol_client_context():
            yield None, object()

        with (
            patch(
                "collect_dxf_preview_profiles.managed_backend",
                autospec=True,
                return_value=managed_backend_context(),
            ),
            patch(
                "collect_dxf_preview_profiles.protocol_client",
                autospec=True,
                return_value=protocol_client_context(),
            ),
            patch("collect_dxf_preview_profiles.maybe_prepare_execution_runtime", autospec=True),
            patch(
                "collect_dxf_preview_profiles.create_artifact",
                autospec=True,
                return_value=("artifact-1", 12.0),
            ),
            patch("collect_dxf_preview_profiles.gateway_process_id", autospec=True, return_value=1234),
            patch(
                "collect_dxf_preview_profiles.collect_process_metrics",
                autospec=True,
                side_effect=[
                    {"working_set_mb": 10.0, "private_memory_mb": 8.0, "handle_count": 1.0, "thread_count": 2.0},
                    {"working_set_mb": 11.0, "private_memory_mb": 9.0, "handle_count": 1.0, "thread_count": 2.0},
                    {"working_set_mb": 12.0, "private_memory_mb": 10.0, "handle_count": 1.0, "thread_count": 2.0},
                    {"working_set_mb": 13.0, "private_memory_mb": 11.0, "handle_count": 1.0, "thread_count": 2.0},
                ],
            ),
            patch(
                "collect_dxf_preview_profiles.prepare_and_snapshot_once",
                autospec=True,
                return_value=preview_record,
            ) as prepare_mock,
            patch(
                "collect_dxf_preview_profiles.run_start_job_cycle",
                autospec=True,
                return_value=execution_record,
            ) as run_mock,
            patch("collect_dxf_preview_profiles.time.perf_counter", autospec=True, side_effect=[0.0, 1.0, 61.0]),
        ):
            payload = collect_long_run_profile(args, launch_spec, sample_path)

        self.assertEqual(payload["status"], "ok")
        self.assertEqual(prepare_mock.call_count, 1)
        self.assertEqual(run_mock.call_count, 2)
        self.assertEqual(payload["preview_setup"]["plan_id"], "plan-1")
        self.assertEqual(len(payload["iterations"]), 2)
        self.assertEqual(payload["summary"]["success_count"], 2)
        self.assertEqual(payload["summary"]["count"], 2)
        self.assertEqual(payload["summary"]["sample_count"], 4)

    def test_collect_long_run_profile_surfaces_preview_setup_failure(self) -> None:
        args = cast(
            argparse.Namespace,
            SimpleNamespace(
                long_run_minutes=1.0,
                include_start_job=True,
                host="127.0.0.1",
                port=9856,
                reuse_running=False,
                artifact_timeout=120.0,
            ),
        )
        launch_spec = cast(LaunchSpecResolution, SimpleNamespace())
        sample_path = WORKSPACE_ROOT / "samples" / "dxf" / "rect_diag.dxf"
        preview_record = PreviewCycleRecord(success=False, artifact_id="artifact-1", error="preview.snapshot failed: timeout")

        @contextmanager
        def managed_backend_context():
            yield SimpleNamespace(manager=None, mode="started")

        @contextmanager
        def protocol_client_context():
            yield None, object()

        with (
            patch(
                "collect_dxf_preview_profiles.managed_backend",
                autospec=True,
                return_value=managed_backend_context(),
            ),
            patch(
                "collect_dxf_preview_profiles.protocol_client",
                autospec=True,
                return_value=protocol_client_context(),
            ),
            patch("collect_dxf_preview_profiles.maybe_prepare_execution_runtime", autospec=True),
            patch(
                "collect_dxf_preview_profiles.create_artifact",
                autospec=True,
                return_value=("artifact-1", 12.0),
            ),
            patch("collect_dxf_preview_profiles.gateway_process_id", autospec=True, return_value=1234),
            patch(
                "collect_dxf_preview_profiles.collect_process_metrics",
                autospec=True,
                side_effect=[
                    {"working_set_mb": 10.0, "private_memory_mb": 8.0, "handle_count": 1.0, "thread_count": 2.0},
                    {"working_set_mb": 11.0, "private_memory_mb": 9.0, "handle_count": 1.0, "thread_count": 2.0},
                ],
            ),
            patch(
                "collect_dxf_preview_profiles.prepare_and_snapshot_once",
                autospec=True,
                return_value=preview_record,
            ),
            patch("collect_dxf_preview_profiles.run_start_job_cycle", autospec=True) as run_mock,
            patch("collect_dxf_preview_profiles.time.perf_counter", autospec=True, return_value=0.0),
        ):
            payload = collect_long_run_profile(args, launch_spec, sample_path)

        self.assertEqual(payload["status"], "error")
        self.assertEqual(run_mock.call_count, 0)
        self.assertIn("preview setup failed", payload["error"])
        self.assertEqual(payload["summary"]["failure_count"], 1)

    def test_control_cycle_summary_keeps_round_count_separate_from_resource_samples(self) -> None:
        summary = summarize_control_cycle_records(
            records=[
                ControlCycleRecord(round_index=1, success=True, pause_to_running_ms=12.0),
                ControlCycleRecord(round_index=2, success=True, pause_to_running_ms=13.0),
            ],
            resource_samples=[
                {"working_set_mb": 10.0, "private_memory_mb": 8.0, "handle_count": 1.0, "thread_count": 2.0},
                {"working_set_mb": 11.0, "private_memory_mb": 9.0, "handle_count": 1.0, "thread_count": 2.0},
                {"working_set_mb": 12.0, "private_memory_mb": 10.0, "handle_count": 1.0, "thread_count": 2.0},
            ],
        )

        self.assertEqual(summary["count"], 2)
        self.assertEqual(summary["sample_count"], 3)

    def test_long_run_summary_keeps_iteration_count_separate_from_resource_samples(self) -> None:
        summary = summarize_long_run_records(
            records=[
                LongRunIterationRecord(iteration_index=1, success=True, execution_total_ms=100.0),
                LongRunIterationRecord(iteration_index=2, success=True, execution_total_ms=101.0),
            ],
            resource_samples=[
                {"working_set_mb": 10.0, "private_memory_mb": 8.0, "handle_count": 1.0, "thread_count": 2.0},
                {"working_set_mb": 11.0, "private_memory_mb": 9.0, "handle_count": 1.0, "thread_count": 2.0},
                {"working_set_mb": 12.0, "private_memory_mb": 10.0, "handle_count": 1.0, "thread_count": 2.0},
                {"working_set_mb": 13.0, "private_memory_mb": 11.0, "handle_count": 1.0, "thread_count": 2.0},
            ],
        )

        self.assertEqual(summary["count"], 2)
        self.assertEqual(summary["sample_count"], 4)


if __name__ == "__main__":
    unittest.main()
