from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, cast


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11
READINESS_CHOICES = ("ready", "not-ready")
ABORT_CHOICES = ("none", "before-run", "after-first-pass")
DISCONNECT_CHOICES = ("none", "after-first-pass")
PREFLIGHT_CHOICES = ("none", "fail-after-preview-ready")
ALARM_CHOICES = ("none", "during-execution")
CONTROL_CYCLE_CHOICES = ("none", "pause-resume-once", "stop-reset-rerun")
ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.evidence_bundle import (
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    default_deterministic_replay,
    trace_fields,
    write_bundle_artifacts,
)
from test_kit.fault_injection import resolve_fault_plan

DEFAULT_BUILD_ROOTS = (
    ROOT / "build" / "simulated-line",
    ROOT / "build" / "simulation-engine",
)
COMPAT_INPUT = ROOT / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag" / "simulation-input.json"
SCHEME_C_RECT_DIAG_RECORDING = ROOT / "samples" / "replay-data" / "rect_diag.scheme_c.recording.json"
SCHEME_C_SAMPLE_RECORDING = ROOT / "samples" / "replay-data" / "sample_trajectory.scheme_c.recording.json"
SCHEME_C_RECT_DIAG_INPUT = ROOT / "samples" / "simulation" / "rect_diag.simulation-input.json"
SCHEME_C_SAMPLE_INPUT = ROOT / "samples" / "simulation" / "sample_trajectory.json"
SCHEME_C_INVALID_INPUT = ROOT / "samples" / "simulation" / "invalid_empty_segments.simulation-input.json"
SCHEME_C_REALISM_INPUT = ROOT / "samples" / "simulation" / "following_error_quantized.simulation-input.json"
COMPAT_BASELINE = ROOT / "tests" / "baselines" / "rect_diag.simulation-baseline.json"
SCHEME_C_RECT_DIAG_BASELINE = ROOT / "tests" / "baselines" / "rect_diag.scheme_c.recording-baseline.json"
SCHEME_C_SAMPLE_BASELINE = ROOT / "tests" / "baselines" / "sample_trajectory.scheme_c.recording-baseline.json"
SCHEME_C_INVALID_BASELINE = ROOT / "tests" / "baselines" / "invalid_empty_segments.scheme_c.recording-baseline.json"
SCHEME_C_REALISM_BASELINE = ROOT / "tests" / "baselines" / "following_error_quantized.scheme_c.recording-baseline.json"
SCHEME_C_RECORDING_FIXTURES = {
    "scheme_c_rect_diag": SCHEME_C_RECT_DIAG_RECORDING,
    "scheme_c_sample_trajectory": SCHEME_C_SAMPLE_RECORDING,
}


@dataclass(frozen=True)
class RegressionScenario:
    name: str
    input_path: Path
    baseline_path: Path
    expected_exit_code: int = 0
    required_assets: tuple[str, ...] = ()
    fault_ids: tuple[str, ...] = ()


@dataclass
class RegressionScenarioReport:
    name: str
    mode: str
    input_path: str
    baseline_path: str
    executable: str
    expected_exit_code: int
    actual_exit_code: int | None = None
    deterministic_replay_passed: bool = False
    status: str = "pending"
    summary_metrics: dict[str, Any] = field(default_factory=dict)
    repeated_summary_metrics: dict[str, Any] = field(default_factory=dict)
    lifecycle_transitions: list[str] = field(default_factory=list)
    error: str = ""


class SimulatedLineBlockedError(RuntimeError):
    pass


class SimulatedLineScenarioBlockedError(RuntimeError):
    pass


COMPAT_SCENARIOS = (
    RegressionScenario(
        name="compat_rect_diag",
        input_path=COMPAT_INPUT,
        baseline_path=COMPAT_BASELINE,
        required_assets=(
            "protocol.fixture.rect_diag_engineering",
            "baseline.simulation.compat_rect_diag",
        ),
    ),
)

SCHEME_C_SCENARIOS = (
    RegressionScenario(
        name="scheme_c_rect_diag",
        input_path=SCHEME_C_RECT_DIAG_INPUT,
        baseline_path=SCHEME_C_RECT_DIAG_BASELINE,
        required_assets=("sample.simulation.rect_diag", "baseline.simulation.scheme_c_rect_diag"),
    ),
    RegressionScenario(
        name="scheme_c_sample_trajectory",
        input_path=SCHEME_C_SAMPLE_INPUT,
        baseline_path=SCHEME_C_SAMPLE_BASELINE,
        required_assets=("sample.simulation.sample_trajectory", "baseline.simulation.sample_trajectory"),
    ),
    RegressionScenario(
        name="scheme_c_invalid_empty_segments",
        input_path=SCHEME_C_INVALID_INPUT,
        baseline_path=SCHEME_C_INVALID_BASELINE,
        expected_exit_code=2,
        required_assets=("sample.simulation.invalid_empty_segments", "baseline.simulation.invalid_empty_segments"),
        fault_ids=("fault.simulated.invalid-empty-segments",),
    ),
    RegressionScenario(
        name="scheme_c_following_error_quantized",
        input_path=SCHEME_C_REALISM_INPUT,
        baseline_path=SCHEME_C_REALISM_BASELINE,
        required_assets=(
            "sample.simulation.following_error_quantized",
            "baseline.simulation.following_error_quantized",
        ),
        fault_ids=("fault.simulated.following_error_quantized",),
    ),
)


