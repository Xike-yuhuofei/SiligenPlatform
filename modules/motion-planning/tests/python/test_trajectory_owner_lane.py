from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

TEST_ROOT = Path(__file__).resolve().parent
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from support import pythonpath_env

from motion_planning.contracts import Point2D
from motion_planning.trajectory_generation import build_trajectory_artifact


def test_motion_planning_owner_builds_expected_trajectory() -> None:
    artifact = build_trajectory_artifact(
        points=[Point2D(0.0, 0.0), Point2D(3.0, 4.0), Point2D(6.0, 8.0)],
        vmax=5.0,
        amax=10.0,
        jmax=100.0,
        sample_dt=0.05,
        sample_ds=0.0,
    )

    assert artifact.total_length == pytest.approx(10.0)
    assert artifact.total_time > 0.0
    assert artifact.samples
    assert artifact.planning_report.total_length_mm == pytest.approx(10.0)


def test_motion_planning_owner_help_smoke() -> None:
    result = subprocess.run(
        [sys.executable, "-m", "motion_planning.trajectory_generation", "--help"],
        capture_output=True,
        text=True,
        env=pythonpath_env(),
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "usage:" in result.stdout.lower()


def test_motion_planning_owner_json_contract_is_stable(tmp_path: Path) -> None:
    input_path = tmp_path / "path.json"
    output_path = tmp_path / "trajectory.json"
    input_path.write_text(
        json.dumps(
            {
                "points": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 100.0, "y": 0.0},
                    {"x": 100.0, "y": 100.0},
                ]
            }
        ),
        encoding="utf-8",
    )

    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "motion_planning.trajectory_generation",
            "--input",
            str(input_path),
            "--output",
            str(output_path),
            "--vmax",
            "10.0",
            "--amax",
            "20.0",
            "--jmax",
            "100.0",
            "--sample-dt",
            "0.05",
        ],
        capture_output=True,
        text=True,
        env=pythonpath_env(),
        check=False,
    )

    assert result.returncode == 0, result.stderr
    payload = json.loads(output_path.read_text(encoding="utf-8"))
    assert sorted(payload.keys()) == ["planningReport", "points", "totalLength", "totalTime"]
    assert payload["points"]
    assert payload["planningReport"]["totalLengthMm"] == pytest.approx(200.0)
    assert payload["totalLength"] == pytest.approx(200.0)
