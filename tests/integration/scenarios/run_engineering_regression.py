from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENGINEERING_DATA_SRC = ROOT / "modules" / "dxf-geometry" / "application"
FIXTURE_ROOT = ROOT / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag"
ENGINEERING_DATA_BRIDGE = ROOT / "scripts" / "engineering-data"

sys.path.insert(0, str(ENGINEERING_DATA_SRC))

from engineering_data.contracts.simulation_input import load_path_bundle  # noqa: E402


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    dxf_path = FIXTURE_ROOT / "rect_diag.dxf"
    expected_pb = load_path_bundle(FIXTURE_ROOT / "rect_diag.pb")
    expected_sim = _load_json(FIXTURE_ROOT / "simulation-input.json")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_root = Path(temp_dir)
        pb_path = temp_root / "rect_diag.pb"
        sim_path = temp_root / "rect_diag.simulation-input.json"

        export_pb = subprocess.run(
            [
                sys.executable,
                str(ENGINEERING_DATA_BRIDGE / "dxf_to_pb.py"),
                "--input", str(dxf_path),
                "--output", str(pb_path),
            ],
            cwd=str(ROOT),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        if export_pb.returncode != 0:
            print(export_pb.stdout)
            print(export_pb.stderr, file=sys.stderr)
            return export_pb.returncode

        export_sim = subprocess.run(
            [
                sys.executable,
                str(ENGINEERING_DATA_BRIDGE / "export_simulation_input.py"),
                "--input", str(pb_path),
                "--output", str(sim_path),
            ],
            cwd=str(ROOT),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        if export_sim.returncode != 0:
            print(export_sim.stdout)
            print(export_sim.stderr, file=sys.stderr)
            return export_sim.returncode

        actual_pb = load_path_bundle(pb_path)
        actual_sim = _load_json(sim_path)

        preview_run_a = subprocess.run(
            [
                sys.executable,
                str(ENGINEERING_DATA_BRIDGE / "generate_preview.py"),
                "--input", str(dxf_path),
                "--output-dir", str(temp_root),
                "--json",
                "--deterministic",
            ],
            cwd=str(ROOT),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        if preview_run_a.returncode != 0:
            print(preview_run_a.stdout)
            print(preview_run_a.stderr, file=sys.stderr)
            return preview_run_a.returncode
        preview_payload_a = json.loads((preview_run_a.stdout or "").strip().splitlines()[-1])
        preview_path_a = Path(preview_payload_a["preview_path"])
        if not preview_path_a.exists():
            print(f"engineering regression failed: preview artifact missing: {preview_path_a}", file=sys.stderr)
            return 1
        preview_html_a = preview_path_a.read_bytes()

        preview_run_b = subprocess.run(
            [
                sys.executable,
                str(ENGINEERING_DATA_BRIDGE / "generate_preview.py"),
                "--input", str(dxf_path),
                "--output-dir", str(temp_root),
                "--json",
                "--deterministic",
            ],
            cwd=str(ROOT),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        if preview_run_b.returncode != 0:
            print(preview_run_b.stdout)
            print(preview_run_b.stderr, file=sys.stderr)
            return preview_run_b.returncode
        preview_payload_b = json.loads((preview_run_b.stdout or "").strip().splitlines()[-1])
        preview_path_b = Path(preview_payload_b["preview_path"])
        if not preview_path_b.exists():
            print(f"engineering regression failed: preview artifact missing: {preview_path_b}", file=sys.stderr)
            return 1
        preview_html_b = preview_path_b.read_bytes()

    actual_pb.header.source_path = ""
    expected_pb.header.source_path = ""
    if actual_pb.SerializeToString() != expected_pb.SerializeToString():
        print("engineering regression failed: exported PathBundle differs from canonical fixture", file=sys.stderr)
        return 1
    if actual_sim != expected_sim:
        print("engineering regression failed: simulation input differs from canonical fixture", file=sys.stderr)
        return 1
    if preview_payload_a != preview_payload_b:
        print("engineering regression failed: deterministic preview metadata mismatch", file=sys.stderr)
        return 1
    if preview_html_a != preview_html_b:
        print("engineering regression failed: deterministic preview html mismatch", file=sys.stderr)
        return 1

    print("engineering regression passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
