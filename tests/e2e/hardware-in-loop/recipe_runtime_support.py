from __future__ import annotations

from typing import Any, Protocol


JsonDict = dict[str, Any]


class TcpRecipeClientLike(Protocol):
    def send_request(self, method: str, params: dict[str, Any] | None, timeout_seconds: float) -> JsonDict: ...


def _result_payload(response: JsonDict) -> JsonDict:
    result = response.get("result")
    return result if isinstance(result, dict) else {}


def resolve_active_recipe(
    client: TcpRecipeClientLike,
    *,
    timeout_seconds: float = 10.0,
) -> tuple[str, str]:
    response = client.send_request("recipe.list", {}, timeout_seconds=timeout_seconds)
    if "error" in response:
        message = str(response.get("error", {}).get("message", "Unknown error"))
        raise RuntimeError(f"recipe.list failed: {message}")

    recipes = _result_payload(response).get("recipes")
    if not isinstance(recipes, list):
        raise RuntimeError("recipe.list returned invalid recipes payload")

    diagnostics: list[str] = []
    for recipe in recipes:
        if not isinstance(recipe, dict):
            continue
        recipe_id = str(recipe.get("id", "")).strip()
        version_id = str(recipe.get("activeVersionId", "")).strip()
        if recipe_id and version_id:
            return recipe_id, version_id

        name = str(recipe.get("name", "")).strip() or recipe_id or "<unnamed>"
        diagnostics.append(
            f"{name}: recipe_id={'yes' if recipe_id else 'no'} "
            f"activeVersionId={'yes' if version_id else 'no'} "
            f"status={str(recipe.get('status', '')).strip() or 'unknown'}"
        )

    detail = "; ".join(diagnostics[:3]) if diagnostics else "recipe.list returned no recipes"
    raise RuntimeError(f"recipe.list returned no active recipe/version pair: {detail}")
