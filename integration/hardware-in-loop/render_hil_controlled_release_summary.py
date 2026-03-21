from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT_DIR = ROOT / "integration" / "reports" / "hil-controlled-test"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render HIL controlled release summary from gate/report artifacts.")
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR))
    parser.add_argument("--profile", default="Local")
    parser.add_argument("--executor", default="")
    parser.add_argument("--gate-summary-json", default="")
    parser.add_argument("--workspace-validation-json", default="")
    parser.add_argument("--hil-closed-loop-summary-json", default="")
    return parser.parse_args()


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _to_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _to_float(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _resolve_path(explicit_path: str, fallback_path: Path) -> Path:
    if explicit_path:
        return Path(explicit_path).resolve()
    return fallback_path.resolve()


def _resolve_hil_summary_path(report_dir: Path, explicit_path: str) -> Path:
    if explicit_path:
        return Path(explicit_path).resolve()
    candidates = (
        report_dir / "hil-closed-loop-summary.json",
        report_dir / "hil-controlled-test" / "hil-closed-loop-summary.json",
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return candidates[0].resolve()


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
    hil_payload: dict[str, Any],
    gate_path: Path,
    workspace_path: Path,
    hil_path: Path,
) -> str:
    gate_status = str(gate_payload.get("overall_status", "failed"))
    workspace_counts = workspace_payload.get("counts", {})
    workspace_map = _status_map_from_workspace(workspace_payload)

    hil_overall_status = str(hil_payload.get("overall_status", "failed"))
    duration_seconds = _to_float(hil_payload.get("duration_seconds", 0))
    elapsed_seconds = _to_float(hil_payload.get("elapsed_seconds", 0))
    timeout_count = _to_int(hil_payload.get("timeout_count", 0))
    reconnect_count = _to_int(hil_payload.get("reconnect_count", 0))
    transition_checks = hil_payload.get("state_transition_checks", [])

    hardware_smoke_ok = workspace_map.get("hardware-smoke") == "passed"
    hil_closed_loop_ok = workspace_map.get("hil-closed-loop") == "passed" and hil_overall_status == "passed"
    duration_ok = duration_seconds > 0 and elapsed_seconds >= duration_seconds
    transition_ok = bool(transition_checks) and all(str(item.get("status", "")) == "passed" for item in transition_checks)
    timeout_ok = timeout_count == 0
    reconnect_ok = reconnect_count > 0
    no_known_failure_skipped = (
        _to_int(workspace_counts.get("known_failure", 0)) == 0
        and _to_int(workspace_counts.get("skipped", 0)) == 0
    )

    conclusion = _determine_conclusion(gate_status)
    executor_text = executor.strip() if executor.strip() else "待填写"
    generated_at = datetime.now(timezone.utc).isoformat()

    workspace_md = workspace_path.with_suffix(".md")
    hil_md = hil_path.with_suffix(".md")
    gate_md = gate_path.with_suffix(".md")

    lines = [
        "# HIL 受控测试发布结论",
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
        f"- `workspace-validation.md`：`{workspace_md}`",
        f"- `hil-closed-loop-summary.json`：`{hil_path}`",
        f"- `hil-closed-loop-summary.md`：`{hil_md}`",
        f"- `hil-controlled-gate-summary.json`：`{gate_path}`",
        f"- `hil-controlled-gate-summary.md`：`{gate_md}`",
        "",
        "## 3. 判定摘要",
        "",
        "| 项目 | 结果 | 说明 |",
        "| --- | --- | --- |",
        f"| hardware smoke | `{_format_result(hardware_smoke_ok)}` | workspace case `hardware-smoke` |",
        f"| hil-closed-loop | `{_format_result(hil_closed_loop_ok)}` | workspace case + overall_status=`{hil_overall_status}` |",
        f"| long soak duration | `{_format_result(duration_ok)}` | duration_seconds=`{duration_seconds}` elapsed_seconds=`{elapsed_seconds}` |",
        f"| state transition checks | `{_format_result(transition_ok)}` | checks=`{len(transition_checks)}` 全部 status=passed |",
        f"| timeout_count | `{_format_result(timeout_ok)}` | timeout_count=`{timeout_count}` |",
        f"| reconnect_count | `{_format_result(reconnect_ok)}` | reconnect_count=`{reconnect_count}` |",
        f"| known failure / skipped | `{_format_result(no_known_failure_skipped)}` | known_failure=`{workspace_counts.get('known_failure', 0)}` skipped=`{workspace_counts.get('skipped', 0)}` |",
        f"| controlled gate | `{_format_result(gate_status == 'passed')}` | gate overall_status=`{gate_status}` |",
        "",
        "## 4. 结论",
        "",
        f"- 结论：`{conclusion}`",
        "- 判定规则：",
        "  - Gate 为 `passed` 时，结论为 `通过`",
        "  - 否则结论为 `阻塞`",
        "",
        "## 5. 范围声明",
        "",
        "本结论只代表：",
        "",
        "- `HIL` 受控测试通过/未通过",
        "",
        "本结论不代表：",
        "",
        "- 工艺质量签收完成",
        "- 整机产能签收完成",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()

    gate_path = _resolve_path(args.gate_summary_json, report_dir / "hil-controlled-gate-summary.json")
    workspace_path = _resolve_path(args.workspace_validation_json, report_dir / "workspace-validation.json")
    hil_path = _resolve_hil_summary_path(report_dir, args.hil_closed_loop_summary_json)

    missing = [path for path in (gate_path, workspace_path, hil_path) if not path.exists()]
    if missing:
        for path in missing:
            print(f"missing required artifact: {path}")
        return 1

    gate_payload = _load_json(gate_path)
    workspace_payload = _load_json(workspace_path)
    hil_payload = _load_json(hil_path)

    markdown = _render_summary(
        report_dir=report_dir,
        profile=args.profile,
        executor=args.executor,
        gate_payload=gate_payload,
        workspace_payload=workspace_payload,
        hil_payload=hil_payload,
        gate_path=gate_path,
        workspace_path=workspace_path,
        hil_path=hil_path,
    )

    output_path = report_dir / "hil-controlled-release-summary.md"
    output_path.write_text(markdown, encoding="utf-8")
    print(f"hil controlled release summary rendered: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
