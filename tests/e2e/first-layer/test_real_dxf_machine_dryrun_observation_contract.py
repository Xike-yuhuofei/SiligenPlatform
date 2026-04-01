from __future__ import annotations

import argparse
import sys
from pathlib import Path


THIS_DIR = Path(__file__).resolve().parent
ROOT = THIS_DIR.parents[2]
FIRST_LAYER_DIR = THIS_DIR
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"

for candidate in (FIRST_LAYER_DIR, HIL_DIR):
    candidate_str = str(candidate)
    if candidate_str not in sys.path:
        sys.path.insert(0, candidate_str)

import run_real_dxf_machine_dryrun as dryrun


def test_load_connection_params_reads_network_ips(tmp_path: Path) -> None:
    config_path = tmp_path / "machine_config.ini"
    config_path.write_text(
        "[Network]\ncontrol_card_ip=192.168.0.1\nlocal_ip=192.168.0.200\n",
        encoding="utf-8",
    )

    params = dryrun.load_connection_params(config_path)

    assert params == {"card_ip": "192.168.0.1", "local_ip": "192.168.0.200"}


def test_resolve_job_timeout_budget_uses_estimate_scale_and_buffer() -> None:
    budget = dryrun.resolve_job_timeout_budget(
        configured_timeout_seconds=120.0,
        estimated_time_seconds=108.2842788696289,
        timeout_scale=2.0,
        timeout_buffer_seconds=15.0,
    )

    assert budget["configured_timeout_seconds"] == 120.0
    assert budget["estimated_time_seconds"] == 108.2842788696289
    assert budget["timeout_scale"] == 2.0
    assert budget["timeout_buffer_seconds"] == 15.0
    assert budget["effective_timeout_seconds"] == 231.5685577392578


def test_resolve_job_timeout_budget_keeps_configured_floor_without_estimate() -> None:
    budget = dryrun.resolve_job_timeout_budget(
        configured_timeout_seconds=120.0,
        estimated_time_seconds=None,
        timeout_scale=2.0,
        timeout_buffer_seconds=15.0,
    )

    assert budget["estimated_time_seconds"] is None
    assert budget["effective_timeout_seconds"] == 120.0


class FakeTcpClient:
    def __init__(self, responses: dict[str, dict]) -> None:
        self._responses = responses
        self.calls: list[tuple[str, dict | None, float]] = []

    def send_request(self, method: str, params: dict | None, timeout_seconds: float) -> dict:
        self.calls.append((method, params, timeout_seconds))
        return self._responses[method]


def test_collect_poll_observation_captures_status_job_and_coord() -> None:
    client = FakeTcpClient(
        {
            "status": {
                "result": {
                    "connected": True,
                    "machine_state": "Idle",
                    "machine_state_reason": "ready",
                    "io": {
                        "estop": False,
                        "door": False,
                        "door_known": True,
                        "limit_x_pos": False,
                        "limit_x_neg": False,
                        "limit_y_pos": False,
                        "limit_y_neg": False,
                    },
                    "effective_interlocks": {
                        "home_boundary_x_active": False,
                        "home_boundary_y_active": False,
                    },
                    "axes": {
                        "X": {"position": 1.25, "velocity": 0.0, "homed": True, "enabled": True},
                        "Y": {"position": -0.5, "velocity": 0.0, "homed": True, "enabled": True},
                    },
                }
            },
            "dxf.job.status": {
                "result": {
                    "job_id": "job-1",
                    "state": "running",
                    "current_segment": 2,
                    "total_segments": 5,
                    "cycle_progress_percent": 40,
                    "overall_progress_percent": 40,
                }
            },
            "motion.coord.status": {
                "result": {
                    "coord_sys": 1,
                    "state": 3,
                    "is_moving": False,
                    "remaining_segments": 0,
                    "current_velocity": 0.0,
                    "raw_status_word": 0,
                    "raw_segment": 0,
                    "mc_status_ret": 0,
                    "buffer_space": 64,
                    "lookahead_space": 64,
                    "position": {"x": 1.25, "y": -0.5},
                    "axes": {
                        "X": {"position": 1.25, "velocity": 0.0, "state": 5, "homed": True, "enabled": True},
                        "Y": {"position": -0.5, "velocity": 0.0, "state": 5, "homed": True, "enabled": True},
                    },
                }
            },
        }
    )

    observation = dryrun.collect_poll_observation(client, job_id="job-1", poll_index=7)

    assert [call[0] for call in client.calls] == ["status", "dxf.job.status", "motion.coord.status"]
    assert client.calls[1][1] == {"job_id": "job-1"}
    assert client.calls[2][1] == {"coord_sys": dryrun.DEFAULT_COORD_SYS}

    machine_status = observation["machine_status"]
    job_status = observation["job_status"]
    coord_status = observation["coord_status"]

    assert machine_status["poll_index"] == 7
    assert job_status["poll_index"] == 7
    assert coord_status["poll_index"] == 7
    assert machine_status["sampled_at"] == job_status["sampled_at"] == coord_status["sampled_at"]
    assert machine_status["machine_state"] == "Idle"
    assert machine_status["io"]["door_known"] is True
    assert machine_status["axes"]["X"]["position"] == 1.25
    assert job_status["state"] == "running"
    assert job_status["current_segment"] == 2
    assert coord_status["coord_sys"] == 1
    assert coord_status["state"] == 3
    assert coord_status["axes"]["Y"]["position"] == -0.5


