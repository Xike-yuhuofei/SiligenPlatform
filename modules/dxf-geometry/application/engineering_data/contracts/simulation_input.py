from __future__ import annotations

"""Stable simulation-input contract surface backed by runtime-execution owner."""

from pathlib import Path
import sys


def _bootstrap_runtime_execution() -> None:
    workspace_root = Path(__file__).resolve().parents[5]
    runtime_root = workspace_root / "modules" / "runtime-execution" / "application"
    runtime_root_str = str(runtime_root)
    if runtime_root_str not in sys.path:
        sys.path.insert(0, runtime_root_str)


_bootstrap_runtime_execution()

from engineering_data.processing.simulation_geometry import (  # noqa: E402
    bundle_contains_high_order_primitives,
    collect_export_notes,
    count_high_order_primitives,
    count_skipped_points,
    load_path_bundle,
)
from runtime_execution.contracts import (  # noqa: E402
    DEFAULT_EXPORTS,
    ExportDefaults,
    IoDelay,
    TriggerPoint,
    ValveConfig,
)
from runtime_execution.simulation_input import (  # noqa: E402
    bundle_to_simulation_payload,
    load_trigger_points_csv,
    project_trigger_points,
    write_simulation_payload,
)

__all__ = [
    "DEFAULT_EXPORTS",
    "ExportDefaults",
    "IoDelay",
    "TriggerPoint",
    "ValveConfig",
    "bundle_contains_high_order_primitives",
    "bundle_to_simulation_payload",
    "collect_export_notes",
    "count_high_order_primitives",
    "count_skipped_points",
    "load_path_bundle",
    "load_trigger_points_csv",
    "project_trigger_points",
    "write_simulation_payload",
]
