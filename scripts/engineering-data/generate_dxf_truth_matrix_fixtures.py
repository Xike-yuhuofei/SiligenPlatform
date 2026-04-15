from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Sequence


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
ENGINEERING_BRIDGE = WORKSPACE_ROOT / "scripts" / "engineering-data"
ENGINEERING_DATA_SRC = WORKSPACE_ROOT / "modules" / "dxf-geometry" / "application"
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"

if str(ENGINEERING_DATA_SRC) not in sys.path:
    sys.path.insert(0, str(ENGINEERING_DATA_SRC))
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from engineering_data.contracts.simulation_input import load_path_bundle  # noqa: E402
from test_kit.dxf_truth_matrix import FullChainTruthCase, full_chain_cases, resolve_full_chain_case  # noqa: E402


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")


def _run_command(command: list[str]) -> None:
    completed = subprocess.run(
        command,
        cwd=str(WORKSPACE_ROOT),
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if completed.returncode != 0:
        raise RuntimeError(
            json.dumps(
                {
                    "command": command,
                    "returncode": completed.returncode,
                    "stdout": completed.stdout.strip(),
                    "stderr": completed.stderr.strip(),
                },
                ensure_ascii=False,
                indent=2,
            )
        )


def _normalize_preview_payload(case: FullChainTruthCase, payload: dict[str, Any]) -> dict[str, Any]:
    normalized = dict(payload)
    source_name = Path(case.source_sample_path).stem
    normalized["preview_path"] = f"samples/dxf/{source_name}-preview.html"
    return normalized


def _normalized_pb_bytes(case: FullChainTruthCase, pb_path: Path) -> bytes:
    bundle = load_path_bundle(pb_path)
    bundle.header.source_path = case.source_sample_path
    return bundle.SerializeToString()


def _generated_payloads(case: FullChainTruthCase) -> tuple[bytes, dict[str, Any], dict[str, Any]]:
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_root = Path(temp_dir)
        temp_pb = temp_root / f"{case.case_id}.pb"
        temp_sim = temp_root / f"{case.case_id}.simulation-input.json"

        _run_command(
            [
                sys.executable,
                str(ENGINEERING_BRIDGE / "dxf_to_pb.py"),
                "--input",
                str(case.source_sample_absolute(WORKSPACE_ROOT)),
                "--output",
                str(temp_pb),
            ]
        )
        _run_command(
            [
                sys.executable,
                str(ENGINEERING_BRIDGE / "export_simulation_input.py"),
                "--input",
                str(temp_pb),
                "--output",
                str(temp_sim),
            ]
        )
        preview_command = [
            sys.executable,
            str(ENGINEERING_BRIDGE / "generate_preview.py"),
            "--input",
            str(case.source_sample_absolute(WORKSPACE_ROOT)),
            "--output-dir",
            str(temp_root),
            "--json",
            "--deterministic",
        ]
        completed = subprocess.run(
            preview_command,
            cwd=str(WORKSPACE_ROOT),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        if completed.returncode != 0:
            raise RuntimeError(
                json.dumps(
                    {
                        "command": preview_command,
                        "returncode": completed.returncode,
                        "stdout": completed.stdout.strip(),
                        "stderr": completed.stderr.strip(),
                    },
                    ensure_ascii=False,
                    indent=2,
                )
            )

        preview_payload = json.loads((completed.stdout or "").splitlines()[-1])
        return (
            _normalized_pb_bytes(case, temp_pb),
            _load_json(temp_sim),
            _normalize_preview_payload(case, preview_payload),
        )


def _check_case(case: FullChainTruthCase) -> list[str]:
    source_bytes = case.source_sample_absolute(WORKSPACE_ROOT).read_bytes()
    expected_dxf = case.dxf_fixture_absolute(WORKSPACE_ROOT).read_bytes()
    generated_pb, generated_sim, generated_preview = _generated_payloads(case)
    existing_pb = _normalized_pb_bytes(case, case.pb_fixture_absolute(WORKSPACE_ROOT))

    issues: list[str] = []
    if source_bytes != expected_dxf:
        issues.append(f"{case.case_id}: dxf fixture differs from source sample")
    if generated_pb != existing_pb:
        issues.append(f"{case.case_id}: pb fixture drifted")
    if generated_sim != _load_json(case.simulation_fixture_absolute(WORKSPACE_ROOT)):
        issues.append(f"{case.case_id}: simulation fixture drifted")
    if generated_preview != _load_json(case.preview_fixture_absolute(WORKSPACE_ROOT)):
        issues.append(f"{case.case_id}: preview fixture drifted")
    return issues


def _write_case(case: FullChainTruthCase) -> None:
    source_path = case.source_sample_absolute(WORKSPACE_ROOT)
    generated_pb, generated_sim, generated_preview = _generated_payloads(case)

    case.fixture_root_absolute(WORKSPACE_ROOT).mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source_path, case.dxf_fixture_absolute(WORKSPACE_ROOT))
    case.pb_fixture_absolute(WORKSPACE_ROOT).write_bytes(generated_pb)
    _write_json(case.simulation_fixture_absolute(WORKSPACE_ROOT), generated_sim)
    _write_json(case.preview_fixture_absolute(WORKSPACE_ROOT), generated_preview)


def _selected_cases(requested_case_ids: Sequence[str]) -> tuple[FullChainTruthCase, ...]:
    if not requested_case_ids:
        return full_chain_cases(WORKSPACE_ROOT)

    selected: list[FullChainTruthCase] = []
    seen: set[str] = set()
    for case_id in requested_case_ids:
        normalized = case_id.strip()
        if not normalized or normalized in seen:
            continue
        selected.append(resolve_full_chain_case(WORKSPACE_ROOT, normalized))
        seen.add(normalized)
    return tuple(selected)


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Regenerate or check full-chain DXF truth matrix fixtures."
    )
    parser.add_argument(
        "--case-id",
        action="append",
        default=[],
        help="Only operate on the named full-chain truth case. Can be repeated.",
    )
    parser.add_argument("--check", action="store_true", help="Verify fixture drift without writing files.")
    parser.add_argument("--write", action="store_true", help="Rewrite fixture files from source samples.")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    do_check = args.check or not args.write
    selected_cases = _selected_cases(args.case_id)

    if args.write:
        for case in selected_cases:
            _write_case(case)
            print(f"wrote fixtures: {case.case_id}")

    if do_check:
        issues: list[str] = []
        for case in selected_cases:
            issues.extend(_check_case(case))
        if issues:
            for issue in issues:
                print(issue, file=sys.stderr)
            return 1
        print("dxf truth matrix fixtures are up to date")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