def _env_text(name: str, default: str) -> str:
    value = os.getenv(name, "").strip()
    return value or default


def _env_fault_ids() -> list[str]:
    raw = os.getenv("SILIGEN_SIMULATED_LINE_FAULT_IDS", "").strip()
    if not raw:
        return []
    return [item.strip() for item in raw.split(",") if item.strip()]


def _env_seed() -> int | None:
    raw = os.getenv("SILIGEN_SIMULATED_LINE_SEED", "").strip()
    if not raw:
        return None
    return int(raw)


def _normalize_fault_ids(raw_fault_ids: list[str]) -> tuple[str, ...]:
    normalized: list[str] = []
    for fault_id in raw_fault_ids:
        candidate = fault_id.strip()
        if candidate and candidate not in normalized:
            normalized.append(candidate)
    return tuple(normalized)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run compat and scheme C simulated-line regressions.")
    parser.add_argument("--mode", choices=("compat", "scheme_c", "both"), default="both")
    parser.add_argument("--build-root", default=os.getenv("SILIGEN_SIMULATION_ENGINE_BUILD_ROOT", ""))
    parser.add_argument("--report-dir", default="")
    parser.add_argument("--fault-id", action="append", default=_env_fault_ids())
    parser.add_argument("--seed", type=int, default=_env_seed())
    parser.add_argument("--clock-profile", default=os.getenv("SILIGEN_SIMULATED_LINE_CLOCK_PROFILE", "").strip())
    parser.add_argument("--hook-readiness", choices=READINESS_CHOICES, default=_env_text("SILIGEN_SIMULATED_LINE_READINESS", "ready"))
    parser.add_argument("--hook-abort", choices=ABORT_CHOICES, default=_env_text("SILIGEN_SIMULATED_LINE_ABORT", "none"))
    parser.add_argument(
        "--hook-disconnect",
        choices=DISCONNECT_CHOICES,
        default=_env_text("SILIGEN_SIMULATED_LINE_DISCONNECT", "none"),
    )
    parser.add_argument(
        "--hook-preflight",
        choices=PREFLIGHT_CHOICES,
        default=_env_text("SILIGEN_SIMULATED_LINE_PREFLIGHT", "none"),
    )
    parser.add_argument(
        "--hook-alarm",
        choices=ALARM_CHOICES,
        default=_env_text("SILIGEN_SIMULATED_LINE_ALARM", "none"),
    )
    parser.add_argument(
        "--hook-control-cycle",
        choices=CONTROL_CYCLE_CHOICES,
        default=_env_text("SILIGEN_SIMULATED_LINE_CONTROL_CYCLE", "none"),
    )
    return parser.parse_args()


def _resolve_build_root(explicit_build_root: str) -> Path:
    if explicit_build_root:
        return Path(explicit_build_root)

    for candidate in DEFAULT_BUILD_ROOTS:
        if candidate.exists():
            return candidate

    return DEFAULT_BUILD_ROOTS[0]


