from __future__ import annotations

import json
import sys
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.asset_catalog import build_asset_catalog, shared_asset_ids_for_smoke


def main() -> int:
    catalog = build_asset_catalog(WORKSPACE_ROOT)
    shared_asset_ids = shared_asset_ids_for_smoke(WORKSPACE_ROOT)
    assert "sample.dxf.rect_diag" in shared_asset_ids
    assert "baseline.simulation.scheme_c_rect_diag" in shared_asset_ids
    assert "fixture.validation-evidence-bundle" in catalog.fixtures
    assert "fault.simulated.invalid-empty-segments" in catalog.faults

    consumer_matrix = {
        "tests/integration": list(shared_asset_ids),
        "runtime-execution": list(shared_asset_ids),
    }
    payload = {
        "shared_asset_ids": list(shared_asset_ids),
        "fixtures": sorted(catalog.fixtures.keys()),
        "faults": sorted(catalog.faults.keys()),
        "consumer_matrix": consumer_matrix,
    }
    print(json.dumps(payload, ensure_ascii=True, indent=2))
    print("shared asset reuse smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
