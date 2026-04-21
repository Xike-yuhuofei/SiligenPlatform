import sys
import unittest
from pathlib import Path

TEST_ROOT = Path(__file__).resolve().parents[1]
if str(TEST_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_ROOT))

from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.preview_planning_context import PreviewPlanningContextOwner


class PreviewPlanningContextOwnerTest(unittest.TestCase):
    def setUp(self) -> None:
        self.owner = PreviewPlanningContextOwner()

    def test_requires_explicit_version_selection_even_when_active_version_exists(self) -> None:
        self.owner.sync_recipe_catalog(
            [
                {"id": "recipe-a", "activeVersionId": "version-a-published"},
            ]
        )
        self.owner.sync_recipe_versions(
            "recipe-a",
            [
                {"id": "version-a-draft", "status": "draft"},
                {"id": "version-a-published", "status": "published"},
            ],
        )

        result = self.owner.freeze_for_prepare()

        self.assertFalse(result.ok)
        self.assertIn("未显式选择配方版本", result.message)
        self.assertEqual(self.owner.state.recipe_id, "recipe-a")
        self.assertEqual(self.owner.state.version_id, "")
        self.assertFalse(self.owner.state.is_valid_for_prepare)

    def test_explicit_draft_selection_blocks_prepare(self) -> None:
        self.owner.sync_recipe_catalog(
            [
                {"id": "recipe-a", "activeVersionId": "version-a-published"},
            ]
        )
        self.owner.select_recipe("recipe-a")
        self.owner.sync_recipe_versions(
            "recipe-a",
            [
                {"id": "version-a-draft", "status": "draft"},
                {"id": "version-a-published", "status": "published"},
            ],
        )
        self.owner.select_version("version-a-draft")

        result = self.owner.freeze_for_prepare()

        self.assertFalse(result.ok)
        self.assertIn("不是已发布版本", result.message)
        self.assertEqual(self.owner.state.version_id, "version-a-draft")
        self.assertFalse(self.owner.state.is_valid_for_prepare)

    def test_missing_explicit_version_keeps_context_invalid(self) -> None:
        self.owner.sync_recipe_catalog(
            [
                {"id": "recipe-a", "activeVersionId": ""},
            ]
        )
        self.owner.sync_recipe_versions(
            "recipe-a",
            [
                {"id": "version-a-draft", "status": "draft"},
            ],
        )

        result = self.owner.freeze_for_prepare()

        self.assertFalse(result.ok)
        self.assertIn("未显式选择配方版本", result.message)

    def test_explicit_published_version_selection_is_retained(self) -> None:
        self.owner.sync_recipe_catalog(
            [
                {"id": "recipe-a", "activeVersionId": "version-a-published"},
            ]
        )
        self.owner.select_recipe("recipe-a")
        self.owner.sync_recipe_versions(
            "recipe-a",
            [
                {"id": "version-a-published", "status": "published"},
                {"id": "version-a-published-2", "status": "published"},
            ],
        )
        self.owner.select_version("version-a-published-2")
        self.owner.sync_recipe_versions(
            "recipe-a",
            [
                {"id": "version-a-published", "status": "published"},
                {"id": "version-a-published-2", "status": "published"},
            ],
        )

        result = self.owner.freeze_for_prepare()

        self.assertTrue(result.ok)
        snapshot = result.snapshot
        self.assertIsNotNone(snapshot)
        if snapshot is None:
            self.fail("freeze_for_prepare should return a snapshot when ok=True")
        self.assertEqual(snapshot.version_id, "version-a-published-2")
        self.assertEqual(self.owner.state.selection_origin, "user_version_selection")

    def test_missing_selected_version_after_refresh_blocks_prepare(self) -> None:
        self.owner.sync_recipe_catalog(
            [
                {"id": "recipe-a", "activeVersionId": "version-a-published"},
            ]
        )
        self.owner.select_recipe("recipe-a")
        self.owner.sync_recipe_versions(
            "recipe-a",
            [
                {"id": "version-a-published", "status": "published"},
                {"id": "version-a-published-2", "status": "published"},
            ],
        )
        self.owner.select_version("version-a-published-2")
        self.owner.sync_recipe_versions(
            "recipe-a",
            [
                {"id": "version-a-published", "status": "published"},
            ],
        )

        result = self.owner.freeze_for_prepare()

        self.assertFalse(result.ok)
        self.assertIn("版本已失效", result.message)
        self.assertEqual(self.owner.state.version_id, "")
        self.assertFalse(self.owner.state.is_valid_for_prepare)


if __name__ == "__main__":
    unittest.main()