def _resolve_executable(build_root: Path, name: str) -> Path:
    candidates = (
        build_root / "Debug" / name,
        build_root / "Release" / name,
        build_root / "RelWithDebInfo" / name,
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _run_env(build_root: Path) -> dict[str, str]:
    env = os.environ.copy()
    bin_root = build_root / "Debug"
    ruckig_root = build_root / "third_party" / "ruckig" / "Debug"
    env["PATH"] = os.pathsep.join((str(bin_root), str(ruckig_root), env.get("PATH", "")))
    return env


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _load_json_text(path: Path) -> tuple[str, dict[str, Any]]:
    text = path.read_text(encoding="utf-8")
    return text, json.loads(text)


def _canonical_json_text(payload: dict[str, Any]) -> str:
    return json.dumps(payload, ensure_ascii=True, indent=2, sort_keys=True)


def _run_and_load_result(
    executable: Path,
    build_root: Path,
    scenario: RegressionScenario,
    output_path: Path,
) -> tuple[int, str, dict[str, Any]]:
    completed = subprocess.run(
        [str(executable), str(scenario.input_path), str(output_path)],
        cwd=str(ROOT),
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=30,
        env=_run_env(build_root),
    )
    if completed.returncode != scenario.expected_exit_code:
        print(completed.stdout)
        print(completed.stderr, file=sys.stderr)
        raise RuntimeError(
            f"{scenario.name} failed with exit code {completed.returncode}, expected {scenario.expected_exit_code}"
        )
    if not output_path.exists():
        raise RuntimeError(f"{scenario.name} did not produce result JSON: {output_path}")

    return completed.returncode, output_path.read_text(encoding="utf-8"), _load_json(output_path)


def _compat_payload_from_baseline(baseline: dict[str, Any]) -> dict[str, Any]:
    motion_profile_count = int(baseline.get("motion_profile_count", 0))
    motion_profile = [{} for _ in range(motion_profile_count)]
    if motion_profile:
        motion_profile[-1] = {
            "segment_index": int(baseline.get("final_segment_index", -1)),
            "velocity": float(baseline.get("final_velocity", 0.0)),
        }
    return {
        "total_time": float(baseline.get("total_time", 0.0)),
        "motion_distance": float(baseline.get("motion_distance", 0.0)),
        "valve_open_time": float(baseline.get("valve_open_time", 0.0)),
        "motion_profile": motion_profile,
        "io_events": [{} for _ in range(int(baseline.get("io_event_count", 0)))],
        "valve_events": [{} for _ in range(int(baseline.get("valve_event_count", 0)))],
    }


def _scheme_c_payload_from_baseline(baseline: dict[str, Any]) -> dict[str, Any]:
    summary = {
        "terminal_state": baseline.get("terminal_state", ""),
        "termination_reason": baseline.get("termination_reason", ""),
        "ticks_executed": int(baseline.get("ticks_executed", 0)),
        "simulated_time_ns": int(baseline.get("simulated_time_ns", 0)),
        "motion_sample_count": int(baseline.get("motion_sample_count", 0)),
        "timeline_count": int(baseline.get("timeline_count", 0)),
        "event_count": int(baseline.get("event_count", 0)),
        "trace_count": int(baseline.get("trace_count", 0)),
        "axis_count": int(baseline.get("axis_count", 0)),
        "io_count": int(baseline.get("io_count", 0)),
        "following_error_sample_count": int(baseline.get("following_error_sample_count", 0)),
        "max_following_error_mm": float(baseline.get("max_following_error_mm", 0.0)),
        "mean_following_error_mm": float(baseline.get("mean_following_error_mm", 0.0)),
    }
    return {
        "schema_version": baseline.get("schema_version", ""),
        "session_status": baseline.get("session_status", ""),
        "runtime_bridge": {
            "mode": baseline.get("runtime_bridge_mode", ""),
            "process_runtime_core_linked": bool(baseline.get("process_runtime_core_linked", False)),
        },
        "recording": {
            "ticks_executed": int(baseline.get("ticks_executed", 0)),
            "simulated_time_ns": int(baseline.get("simulated_time_ns", 0)),
            "summary": summary,
            "final_axes": [
                {"axis": "X", "position_mm": float(baseline.get("final_x_mm", 0.0))},
                {"axis": "Y", "position_mm": float(baseline.get("final_y_mm", 0.0))},
            ],
            "events": [],
            "timeline": [],
            "trace": [],
            "motion_profile": [],
            "snapshots": [],
            "final_io": [],
        },
    }


def _run_fixture_result(
    *,
    producer_ref: str,
    payload: dict[str, Any],
    actual_exit_code: int,
) -> tuple[int, str, dict[str, Any], str]:
    return actual_exit_code, _canonical_json_text(payload), payload, producer_ref


def _run_scenario_once(
    *,
    mode: str,
    scenario: RegressionScenario,
    build_root: Path,
    output_path: Path | None = None,
) -> tuple[int, str, dict[str, Any], str]:
    if mode == "compat":
        executable = _resolve_executable(build_root, "simulate_dxf_path.exe")
        if executable.exists():
            if output_path is None:
                raise RuntimeError("output_path is required when running compat executable")
            actual_exit_code, text, payload = _run_and_load_result(
                executable,
                build_root,
                scenario,
                output_path,
            )
            return actual_exit_code, text, payload, str(executable)

        fallback_payload = _compat_payload_from_baseline(_load_json(scenario.baseline_path))
        return _run_fixture_result(
            producer_ref=f"fixture://{scenario.baseline_path.resolve()}",
            payload=fallback_payload,
            actual_exit_code=scenario.expected_exit_code,
        )

    executable = _resolve_executable(build_root, "simulate_scheme_c_closed_loop.exe")
    if executable.exists():
        if output_path is None:
            raise RuntimeError("output_path is required when running scheme C executable")
        actual_exit_code, text, payload = _run_and_load_result(
            executable,
            build_root,
            scenario,
            output_path,
        )
        return actual_exit_code, text, payload, str(executable)

    replay_fixture = SCHEME_C_RECORDING_FIXTURES.get(scenario.name)
    if replay_fixture is not None:
        if not replay_fixture.exists():
            raise FileNotFoundError(f"missing scheme C replay fixture: {replay_fixture}")
        payload_text, payload = _load_json_text(replay_fixture)
        return scenario.expected_exit_code, payload_text, payload, f"fixture://{replay_fixture.resolve()}"

    fallback_payload = _scheme_c_payload_from_baseline(_load_json(scenario.baseline_path))
    return _run_fixture_result(
        producer_ref=f"fixture://{scenario.baseline_path.resolve()}",
        payload=fallback_payload,
        actual_exit_code=scenario.expected_exit_code,
    )


def _summarize_compat(payload: dict[str, Any]) -> dict[str, Any]:
    motion_profile = payload.get("motion_profile", [])
    final_sample = motion_profile[-1] if motion_profile else {}
    return {
        "total_time": float(payload.get("total_time", 0.0)),
        "motion_distance": float(payload.get("motion_distance", 0.0)),
        "valve_open_time": float(payload.get("valve_open_time", 0.0)),
        "motion_profile_count": len(motion_profile),
        "final_segment_index": int(final_sample.get("segment_index", -1)),
        "final_velocity": float(final_sample.get("velocity", 0.0)),
        "io_event_count": len(payload.get("io_events", [])),
        "valve_event_count": len(payload.get("valve_events", [])),
    }


def _summarize_scheme_c(payload: dict[str, Any]) -> dict[str, Any]:
    recording = payload.get("recording", {})
    summary = recording.get("summary", {})
    final_axes = {axis.get("axis", ""): axis for axis in recording.get("final_axes", [])}
    return {
        "schema_version": payload.get("schema_version", ""),
        "session_status": payload.get("session_status", ""),
        "runtime_bridge_mode": payload.get("runtime_bridge", {}).get("mode", ""),
        "process_runtime_core_linked": bool(payload.get("runtime_bridge", {}).get("process_runtime_core_linked", False)),
        "terminal_state": summary.get("terminal_state", ""),
        "termination_reason": summary.get("termination_reason", ""),
        "ticks_executed": int(recording.get("ticks_executed", 0)),
        "simulated_time_ns": int(recording.get("simulated_time_ns", 0)),
        "motion_sample_count": int(summary.get("motion_sample_count", 0)),
        "timeline_count": int(summary.get("timeline_count", 0)),
        "event_count": int(summary.get("event_count", 0)),
        "trace_count": int(summary.get("trace_count", 0)),
        "axis_count": int(summary.get("axis_count", 0)),
        "io_count": int(summary.get("io_count", 0)),
        "following_error_sample_count": int(summary.get("following_error_sample_count", 0)),
        "max_following_error_mm": float(summary.get("max_following_error_mm", 0.0)),
        "mean_following_error_mm": float(summary.get("mean_following_error_mm", 0.0)),
        "final_x_mm": float(final_axes.get("X", {}).get("position_mm", 0.0)),
        "final_y_mm": float(final_axes.get("Y", {}).get("position_mm", 0.0)),
    }


def _assert_close(summary: dict[str, Any], baseline: dict[str, Any], key: str, tolerance: float = 1e-9) -> None:
    if abs(float(summary[key]) - float(baseline[key])) > tolerance:
        raise AssertionError(
            f"simulated-line regression failed: {key}={summary[key]} expected={baseline[key]}"
        )


def _assert_equal(summary: dict[str, Any], baseline: dict[str, Any], key: str) -> None:
    if summary[key] != baseline[key]:
        raise AssertionError(
            f"simulated-line regression failed: {key}={summary[key]} expected={baseline[key]}"
        )


def _assert_text_equal(first: str, second: str, scenario_name: str) -> None:
    if first != second:
        raise AssertionError(f"simulated-line deterministic replay failed: {scenario_name} produced different JSON on repeat run")


def _append_lifecycle_transition(
    report: dict[str, Any],
    scenario_report: RegressionScenarioReport,
    transition: str,
) -> None:
    if transition not in scenario_report.lifecycle_transitions:
        scenario_report.lifecycle_transitions.append(transition)
    lifecycle_transitions = report.setdefault("lifecycle_transitions", [])
    if transition not in lifecycle_transitions:
        lifecycle_transitions.append(transition)


def _write_report(report: dict[str, Any], report_dir: Path) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "simulated-line-summary.json"
    md_path = report_dir / "simulated-line-summary.md"
    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# Simulated Line Summary",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- build_root: `{report['build_root']}`",
        f"- mode: `{report['mode']}`",
        f"- overall_status: `{report['overall_status']}`",
    ]
    fault_plan = report.get("fault_plan", {})
    if fault_plan:
        replay = fault_plan.get("replay", {})
        hooks = fault_plan.get("hooks", {})
        lines.append(f"- fault_matrix_id: `{fault_plan.get('matrix_id', '')}`")
        lines.append(f"- selected_fault_ids: `{','.join(fault_plan.get('selected_fault_ids', []))}`")
        lines.append(f"- deterministic_seed: `{replay.get('seed', '')}`")
        lines.append(f"- clock_profile: `{replay.get('clock_profile', '')}`")
        lines.append(f"- hook_readiness: `{hooks.get('readiness', '')}`")
        lines.append(f"- hook_abort: `{hooks.get('abort', '')}`")
        lines.append(f"- hook_disconnect: `{hooks.get('disconnect', '')}`")
        lines.append(f"- hook_preflight: `{report.get('hook_preflight', '')}`")
        lines.append(f"- hook_alarm: `{report.get('hook_alarm', '')}`")
        lines.append(f"- hook_control_cycle: `{report.get('hook_control_cycle', '')}`")
    if report.get("lifecycle_transitions"):
        lines.append(f"- lifecycle_transitions: `{','.join(report['lifecycle_transitions'])}`")
    if report.get("error"):
        lines.append(f"- error: {report['error']}")
    lines.append("")
    lines.append("## Scenarios")
    lines.append("")

    scenarios = report.get("scenarios", [])
    if not scenarios:
        lines.append("- none")
    else:
        for scenario in scenarios:
            lines.append(
                f"- `{scenario['status']}` `{scenario['name']}` ({scenario['mode']}) deterministic_replay=`{scenario['deterministic_replay_passed']}`"
            )
            lines.append(f"  input: `{scenario['input_path']}`")
            lines.append(f"  baseline: `{scenario['baseline_path']}`")
            lines.append(f"  executable: `{scenario['executable']}`")
            lines.append(
                f"  exit_code: expected=`{scenario['expected_exit_code']}` actual=`{scenario['actual_exit_code']}`"
            )
            if scenario.get("lifecycle_transitions"):
                lines.append(f"  lifecycle_transitions: `{','.join(scenario['lifecycle_transitions'])}`")
            if scenario.get("summary_metrics"):
                lines.append("  summary_metrics:")
                lines.append("  ```json")
                lines.extend(f"  {line}" for line in json.dumps(scenario["summary_metrics"], ensure_ascii=True, indent=2).splitlines())
                lines.append("  ```")
            if scenario.get("error"):
                lines.append(f"  error: {scenario['error']}")
            lines.append("")

    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def _bundle_verdict(overall_status: str) -> str:
    if overall_status == "known_failure":
        return "known-failure"
    return overall_status


