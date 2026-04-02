from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]


def main() -> int:
    commands = [
        (
            "application-contracts",
            [sys.executable, str(ROOT / "tests" / "contracts" / "test_protocol_compatibility.py")],
        ),
        (
            "shared-application-contracts",
            [
                sys.executable,
                str(
                    ROOT
                    / "shared"
                    / "contracts"
                    / "application"
                    / "tests"
                    / "test_application_contracts_compatibility.py"
                ),
            ],
        ),
        (
            "hmi-preview-gate-contract",
            [sys.executable, str(ROOT / "apps" / "hmi-app" / "tests" / "unit" / "test_protocol_preview_gate_contract.py")],
        ),
        ("engineering-contracts", [sys.executable, str(ROOT / "tests" / "contracts" / "test_engineering_contracts.py")]),
        ("engineering-data", [sys.executable, str(ROOT / "tests" / "contracts" / "test_engineering_data_compatibility.py")]),
        (
            "transport-gateway",
            [sys.executable, str(ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "tests" / "test_transport_gateway_compatibility.py")],
        ),
    ]

    for label, command in commands:
        completed = subprocess.run(
            command,
            cwd=str(ROOT),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        print(f"[{label}] returncode={completed.returncode}")
        if completed.stdout:
            print(completed.stdout.strip())
        if completed.stderr:
            print(completed.stderr.strip(), file=sys.stderr)
        if completed.returncode != 0:
            return completed.returncode
    print("protocol compatibility suite passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
