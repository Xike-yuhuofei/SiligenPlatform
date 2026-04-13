from __future__ import annotations

import sys
from pathlib import Path


THIS_DIR = Path(__file__).resolve().parent
ROOT = THIS_DIR.parents[3]
FIRST_LAYER_DIR = THIS_DIR
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"

for candidate in (FIRST_LAYER_DIR, HIL_DIR):
    candidate_str = str(candidate)
    if candidate_str not in sys.path:
        sys.path.insert(0, candidate_str)

import run_dxf_stop_home_auto_validation as validation


def _probe_report(
    *,
    overall_status: str = "passed",
    verdict_kind: str = "passed",
    terminal_state: str = "cancelled",
    axes_stopped_before_home: bool | None = True,
    home_auto_ok: bool | None = True,
    mc_prftrap_detected: bool = False,
    prftrap_hit_count: int = 0,
    is_moving: bool = False,
    remaining_segments: int = 0,
    current_velocity: float = 0.0,
) -> dict[str, object]:
    return {
        "overall_status": overall_status,
        "verdict": {"kind": verdict_kind},
        "observation_summary": {
            "job_terminal_state_after_stop": terminal_state,
            "axes_stopped_before_home": axes_stopped_before_home,
            "home_auto_ok": home_auto_ok,
            "mc_prftrap_detected": mc_prftrap_detected,
            "prftrap_hit_count": prftrap_hit_count,
            "coord_after_stop": {
                "is_moving": is_moving,
                "remaining_segments": remaining_segments,
                "current_velocity": current_velocity,
            },
        },
    }


def test_evaluate_hardware_smoke_blocks_without_manual_confirmation() -> None:
    result = validation.evaluate_hardware_smoke(
        {
            "summary": {
                "observation_result": "ready_for_gate_review",
                "next_action": "continue",
                "a4_signal": "NeedsManualReview",
                "dominant_gap": "none",
            },
            "manual_checkpoints": [{"id": "manual-estop", "detail": "confirm estop"}],
        },
        manual_checks_confirmed=False,
    )

    assert result["gate_status"] == "blocked"
    assert result["verdict_kind"] == "manual_confirmation_required"
    assert "manual safety checkpoints" in result["message"]


def test_evaluate_hardware_smoke_blocks_when_automatic_gate_failed() -> None:
    result = validation.evaluate_hardware_smoke(
        {
            "summary": {
                "observation_result": "blocked",
                "next_action": "fix runtime-service executable",
                "a4_signal": "No-Go",
                "dominant_gap": "owner-boundary",
            }
        },
        manual_checks_confirmed=True,
    )

    assert result["gate_status"] == "blocked"
    assert result["verdict_kind"] == "blocked_hardware_smoke"
    assert result["dominant_gap"] == "owner-boundary"


def test_load_timestamped_report_accepts_utf8_bom_json(tmp_path: Path) -> None:
    report_dir = tmp_path / "20260412-214121"
    report_dir.mkdir(parents=True)
    json_path = report_dir / validation.SMOKE_SUMMARY_NAME
    json_path.write_text('{"summary":{"observation_result":"ready_for_gate_review"}}', encoding="utf-8-sig")

    loaded_report_dir, payload = validation.load_timestamped_report(tmp_path, validation.SMOKE_SUMMARY_NAME)

    assert loaded_report_dir == report_dir
    assert payload["summary"]["observation_result"] == "ready_for_gate_review"


def test_evaluate_probe_attempt_accepts_valid_pass_sample() -> None:
    result = validation.evaluate_probe_attempt(_probe_report())

    assert result["attempt_status"] == "valid_pass"
    assert result["failure_branch"] == ""
    assert result["validity_checks"]["mc_prftrap_not_detected"] is True


def test_evaluate_probe_attempt_marks_completed_terminal_as_skipped() -> None:
    result = validation.evaluate_probe_attempt(
        _probe_report(
            overall_status="skipped",
            verdict_kind="stop_window_missed",
            terminal_state="completed",
            axes_stopped_before_home=None,
            home_auto_ok=None,
        )
    )

    assert result["attempt_status"] == "skipped"
    assert "更早触发 stop" in result["message"]


def test_evaluate_probe_attempt_routes_motion_active_failure_to_stop_drain_branch() -> None:
    result = validation.evaluate_probe_attempt(
        _probe_report(
            overall_status="failed",
            verdict_kind="home_auto_failed_after_stop",
            axes_stopped_before_home=False,
            home_auto_ok=False,
            is_moving=True,
            remaining_segments=2,
            current_velocity=5.0,
        )
    )

    assert result["attempt_status"] == "failed"
    assert result["failure_branch"] == "stop_drain_incomplete"


def test_evaluate_probe_attempt_routes_stopped_prftrap_failure_to_axis_stopped_branch() -> None:
    result = validation.evaluate_probe_attempt(
        _probe_report(
            overall_status="failed",
            verdict_kind="mc_prftrap_after_stop",
            axes_stopped_before_home=True,
            home_auto_ok=False,
            mc_prftrap_detected=True,
            prftrap_hit_count=1,
        )
    )

    assert result["attempt_status"] == "failed"
    assert result["failure_branch"] == "axis_stopped_but_prftrap"


def test_evaluate_series_progress_passes_after_required_valid_runs() -> None:
    result = validation.evaluate_series_progress(
        valid_runs=3,
        skipped_runs=0,
        required_valid_runs=3,
        max_skipped_runs=3,
        last_attempt_status="valid_pass",
        last_failure_branch="",
    )

    assert result["done"] is True
    assert result["overall_status"] == "passed"


def test_evaluate_series_progress_blocks_when_skips_exceed_budget() -> None:
    result = validation.evaluate_series_progress(
        valid_runs=1,
        skipped_runs=4,
        required_valid_runs=3,
        max_skipped_runs=3,
        last_attempt_status="skipped",
        last_failure_branch="",
    )

    assert result["done"] is True
    assert result["overall_status"] == "blocked"
    assert result["verdict_kind"] == "trigger_window_unstable"


def test_evaluate_series_progress_stops_on_failed_attempt() -> None:
    result = validation.evaluate_series_progress(
        valid_runs=1,
        skipped_runs=0,
        required_valid_runs=3,
        max_skipped_runs=3,
        last_attempt_status="failed",
        last_failure_branch="axis_stopped_but_prftrap",
    )

    assert result["done"] is True
    assert result["overall_status"] == "failed"
    assert result["verdict_kind"] == "axis_stopped_but_prftrap"