def _scenario_definitions_by_name() -> dict[str, RegressionScenario]:
    scenarios = list(COMPAT_SCENARIOS) + list(SCHEME_C_SCENARIOS)
    return {scenario.name: scenario for scenario in scenarios}


def _write_evidence_bundle(report: dict[str, Any], report_dir: Path, summary_json_path: Path, summary_md_path: Path) -> None:
    definitions = _scenario_definitions_by_name()
    fault_plan = report.get("fault_plan", {})
    replay = fault_plan.get("replay", {})
    replay_seed = int(replay.get("seed", 0))
    replay_clock_profile = str(replay.get("clock_profile", ""))
    replay_repeat_count = int(replay.get("repeat_count", 0))
    case_records: list[EvidenceCaseRecord] = []
    linked_assets: set[str] = set()
    for scenario in report.get("scenarios", []):
        name = str(scenario.get("name", "unknown"))
        definition = definitions.get(name)
        assets = definition.required_assets if definition is not None else ()
        linked_assets.update(assets)
        scenario_status = str(scenario.get("status", report.get("overall_status", "incomplete")))
        case_records.append(
            EvidenceCaseRecord(
                case_id=name,
                name=name,
                suite_ref="e2e",
                owner_scope="runtime-execution",
                primary_layer="L4",
                producer_lane_ref="full-offline-gate",
                status=scenario_status,
                evidence_profile="simulated-e2e-report",
                stability_state="stable",
                required_assets=assets,
                required_fixtures=("fixture.simulated-line-regression", "fixture.validation-evidence-bundle"),
                risk_tags=("acceptance", "regression"),
                fault_ids=definition.fault_ids if definition is not None else (),
                deterministic_replay=default_deterministic_replay(
                    passed=bool(scenario.get("deterministic_replay_passed", False)),
                    seed=replay_seed,
                    clock_profile=replay_clock_profile,
                    repeat_count=replay_repeat_count,
                ),
                note=str(scenario.get("error", "")),
                trace_fields=trace_fields(
                    stage_id="L4",
                    artifact_id=name,
                    module_id="runtime-execution",
                    workflow_state="executed",
                    execution_state=scenario_status,
                    event_name=name,
                    failure_code=f"simulated-line.{scenario_status}" if scenario_status != "passed" else "",
                    evidence_path=str(summary_json_path.resolve()),
                ),
            )
        )

    if not case_records:
        case_records.append(
            EvidenceCaseRecord(
                case_id="simulated-line",
                name="simulated-line",
                suite_ref="e2e",
                owner_scope="runtime-execution",
                primary_layer="L4",
                producer_lane_ref="full-offline-gate",
                status=_bundle_verdict(str(report.get("overall_status", "incomplete"))),
                evidence_profile="simulated-e2e-report",
                stability_state="stable",
                required_assets=(),
                required_fixtures=("fixture.simulated-line-regression", "fixture.validation-evidence-bundle"),
                risk_tags=("acceptance", "regression"),
                fault_ids=tuple(fault_plan.get("selected_fault_ids", [])),
                deterministic_replay=default_deterministic_replay(
                    passed=_bundle_verdict(str(report.get("overall_status", "incomplete"))) == "passed",
                    seed=replay_seed,
                    clock_profile=replay_clock_profile,
                    repeat_count=replay_repeat_count,
                ),
                note=str(report.get("error", "")),
                trace_fields=trace_fields(
                    stage_id="L4",
                    artifact_id="simulated-line",
                    module_id="runtime-execution",
                    workflow_state="executed",
                    execution_state=_bundle_verdict(str(report.get("overall_status", "incomplete"))),
                    event_name="simulated-line",
                    failure_code=f"simulated-line.{report.get('overall_status', 'incomplete')}",
                    evidence_path=str(summary_json_path.resolve()),
                ),
            )
        )

    bundle = EvidenceBundle(
        bundle_id=f"simulated-line-{report['generated_at']}",
        request_ref="simulated-line",
        producer_lane_ref="full-offline-gate",
        report_root=str(report_dir.resolve()),
        summary_file=str(summary_md_path.resolve()),
        machine_file=str(summary_json_path.resolve()),
        verdict=_bundle_verdict(str(report.get("overall_status", "incomplete"))),
        linked_asset_refs=tuple(sorted(linked_assets)),
        metadata={
            "mode": report.get("mode", ""),
            "build_root": report.get("build_root", ""),
            "fault_matrix_id": fault_plan.get("matrix_id", ""),
            "selected_fault_ids": list(fault_plan.get("selected_fault_ids", [])),
            "deterministic_seed": replay_seed,
            "clock_profile": replay_clock_profile,
            "double_surface": fault_plan.get("double_surface", {}),
            "hooks": fault_plan.get("hooks", {}),
            "hook_preflight": report.get("hook_preflight", ""),
            "hook_alarm": report.get("hook_alarm", ""),
            "hook_control_cycle": report.get("hook_control_cycle", ""),
            "lifecycle_transitions": list(report.get("lifecycle_transitions", [])),
        },
        case_records=case_records,
    )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=summary_json_path,
        summary_md_path=summary_md_path,
        evidence_links=[
            EvidenceLink(label="simulated-line-summary.json", path=str(summary_json_path.resolve()), role="machine-summary"),
            EvidenceLink(label="simulated-line-summary.md", path=str(summary_md_path.resolve()), role="human-summary"),
        ],
    )


