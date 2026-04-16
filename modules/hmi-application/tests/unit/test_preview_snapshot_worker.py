import sys
import unittest
from unittest.mock import patch

from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.adapters.qt_workers import PreviewSnapshotWorker
from unit.preview_test_support import WorkerFakeProtocol, worker_import_modules


class PreviewSnapshotWorkerTest(unittest.TestCase):
    def setUp(self) -> None:
        WorkerFakeProtocol.calls = []

    def test_worker_uses_300s_timeout_for_prepare_and_snapshot(self) -> None:
        worker = PreviewSnapshotWorker(
            host="127.0.0.1",
            port=9527,
            artifact_id="artifact-1",
            recipe_id="recipe-1",
            version_id="version-1",
            speed_mm_s=20.0,
            dry_run=False,
            dry_run_speed_mm_s=20.0,
        )
        emitted = []
        worker.completed.connect(lambda ok, payload, error: emitted.append((ok, payload, error)))

        with patch.dict(sys.modules, worker_import_modules(), clear=False):
            worker.run()

        self.assertEqual(
            WorkerFakeProtocol.calls,
            [
                ("dxf.plan.prepare", "artifact-1", "recipe-1", "version-1", 20.0, False, 20.0, 300.0),
                ("dxf.preview.snapshot", "plan-1", 4000, 300.0),
            ],
        )
        self.assertTrue(emitted)
        self.assertTrue(emitted[0][0])
        self.assertEqual(emitted[0][1]["plan_id"], "plan-1")
        self.assertEqual(emitted[0][1]["snapshot_hash"], "fp-1")
        self.assertEqual(emitted[0][1]["performance_profile"]["prepare_total_ms"], 123)
        self.assertTrue(emitted[0][1]["worker_profile"]["plan_prepare_rpc_ms"] >= 0)

    def test_worker_cancelled_before_run_does_not_emit_result(self) -> None:
        worker = PreviewSnapshotWorker(
            host="127.0.0.1",
            port=9527,
            artifact_id="artifact-1",
            recipe_id="recipe-1",
            version_id="version-1",
            speed_mm_s=20.0,
            dry_run=False,
            dry_run_speed_mm_s=20.0,
        )
        emitted = []
        worker.completed.connect(lambda ok, payload, error: emitted.append((ok, payload, error)))

        worker.cancel()
        with patch.dict(sys.modules, worker_import_modules(), clear=False):
            worker.run()

        self.assertEqual(emitted, [])


if __name__ == "__main__":
    unittest.main()
