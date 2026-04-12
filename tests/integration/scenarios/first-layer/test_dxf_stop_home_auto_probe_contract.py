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
        sys.path.insert(0, str(candidate))

import run_dxf_stop_home_auto_probe as probe


def _status_payload(
    *,
    homed_x: bool = True,
    homed_y: bool = True,
    home_boundary_x: bool = False,
    home_boundary_y: bool = False,
    estop: bool = False,
    door_open: bool = False,
) -> dict[str, object]:
    return {
        "result": {
            "axes": {
                "X": {"homed": homed_x},
                "Y": {"homed": homed_y},
            },
            "effective_interlocks": {
                "home_boundary_x_active": home_boundary_x,
                "home_boundary_y_active": home_boundary_y,
            },
            "io": {
                "estop": estop,
                "door": door_open,
                "limit_x_pos": False,
                "limit_x_neg": False,
                "limit_y_pos": False,
                "limit_y_neg": False,
            },
        }
    }


def test_extract_prftrap_hits_returns_only_matching_lines() -> None:
    hits = probe.extract_prftrap_hits(
        "\n".join(
            (
                "[motion] ok",
                "MC_PrfTrap failed for axis 0 with error code: 1",
                "something else",
                "prftrap secondary trace",
            )
        )
    )

    assert hits == [
        "MC_PrfTrap failed for axis 0 with error code: 1",
        "prftrap secondary trace",
    ]


def test_summarize_home_auto_response_collects_failed_axis_messages() -> None:
    summary = probe.summarize_home_auto_response(
        {
            "result": {
                "accepted": False,
                "summary_state": "failed",
                "message": "ready zero rejected",
                "axis_results": [
                    {"axis": "X", "success": False, "message": "MC_PrfTrap failed for axis 0 with error code: 1"},
                    {"axis": "Y", "success": True, "message": ""},
                ],
            }
        }
    )

    assert summary["ok"] is False
    assert summary["summary_state"] == "failed"
    assert summary["failed_axis_messages"] == ["X: MC_PrfTrap failed for axis 0 with error code: 1"]
    assert summary["contains_prftrap"] is True


def test_should_issue_stop_requires_running_motion_and_min_samples() -> None:
    observation = {
        "job_status": {"state": "running"},
        "coord_status": {
            "is_moving": True,
            "current_velocity": 10.0,
            "remaining_segments": 3,
            "raw_segment": 2,
            "raw_status_word": 1,
            "axes": {
                "X": {"position": 1.0, "velocity": 10.0},
                "Y": {"position": 0.5, "velocity": 5.0},
            },
            "position": {"x": 1.0, "y": 0.5},
        },
    }
    coord_window = [
        {
            "is_moving": False,
            "current_velocity": 0.0,
            "remaining_segments": 0,
            "raw_segment": 0,
            "raw_status_word": 0,
            "axes": {
                "X": {"position": 0.0, "velocity": 0.0},
                "Y": {"position": 0.0, "velocity": 0.0},
            },
            "position": {"x": 0.0, "y": 0.0},
        },
        observation["coord_status"],
    ]

    assert (
        probe.should_issue_stop(
            observation,
            coord_window=coord_window,
            running_sample_count=1,
            min_running_samples=2,
            position_epsilon_mm=0.001,
            velocity_epsilon_mm_s=0.001,
        )
        is False
    )
    assert (
        probe.should_issue_stop(
            observation,
            coord_window=coord_window,
            running_sample_count=2,
            min_running_samples=2,
            position_epsilon_mm=0.001,
            velocity_epsilon_mm_s=0.001,
        )
        is True
    )


def test_snapshot_indicates_axes_stopped_rejects_active_motion() -> None:
    snapshot = {
        "coord_status": {
            "is_moving": False,
            "current_velocity": 0.0,
            "remaining_segments": 0,
            "raw_segment": 0,
            "raw_status_word": 0,
            "axes": {
                "X": {"velocity": 0.0},
                "Y": {"velocity": 0.2},
            },
        }
    }

    assert probe.snapshot_indicates_axes_stopped(snapshot, velocity_epsilon_mm_s=0.001) is False


def test_snapshot_indicates_axes_stopped_accepts_program_stop_with_zero_axis_velocity() -> None:
    snapshot = {
        "coord_status": {
            "is_moving": False,
            "current_velocity": 0.0,
            "remaining_segments": 0,
            "raw_segment": -858993460,
            "raw_status_word": probe.dryrun.CRDSYS_STATUS_PROG_STOP,
            "axes": {
                "X": {"velocity": 0.0},
                "Y": {"velocity": 0.0},
            },
        }
    }

    assert probe.snapshot_indicates_axes_stopped(snapshot, velocity_epsilon_mm_s=0.001) is True


def test_evaluate_preflight_home_normalization_requires_home_when_axes_non_homed() -> None:
    decision = probe.evaluate_preflight_home_normalization(
        _status_payload(homed_x=False, homed_y=False),
    )

    assert decision == {
        "non_homed_axes": ["X", "Y"],
        "home_boundary_axes": [],
        "should_home": True,
    }


def test_evaluate_preflight_home_normalization_requires_home_when_home_boundary_is_active() -> None:
    decision = probe.evaluate_preflight_home_normalization(
        _status_payload(home_boundary_x=True),
    )

    assert decision == {
        "non_homed_axes": [],
        "home_boundary_axes": ["X"],
        "should_home": True,
    }


def test_evaluate_preflight_home_normalization_keeps_non_home_safety_blockers() -> None:
    try:
        probe.evaluate_preflight_home_normalization(
            _status_payload(estop=True),
        )
    except RuntimeError as exc:
        assert "estop" in str(exc)
    else:
        raise AssertionError("expected RuntimeError for estop safety blocker")
