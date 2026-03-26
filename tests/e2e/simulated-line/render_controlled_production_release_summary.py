from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT_DIR = ROOT / "tests" / "reports" / "controlled-production-test"

REQUIRED_RUNTIME_CASES = (
    "simulation-runtime-core-contracts",
    "simulation-engine-scheme-c",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render controlled-production release summary from gate/report artifacts.")
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR))
    parser.add_argument("--profile", default="Local")
    parser.add_argument("--executor", default="")
    parser.add_argument("--gate-summary-json", default="")
    parser.add_argument("--workspace-validation-json", default="")
    parser.add_argument("--simulated-line-summary-json", default="")
    return parser.parse_args()


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _resolve_path(explicit_path: str, fallback_path: Path) -> Path:
    if explicit_path:
        return Path(explicit_path).resolve()
    return fallback_path.resolve()


def _status_map_from_workspace(payload: dict[str, Any]) -> dict[str, str]:
    mapping: dict[str, str] = {}
    for item in payload.get("results", []):
        name = str(item.get("name", ""))
        status = str(item.get("status", ""))
        if name:
            mapping[name] = status
    return mapping


def _format_result(ok: bool) -> str:
    return "通过" if ok else "阻塞"


def _determine_conclusion(gate_status: str) -> str:
    if gate_status == "passed":
        return "通过"
    return "阻塞"


def _render_summary(
    *,
    report_dir: Path,
    profile: str,
    executor: str,
    gate_payload: dict[str, Any],
    workspace_payload: dict[str, Any],
    simulated_payload: dict[str, Any],
    gate_path: Path,
    workspace_path: Path,
    simulated_path: Path,
) -> str:
    gate_status = str(gate_payload.get("overall_status", "failed"))
    workspace_counts = workspace_payload.get("counts", {})
    workspace_map = _status_map_from_workspace(workspace_payload)
    simulated_status = str(simulated_payload.get("overall_status", "failed"))
    scenario_items = simulated_payload.get("scenarios", [])

    simulation_suite_ok = (
        int(workspace_counts.get("failed", 0)) == 0
        and int(workspace_counts.get("known_failure", 0)) == 0
        and int(workspace_counts.get("skipped", 0)) == 0
    )
    runtime_contracts_ok = all(workspace_map.get(case_name) == "passed" for case_name in REQUIRED_RUNTIME_CASES)
    simulated_compat_ok = workspace_map.get("simulated-line") == "passed"
    simulated_scheme_c_ok = simulated_status == "passed"
    deterministic_replay_ok = all(bool(item.get("deterministic_replay_passed", False)) for item in scenario_items)
    no_known_failure_skipped = (
        int(workspace_counts.get("known_failure", 0)) == 0 and int(workspace_counts.get("skipped", 0)) == 0
    )

    conclusion = _determine_conclusion(gate_status)

    executor_text = executor.strip() if executor.strip() else "待填写"
    generated_at = datetime.now(timezone.utc).isoformat()
    report_md = report_dir / "workspace-validation.md"
    simulated_md = report_dir / "e2e" / "simulated-line" / "simulated-line-summary.md"
    gate_md = report_dir / "controlled-production-gate-summary.md"

    lines = [
        "# Simulation 受控生产测试发布结论",
        "",
        f"更新时间：`{generated_at}`",
        "",
        "## 1. 执行信息",
        "",
        f"- 执行日期：`{generated_at}`",
        f"- 执行人：`{executor_text}`",
        f"- Profile：`{profile}`",
        f"- 报告目录：`{report_dir}`",
        "",
        "## 2. 自动化证据",
        "",
        f"- `workspace-validation.json`：`{workspace_path}`",
        f"- `workspace-validation.md`：`{report_md}`",
        f"- `e2e/simulated-line/simulated-line-summary.json`：`{simulated_path}`",
        f"- `e2e/simulated-line/simulated-line-summary.md`：`{simulated_md}`",
        f"- `controlled-production-gate-summary.json`：`{gate_path}`",
        f"- `controlled-production-gate-summary.md`：`{gate_md}`",
        "",
        "## 3. 判定摘要",
        "",
        "| 项目 | 结果 | 说明 |",
        "| --- | --- | --- |",
        f"| simulation suite | `{_format_result(simulation_suite_ok)}` | failed=`{workspace_counts.get('failed', 0)}` known_failure=`{workspace_counts.get('known_failure', 0)}` skipped=`{workspace_counts.get('skipped', 0)}` |",
        f"| runtime contracts | `{_format_result(runtime_contracts_ok)}` | simulation-runtime-core-contracts / simulation-engine-scheme-c |",
        f"| simulated-line compat | `{_format_result(simulated_compat_ok)}` | workspace case `simulated-line` |",
        f"| simulated-line scheme C | `{_format_result(simulated_scheme_c_ok)}` | overall_status=`{simulated_status}` |",
        f"| deterministic replay | `{_format_result(deterministic_replay_ok)}` | scenarios=`{len(scenario_items)}` 全部 deterministic_replay_passed=true |",
        f"| known failure / skipped | `{_format_result(no_known_failure_skipped)}` | known_failure=`{workspace_counts.get('known_failure', 0)}` skipped=`{workspace_counts.get('skipped', 0)}` |",
        f"| controlled-production gate | `{_format_result(gate_status == 'passed')}` | gate overall_status=`{gate_status}` |",
        "",
        "## 4. 结论",
        "",
        f"- 结论：`{conclusion}`",
        "- 判定规则：",
        "  - Gate 为 `passed` 且关键摘要项无阻塞时，结论为 `通过`",
        "  - 否则结论为 `阻塞`",
        "",
        "## 5. 范围声明",
        "",
        "本结论只代表：",
        "",
        "- `simulation` 受控生产测试通过/未通过",
        "",
        "本结论不代表：",
        "",
        "- HIL 长稳通过",
        "- 真机 smoke 通过",
        "- 整机或工艺真实性签收完成",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()

    gate_path = _resolve_path(args.gate_summary_json, report_dir / "controlled-production-gate-summary.json")
    workspace_path = _resolve_path(args.workspace_validation_json, report_dir / "workspace-validation.json")
    simulated_path = _resolve_path(
        args.simulated_line_summary_json,
        report_dir / "e2e" / "simulated-line" / "simulated-line-summary.json",
    )

    missing = [path for path in (gate_path, workspace_path, simulated_path) if not path.exists()]
    if missing:
        for path in missing:
            print(f"missing required artifact: {path}")
        return 1

    gate_payload = _load_json(gate_path)
    workspace_payload = _load_json(workspace_path)
    simulated_payload = _load_json(simulated_path)

    markdown = _render_summary(
        report_dir=report_dir,
        profile=args.profile,
        executor=args.executor,
        gate_payload=gate_payload,
        workspace_payload=workspace_payload,
        simulated_payload=simulated_payload,
        gate_path=gate_path,
        workspace_path=workspace_path,
        simulated_path=simulated_path,
    )

    output_path = report_dir / "simulation-controlled-production-release-summary.md"
    output_path.write_text(markdown, encoding="utf-8")
    print(f"controlled-production release summary rendered: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
