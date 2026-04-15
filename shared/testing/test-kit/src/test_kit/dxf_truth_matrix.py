from __future__ import annotations

import json
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from typing import Any


TRUTH_MATRIX_SCHEMA_VERSION = "dxf-truth-matrix.v1"
TRUTH_MATRIX_MANIFEST_PATH = "shared/contracts/engineering/fixtures/dxf-truth-matrix.json"
_CANONICAL_SOURCE_ROOTS = ("samples", "shared/contracts")


def truth_matrix_manifest_path(workspace_root: Path) -> Path:
    return workspace_root.resolve() / TRUTH_MATRIX_MANIFEST_PATH


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _relative_under_root(relative_path: str, roots: tuple[str, ...]) -> bool:
    return any(
        relative_path == root_name or relative_path.startswith(f"{root_name}/")
        for root_name in roots
    )


def _require_non_empty_string(raw_value: object, *, field_name: str, entry_id: str) -> str:
    if not isinstance(raw_value, str) or not raw_value.strip():
        raise ValueError(f"{entry_id}: {field_name} must be a non-empty string")
    return raw_value.strip()


def _require_canonical_relative_path(
    workspace_root: Path,
    raw_value: object,
    *,
    field_name: str,
    entry_id: str,
    allowed_roots: tuple[str, ...],
) -> str:
    relative_path = _require_non_empty_string(raw_value, field_name=field_name, entry_id=entry_id)
    if not _relative_under_root(relative_path, allowed_roots):
        allowed = ", ".join(allowed_roots)
        raise ValueError(f"{entry_id}: {field_name} must stay under canonical roots: {allowed}")
    absolute_path = workspace_root.resolve() / relative_path
    if not absolute_path.exists():
        raise FileNotFoundError(f"{entry_id}: {field_name} missing on disk: {absolute_path}")
    return relative_path


@dataclass(frozen=True)
class FullChainTruthCase:
    case_id: str
    topology_family: str
    source_sample_path: str
    fixture_root: str
    dxf_fixture: str
    pb_fixture: str
    preview_fixture: str
    simulation_fixture: str
    default_runtime_sample: bool
    default_hil_sample: bool
    notes: str = ""

    def source_sample_absolute(self, workspace_root: Path) -> Path:
        return workspace_root.resolve() / self.source_sample_path

    def fixture_root_absolute(self, workspace_root: Path) -> Path:
        return workspace_root.resolve() / self.fixture_root

    def dxf_fixture_absolute(self, workspace_root: Path) -> Path:
        return workspace_root.resolve() / self.dxf_fixture

    def pb_fixture_absolute(self, workspace_root: Path) -> Path:
        return workspace_root.resolve() / self.pb_fixture

    def preview_fixture_absolute(self, workspace_root: Path) -> Path:
        return workspace_root.resolve() / self.preview_fixture

    def simulation_fixture_absolute(self, workspace_root: Path) -> Path:
        return workspace_root.resolve() / self.simulation_fixture

    @property
    def sample_asset_id(self) -> str:
        return f"sample.dxf.{self.case_id}"

    @property
    def compat_baseline_asset_id(self) -> str:
        return f"baseline.simulation.compat_{self.case_id}"

    @property
    def dxf_fixture_asset_id(self) -> str:
        return f"protocol.fixture.{self.case_id}_dxf"

    @property
    def pb_fixture_asset_id(self) -> str:
        return f"protocol.fixture.{self.case_id}_pb"

    @property
    def preview_fixture_asset_id(self) -> str:
        return f"protocol.fixture.{self.case_id}_preview_artifact"

    @property
    def engineering_fixture_asset_id(self) -> str:
        return f"protocol.fixture.{self.case_id}_engineering"

    def engineering_fixture_asset_ids(self) -> tuple[str, ...]:
        return (
            self.dxf_fixture_asset_id,
            self.pb_fixture_asset_id,
            self.engineering_fixture_asset_id,
            self.preview_fixture_asset_id,
        )

    def compat_required_asset_ids(self) -> tuple[str, ...]:
        return (
            self.engineering_fixture_asset_id,
            self.compat_baseline_asset_id,
        )


def load_truth_matrix_manifest(workspace_root: Path) -> dict[str, Any]:
    manifest_file = truth_matrix_manifest_path(workspace_root)
    if not manifest_file.exists():
        raise FileNotFoundError(f"dxf truth matrix missing: {manifest_file}")
    payload = _load_json(manifest_file)
    if payload.get("schema_version") != TRUTH_MATRIX_SCHEMA_VERSION:
        raise ValueError(
            f"dxf truth matrix schema_version must be {TRUTH_MATRIX_SCHEMA_VERSION}"
        )
    return payload


