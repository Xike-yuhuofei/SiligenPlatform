from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path
from typing import Any, cast


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.asset_catalog import (
    advisory_baseline_governance_issues,
    baseline_governance_summary,
    baseline_manifest_path,
    baseline_manifest_schema_path,
    blocking_baseline_governance_issues,
    build_asset_catalog,
    build_asset_inventory,
    default_performance_sample_asset_ids,
    default_performance_samples,
    load_baseline_manifest,
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
        inventory = cast(dict[str, Any], build_asset_inventory(WORKSPACE_ROOT))
        self.assertIn("samples", inventory["source_roots"])
        self.assertIn("tests/baselines", inventory["source_roots"])
        self.assertIn("shared/testing", inventory["source_roots"])
        self.assertIn("tests/reports", inventory["evidence_output_roots"])
        self.assertIn("baseline.simulation.compat_rect_diag", inventory["baseline_manifest_entry_ids"])
        self.assertIn("baseline.preview.rect_diag_snapshot", inventory["baseline_manifest_entry_ids"])

    def test_required_shared_assets_and_fixtures_exist(self) -> None:
        catalog = build_asset_catalog(WORKSPACE_ROOT)
        for asset_id in (
            "sample.dxf.rect_diag",
            "sample.dxf.rect_medium_ladder",
            "sample.dxf.rect_large_ladder",
            "sample.simulation.rect_diag",
            "baseline.simulation.scheme_c_rect_diag",
            "baseline.simulation.following_error_quantized",
            "baseline.preview.rect_diag_snapshot",
            "baseline.performance.dxf_preview_thresholds",
            "protocol.fixture.rect_diag_pb",
        ):
            self.assertIn(asset_id, catalog.assets)
        for fixture_id in (
            "fixture.workspace-validation-runner",
            "fixture.validation-evidence-bundle",
            "fixture.baseline-governance-smoke",
            "fixture.fault-matrix-smoke",
            "fixture.simulated-line-regression",
            "fixture.hil-closed-loop",
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

    def test_baseline_manifest_schema_and_workflow_are_frozen(self) -> None:
        schema = cast(dict[str, Any], json.loads(baseline_manifest_schema_path(WORKSPACE_ROOT).read_text(encoding="utf-8")))
        manifest = cast(dict[str, Any], load_baseline_manifest(WORKSPACE_ROOT))
        self.assertEqual(schema["title"], "SiligenSuite Baseline Manifest")
        self.assertIn("baselines", schema["required"])
        self.assertEqual(manifest["schema_version"], "baseline-manifest.v1")
        self.assertEqual(manifest["manifest_owner"], "shared/testing")
        self.assertTrue(manifest["reviewer_workflow"]["required_reviewers"])
        self.assertTrue(manifest["reviewer_workflow"]["steps"])
        self.assertEqual(
            Path(baseline_manifest_path(WORKSPACE_ROOT)).name,
            "baseline-manifest.json",
        )

    def test_baseline_governance_has_no_second_source_or_stale_entries(self) -> None:
        summary = baseline_governance_summary(WORKSPACE_ROOT)
        self.assertEqual(blocking_baseline_governance_issues(summary), [])
        self.assertEqual(advisory_baseline_governance_issues(summary), [])

    def test_default_performance_samples_only_use_tracked_files(self) -> None:
        samples = default_performance_samples(WORKSPACE_ROOT)
        self.assertEqual(list(samples.keys()), ["small", "medium", "large"])
        self.assertTrue(samples["small"].exists())
        self.assertEqual(samples["small"].name, "rect_diag.dxf")
        self.assertTrue(samples["medium"].exists())
        self.assertEqual(samples["medium"].name, "rect_medium_ladder.dxf")
        self.assertTrue(samples["large"].exists())
        self.assertEqual(samples["large"].name, "rect_large_ladder.dxf")
        self.assertEqual(
            default_performance_sample_asset_ids(WORKSPACE_ROOT),
            (
                "sample.dxf.rect_diag",
                "sample.dxf.rect_medium_ladder",
                "sample.dxf.rect_large_ladder",
            ),
        )


if __name__ == "__main__":
    unittest.main()