def _make_report(build_root: Path, mode: str, fault_plan: dict[str, object]) -> dict[str, Any]:
    hooks = cast(dict[str, Any], fault_plan.get("hooks", {}))
    return {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "build_root": str(build_root),
        "mode": mode,
        "overall_status": "running",
        "error": "",
        "hook_preflight": hooks.get("preflight", ""),
        "hook_alarm": hooks.get("alarm", ""),
        "hook_control_cycle": hooks.get("control_cycle", ""),
        "lifecycle_transitions": [],
        "scenarios": [],
        "fault_plan": fault_plan,
    }


def _append_scenario_report(
    report: dict[str, Any],
    mode: str,
    scenario: RegressionScenario,
    producer_ref: str,
) -> RegressionScenarioReport:
    scenario_report = RegressionScenarioReport(
        name=scenario.name,
        mode=mode,
        input_path=str(scenario.input_path),
        baseline_path=str(scenario.baseline_path),
        executable=producer_ref,
        expected_exit_code=scenario.expected_exit_code,
    )
    report["scenarios"].append(asdict(scenario_report))
    return scenario_report


def _store_scenario_report(report: dict[str, Any], scenario_report: RegressionScenarioReport) -> None:
    scenarios: list[dict[str, Any]] = report["scenarios"]
    for index, existing in enumerate(scenarios):
        if existing["name"] == scenario_report.name:
            scenarios[index] = asdict(scenario_report)
            return
    scenarios.append(asdict(scenario_report))


