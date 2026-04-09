from __future__ import annotations

from pathlib import Path
import subprocess
import sys

import pytest

TEST_ROOT = Path(__file__).resolve().parent
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from support import WORKSPACE_ROOT, pythonpath_env


WORKSPACE_TRAJECTORY_SCRIPT = WORKSPACE_ROOT / "scripts" / "engineering-data" / "path_to_trajectory.py"
WORKSPACE_EXPORT_SIMULATION_INPUT_SCRIPT = WORKSPACE_ROOT / "scripts" / "engineering-data" / "export_simulation_input.py"
MODULE_TRAJECTORY_CLI = (
    WORKSPACE_ROOT
    / "modules"
    / "dxf-geometry"
    / "application"
    / "engineering_data"
    / "cli"
    / "path_to_trajectory.py"
)


@pytest.mark.parametrize(
    "module_name",
    [
        "engineering_data.cli.dxf_to_pb",
        "engineering_data.cli.export_simulation_input",
    ],
)
def test_engineering_data_cli_help_smoke(module_name: str) -> None:
    result = subprocess.run(
        [sys.executable, "-m", module_name, "--help"],
        capture_output=True,
        text=True,
        env=pythonpath_env(),
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "usage:" in result.stdout.lower()


def test_workspace_trajectory_cli_help_smoke() -> None:
    result = subprocess.run(
        [sys.executable, str(WORKSPACE_TRAJECTORY_SCRIPT), "--help"],
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "usage:" in result.stdout.lower()


def test_workspace_export_simulation_input_cli_help_smoke() -> None:
    result = subprocess.run(
        [sys.executable, str(WORKSPACE_EXPORT_SIMULATION_INPUT_SCRIPT), "--help"],
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "usage:" in result.stdout.lower()


def test_module_trajectory_cli_is_deleted_after_owner_cutover() -> None:
    assert not MODULE_TRAJECTORY_CLI.exists()