def test_build_report_keeps_legacy_history_and_new_contract() -> None:
    args = argparse.Namespace(
        gateway_exe=Path("D:/fake/siligen_runtime_gateway.exe"),
        config_path=Path("D:/fake/machine_config.ini"),
        dxf_file=Path("D:/fake/sample.dxf"),
        contradiction_progress_threshold_percent=dryrun.DEFAULT_CONTRADICTION_PROGRESS_THRESHOLD_PERCENT,
        contradiction_consecutive_samples=dryrun.DEFAULT_CONTRADICTION_CONSECUTIVE_SAMPLES,
        contradiction_grace_seconds=dryrun.DEFAULT_CONTRADICTION_GRACE_SECONDS,
        position_epsilon_mm=dryrun.DEFAULT_POSITION_EPSILON_MM,
        velocity_epsilon_mm_s=dryrun.DEFAULT_VELOCITY_EPSILON_MM_S,
        job_timeout=120.0,
        job_timeout_scale=dryrun.DEFAULT_JOB_TIMEOUT_SCALE,
        job_timeout_buffer_seconds=dryrun.DEFAULT_JOB_TIMEOUT_BUFFER_SECONDS,
    )
    steps = [dryrun.Step(name="dxf-job-start", status="passed", note="job_id=job-1", timestamp="2026-03-31T00:00:00Z")]
    job_history = [{"job_id": "job-1", "state": "running", "poll_index": 0, "sampled_at": "2026-03-31T00:00:01Z"}]
    machine_history = [
        {
            "machine_state": "Idle",
            "poll_index": 0,
            "sampled_at": "2026-03-31T00:00:01Z",
            "io": {},
            "axes": {},
        }
    ]
    coord_history = [
        {
            "coord_sys": 1,
            "state": 3,
            "poll_index": 0,
            "sampled_at": "2026-03-31T00:00:01Z",
            "position": {"x": 0.0, "y": 0.0},
            "axes": {},
        }
    ]
    phase_timeline = [{"phase": "start", "source": "dxf.job.start", "detail": {"job_id": "job-1"}, "timestamp": "2026-03-31T00:00:00Z"}]
    verdict = dryrun.build_report_verdict(
        verdict_kind="motion_timeout_unclassified",
        final_job_status={"state": "failed", "error_message": "等待运动完成超时"},
        error_message="dry-run job did not complete successfully",
    )

    report = dryrun.build_report(
        args=args,
        steps=steps,
        artifacts={
            "job_timeout_budget": {
                "configured_timeout_seconds": 120.0,
                "estimated_time_seconds": 108.2842788696289,
                "timeout_scale": 2.0,
                "timeout_buffer_seconds": 15.0,
                "effective_timeout_seconds": 231.5685577392578,
            }
        },
        overall_status="failed",
        error_message="dry-run job did not complete successfully",
        job_status_history=job_history,
        machine_status_history=machine_history,
        coord_status_history=coord_history,
        phase_timeline=phase_timeline,
        verdict=verdict,
        first_contradiction_sample=None,
    )

    assert report["job_status_history"] == job_history
    assert report["machine_status_history"] == machine_history
    assert report["coord_status_history"] == coord_history
    assert report["phase_timeline"] == phase_timeline
    assert report["verdict"]["kind"] == "motion_timeout_unclassified"
    assert report["observation_summary"]["job_status_samples"] == 1
    assert report["observation_summary"]["machine_status_samples"] == 1
    assert report["observation_summary"]["coord_status_samples"] == 1
    assert report["observation_summary"]["effective_job_timeout_seconds"] == 231.5685577392578

    evidence_contract = report["evidence_contract"]
    assert evidence_contract["authority"]["physical_execution"] == "motion.coord.status + axis feedback"
    assert evidence_contract["authority"]["job_status"] == "display_progress_only"
    assert "coord_running" in evidence_contract["phase_enum"]
    assert "state_contradiction" in evidence_contract["verdict_enum"]
    assert "state_contradiction" in evidence_contract["verdict_detection_enabled"]
    assert "job_status_history" in evidence_contract["compatibility_fields_retained"]


