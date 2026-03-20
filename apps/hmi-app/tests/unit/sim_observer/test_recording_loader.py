import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.adapters.recording_loader import (  # noqa: E402
    RecordingSchemaError,
    RecordingSourceError,
    load_recording_from_file,
    load_recording_from_mapping,
)


class RecordingLoaderTest(unittest.TestCase):
    def test_load_recording_from_mapping_accepts_minimal_payload(self) -> None:
        payload = load_recording_from_mapping(
            {
                "summary": {},
                "snapshots": [],
                "timeline": [],
                "trace": [],
                "motion_profile": [],
            }
        )
        self.assertEqual(payload.summary, {})

    def test_missing_summary_raises_schema_error(self) -> None:
        with self.assertRaises(RecordingSchemaError):
            load_recording_from_mapping(
                {
                    "snapshots": [],
                    "timeline": [],
                    "trace": [],
                    "motion_profile": [],
                }
            )

    def test_invalid_motion_profile_type_raises_schema_error(self) -> None:
        with self.assertRaises(RecordingSchemaError):
            load_recording_from_mapping(
                {
                    "summary": {},
                    "snapshots": [],
                    "timeline": [],
                    "trace": [],
                    "motion_profile": "bad",
                }
            )

    def test_missing_file_raises_source_error(self) -> None:
        with self.assertRaises(RecordingSourceError):
            load_recording_from_file(PROJECT_ROOT / "tests" / "fixtures" / "sim_observer" / "missing.json")


if __name__ == "__main__":
    unittest.main()
