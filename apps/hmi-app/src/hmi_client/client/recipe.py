"""Recipe management for Siligen HMI."""
import json
import os
from dataclasses import dataclass, asdict, field
from datetime import datetime
from typing import Optional, List


@dataclass
class Recipe:
    """Recipe data structure for dispensing parameters."""
    name: str = "Untitled"
    version: str = "1.0"
    dxf_path: str = ""

    # Motion parameters
    max_velocity: float = 50.0
    max_acceleration: float = 100.0

    # Dispensing parameters
    dispense_speed: float = 10.0
    dispense_height: float = 0.5

    # Valve parameters
    valve_on_delay: int = 0
    valve_off_delay: int = 0

    # Process parameters
    purge_interval: int = 100
    purge_duration: int = 10000

    # Metadata
    created_at: str = ""
    modified_at: str = ""
    author: str = ""

    def __post_init__(self):
        if not self.created_at:
            self.created_at = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        if not self.modified_at:
            self.modified_at = self.created_at


class RecipeManager:
    """Manages recipe loading, saving, and listing."""

    def __init__(self, recipes_dir: str = "recipes"):
        self._recipes_dir = recipes_dir
        self._current_recipe: Optional[Recipe] = None
        os.makedirs(recipes_dir, exist_ok=True)

    @property
    def current_recipe(self) -> Optional[Recipe]:
        return self._current_recipe

    def list_recipes(self) -> List[str]:
        """List all available recipe names."""
        recipes = []
        if os.path.exists(self._recipes_dir):
            for f in os.listdir(self._recipes_dir):
                if f.endswith(".json"):
                    recipes.append(f[:-5])
        return sorted(recipes)

    def load(self, name: str) -> Optional[Recipe]:
        """Load a recipe by name."""
        filepath = os.path.join(self._recipes_dir, f"{name}.json")
        if not os.path.exists(filepath):
            return None
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                data = json.load(f)
            self._current_recipe = Recipe(**data)
            return self._current_recipe
        except (json.JSONDecodeError, TypeError):
            return None

    def save(self, recipe: Optional[Recipe] = None, name: Optional[str] = None) -> bool:
        """Save recipe to file."""
        recipe = recipe or self._current_recipe
        if not recipe:
            return False
        recipe.modified_at = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        save_name = name or recipe.name
        filepath = os.path.join(self._recipes_dir, f"{save_name}.json")
        try:
            with open(filepath, "w", encoding="utf-8") as f:
                json.dump(asdict(recipe), f, indent=2, ensure_ascii=False)
            self._current_recipe = recipe
            return True
        except IOError:
            return False

    def delete(self, name: str) -> bool:
        """Delete a recipe by name."""
        filepath = os.path.join(self._recipes_dir, f"{name}.json")
        if os.path.exists(filepath):
            os.remove(filepath)
            return True
        return False

    def new_recipe(self, name: str = "Untitled", author: str = "") -> Recipe:
        """Create a new recipe."""
        self._current_recipe = Recipe(name=name, author=author)
        return self._current_recipe