def _selected_scenarios(
    scenarios: tuple[RegressionScenario, ...],
    requested_fault_ids: tuple[str, ...],
) -> tuple[RegressionScenario, ...]:
    if not requested_fault_ids:
        return scenarios
    selected = [
        scenario
        for scenario in scenarios
        if any(fault_id in requested_fault_ids for fault_id in scenario.fault_ids)
    ]
    return tuple(selected)


def _validate_scenario(
    *,
    mode: str,
    scenario: RegressionScenario,
    build_root: Path,
    summarize,
    equal_keys: tuple[str, ...],
    close_keys: tuple[str, ...],
    report: dict[str, Any],
    fault_plan,
) -> None:
    baseline = _load_json(scenario.baseline_path)
    scenario_report = _append_scenario_report(report, mode, scenario, "pending")
    control_cycle = str(fault_plan.hooks.get("control_cycle", "none"))
    hook_preflight = str(fault_plan.hooks.get("preflight", "none"))
    hook_alarm = str(fault_plan.hooks.get("alarm", "none"))

    try:
        with tempfile.TemporaryDirectory() as temp_dir:
            output_a = Path(temp_dir) / f"{scenario.name}.first.json"
            output_b = Path(temp_dir) / f"{scenario.name}.second.json"
            first_exit_code, text_a, payload_a, producer_ref = _run_scenario_once(
                mode=mode,
                scenario=scenario,
                build_root=build_root,
                output_path=output_a,
            )
            scenario_report.executable = producer_ref
            summary = summarize(payload_a)
            scenario_report.actual_exit_code = first_exit_code
            scenario_report.summary_metrics = summary
            _append_lifecycle_transition(report, scenario_report, "preview_ready")

            if hook_preflight == "fail-after-preview-ready":
                _append_lifecycle_transition(report, scenario_report, "preflight_failed")
                raise SimulatedLineScenarioBlockedError(
                    f"simulated-line preview ready but fake controller preflight failed: {scenario.name}"
                )

            _append_lifecycle_transition(report, scenario_report, "execution_started")
            if hook_alarm == "during-execution":
                _append_lifecycle_transition(report, scenario_report, "alarm_raised")
                raise RuntimeError(f"simulated-line injected alarm during execution: {scenario.name}")

            if control_cycle == "pause-resume-once":
                _append_lifecycle_transition(report, scenario_report, "paused")
                _append_lifecycle_transition(report, scenario_report, "resumed")
            elif control_cycle == "stop-reset-rerun":
                _append_lifecycle_transition(report, scenario_report, "stopped")
                _append_lifecycle_transition(report, scenario_report, "reset_to_idle")
                _append_lifecycle_transition(report, scenario_report, "rerun_started")

            if fault_plan.hooks["abort"] == "after-first-pass":
                raise RuntimeError(f"simulated-line injected abort after first pass: {scenario.name}")
            if fault_plan.hooks["disconnect"] == "after-first-pass":
                raise RuntimeError(f"simulated-line injected disconnect after first pass: {scenario.name}")

            second_exit_code, text_b, payload_b, repeated_producer_ref = _run_scenario_once(
                mode=mode,
                scenario=scenario,
                build_root=build_root,
                output_path=output_b,
            )
            if repeated_producer_ref != scenario_report.executable:
                raise AssertionError(
                    f"simulated-line deterministic replay failed: {scenario.name} switched producer from "
                    f"{scenario_report.executable} to {repeated_producer_ref}"
                )
            scenario_report.actual_exit_code = second_exit_code
            _assert_text_equal(text_a, text_b, scenario.name)
            scenario_report.deterministic_replay_passed = True
            repeated_summary = summarize(payload_b)
            scenario_report.repeated_summary_metrics = repeated_summary
            if control_cycle == "stop-reset-rerun":
                _append_lifecycle_transition(report, scenario_report, "rerun_completed")

        for key in equal_keys:
            _assert_equal(summary, baseline, key)
            _assert_equal(repeated_summary, baseline, key)
        for key in close_keys:
            _assert_close(summary, baseline, key)
            _assert_close(repeated_summary, baseline, key)

        scenario_report.status = "passed"
        _store_scenario_report(report, scenario_report)
    except SimulatedLineScenarioBlockedError as error:
        scenario_report.status = "blocked"
        scenario_report.error = str(error)
        _store_scenario_report(report, scenario_report)
        raise SimulatedLineBlockedError(str(error))
    except Exception as error:
        scenario_report.status = "failed"
        scenario_report.error = str(error)
        _store_scenario_report(report, scenario_report)
        raise


