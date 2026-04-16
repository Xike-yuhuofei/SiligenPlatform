from __future__ import annotations

from dataclasses import dataclass


@dataclass
class PreviewPlanningContextState:
    recipe_id: str = ""
    version_id: str = ""
    version_status: str = ""
    selection_origin: str = ""
    invalid_reason: str = "当前未形成有效的预览工艺上下文。"
    is_valid_for_prepare: bool = False


@dataclass(frozen=True)
class PreparePlanContextSnapshot:
    recipe_id: str
    version_id: str
    selection_origin: str = ""


@dataclass(frozen=True)
class PreviewPlanningContextFreezeResult:
    ok: bool
    snapshot: PreparePlanContextSnapshot | None = None
    message: str = ""


__all__ = [
    "PreparePlanContextSnapshot",
    "PreviewPlanningContextFreezeResult",
    "PreviewPlanningContextState",
]
