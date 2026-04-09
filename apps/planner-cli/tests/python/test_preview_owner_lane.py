from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

TEST_ROOT = Path(__file__).resolve().parent
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from support import FIXTURE_ROOT, WORKSPACE_PREVIEW_SCRIPT

from planner_cli_preview.contracts import PreviewRequest
from planner_cli_preview.preview_renderer import generate_preview


def _load_json(name: str) -> dict:
    return json.loads((FIXTURE_ROOT / name).read_text(encoding="utf-8"))


def _run_workspace_preview(tmp_path: Path, *, deterministic: bool) -> tuple[dict, str]:
    args = [
        sys.executable,
        str(WORKSPACE_PREVIEW_SCRIPT),
        "--input",
        str(FIXTURE_ROOT / "rect_diag.dxf"),
        "--output-dir",
        str(tmp_path),
        "--title",
        "rect_diag",
        "--speed",
        "10.0",
        "--json",
    ]
    if deterministic:
        args.append("--deterministic")

    result = subprocess.run(args, capture_output=True, text=True, check=False)
    assert result.returncode == 0, result.stderr
    return json.loads(result.stdout), result.stdout


def test_preview_owner_matches_fixture(tmp_path) -> None:
    artifact = generate_preview(
        PreviewRequest(
            input_path=FIXTURE_ROOT / "rect_diag.dxf",
            output_dir=tmp_path,
            title="rect_diag",
            speed_mm_s=10.0,
            deterministic_name=True,
        )
    )
    html_text = (tmp_path / "rect_diag-preview.html").read_text(encoding="utf-8")
    expected = _load_json("preview-artifact.json")

    assert artifact.to_dict()["preview_path"].endswith("rect_diag-preview.html")
    assert artifact.entity_count == expected["entity_count"]
    assert artifact.segment_count == expected["segment_count"]
    assert artifact.point_count == expected["point_count"]
    assert artifact.total_length_mm == pytest.approx(expected["total_length_mm"])
    assert artifact.estimated_time_s == pytest.approx(expected["estimated_time_s"])
    assert artifact.width_mm == pytest.approx(expected["width_mm"])
    assert artifact.height_mm == pytest.approx(expected["height_mm"])
    assert "<svg" in html_text
    assert "rect_diag" in html_text


def test_workspace_preview_wrapper_help_smoke() -> None:
    result = subprocess.run(
        [sys.executable, str(WORKSPACE_PREVIEW_SCRIPT), "--help"],
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "usage:" in result.stdout.lower()


def test_workspace_preview_wrapper_matches_fixture_and_is_deterministic(tmp_path) -> None:
    payload_a, _ = _run_workspace_preview(tmp_path, deterministic=True)
    html_a = Path(payload_a["preview_path"]).read_text(encoding="utf-8")

    payload_b, _ = _run_workspace_preview(tmp_path, deterministic=True)
    html_b = Path(payload_b["preview_path"]).read_text(encoding="utf-8")
    expected = _load_json("preview-artifact.json")

    assert payload_a["preview_path"].endswith("rect_diag-preview.html")
    assert payload_a == payload_b
    assert html_a == html_b
    assert payload_a["entity_count"] == expected["entity_count"]
    assert payload_a["segment_count"] == expected["segment_count"]
    assert payload_a["point_count"] == expected["point_count"]
    assert payload_a["total_length_mm"] == pytest.approx(expected["total_length_mm"])
    assert payload_a["estimated_time_s"] == pytest.approx(expected["estimated_time_s"])
    assert payload_a["width_mm"] == pytest.approx(expected["width_mm"])
    assert payload_a["height_mm"] == pytest.approx(expected["height_mm"])
