from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any, cast


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.fault_injection import build_fault_matrix, simulated_line_double_surface


EXPECTED_FAULT_IDS = (
    "fault.simulated.following_error_quantized",
    "fault.simulated.invalid-empty-segments",
)


def _fault_projection(matrix: dict[str, object]) -> dict[str, dict[str, object]]:
    faults = matrix.get("faults", {})
    if not isinstance(faults, dict):
        raise AssertionError("fault matrix must serialize faults as an object")

    projection: dict[str, dict[str, object]] = {}
    for fault_id in EXPECTED_FAULT_IDS:
        fault = faults.get(fault_id)
        if not isinstance(fault, dict):
            raise AssertionError(f"fault matrix missing required fault: {fault_id}")
        projection[fault_id] = {
            "injector_id": fault.get("injector_id"),
            "source_asset_refs": fault.get("source_asset_refs"),
            "default_seed": fault.get("default_seed"),
            "clock_profile": fault.get("clock_profile"),
            "supported_hooks": fault.get("supported_hooks"),
        }
    return projection


def main() -> int:
    integration_matrix = cast(dict[str, Any], build_fault_matrix(WORKSPACE_ROOT, consumer_scope="tests/integration"))
    runtime_matrix = cast(dict[str, Any], build_fault_matrix(WORKSPACE_ROOT, consumer_scope="runtime-execution"))

    assert integration_matrix["matrix_id"] == runtime_matrix["matrix_id"]
    assert integration_matrix["matrix_id"] == "fault-matrix.simulated-line.v1"
    assert tuple(integration_matrix["owner_scopes"]) == ("tests/integration", "runtime-execution")
    assert tuple(runtime_matrix["owner_scopes"]) == ("tests/integration", "runtime-execution")
    assert integration_matrix["double_surface"] == runtime_matrix["double_surface"] == simulated_line_double_surface()

    integration_projection = _fault_projection(integration_matrix)
    runtime_projection = _fault_projection(runtime_matrix)
    assert integration_projection == runtime_projection

    payload = {
        "matrix_id": integration_matrix["matrix_id"],
        "owner_scopes": integration_matrix["owner_scopes"],
        "double_surface": integration_matrix["double_surface"],
        "faults": integration_projection,
    }
    print(json.dumps(payload, ensure_ascii=True, indent=2))
    print("fault matrix smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
