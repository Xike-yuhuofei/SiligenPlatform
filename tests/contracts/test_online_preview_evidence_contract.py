from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
FIRST_LAYER_DIR = ROOT / "tests" / "e2e" / "first-layer"
if str(FIRST_LAYER_DIR) not in sys.path:
    sys.path.insert(0, str(FIRST_LAYER_DIR))

from runtime_gateway_harness import resolve_default_exe  # noqa: E402


SCRIPT_PATH = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_real_dxf_preview_snapshot.py"


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _single_report_dir(report_root: Path) -> Path:
    dirs = [path for path in report_root.iterdir() if path.is_dir()]
    assert len(dirs) == 1, f"expected exactly one report dir in {report_root}, got {len(dirs)}"
    return dirs[0]


def test_online_preview_evidence_bundle_contract(tmp_path: Path) -> None:
    gateway_exe = resolve_default_exe("siligen_runtime_gateway.exe")
    if not gateway_exe.exists():
        pytest.skip(f"runtime gateway executable missing: {gateway_exe}")

    report_root = tmp_path / "preview-evidence"
    report_root.mkdir(parents=True, exist_ok=True)

    completed = subprocess.run(
        [
            sys.executable,
            str(SCRIPT_PATH),
            "--gateway-exe",
            str(gateway_exe),
            "--report-root",
            str(report_root),
        ],
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        timeout=180,
        check=False,
    )
    assert completed.returncode == 0, (
        "preview evidence script failed\n"
        f"stdout:\n{completed.stdout}\n"
        f"stderr:\n{completed.stderr}"
    )

    report_dir = _single_report_dir(report_root)
    required_paths = {
        "plan_prepare": report_dir / "plan-prepare.json",
        "snapshot": report_dir / "snapshot.json",
        "glue_points": report_dir / "glue_points.json",
        "motion_preview": report_dir / "motion_preview.json",
        "execution_polyline": report_dir / "execution_polyline.json",
        "preview_verdict": report_dir / "preview-verdict.json",
        "preview_evidence": report_dir / "preview-evidence.md",
        "hmi_preview": report_dir / "hmi-preview.png",
        "online_smoke_log": report_dir / "online-smoke.log",
    }
    for label, path in required_paths.items():
        assert path.exists(), f"missing {label} artifact: {path}"

    plan_prepare = _load_json(required_paths["plan_prepare"])
    snapshot = _load_json(required_paths["snapshot"])
    motion_preview = json.loads(required_paths["motion_preview"].read_text(encoding="utf-8"))
    preview_verdict = _load_json(required_paths["preview_verdict"])
    preview_evidence = required_paths["preview_evidence"].read_text(encoding="utf-8")

    assert {"plan_id", "plan_fingerprint"}.issubset(plan_prepare.keys())
    assert {
        "snapshot_id",
        "snapshot_hash",
        "plan_id",
        "preview_state",
        "preview_source",
        "preview_kind",
        "glue_points",
        "motion_preview",
        "execution_polyline",
    }.issubset(snapshot.keys())
    assert {
        "source",
        "kind",
        "source_point_count",
        "point_count",
        "is_sampled",
        "sampling_strategy",
        "polyline",
    }.issubset(snapshot["motion_preview"].keys())
    assert {
        "verdict",
        "launch_mode",
        "online_ready",
        "dxf_file",
        "artifact_id",
        "preview_source",
        "preview_kind",
        "snapshot_hash",
        "plan_id",
        "plan_fingerprint",
        "geometry_semantics_match",
        "order_semantics_match",
        "dispense_motion_semantics_match",
        "glue_point_count",
        "motion_preview_source",
        "motion_preview_kind",
        "motion_preview_point_count",
        "motion_preview_source_point_count",
        "motion_preview_is_sampled",
        "motion_preview_sampling_strategy",
        "execution_polyline_source_point_count",
        "glue_point_spacing_median_mm",
        "corner_duplicate_point_count",
    }.issubset(preview_verdict.keys())
    assert preview_verdict["verdict"] in {
        "passed",
        "failed",
        "invalid-source",
        "mismatch",
        "not-ready",
        "incomplete",
    }
    assert preview_verdict["launch_mode"] == "online"
    assert preview_verdict["plan_id"] == plan_prepare["plan_id"]
    assert preview_verdict["plan_fingerprint"] == plan_prepare["plan_fingerprint"]
    assert preview_verdict["snapshot_hash"] == snapshot["snapshot_hash"]
    assert preview_verdict["preview_source"] == snapshot["preview_source"]
    assert preview_verdict["preview_kind"] == snapshot["preview_kind"]
    assert preview_verdict["preview_source"] == "planned_glue_snapshot"
    assert preview_verdict["preview_kind"] == "glue_points"
    assert preview_verdict["glue_point_count"] == snapshot["glue_point_count"]
    assert preview_verdict["motion_preview_source"] == snapshot["motion_preview"]["source"]
    assert preview_verdict["motion_preview_kind"] == snapshot["motion_preview"]["kind"]
    assert preview_verdict["motion_preview_point_count"] == snapshot["motion_preview"]["point_count"]
    assert (
        preview_verdict["motion_preview_source_point_count"]
        == snapshot["motion_preview"]["source_point_count"]
    )
    assert preview_verdict["motion_preview_is_sampled"] == snapshot["motion_preview"]["is_sampled"]
    assert (
        preview_verdict["motion_preview_sampling_strategy"]
        == snapshot["motion_preview"]["sampling_strategy"]
    )
    assert snapshot["motion_preview"]["point_count"] == len(motion_preview)
    assert preview_verdict["execution_polyline_source_point_count"] == snapshot["execution_polyline_source_point_count"]
    assert "plan-prepare.json" in preview_evidence
    assert "preview-verdict.json" in preview_evidence
    assert "glue_points.json" in preview_evidence
    assert "motion_preview.json" in preview_evidence
    assert "execution_polyline.json" in preview_evidence
    assert "hmi-preview.png" in preview_evidence
