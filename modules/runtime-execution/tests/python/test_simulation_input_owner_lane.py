from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import pytest

TEST_ROOT = Path(__file__).resolve().parent
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from support import FIXTURE_ROOT, pythonpath_env

from engineering_data.processing.simulation_geometry import collect_export_notes, load_path_bundle
from runtime_execution import DEFAULT_EXPORTS, bundle_to_simulation_payload


def _load_json(name: str) -> dict:
    return json.loads((FIXTURE_ROOT / name).read_text(encoding="utf-8"))


def test_runtime_execution_owner_payload_matches_canonical_fixture() -> None:
    bundle = load_path_bundle(FIXTURE_ROOT / "rect_diag.pb")

    payload = bundle_to_simulation_payload(
        bundle=bundle,
        defaults=DEFAULT_EXPORTS,
    )

    assert payload == _load_json("simulation-input.json")
    assert collect_export_notes(bundle) == []


def test_runtime_execution_export_cli_writes_payload_and_projects_triggers(tmp_path: Path) -> None:
    fixture_payload = _load_json("simulation-input.json")
    first_segment = fixture_payload["segments"][0]
    last_segment = fixture_payload["segments"][-1]
    trigger_csv = tmp_path / "triggers.csv"
    trigger_csv.write_text(
        "\n".join(
            [
                "x_mm,y_mm,channel,state",
                f"{first_segment['start']['x']},{first_segment['start']['y']},DO_CAMERA_TRIGGER,true",
                f"{last_segment['end']['x']},{last_segment['end']['y']},DO_CAMERA_TRIGGER,false",
            ]
        ),
        encoding="utf-8",
    )
    output_path = tmp_path / "runtime-simulation-input.json"

    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "runtime_execution.export_simulation_input",
            "--input",
            str(FIXTURE_ROOT / "rect_diag.pb"),
            "--output",
            str(output_path),
            "--trigger-points-csv",
            str(trigger_csv),
        ],
        capture_output=True,
        text=True,
        env=pythonpath_env(),
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert output_path.exists()

    payload = json.loads(output_path.read_text(encoding="utf-8"))
    assert payload["segments"] == fixture_payload["segments"]
    assert len(payload["triggers"]) == 2
    assert payload["triggers"][0]["trigger_position"] == pytest.approx(0.0)
    assert payload["triggers"][1]["trigger_position"] >= payload["triggers"][0]["trigger_position"]
    assert payload["triggers"][0]["state"] is True
    assert payload["triggers"][1]["state"] is False
    assert "Mapped 2 trigger points" in result.stdout


def test_runtime_execution_export_cli_help_smoke() -> None:
    result = subprocess.run(
        [sys.executable, "-m", "runtime_execution.export_simulation_input", "--help"],
        capture_output=True,
        text=True,
        env=pythonpath_env(),
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "usage:" in result.stdout.lower()