@lru_cache(maxsize=None)
def _full_chain_cases_cached(workspace_root_str: str) -> tuple[FullChainTruthCase, ...]:
    workspace_root = Path(workspace_root_str).resolve()
    payload = load_truth_matrix_manifest(workspace_root)
    raw_cases = payload.get("full_chain_canonical_cases")
    if not isinstance(raw_cases, list) or not raw_cases:
        raise ValueError("dxf truth matrix full_chain_canonical_cases must be a non-empty array")

    cases: list[FullChainTruthCase] = []
    seen_case_ids: set[str] = set()
    runtime_defaults: list[str] = []
    hil_defaults: list[str] = []

    for raw_case in raw_cases:
        if not isinstance(raw_case, dict):
            raise ValueError("dxf truth matrix full_chain_canonical_cases entry must be an object")

        case_id = _require_non_empty_string(
            raw_case.get("case_id"),
            field_name="case_id",
            entry_id="full_chain_case",
        )
        if case_id in seen_case_ids:
            raise ValueError(f"dxf truth matrix contains duplicate case_id: {case_id}")
        seen_case_ids.add(case_id)

        topology_family = _require_non_empty_string(
            raw_case.get("topology_family"),
            field_name="topology_family",
            entry_id=case_id,
        )
        source_sample_path = _require_canonical_relative_path(
            workspace_root,
            raw_case.get("source_sample_path"),
            field_name="source_sample_path",
            entry_id=case_id,
            allowed_roots=("samples",),
        )
        fixture_root = _require_canonical_relative_path(
            workspace_root,
            raw_case.get("fixture_root"),
            field_name="fixture_root",
            entry_id=case_id,
            allowed_roots=("shared/contracts",),
        )
        dxf_fixture = _require_canonical_relative_path(
            workspace_root,
            raw_case.get("dxf_fixture"),
            field_name="dxf_fixture",
            entry_id=case_id,
            allowed_roots=("shared/contracts",),
        )
        pb_fixture = _require_canonical_relative_path(
            workspace_root,
            raw_case.get("pb_fixture"),
            field_name="pb_fixture",
            entry_id=case_id,
            allowed_roots=("shared/contracts",),
        )
        preview_fixture = _require_canonical_relative_path(
            workspace_root,
            raw_case.get("preview_fixture"),
            field_name="preview_fixture",
            entry_id=case_id,
            allowed_roots=("shared/contracts",),
        )
        simulation_fixture = _require_canonical_relative_path(
            workspace_root,
            raw_case.get("simulation_fixture"),
            field_name="simulation_fixture",
            entry_id=case_id,
            allowed_roots=("shared/contracts",),
        )

        default_runtime_sample = bool(raw_case.get("default_runtime_sample", False))
        default_hil_sample = bool(raw_case.get("default_hil_sample", False))
        if default_runtime_sample:
            runtime_defaults.append(case_id)
        if default_hil_sample:
            hil_defaults.append(case_id)

        notes = raw_case.get("notes")
        normalized_notes = notes.strip() if isinstance(notes, str) else ""
        cases.append(
            FullChainTruthCase(
                case_id=case_id,
                topology_family=topology_family,
                source_sample_path=source_sample_path,
                fixture_root=fixture_root,
                dxf_fixture=dxf_fixture,
                pb_fixture=pb_fixture,
                preview_fixture=preview_fixture,
                simulation_fixture=simulation_fixture,
                default_runtime_sample=default_runtime_sample,
                default_hil_sample=default_hil_sample,
                notes=normalized_notes,
            )
        )

    if len(runtime_defaults) != 1:
        raise ValueError(
            "dxf truth matrix must declare exactly one default_runtime_sample case"
        )
    if len(hil_defaults) != 1:
        raise ValueError(
            "dxf truth matrix must declare exactly one default_hil_sample case"
        )

    return tuple(cases)


def full_chain_cases(workspace_root: Path) -> tuple[FullChainTruthCase, ...]:
    return _full_chain_cases_cached(str(workspace_root.resolve()))


def resolve_full_chain_case(workspace_root: Path, case_id: str) -> FullChainTruthCase:
    for case in full_chain_cases(workspace_root):
        if case.case_id == case_id:
            return case
    raise KeyError(f"unknown dxf full-chain case_id: {case_id}")


def default_runtime_case(workspace_root: Path) -> FullChainTruthCase:
    for case in full_chain_cases(workspace_root):
        if case.default_runtime_sample:
            return case
    raise RuntimeError("default_runtime_sample case not found")


def default_hil_case(workspace_root: Path) -> FullChainTruthCase:
    for case in full_chain_cases(workspace_root):
        if case.default_hil_sample:
            return case
    raise RuntimeError("default_hil_sample case not found")


def full_chain_case_ids(workspace_root: Path) -> tuple[str, ...]:
    return tuple(case.case_id for case in full_chain_cases(workspace_root))
