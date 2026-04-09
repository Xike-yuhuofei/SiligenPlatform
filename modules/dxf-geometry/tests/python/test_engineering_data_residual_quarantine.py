from __future__ import annotations

import json
from pathlib import Path
import sys

import pytest

TEST_ROOT = Path(__file__).resolve().parent
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from support import FIXTURE_ROOT, WORKSPACE_ROOT

from engineering_data.contracts.simulation_input import (
    DEFAULT_EXPORTS,
    TriggerPoint,
    bundle_contains_fallback_primitives,
    bundle_to_simulation_payload,
    collect_export_notes,
    count_fallback_primitives,
    count_skipped_points,
    load_path_bundle,
    project_trigger_points,
)
from engineering_data.processing.simulation_geometry import (
    bundle_contains_fallback_primitives as geometry_bundle_contains_fallback_primitives,
    collect_export_notes as geometry_collect_export_notes,
    count_fallback_primitives as geometry_count_fallback_primitives,
    count_skipped_points as geometry_count_skipped_points,
    load_path_bundle as geometry_load_path_bundle,
)
from motion_planning.contracts import Point2D as MotionPlanningPoint2D
from motion_planning.trajectory_generation import (
    build_trajectory_artifact as motion_planning_build_trajectory_artifact,
)
from runtime_execution.simulation_input import (
    bundle_to_simulation_payload as runtime_bundle_to_simulation_payload,
    project_trigger_points as runtime_project_trigger_points,
)


TRAJECTORY_COMPAT_SHELL = (
    WORKSPACE_ROOT
    / "modules"
    / "dxf-geometry"
    / "application"
    / "engineering_data"
    / "trajectory"
    / "offline_path_to_trajectory.py"
)
MODULE_TRAJECTORY_CLI = (
    WORKSPACE_ROOT
    / "modules"
    / "dxf-geometry"
    / "application"
    / "engineering_data"
    / "cli"
    / "path_to_trajectory.py"
)
WORKSPACE_TRAJECTORY_SCRIPT = WORKSPACE_ROOT / "scripts" / "engineering-data" / "path_to_trajectory.py"
SIMULATION_RUNTIME_RESIDUAL = (
    WORKSPACE_ROOT
    / "modules"
    / "dxf-geometry"
    / "application"
    / "engineering_data"
    / "residual"
    / "simulation_runtime_export.py"
)


def _load_json(name: str) -> dict:
    return json.loads((FIXTURE_ROOT / name).read_text(encoding="utf-8"))


def test_simulation_contract_surface_stays_bound_to_runtime_owner_and_geometry_helpers() -> None:
    assert bundle_to_simulation_payload is runtime_bundle_to_simulation_payload
    assert project_trigger_points is runtime_project_trigger_points
    assert load_path_bundle is geometry_load_path_bundle
    assert collect_export_notes is geometry_collect_export_notes
    assert count_skipped_points is geometry_count_skipped_points
    assert count_fallback_primitives is geometry_count_fallback_primitives
    assert bundle_contains_fallback_primitives is geometry_bundle_contains_fallback_primitives
    assert not SIMULATION_RUNTIME_RESIDUAL.exists()


def test_trajectory_shells_are_removed_from_dxf_geometry_live_surface() -> None:
    assert not TRAJECTORY_COMPAT_SHELL.exists()
    assert not MODULE_TRAJECTORY_CLI.exists()


def test_workspace_trajectory_entry_points_to_motion_planning_owner() -> None:
    script_text = WORKSPACE_TRAJECTORY_SCRIPT.read_text(encoding="utf-8")
    assert "motion_planning.trajectory_generation" in script_text
    assert "engineering_data.trajectory.offline_path_to_trajectory" not in script_text


def test_simulation_payload_projects_triggers_without_changing_fixture_notes() -> None:
    bundle = load_path_bundle(FIXTURE_ROOT / "rect_diag.pb")
    fixture_payload = _load_json("simulation-input.json")
    first_segment = fixture_payload["segments"][0]
    last_segment = fixture_payload["segments"][-1]

    trigger_points = [
        TriggerPoint(
            x_mm=float(first_segment["start"]["x"]),
            y_mm=float(first_segment["start"]["y"]),
            channel="DO_CAMERA_TRIGGER",
            state=True,
        ),
        TriggerPoint(
            x_mm=float(last_segment["end"]["x"]),
            y_mm=float(last_segment["end"]["y"]),
            channel="DO_CAMERA_TRIGGER",
            state=False,
        ),
    ]

    payload = bundle_to_simulation_payload(
        bundle=bundle,
        defaults=DEFAULT_EXPORTS,
        trigger_points=trigger_points,
    )

    assert payload["segments"] == fixture_payload["segments"]
    assert len(payload["triggers"]) == 2
    assert payload["triggers"][0]["trigger_position"] == pytest.approx(0.0)
    assert payload["triggers"][1]["trigger_position"] >= payload["triggers"][0]["trigger_position"]
    assert count_skipped_points(bundle) == 0
    assert count_fallback_primitives(bundle) == {}
    assert not bundle_contains_fallback_primitives(bundle)
    assert collect_export_notes(bundle) == []


def test_motion_planning_owner_trajectory_artifact_keeps_basic_behavior_and_error_branch() -> None:
    artifact = motion_planning_build_trajectory_artifact(
        points=[
            MotionPlanningPoint2D(0.0, 0.0),
            MotionPlanningPoint2D(3.0, 4.0),
            MotionPlanningPoint2D(6.0, 8.0),
        ],
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

    with pytest.raises(ValueError, match="Not enough points"):
        motion_planning_build_trajectory_artifact(
            points=[MotionPlanningPoint2D(0.0, 0.0)],
            vmax=5.0,
            amax=10.0,
            jmax=100.0,
            sample_dt=0.05,
            sample_ds=0.0,
        )
