from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11
ROOT = Path(__file__).resolve().parents[2]
SIM_EXE = ROOT / "packages" / "simulation-engine" / "build" / "Debug" / "simulate_dxf_path.exe"
SIM_INPUT = ROOT / "packages" / "engineering-contracts" / "fixtures" / "cases" / "rect_diag" / "simulation-input.json"
BASELINE = ROOT / "integration" / "regression-baselines" / "rect_diag.simulation-baseline.json"


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _summarize(payload: dict) -> dict:
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


def main() -> int:
    if not SIM_EXE.exists():
        print(f"simulation-engine executable missing: {SIM_EXE}", file=sys.stderr)
        return KNOWN_FAILURE_EXIT_CODE

    baseline = _load_json(BASELINE)
    with tempfile.TemporaryDirectory() as temp_dir:
        output_path = Path(temp_dir) / "rect_diag.result.json"
        completed = subprocess.run(
            [str(SIM_EXE), str(SIM_INPUT), str(output_path)],
            cwd=str(ROOT),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=30,
        )
        if completed.returncode != 0:
            print(completed.stdout)
            print(completed.stderr, file=sys.stderr)
            return completed.returncode
        summary = _summarize(_load_json(output_path))

    numeric_keys = ("total_time", "motion_distance", "valve_open_time", "final_velocity")
    for key in numeric_keys:
        if abs(summary[key] - baseline[key]) > 1e-9:
            print(f"simulated-line regression failed: {key}={summary[key]} expected={baseline[key]}", file=sys.stderr)
            return 1

    exact_keys = ("motion_profile_count", "final_segment_index", "io_event_count", "valve_event_count")
    for key in exact_keys:
        if summary[key] != baseline[key]:
            print(f"simulated-line regression failed: {key}={summary[key]} expected={baseline[key]}", file=sys.stderr)
            return 1

    print("simulated-line regression passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
