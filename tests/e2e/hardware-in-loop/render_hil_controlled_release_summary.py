from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT_DIR = ROOT / "tests" / "reports" / "hil-controlled-test"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render HIL controlled release summary from controlled gate artifacts.")
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR))
    parser.add_argument("--profile", default="Local")
    parser.add_argument("--executor", default="")
    parser.add_argument("--gate-summary-json", default="")
    parser.add_argument("--offline-prereq-json", default="")
    parser.add_argument("--hardware-smoke-summary-json", default="")
    parser.add_argument("--hil-closed-loop-summary-json", default="")
    parser.add_argument("--hil-case-matrix-summary-json", default="")
    return parser.parse_args()


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _resolve(explicit: str, fallback: Path) -> Path:
    return Path(explicit).resolve() if explicit else fallback.resolve()


def _check_status(checks: list[dict[str, Any]], name: str) -> str:
    item = next((entry for entry in checks if str(entry.get("name", "")) == name), {})
    return str(item.get("status", "missing"))


def _format_result(ok: bool) -> str:
    return "通过" if ok else "阻塞"


def _render_summary(
    *,
    report_dir: Path,
    profile: str,
    executor: str,
    gate_payload: dict[str, Any],
    offline_payload: dict[str, Any],
    hardware_payload: dict[str, Any],
    hil_payload: dict[str, Any],
    case_matrix_payload: dict[str, Any] | None,
    gate_path: Path,
    offline_path: Path,
    hardware_path: Path,
    hil_path: Path,
    case_matrix_path: Path | None,
) -> str:
    checks = gate_payload.get("checks", [])
    overall_status = str(gate_payload.get("overall_status", "failed"))
    admission = hil_payload.get("admission", {}) if isinstance(hil_payload.get("admission"), dict) else {}
    safety_preflight = admission.get("safety_preflight", {}) if isinstance(admission.get("safety_preflight"), dict) else {}
    failure_classification = hil_payload.get("failure_classification", {}) if isinstance(hil_payload.get("failure_classification"), dict) else {}

    offline_ok = _check_status(checks, "offline-prereq-counts") == "passed" and _check_status(checks, "offline-prereq-required-cases") == "passed"
    hardware_ok = _check_status(checks, "hardware-smoke-overall-status") == "passed"
    hil_ok = _check_status(checks, "hil-closed-loop-overall-status") == "passed"
    bundle_schema_ok = _check_status(checks, "hil-bundle-schema-version") == "passed"
    bundle_manifest_ok = _check_status(checks, "hil-bundle-report-manifest") == "passed"
    bundle_index_ok = _check_status(checks, "hil-bundle-report-index") == "passed"
    admission_ok = _check_status(checks, "hil-bundle-admission-metadata") == "passed"
    preflight_ok = _check_status(checks, "hil-bundle-safety-preflight") == "passed"
    override_ok = _check_status(checks, "hil-bundle-operator-override") == "passed"
    case_matrix_required = bool(gate_payload.get("require_hil_case_matrix", False))
    case_matrix_ok = (not case_matrix_required) or _check_status(checks, "hil-case-matrix-overall-status") == "passed"

    generated_at = datetime.now(timezone.utc).isoformat()
    executor_text = executor.strip() or "待填写"
    gate_md = gate_path.with_suffix(".md")
    offline_md = offline_path.with_suffix(".md")
    hardware_md = hardware_path.with_suffix(".md")
    hil_md = hil_path.with_suffix(".md")

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
        f"- `offline-prereq/workspace-validation.json`：`{offline_path}`",
        f"- `offline-prereq/workspace-validation.md`：`{offline_md}`",
        f"- `hardware-smoke-summary.json`：`{hardware_path}`",
        f"- `hardware-smoke-summary.md`：`{hardware_md}`",
        f"- `hil-closed-loop-summary.json`：`{hil_path}`",
        f"- `hil-closed-loop-summary.md`：`{hil_md}`",
        f"- `hil-controlled-gate-summary.json`：`{gate_path}`",
        f"- `hil-controlled-gate-summary.md`：`{gate_md}`",
    ]
    if case_matrix_path is not None:
        lines.append(f"- `hil-case-matrix/case-matrix-summary.json`：`{case_matrix_path}`")
        lines.append(f"- `hil-case-matrix/case-matrix-summary.md`：`{case_matrix_path.with_suffix('.md')}`")

    lines.extend(
        [
            "",
            "## 3. 判定摘要",
            "",
            "| 项目 | 结果 | 说明 |",
            "| --- | --- | --- |",
            f"| offline prerequisites | `{_format_result(offline_ok)}` | required cases={','.join(gate_payload.get('required_offline_cases', []))} |",
            f"| hardware smoke | `{_format_result(hardware_ok)}` | overall_status=`{hardware_payload.get('overall_status', '')}` |",
            f"| hil-closed-loop | `{_format_result(hil_ok)}` | overall_status=`{hil_payload.get('overall_status', '')}` |",
            f"| admission metadata | `{_format_result(admission_ok)}` | admission_decision=`{admission.get('admission_decision', '')}` |",
            f"| safety preflight | `{_format_result(preflight_ok)}` | passed=`{admission.get('safety_preflight_passed', False)}` estop=`{safety_preflight.get('estop_active', False)}` limits=`{','.join(safety_preflight.get('limit_blockers', []))}` |",
            f"| operator override | `{_format_result(override_ok)}` | used=`{admission.get('operator_override_used', False)}` reason=`{admission.get('operator_override_reason', '')}` |",
            f"| evidence schema | `{_format_result(bundle_schema_ok and bundle_manifest_ok and bundle_index_ok)}` | bundle/manifest/index compatibility |",
            f"| failure classification | `{failure_classification.get('category', '')}:{failure_classification.get('code', '')}` | blocking=`{failure_classification.get('blocking', False)}` |",
        ]
    )
    if case_matrix_required:
        lines.append(
            f"| hil-case-matrix | `{_format_result(case_matrix_ok)}` | overall_status=`{(case_matrix_payload or {}).get('overall_status', 'missing')}` |"
        )
    lines.extend(
        [
            f"| controlled gate | `{_format_result(overall_status == 'passed')}` | gate overall_status=`{overall_status}` |",
            "",
            "## 4. 结论",
            "",
            f"- 结论：`{'通过' if overall_status == 'passed' else '阻塞'}`",
            "- 判定规则：",
            "  - offline prerequisites、hardware smoke、hil-closed-loop、admission/preflight、evidence schema 均通过时，结论为 `通过`。",
            "  - 任一 blocking gate 失败时，结论为 `阻塞`。",
            "",
            "## 5. 范围声明",
            "",
            "本结论只代表：",
            "",
            "- `limited-hil` 受控测试门禁是否满足发布前提",
            "",
            "本结论不代表：",
            "",
            "- 工艺质量签收完成",
            "- 整机产能签收完成",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    gate_path = _resolve(args.gate_summary_json, report_dir / "hil-controlled-gate-summary.json")
    offline_path = _resolve(args.offline_prereq_json, report_dir / "offline-prereq" / "workspace-validation.json")
    hardware_path = _resolve(args.hardware_smoke_summary_json, report_dir / "hardware-smoke" / "hardware-smoke-summary.json")
    hil_path = _resolve(args.hil_closed_loop_summary_json, report_dir / "hil-closed-loop-summary.json")
    case_matrix_required = False
    case_matrix_path: Path | None = None

    missing = [path for path in (gate_path, offline_path, hardware_path, hil_path) if not path.exists()]
    if missing:
        for path in missing:
            print(f"missing required artifact: {path}")
        return 1

    gate_payload = _load_json(gate_path)
    case_matrix_required = bool(gate_payload.get("require_hil_case_matrix", False))
    if case_matrix_required:
        case_matrix_path = _resolve(args.hil_case_matrix_summary_json, report_dir / "hil-case-matrix" / "case-matrix-summary.json")
        if not case_matrix_path.exists():
            print(f"missing required case-matrix artifact: {case_matrix_path}")
            return 1

    markdown = _render_summary(
        report_dir=report_dir,
        profile=args.profile,
        executor=args.executor,
        gate_payload=gate_payload,
        offline_payload=_load_json(offline_path),
        hardware_payload=_load_json(hardware_path),
        hil_payload=_load_json(hil_path),
        case_matrix_payload=_load_json(case_matrix_path) if case_matrix_path is not None else None,
        gate_path=gate_path,
        offline_path=offline_path,
        hardware_path=hardware_path,
        hil_path=hil_path,
        case_matrix_path=case_matrix_path,
    )

    output_path = report_dir / "hil-controlled-release-summary.md"
    output_path.write_text(markdown, encoding="utf-8")
    print(f"hil controlled release summary rendered: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
