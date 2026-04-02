from __future__ import annotations

import sys
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.asset_catalog import (
    build_asset_catalog,
    build_asset_inventory,
    shared_asset_ids_for_smoke,
)


class SharedTestAssetsContractTest(unittest.TestCase):
    def test_asset_catalog_uses_canonical_roots(self) -> None:
        catalog = build_asset_catalog(WORKSPACE_ROOT)
        for asset in catalog.assets.values():
            self.assertFalse(
                asset.canonical_path.startswith("tests/reports/"),
                msg=f"{asset.asset_id} must not use report output as source of truth",
            )
            self.assertTrue(
                asset.canonical_path.startswith(("samples/", "tests/baselines/", "shared/", "docs/validation/")),
                msg=f"{asset.asset_id} must stay under canonical roots",
            )

    def test_asset_inventory_lists_canonical_roots_and_manifests(self) -> None:
        inventory = build_asset_inventory(WORKSPACE_ROOT)
        self.assertIn("samples", inventory["source_roots"])
        self.assertIn("shared/testing", inventory["source_roots"])
        self.assertIn("tests/reports", inventory["evidence_output_roots"])

    def test_required_shared_assets_and_fixtures_exist(self) -> None:
        catalog = build_asset_catalog(WORKSPACE_ROOT)
        for asset_id in (
            "sample.dxf.rect_diag",
            "sample.simulation.rect_diag",
            "baseline.simulation.scheme_c_rect_diag",
            "baseline.simulation.following_error_quantized",
            "baseline.preview.rect_diag_snapshot",
            "protocol.fixture.rect_diag_pb",
        ):
            self.assertIn(asset_id, catalog.assets)
        for fixture_id in (
            "fixture.workspace-validation-runner",
            "fixture.validation-evidence-bundle",
            "fixture.fault-matrix-smoke",
            "fixture.simulated-line-regression",
        ):
            self.assertIn(fixture_id, catalog.fixtures)
        for fault_id in (
            "fault.simulated.invalid-empty-segments",
            "fault.simulated.following_error_quantized",
            "fault.hil.tcp-disconnect",
        ):
            self.assertIn(fault_id, catalog.faults)

    def test_shared_asset_smoke_ids_are_cross_surface_reusable(self) -> None:
        shared_asset_ids = shared_asset_ids_for_smoke(WORKSPACE_ROOT)
        self.assertGreaterEqual(len(shared_asset_ids), 3)
        self.assertIn("sample.dxf.rect_diag", shared_asset_ids)
        self.assertIn("baseline.simulation.scheme_c_rect_diag", shared_asset_ids)


if __name__ == "__main__":
    unittest.main()
