import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.tools.mock_server import MockState


class MockServerRecipeContextTest(unittest.TestCase):
    def test_recipe_versions_exposes_active_published_version(self) -> None:
        state = MockState(seed_alarms=False)
        recipe = state.recipes[0]

        response = state.handle_request("recipe.versions", {"recipeId": recipe["id"]})

        self.assertIn("result", response)
        versions = response["result"]["versions"]
        self.assertEqual(len(versions), 1)
        version = versions[0]
        self.assertEqual(version["id"], recipe["activeVersionId"])
        self.assertEqual(version["status"], "published")
        parameter_keys = {entry["key"] for entry in version["parameters"]}
        self.assertIn("trigger_spatial_interval_mm", parameter_keys)
        self.assertIn("point_flying_direction_mode", parameter_keys)

    def test_recipe_create_also_creates_selectable_version(self) -> None:
        state = MockState(seed_alarms=False)

        create_response = state.handle_request("recipe.create", {"name": "Operator Journey Recipe"})
        self.assertIn("result", create_response)
        recipe = create_response["result"]["recipe"]

        versions_response = state.handle_request("recipe.versions", {"recipeId": recipe["id"]})

        self.assertIn("result", versions_response)
        versions = versions_response["result"]["versions"]
        self.assertEqual(len(versions), 1)
        self.assertEqual(versions[0]["id"], recipe["activeVersionId"])
        self.assertEqual(versions[0]["recipeId"], recipe["id"])
        self.assertEqual(versions[0]["status"], "published")


if __name__ == "__main__":
    unittest.main()