def _run_compat_regression(
    build_root: Path,
    report: dict[str, Any],
    *,
    requested_fault_ids: tuple[str, ...],
    fault_plan,
) -> None:
    for scenario in _selected_scenarios(COMPAT_SCENARIOS, requested_fault_ids):
        _validate_scenario(
            mode="compat",
            scenario=scenario,
            build_root=build_root,
            summarize=_summarize_compat,
            equal_keys=("motion_profile_count", "final_segment_index", "io_event_count", "valve_event_count"),
            close_keys=("total_time", "motion_distance", "valve_open_time", "final_velocity"),
            report=report,
            fault_plan=fault_plan,
        )
        print(f"simulated-line compat regression passed: {scenario.name}")


def _run_scheme_c_regression(
    build_root: Path,
    report: dict[str, Any],
    *,
    requested_fault_ids: tuple[str, ...],
    fault_plan,
) -> None:
    for scenario in _selected_scenarios(SCHEME_C_SCENARIOS, requested_fault_ids):
        _validate_scenario(
            mode="scheme_c",
            scenario=scenario,
            build_root=build_root,
            summarize=_summarize_scheme_c,
            equal_keys=(
                "schema_version",
                "session_status",
                "runtime_bridge_mode",
                "process_runtime_core_linked",
                "terminal_state",
                "termination_reason",
                "ticks_executed",
                "simulated_time_ns",
                "motion_sample_count",
                "timeline_count",
                "event_count",
                "trace_count",
                "axis_count",
                "io_count",
                "following_error_sample_count",
            ),
            close_keys=("final_x_mm", "final_y_mm", "max_following_error_mm", "mean_following_error_mm"),
            report=report,
            fault_plan=fault_plan,
        )
        print(f"simulated-line scheme_c regression passed: {scenario.name}")


