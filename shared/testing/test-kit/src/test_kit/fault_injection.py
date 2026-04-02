from __future__ import annotations

from dataclasses import asdict, dataclass
from pathlib import Path

from .asset_catalog import (
    DEFAULT_CLOCK_PROFILE,
    DEFAULT_DETERMINISTIC_SEED,
    FAULT_MATRIX_ID,
    build_asset_catalog,
)


ALLOWED_CONSUMER_SCOPES = ("tests/integration", "runtime-execution")
SIMULATED_LINE_DOUBLE_SURFACE = {
    "fake_controller": ["readiness", "abort"],
    "fake_io": ["disconnect"],
}


@dataclass(frozen=True)
class DeterministicReplayConfig:
    seed: int = DEFAULT_DETERMINISTIC_SEED
    clock_profile: str = DEFAULT_CLOCK_PROFILE
    repeat_count: int = 2

    def to_dict(self) -> dict[str, object]:
        return asdict(self)


@dataclass(frozen=True)
class FaultInjectionPlan:
    matrix_id: str
    consumer_scope: str
    owner_scopes: tuple[str, ...]
    selected_fault_ids: tuple[str, ...]
    replay: DeterministicReplayConfig
    hooks: dict[str, str]
    double_surface: dict[str, list[str]]
    faults: dict[str, dict[str, object]]

    def to_dict(self) -> dict[str, object]:
        return {
            "matrix_id": self.matrix_id,
            "consumer_scope": self.consumer_scope,
            "owner_scopes": list(self.owner_scopes),
            "selected_fault_ids": list(self.selected_fault_ids),
            "replay": self.replay.to_dict(),
            "hooks": self.hooks,
            "double_surface": self.double_surface,
            "faults": self.faults,
        }


def simulated_line_double_surface() -> dict[str, list[str]]:
    return {
        key: list(value)
        for key, value in SIMULATED_LINE_DOUBLE_SURFACE.items()
    }


def build_fault_matrix(workspace_root: Path, *, consumer_scope: str) -> dict[str, object]:
    if consumer_scope not in ALLOWED_CONSUMER_SCOPES:
        raise ValueError(f"unsupported consumer_scope for simulated fault matrix: {consumer_scope}")

    catalog = build_asset_catalog(workspace_root)
    faults: dict[str, dict[str, object]] = {}
    for fault in sorted(catalog.faults.values(), key=lambda item: item.fault_id):
        if fault.safety_level != "simulated-safe":
            continue
        if not fault.injector_id.startswith("simulated-line."):
            continue
        faults[fault.fault_id] = {
            "fault_id": fault.fault_id,
            "scenario_kind": fault.scenario_kind,
            "owner_scope": fault.owner_scope,
            "source_asset_refs": list(fault.source_asset_refs),
            "injector_id": fault.injector_id,
            "default_seed": fault.default_seed,
            "clock_profile": fault.clock_profile,
            "supported_hooks": list(fault.supported_hooks),
            "expected_outcome": fault.expected_outcome,
            "required_evidence_fields": list(fault.required_evidence_fields),
        }

    return {
        "matrix_id": FAULT_MATRIX_ID,
        "consumer_scope": consumer_scope,
        "owner_scopes": list(ALLOWED_CONSUMER_SCOPES),
        "double_surface": simulated_line_double_surface(),
        "faults": faults,
    }


def resolve_fault_plan(
    workspace_root: Path,
    *,
    consumer_scope: str,
    requested_fault_ids: tuple[str, ...] = (),
    seed: int | None = None,
    clock_profile: str | None = None,
    hook_readiness: str = "ready",
    hook_abort: str = "none",
    hook_disconnect: str = "none",
) -> FaultInjectionPlan:
    matrix = build_fault_matrix(workspace_root, consumer_scope=consumer_scope)
    available_fault_ids = tuple(sorted(matrix["faults"]))
    selected_fault_ids = requested_fault_ids or available_fault_ids
    unknown_fault_ids = [fault_id for fault_id in selected_fault_ids if fault_id not in matrix["faults"]]
    if unknown_fault_ids:
        raise ValueError(f"unknown simulated fault ids: {', '.join(unknown_fault_ids)}")

    replay = DeterministicReplayConfig(
        seed=DEFAULT_DETERMINISTIC_SEED if seed is None else seed,
        clock_profile=clock_profile or DEFAULT_CLOCK_PROFILE,
        repeat_count=2,
    )
    selected_faults = {
        fault_id: dict(matrix["faults"][fault_id])
        for fault_id in selected_fault_ids
    }
    return FaultInjectionPlan(
        matrix_id=str(matrix["matrix_id"]),
        consumer_scope=consumer_scope,
        owner_scopes=tuple(str(item) for item in matrix["owner_scopes"]),
        selected_fault_ids=tuple(selected_fault_ids),
        replay=replay,
        hooks={
            "readiness": hook_readiness,
            "abort": hook_abort,
            "disconnect": hook_disconnect,
        },
        double_surface=simulated_line_double_surface(),
        faults=selected_faults,
    )
