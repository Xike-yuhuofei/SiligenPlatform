from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ENGINEERING_DATA_SRC = ROOT / "packages" / "engineering-data" / "src"
FIXTURE_ROOT = ROOT / "packages" / "engineering-contracts" / "fixtures" / "cases" / "rect_diag"

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
                str(ROOT / "packages" / "engineering-data" / "scripts" / "dxf_to_pb.py"),
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
                str(ROOT / "packages" / "engineering-data" / "scripts" / "export_simulation_input.py"),
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

    actual_pb.header.source_path = ""
    expected_pb.header.source_path = ""
    if actual_pb.SerializeToString() != expected_pb.SerializeToString():
        print("engineering regression failed: exported PathBundle differs from canonical fixture", file=sys.stderr)
        return 1
    if actual_sim != expected_sim:
        print("engineering regression failed: simulation input differs from canonical fixture", file=sys.stderr)
        return 1

    print("engineering regression passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
