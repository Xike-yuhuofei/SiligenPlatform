from __future__ import annotations

import json
import sys
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.validation_layers import build_request, route_validation_request


def _assert_quick_route() -> dict[str, object]:
    request = build_request(
        requested_suites=["contracts", "protocol-compatibility"],
        changed_scopes=["shared/testing", "tests/integration"],
        risk_profile="medium",
        desired_depth="quick",
    )
    routed = route_validation_request(request)
    assert routed.selected_lane_ref == "quick-gate"
    assert routed.selected_layer_refs == (
        "L0-structure-gate",
        "L1-module-contract",
        "L2-offline-integration",
    )
    return routed.to_metadata()


def _assert_hil_route() -> dict[str, object]:
    request = build_request(
        requested_suites=["e2e"],
        changed_scopes=["runtime-execution"],
        risk_profile="hardware-sensitive",
        desired_depth="hil",
        include_hardware_smoke=True,
        include_hil_closed_loop=True,
    )
    routed = route_validation_request(request)
    assert routed.selected_lane_ref == "limited-hil"
    assert "L2-offline-integration" in routed.selected_layer_refs
    assert "L3-simulated-e2e" in routed.selected_layer_refs
    assert "L5-limited-hil" in routed.selected_layer_refs
    return routed.to_metadata()


def main() -> int:
    payload = {
        "quick_route": _assert_quick_route(),
        "hil_route": _assert_hil_route(),
    }
    print(json.dumps(payload, ensure_ascii=True, indent=2))
    print("layered validation routing smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
