from __future__ import annotations

import sys
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.asset_catalog import (
    FAULT_SCENARIO_ALLOWED_HOOKS,
    FAULT_SCENARIO_SCHEMA_VERSION,
    build_asset_catalog,
)
from test_kit.evidence_bundle import REQUIRED_TRACE_FIELDS


class FaultScenarioContractTest(unittest.TestCase):
    def test_fault_scenarios_publish_schema_and_replay_metadata(self) -> None:
        catalog = build_asset_catalog(WORKSPACE_ROOT)
        self.assertIn("fault.simulated.invalid-empty-segments", catalog.faults)
        self.assertIn("fault.simulated.following_error_quantized", catalog.faults)

        for fault in catalog.faults.values():
            self.assertEqual(fault.schema_version, FAULT_SCENARIO_SCHEMA_VERSION)
            self.assertTrue(fault.owner_scope)
            self.assertTrue(fault.injector_id)
            self.assertIsInstance(fault.default_seed, int)
            self.assertGreaterEqual(fault.default_seed, 0)
            self.assertTrue(fault.clock_profile)
            self.assertTrue(fault.source_asset_refs)
            self.assertTrue(set(fault.required_evidence_fields).issubset(REQUIRED_TRACE_FIELDS))
            self.assertTrue(set(fault.supported_hooks).issubset(FAULT_SCENARIO_ALLOWED_HOOKS))
            for asset_id in fault.source_asset_refs:
                self.assertIn(asset_id, catalog.assets)

    def test_simulated_faults_do_not_bleed_into_hil_only_layers(self) -> None:
        catalog = build_asset_catalog(WORKSPACE_ROOT)
        for fault_id in ("fault.simulated.invalid-empty-segments", "fault.simulated.following_error_quantized"):
            fault = catalog.faults[fault_id]
            self.assertEqual(fault.safety_level, "simulated-safe")
            self.assertTrue(fault.injector_id.startswith("simulated-line."))
            self.assertNotIn("L5-limited-hil", fault.applicable_layers)

        for fault_id in ("fault.hil.tcp-disconnect", "fault.hil.dispenser-state-timeout"):
            fault = catalog.faults[fault_id]
            self.assertEqual(fault.safety_level, "hardware-bounded")
            self.assertFalse(fault.injector_id.startswith("simulated-line."))
            self.assertEqual(fault.applicable_layers, ("L5-limited-hil",))


if __name__ == "__main__":
    unittest.main()