def test_update_runtime_phases_adds_coord_running_and_axis_motion() -> None:
    phase_timeline: list[dict] = []
    coord_window = [
        {
            "sampled_at": "2026-03-31T00:00:00+00:00",
            "poll_index": 0,
            "is_moving": False,
            "remaining_segments": 0,
            "raw_status_word": 0,
            "raw_segment": 0,
            "current_velocity": 0.0,
            "position": {"x": 0.0, "y": 0.0},
            "axes": {
                "X": {"position": 0.0, "velocity": 0.0},
                "Y": {"position": 0.0, "velocity": 0.0},
            },
        },
        {
            "sampled_at": "2026-03-31T00:00:01+00:00",
            "poll_index": 1,
            "is_moving": True,
            "remaining_segments": 2,
            "raw_status_word": 1,
            "raw_segment": 1,
            "current_velocity": 10.0,
            "position": {"x": 1.0, "y": 0.5},
            "axes": {
                "X": {"position": 1.0, "velocity": 10.0},
                "Y": {"position": 0.5, "velocity": 5.0},
            },
        },
    ]

    coord_running_recorded, axis_motion_recorded = dryrun.update_runtime_phases(
        phase_timeline,
        coord_status=coord_window[-1],
        coord_running_recorded=False,
        axis_motion_recorded=False,
        coord_window=coord_window,
        position_epsilon_mm=dryrun.DEFAULT_POSITION_EPSILON_MM,
        velocity_epsilon_mm_s=dryrun.DEFAULT_VELOCITY_EPSILON_MM_S,
    )

    assert coord_running_recorded is True
    assert axis_motion_recorded is True
    assert [item["phase"] for item in phase_timeline] == ["coord_running", "axis_motion"]


def test_detect_state_contradiction_on_high_progress_zero_motion_window() -> None:
    job_status_history = []
    coord_status_history = []
    for index in range(5):
        timestamp = f"2026-03-31T00:00:0{index}+00:00"
        job_status_history.append(
            {
                "sampled_at": timestamp,
                "poll_index": index,
                "state": "running",
                "current_segment": 4,
                "total_segments": 5,
                "cycle_progress_percent": 99,
                "overall_progress_percent": 99,
            }
        )
        coord_status_history.append(
            {
                "sampled_at": timestamp,
                "poll_index": index,
                "coord_sys": 1,
                "state": 0,
                "is_moving": False,
                "remaining_segments": 0,
                "current_velocity": 0.0,
                "raw_status_word": 0,
                "raw_segment": 0,
                "position": {"x": 0.0, "y": 0.0},
                "axes": {
                    "X": {"position": 0.0, "velocity": 0.0},
                    "Y": {"position": 0.0, "velocity": 0.0},
                },
            }
        )

    contradiction = dryrun.detect_state_contradiction(
        job_status_history=job_status_history,
        coord_status_history=coord_status_history,
        progress_threshold_percent=dryrun.DEFAULT_CONTRADICTION_PROGRESS_THRESHOLD_PERCENT,
        consecutive_samples=dryrun.DEFAULT_CONTRADICTION_CONSECUTIVE_SAMPLES,
        grace_seconds=0.0,
        position_epsilon_mm=dryrun.DEFAULT_POSITION_EPSILON_MM,
        velocity_epsilon_mm_s=dryrun.DEFAULT_VELOCITY_EPSILON_MM_S,
    )

    assert contradiction is not None
    assert contradiction["kind"] == "state_contradiction"
    assert contradiction["first_poll_index"] == 0
    assert contradiction["last_poll_index"] == 4
