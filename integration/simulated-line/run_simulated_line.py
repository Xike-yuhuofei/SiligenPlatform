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
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11
ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BUILD_ROOTS = (
    ROOT / "build" / "simulation-engine",
    ROOT / "packages" / "simulation-engine" / "build",
)
COMPAT_INPUT = ROOT / "packages" / "engineering-contracts" / "fixtures" / "cases" / "rect_diag" / "simulation-input.json"
SCHEME_C_RECT_DIAG_INPUT = ROOT / "examples" / "simulation" / "rect_diag.simulation-input.json"
SCHEME_C_SAMPLE_INPUT = ROOT / "examples" / "simulation" / "sample_trajectory.json"
SCHEME_C_INVALID_INPUT = ROOT / "examples" / "simulation" / "invalid_empty_segments.simulation-input.json"
SCHEME_C_REALISM_INPUT = ROOT / "examples" / "simulation" / "following_error_quantized.simulation-input.json"
COMPAT_BASELINE = ROOT / "integration" / "regression-baselines" / "rect_diag.simulation-baseline.json"
SCHEME_C_RECT_DIAG_BASELINE = ROOT / "integration" / "regression-baselines" / "rect_diag.scheme_c.recording-baseline.json"
SCHEME_C_SAMPLE_BASELINE = ROOT / "integration" / "regression-baselines" / "sample_trajectory.scheme_c.recording-baseline.json"
SCHEME_C_INVALID_BASELINE = ROOT / "integration" / "regression-baselines" / "invalid_empty_segments.scheme_c.recording-baseline.json"
SCHEME_C_REALISM_BASELINE = ROOT / "integration" / "regression-baselines" / "following_error_quantized.scheme_c.recording-baseline.json"


@dataclass(frozen=True)
class RegressionScenario:
    name: str
    input_path: Path
    baseline_path: Path
    expected_exit_code: int = 0


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
    error: str = ""


COMPAT_SCENARIOS = (
    RegressionScenario(
        name="compat_rect_diag",
        input_path=COMPAT_INPUT,
        baseline_path=COMPAT_BASELINE,
    ),
)

SCHEME_C_SCENARIOS = (
    RegressionScenario(
        name="scheme_c_rect_diag",
        input_path=SCHEME_C_RECT_DIAG_INPUT,
        baseline_path=SCHEME_C_RECT_DIAG_BASELINE,
    ),
    RegressionScenario(
        name="scheme_c_sample_trajectory",
        input_path=SCHEME_C_SAMPLE_INPUT,
        baseline_path=SCHEME_C_SAMPLE_BASELINE,
    ),
    RegressionScenario(
        name="scheme_c_invalid_empty_segments",
        input_path=SCHEME_C_INVALID_INPUT,
        baseline_path=SCHEME_C_INVALID_BASELINE,
        expected_exit_code=2,
    ),
    RegressionScenario(
        name="scheme_c_following_error_quantized",
        input_path=SCHEME_C_REALISM_INPUT,
        baseline_path=SCHEME_C_REALISM_BASELINE,
    ),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run compat and scheme C simulated-line regressions.")
    parser.add_argument("--mode", choices=("compat", "scheme_c", "both"), default="both")
    parser.add_argument("--build-root", default=os.getenv("SILIGEN_SIMULATION_ENGINE_BUILD_ROOT", ""))
    parser.add_argument("--report-dir", default="")
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


def _make_report(build_root: Path, mode: str) -> dict[str, Any]:
    return {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "build_root": str(build_root),
        "mode": mode,
        "overall_status": "running",
        "error": "",
        "scenarios": [],
    }


def _append_scenario_report(
    report: dict[str, Any],
    mode: str,
    scenario: RegressionScenario,
    executable: Path,
) -> RegressionScenarioReport:
    scenario_report = RegressionScenarioReport(
        name=scenario.name,
        mode=mode,
        input_path=str(scenario.input_path),
        baseline_path=str(scenario.baseline_path),
        executable=str(executable),
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


def _validate_scenario(
    *,
    mode: str,
    scenario: RegressionScenario,
    executable: Path,
    build_root: Path,
    summarize,
    equal_keys: tuple[str, ...],
    close_keys: tuple[str, ...],
    report: dict[str, Any],
) -> None:
    baseline = _load_json(scenario.baseline_path)
    scenario_report = _append_scenario_report(report, mode, scenario, executable)

    try:
        with tempfile.TemporaryDirectory() as temp_dir:
            output_a = Path(temp_dir) / f"{scenario.name}.first.json"
            output_b = Path(temp_dir) / f"{scenario.name}.second.json"
            first_exit_code, text_a, payload_a = _run_and_load_result(executable, build_root, scenario, output_a)
            second_exit_code, text_b, payload_b = _run_and_load_result(executable, build_root, scenario, output_b)
            scenario_report.actual_exit_code = second_exit_code
            _assert_text_equal(text_a, text_b, scenario.name)
            scenario_report.deterministic_replay_passed = True
            summary = summarize(payload_a)
            repeated_summary = summarize(payload_b)

        for key in equal_keys:
            _assert_equal(summary, baseline, key)
            _assert_equal(repeated_summary, baseline, key)
        for key in close_keys:
            _assert_close(summary, baseline, key)
            _assert_close(repeated_summary, baseline, key)

        scenario_report.actual_exit_code = first_exit_code
        scenario_report.summary_metrics = summary
        scenario_report.repeated_summary_metrics = repeated_summary
        scenario_report.status = "passed"
        _store_scenario_report(report, scenario_report)
    except Exception as error:
        scenario_report.status = "failed"
        scenario_report.error = str(error)
        _store_scenario_report(report, scenario_report)
        raise


def _run_compat_regression(build_root: Path, report: dict[str, Any]) -> None:
    executable = _resolve_executable(build_root, "simulate_dxf_path.exe")
    if not executable.exists():
        raise FileNotFoundError(f"compat simulation executable missing: {executable}")

    for scenario in COMPAT_SCENARIOS:
        _validate_scenario(
            mode="compat",
            scenario=scenario,
            executable=executable,
            build_root=build_root,
            summarize=_summarize_compat,
            equal_keys=("motion_profile_count", "final_segment_index", "io_event_count", "valve_event_count"),
            close_keys=("total_time", "motion_distance", "valve_open_time", "final_velocity"),
            report=report,
        )
        print(f"simulated-line compat regression passed: {scenario.name}")


def _run_scheme_c_regression(build_root: Path, report: dict[str, Any]) -> None:
    executable = _resolve_executable(build_root, "simulate_scheme_c_closed_loop.exe")
    if not executable.exists():
        raise FileNotFoundError(f"scheme C simulation executable missing: {executable}")

    for scenario in SCHEME_C_SCENARIOS:
        _validate_scenario(
            mode="scheme_c",
            scenario=scenario,
            executable=executable,
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
        )
        print(f"simulated-line scheme_c regression passed: {scenario.name}")


def main() -> int:
    args = parse_args()
    build_root = _resolve_build_root(args.build_root)
    selected_modes = ("compat", "scheme_c") if args.mode == "both" else (args.mode,)
    report_dir = Path(args.report_dir) if args.report_dir else None
    report = _make_report(build_root, args.mode)
    exit_code = 0

    try:
        for mode in selected_modes:
            if mode == "compat":
                _run_compat_regression(build_root, report)
            elif mode == "scheme_c":
                _run_scheme_c_regression(build_root, report)
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
            print(f"simulated-line json report: {json_path}")
            print(f"simulated-line markdown report: {md_path}")

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
