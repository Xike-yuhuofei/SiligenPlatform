from __future__ import annotations

import json
import sys
from datetime import date
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any, cast


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.asset_catalog import baseline_governance_summary, build_asset_inventory


def _write_json(path: Path, payload: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")


def _assert_workspace_governance() -> dict[str, object]:
    inventory = cast(dict[str, Any], build_asset_inventory(WORKSPACE_ROOT))
    governance = cast(dict[str, Any], baseline_governance_summary(WORKSPACE_ROOT))
    assert governance["blocking_issue_count"] == 0
    assert governance["advisory_issue_count"] == 0
    assert "samples" in inventory["source_roots"]
    assert "tests/baselines" in inventory["source_roots"]
    assert "baseline.simulation.compat_rect_diag" in inventory["baseline_manifest_entry_ids"]
    return {
        "inventory": inventory,
        "governance": governance,
    }


def _assert_synthetic_detection() -> dict[str, object]:
    with TemporaryDirectory(prefix="baseline-governance-smoke-") as temp_dir:
        root = Path(temp_dir)
        (root / "samples" / "dxf").mkdir(parents=True, exist_ok=True)
        (root / "tests" / "baselines").mkdir(parents=True, exist_ok=True)
        (root / "data" / "baselines" / "simulation").mkdir(parents=True, exist_ok=True)

        (root / "samples" / "dxf" / "rect_diag.dxf").write_text("0\nSECTION\n0\nEOF\n", encoding="utf-8")
        _write_json(
            root / "tests" / "baselines" / "rect_diag.simulation-baseline.json",
            {"kind": "canonical"},
        )
        _write_json(
            root / "tests" / "baselines" / "unused.simulation-baseline.json",
            {"kind": "unused"},
        )
        _write_json(
            root / "data" / "baselines" / "simulation" / "rect_diag.simulation-baseline.json",
            {"kind": "deprecated-duplicate"},
        )
        (root / "tests" / "baselines" / "baseline-manifest.schema.json").write_text(
            (WORKSPACE_ROOT / "tests" / "baselines" / "baseline-manifest.schema.json").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        _write_json(
            root / "tests" / "baselines" / "baseline-manifest.json",
            {
                "schema_version": "baseline-manifest.v1",
                "manifest_owner": "shared/testing",
                "reviewer_workflow": {
                    "required_reviewers": ["owner-scope"],
                    "required_evidence": ["tests/reports/**/workspace-validation.json"],
                    "disallowed_sources": ["data/baselines/**"],
                    "steps": ["refresh baseline with evidence"],
                },
                "baselines": [
                    {
                        "baseline_id": "baseline.simulation.compat_rect_diag",
                        "canonical_path": "tests/baselines/rect_diag.simulation-baseline.json",
                        "baseline_kind": "simulation-summary",
                        "owner_scope": "runtime-execution",
                        "source_asset_refs": ["sample.dxf.rect_diag"],
                        "consumer_layers": ["L3"],
                        "update_runner": "tests/e2e/simulated-line/run_simulated_line.py",
                        "update_reviewers": ["runtime-execution"],
                        "reviewer_guidance": "synthetic governance smoke",
                        "lifecycle": {
                            "state": "active",
                            "review_by": "2026-03-01"
                        },
                        "deprecated_source_paths": [
                            "data/baselines/simulation/rect_diag.simulation-baseline.json"
                        ]
                    }
                ]
            },
        )

        governance = baseline_governance_summary(root, today=date(2026, 4, 1))
        assert governance["duplicate_source_issues"], governance
        assert governance["deprecated_root_issues"], governance
        assert governance["stale_baseline_ids"] == ["baseline.simulation.compat_rect_diag"], governance
        assert governance["unused_baseline_paths"] == ["tests/baselines/unused.simulation-baseline.json"], governance
        return governance


def main() -> int:
    payload = {
        "workspace": _assert_workspace_governance(),
        "synthetic_detection": _assert_synthetic_detection(),
    }
    print(json.dumps(payload, ensure_ascii=True, indent=2))
    print("baseline governance smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