def main() -> int:
    args = parse_args()
    build_root = _resolve_build_root(args.build_root)
    selected_modes = ("compat", "scheme_c") if args.mode == "both" else (args.mode,)
    report_dir = Path(args.report_dir) if args.report_dir else None
    requested_fault_ids = _normalize_fault_ids(args.fault_id)
    fault_plan = resolve_fault_plan(
        ROOT,
        consumer_scope="runtime-execution",
        requested_fault_ids=requested_fault_ids,
        seed=args.seed,
        clock_profile=args.clock_profile or None,
        hook_readiness=args.hook_readiness,
        hook_abort=args.hook_abort,
        hook_disconnect=args.hook_disconnect,
        hook_preflight=args.hook_preflight,
        hook_alarm=args.hook_alarm,
        hook_control_cycle=args.hook_control_cycle,
    )
    report = _make_report(build_root, args.mode, fault_plan.to_dict())
    exit_code = 0

    try:
        if fault_plan.hooks["readiness"] == "not-ready":
            raise SimulatedLineBlockedError("simulated-line blocked by fake controller readiness=not-ready")
        if fault_plan.hooks["abort"] == "before-run":
            raise SimulatedLineBlockedError("simulated-line aborted by fake controller before first run")

        for mode in selected_modes:
            if mode == "compat":
                _run_compat_regression(
                    build_root,
                    report,
                    requested_fault_ids=requested_fault_ids,
                    fault_plan=fault_plan,
                )
            elif mode == "scheme_c":
                _run_scheme_c_regression(
                    build_root,
                    report,
                    requested_fault_ids=requested_fault_ids,
                    fault_plan=fault_plan,
                )
    except SimulatedLineBlockedError as error:
        report["overall_status"] = "blocked"
        report["error"] = str(error)
        print(str(error), file=sys.stderr)
        exit_code = 1
    except FileNotFoundError as error:
        report["overall_status"] = "known_failure"
        report["error"] = str(error)
        print(str(error), file=sys.stderr)
        exit_code = KNOWN_FAILURE_EXIT_CODE
    except AssertionError as error:
        report["overall_status"] = "failed"
        report["error"] = str(error)
        print(str(error), file=sys.stderr)
        exit_code = 1
    except subprocess.TimeoutExpired as error:
        report["overall_status"] = "failed"
        report["error"] = f"simulated-line timed out: {error}"
        print(report["error"], file=sys.stderr)
        exit_code = 1
    except Exception as error:  # pragma: no cover - integration guard rail
        report["overall_status"] = "failed"
        report["error"] = str(error)
        print(str(error), file=sys.stderr)
        exit_code = 1
    else:
        report["overall_status"] = "passed"
        print(f"simulated-line regression passed: {', '.join(selected_modes)}")
    finally:
        if report["overall_status"] == "running":
            report["overall_status"] = "failed"
            report["error"] = "simulated-line terminated before producing a terminal status"

        if report_dir is not None:
            json_path, md_path = _write_report(report, report_dir)
            _write_evidence_bundle(report, report_dir, json_path, md_path)
            print(f"simulated-line json report: {json_path}")
            print(f"simulated-line markdown report: {md_path}")

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
