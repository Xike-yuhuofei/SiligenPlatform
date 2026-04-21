import json
import subprocess
import tempfile
import unittest
from pathlib import Path


APP_ROOT = Path(__file__).resolve().parents[2]
WORKSPACE_ROOT = APP_ROOT.parents[1]
POWERSHELL = "powershell"
FORMAL_CONTRACT_GUARD = (
    WORKSPACE_ROOT / "scripts" / "validation" / "assert-hmi-formal-gateway-launch-contract.ps1"
)


class HmiFormalGatewayContractGuardTest(unittest.TestCase):
    def test_guard_rejects_missing_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "gateway-launch.json"
            completed = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(FORMAL_CONTRACT_GUARD),
                    "-ContractPath",
                    str(contract_path),
                ],
                cwd=str(WORKSPACE_ROOT),
                capture_output=True,
                text=True,
            )

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("formal gateway launch contract missing", f"{completed.stdout}\n{completed.stderr}")

    def test_guard_rejects_missing_executable_field(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "gateway-launch.json"
            contract_path.write_text(json.dumps({"cwd": "D:/Deploy/Siligen"}), encoding="utf-8")
            completed = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(FORMAL_CONTRACT_GUARD),
                    "-ContractPath",
                    str(contract_path),
                ],
                cwd=str(WORKSPACE_ROOT),
                capture_output=True,
                text=True,
            )

        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("missing executable", f"{completed.stdout}\n{completed.stderr}")

    def test_guard_rejects_noncanonical_build_root(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace_root = Path(temp_dir)
            contract_path = workspace_root / "gateway-launch.json"
            contract_path.write_text(
                json.dumps(
                    {
                        "executable": "build/bin/Debug/siligen_runtime_gateway.exe",
                        "cwd": "build/bin/Debug",
                        "pathEntries": ["build/bin/Debug"],
                    }
                ),
                encoding="utf-8",
            )
            completed = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(FORMAL_CONTRACT_GUARD),
                    "-WorkspaceRoot",
                    str(workspace_root),
                    "-ContractPath",
                    str(contract_path),
                ],
                cwd=str(WORKSPACE_ROOT),
                capture_output=True,
                text=True,
        )

        self.assertNotEqual(completed.returncode, 0)
        output = f"{completed.stdout}\n{completed.stderr}".replace("\\\\", "\\")
        self.assertIn("canonical build\\ca root", output)

    def test_guard_accepts_valid_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace_root = Path(temp_dir)
            contract_path = workspace_root / "gateway-launch.json"
            contract_path.write_text(
                json.dumps(
                    {
                        "executable": "build/ca/bin/Debug/siligen_runtime_gateway.exe",
                        "cwd": "build/ca/bin/Debug",
                        "args": [],
                        "pathEntries": ["build/ca/bin/Debug"],
                    }
                ),
                encoding="utf-8",
            )
            completed = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(FORMAL_CONTRACT_GUARD),
                    "-WorkspaceRoot",
                    str(workspace_root),
                    "-ContractPath",
                    str(contract_path),
                ],
                cwd=str(WORKSPACE_ROOT),
                capture_output=True,
                text=True,
            )

        self.assertEqual(completed.returncode, 0, msg=completed.stderr)
        self.assertIn(str(contract_path), completed.stdout)
