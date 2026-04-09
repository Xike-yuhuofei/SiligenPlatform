from __future__ import annotations

import json
import sys
from pathlib import Path


def _load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def _branch_percent(summary: dict) -> float:
    num_branches = float(summary.get("num_branches", 0) or 0)
    covered_branches = float(summary.get("covered_branches", 0) or 0)
    if num_branches > 0:
        return round((covered_branches / num_branches) * 100.0, 2)
    return round(float(summary.get("percent_covered", 0) or 0), 2)


def build_summary(coverage_payload: dict, threshold_payload: dict) -> dict:
    python_thresholds = threshold_payload["python"]
    files = coverage_payload.get("files", {})
    critical_results: list[dict] = []
    for entry in python_thresholds["critical_paths"]:
        prefix = entry["path_prefix"].replace("\\", "/")
        matching = [
            payload
            for name, payload in files.items()
            if name.replace("\\", "/").startswith(prefix)
        ]
        covered_branches = sum(float(item["summary"].get("covered_branches", 0) or 0) for item in matching)
        num_branches = sum(float(item["summary"].get("num_branches", 0) or 0) for item in matching)
        covered_lines = sum(float(item["summary"].get("covered_lines", 0) or 0) for item in matching)
        num_statements = sum(float(item["summary"].get("num_statements", 0) or 0) for item in matching)
        if num_branches > 0:
            actual = round((covered_branches / num_branches) * 100.0, 2)
        elif num_statements > 0:
            actual = round((covered_lines / num_statements) * 100.0, 2)
        else:
            actual = 0.0
        threshold = float(entry["branch_min"])
        critical_results.append(
            {
                "path_prefix": prefix,
                "actual_branch_percent": actual,
                "threshold": threshold,
                "status": "passed" if actual >= threshold else "failed",
            }
        )

    return {
        "global_branch_percent": _branch_percent(coverage_payload["totals"]),
        "global_branch_min": float(python_thresholds["global_branch_min"]),
        "enforcement": python_thresholds["enforcement"],
        "critical_results": critical_results,
    }


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: python-coverage-summary.py <coverage-json> <threshold-json>")

    coverage_path = Path(sys.argv[1]).resolve()
    threshold_path = Path(sys.argv[2]).resolve()
    summary = build_summary(_load_json(coverage_path), _load_json(threshold_path))
    json.dump(summary, sys.stdout, ensure_ascii=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
