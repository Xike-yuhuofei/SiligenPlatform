from __future__ import annotations

from .domain.preview_planning_context_types import (
    PreparePlanContextSnapshot,
    PreviewPlanningContextFreezeResult,
    PreviewPlanningContextState,
)


class PreviewPlanningContextOwner:
    def __init__(self) -> None:
        self._state = PreviewPlanningContextState()
        self._recipes: dict[str, dict] = {}
        self._recipe_order: list[str] = []
        self._versions_by_recipe: dict[str, dict[str, dict]] = {}

    @property
    def state(self) -> PreviewPlanningContextState:
        return self._state

    def current_selection(self) -> tuple[str, str]:
        return self._state.recipe_id, self._state.version_id

    def sync_recipe_catalog(self, recipes: list[dict] | tuple[dict, ...]) -> None:
        self._recipes = {}
        self._recipe_order = []
        for recipe in recipes:
            recipe_id = str(recipe.get("id", "")).strip()
            if not recipe_id:
                continue
            self._recipes[recipe_id] = dict(recipe)
            self._recipe_order.append(recipe_id)
        if not self._recipes:
            self._clear("当前未找到可用配方，无法生成在线预览。")
            self._versions_by_recipe.clear()
            return
        if self._state.recipe_id not in self._recipes:
            self._state.recipe_id = self._resolve_default_recipe_id()
            self._state.version_id = ""
            self._state.version_status = ""
            self._state.selection_origin = "recipe_default"
        self._recompute_state()

    def sync_recipe_versions(self, recipe_id: str, versions: list[dict] | tuple[dict, ...]) -> None:
        normalized_recipe_id = str(recipe_id or "").strip()
        version_map: dict[str, dict] = {}
        for version in versions:
            version_id = str(version.get("id", "")).strip()
            if not version_id:
                continue
            version_map[version_id] = dict(version)
        if normalized_recipe_id:
            self._versions_by_recipe[normalized_recipe_id] = version_map
        if normalized_recipe_id == self._state.recipe_id:
            self._recompute_state()

    def select_recipe(self, recipe_id: str, *, selection_origin: str = "user_recipe_selection") -> None:
        normalized_recipe_id = str(recipe_id or "").strip()
        if not normalized_recipe_id:
            self._clear("当前未选择配方，无法生成在线预览。")
            return
        if normalized_recipe_id != self._state.recipe_id:
            self._state.version_id = ""
            self._state.version_status = ""
        self._state.recipe_id = normalized_recipe_id
        self._state.selection_origin = selection_origin
        self._recompute_state()

    def select_version(self, version_id: str, *, selection_origin: str = "user_version_selection") -> None:
        normalized_version_id = str(version_id or "").strip()
        self._state.version_id = normalized_version_id
        self._state.selection_origin = selection_origin
        self._recompute_state()

    def freeze_for_prepare(self) -> PreviewPlanningContextFreezeResult:
        if not self._state.is_valid_for_prepare or not self._state.recipe_id or not self._state.version_id:
            return PreviewPlanningContextFreezeResult(
                ok=False,
                message=self._state.invalid_reason or "当前未形成有效的预览工艺上下文。",
            )
        return PreviewPlanningContextFreezeResult(
            ok=True,
            snapshot=PreparePlanContextSnapshot(
                recipe_id=self._state.recipe_id,
                version_id=self._state.version_id,
                selection_origin=self._state.selection_origin,
            ),
        )

    def _resolve_default_recipe_id(self) -> str:
        return self._recipe_order[0] if self._recipe_order else ""

    def _recompute_state(self) -> None:
        recipe_id = str(self._state.recipe_id or "").strip()
        if not recipe_id:
            self._clear("当前未选择配方，无法生成在线预览。")
            return
        if recipe_id not in self._recipes:
            self._clear("当前配方上下文已失效，请重新选择配方后再生成预览。")
            return
        versions = self._versions_by_recipe.get(recipe_id)
        if versions is None:
            self._state.is_valid_for_prepare = False
            self._state.invalid_reason = "当前配方版本尚未加载完成，无法生成在线预览。"
            return

        version_id = str(self._state.version_id or "").strip()
        if version_id and version_id in versions:
            self._apply_version_state(version_id, versions[version_id])
            return

        if version_id:
            self._state.version_id = ""
            self._state.version_status = ""
            self._state.is_valid_for_prepare = False
            self._state.invalid_reason = "当前选中的配方版本已失效，请重新选择已发布版本后再生成在线预览。"
            return

        self._state.version_id = ""
        self._state.version_status = ""
        self._state.is_valid_for_prepare = False
        self._state.invalid_reason = "当前未显式选择配方版本，无法生成在线预览。"

    def _apply_version_state(self, version_id: str, version: dict) -> None:
        status = str(version.get("status", "")).strip().lower()
        self._state.version_id = version_id
        self._state.version_status = status
        if status == "published":
            self._state.is_valid_for_prepare = True
            self._state.invalid_reason = ""
            return
        self._state.is_valid_for_prepare = False
        if status:
            self._state.invalid_reason = f"当前选中的版本状态为 {status}，不是已发布版本，无法生成在线预览。"
        else:
            self._state.invalid_reason = "当前选中的版本状态未知，无法生成在线预览。"

    def _clear(self, message: str) -> None:
        self._state.recipe_id = ""
        self._state.version_id = ""
        self._state.version_status = ""
        self._state.selection_origin = ""
        self._state.is_valid_for_prepare = False
        self._state.invalid_reason = message


__all__ = [
    "PreparePlanContextSnapshot",
    "PreviewPlanningContextFreezeResult",
    "PreviewPlanningContextOwner",
    "PreviewPlanningContextState",
]
