from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.workspace_layout import load_workspace_layout  # noqa: E402


VALIDATION_WAVES = tuple(f"Wave {index}" for index in range(7))
WAVE_INDEX = {wave: index for index, wave in enumerate(VALIDATION_WAVES)}

REQUIRED_DOCS = {
    "S01": "dsp-e2e-spec-s01-stage-io-matrix.md",
    "S02": "dsp-e2e-spec-s02-stage-responsibility-acceptance.md",
    "S03": "dsp-e2e-spec-s03-stage-errorcode-rollback.md",
    "S04": "dsp-e2e-spec-s04-stage-artifact-dictionary.md",
    "S05": "dsp-e2e-spec-s05-module-boundary-interface-contract.md",
    "S06": "dsp-e2e-spec-s06-repo-structure-guide.md",
    "S07": "dsp-e2e-spec-s07-state-machine-command-bus.md",
    "S08": "dsp-e2e-spec-s08-system-sequence-template.md",
    "S09": "dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md",
    "S10": "dsp-e2e-spec-s10-frozen-directory-index.md",
}
REQUIRED_SUPPLEMENTAL_DOCS = {
    "project-chain-standard-v1.md": "冻结项目内正式对象链、live 控制链与支撑链的统一定义与分类边界",
    "internal_execution_contract_v_1.md": "冻结内部执行契约与执行语义边界",
    "dxf-input-governance-standard-v1.md": "冻结 DXF 输入治理、验证报告、错误码、policy、fixture 与门禁收敛标准",
}
DOC_MIN_WAVES = {
    "S01": "Wave 0",
    "S02": "Wave 0",
    "S03": "Wave 0",
    "S04": "Wave 2",
    "S05": "Wave 3",
    "S06": "Wave 1",
    "S07": "Wave 4",
    "S08": "Wave 4",
    "S09": "Wave 5",
    "S10": "Wave 0",
}
REQUIRED_TERMS = (
    "ExecutionPackageBuilt",
    "ExecutionPackageValidated",
    "PreflightBlocked",
    "ArtifactSuperseded",
    "WorkflowArchived",
)
REQUIRED_TERMS_BY_AXIS = {
    "S01": REQUIRED_TERMS,
    "S02": REQUIRED_TERMS,
}
STAGE_CHAIN = (
    "S0",
    "S1",
    "S2",
    "S3",
    "S4",
    "S5",
    "S6",
    "S7",
    "S8",
    "S9",
    "S10",
    "S11A",
    "S11B",
    "S12",
    "S13",
    "S14",
    "S15",
    "S16",
)
REQUIRED_SECTIONS = {
    "S01": (
        "# 1. 阶段输入输出总矩阵",
        "## 1.1 主矩阵",
        "# 2. 失败归属层映射",
        "# 3. 回退目标判定表（统一口径）",
    ),
    "S02": (
        "# 1. 阶段通用验收硬规则",
        "# 2. 阶段职责与验收准则",
        "# 3. 跨阶段验收硬约束",
    ),
    "S03": (
        "# 1. 失败码与失败载荷设计规则",
        "# 2. 失败处理总规则",
        "# 3. 分阶段失败码与回退策略总表",
    ),
    "S10": (
        "# 1. 整体定位",
        "# 2. 文档使用顺序（推荐）",
        "# 3. 9 份文档的一致性主轴",
        "# 4. 每份文档的角色定义",
    ),
}
TERMINOLOGY_POLICY_TOKENS = {
    "S01": (
        "### 规则 B：S11A 与 S11B 不可合并",
        "### 规则 C：S12 阻断不改写 S5-S11 产物",
    ),
    "S02": (
        "## 1.4 回退必须显式触发 `ArtifactSuperseded`",
        "## 1.5 归档必须显式触发 `WorkflowArchived`",
    ),
    "S03": (
        "## 2.4 S12 设备未 ready，不触发规划回退",
        "## 2.5 S13 首件拒绝后，必须先分流再回退",
    ),
    "S10": (
        "## 3.4 主轴 D：关键事件集一致",
        "## 3.8 主轴 H：测试与验收口径一致",
    ),
}
ARTIFACT_NAMES = (
    "JobDefinition",
    "SourceDrawing",
    "CanonicalGeometry",
    "TopologyModel",
    "FeatureGraph",
    "ProcessPlan",
    "CoordinateTransformSet",
    "AlignmentCompensation",
    "ProcessPath",
    "MotionPlan",
    "DispenseTimingPlan",
    "ExecutionPackage",
    "MachineReadySnapshot",
    "FirstArticleResult",
    "ExecutionRecord",
)
MODULE_IDS = tuple(f"M{index}" for index in range(12))
S06_REQUIRED_TOKENS = (
    "## 3.1 `apps/`",
    "## 3.2 `modules/`",
    "## 3.3 `shared/`",
    "## 3.4 `docs/`",
    "## 3.5 `samples/`",
    "## 3.6 `tests/`",
    "## 8.1 允许的依赖方向",
    "## 8.2 明确禁止的依赖方向",
    "## 10.1 模块内测试跟模块走",
)
S08_CHAIN_TOKENS = {
    "主成功链": (
        "# 3. 主成功链时序图",
        "`ExecutionPackageValidated`",
        "`PreflightPassed`",
        "`WorkflowArchived`",
    ),
    "回退链": (
        "# 4. 首件失败回退链时序图",
        "`ArtifactSuperseded`",
    ),
    "阻断链": (
        "# 5. 设备未就绪阻断链时序图",
        "`PreflightBlocked`",
    ),
    "恢复链": (
        "# 6. 运行时故障恢复 / 终止链时序图",
        "`ExecutionRecovered`",
    ),
    "归档链": (
        "# 7. 执行固化与归档链时序图",
        "`ExecutionRecordBuilt`",
        "`WorkflowArchived`",
    ),
}
SCENARIO_CLASSES = ("success", "block", "rollback", "recovery", "archive")
SCENARIO_SECTION_TOKENS = {
    "success": "## 5.1 主成功链测试",
    "rollback": "## 5.2 首件失败回退链测试",
    "block": "## 5.3 设备未就绪阻断链测试",
    "recovery": "## 5.4 运行时故障恢复 / 终止链测试",
    "archive": "## 5.5 归档与追溯链测试",
}
S10_AXIS_TOKENS = {
    "S01": "## 4.1 s01《阶段输入输出矩阵》",
    "S02": "## 4.2 s02《阶段职责与验收准则表》",
    "S03": "## 4.3 s03《阶段失败码与回退策略表》",
    "S04": "## 4.4 s04《阶段产物数据字典》",
    "S05": "## 4.5 s05《模块边界与接口契约表》",
    "S06": "## 4.6 s06《仓库目录结构建议》",
    "S07": "## 4.7 s07《状态机与命令总线模板》",
    "S08": "## 4.8 s08《系统时序图模板》",
    "S09": "## 4.9 s09《测试矩阵与验收基线》",
}


@dataclass(frozen=True)
class Finding:
    rule_id: str
    message: str
    file: str | None = None
    min_wave: str | None = None


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.exists() else ""


def _relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def _is_wave_active(min_wave: str, requested_wave_index: int | None) -> bool:
    if requested_wave_index is None:
        return True
    return WAVE_INDEX[min_wave] <= requested_wave_index


def _active_doc_axes(requested_wave_index: int | None) -> tuple[str, ...]:
    return tuple(
        axis
        for axis in REQUIRED_DOCS
        if _is_wave_active(DOC_MIN_WAVES[axis], requested_wave_index)
    )


def _report_markdown(payload: dict[str, object]) -> str:
    findings: list[dict[str, str | None]] = payload["findings"]  # type: ignore[assignment]
    lines = [
        "# DSP E2E Spec Docset Validation",
        "",
        f"- workspace_root: `{payload['workspace_root']}`",
        f"- formal_docset_root: `{payload['formal_docset_root']}`",
        f"- index_file: `{payload['index_file']}`",
        f"- requested_wave: `{payload['requested_wave']}`",
        f"- active_waves: `{', '.join(payload['active_waves'])}`",
        f"- active_doc_axes: `{', '.join(payload['active_doc_axes'])}`",
        f"- report_stem: `{payload['report_stem']}`",
        f"- scenario_classes: `{', '.join(payload['scenario_classes'])}`",
        f"- missing_doc_count: `{payload['missing_doc_count']}`",
        f"- finding_count: `{payload['finding_count']}`",
        "",
        "## Files",
        "",
        "| Axis | Min wave | Active | File | Exists |",
        "|---|---|---|---|---|",
    ]
    for axis, item in payload["files"].items():  # type: ignore[assignment]
        lines.append(
            f"| `{axis}` | `{item['min_wave']}` | `{str(item['active']).lower()}` | `{item['path']}` | `{str(item['exists']).lower()}` |"
        )

    lines.extend(["", "## Findings", ""])
    if not findings:
        lines.append("- pass")
    else:
        for finding in findings:
            location = f" ({finding['file']})" if finding.get("file") else ""
            min_wave = f" [min_wave={finding['min_wave']}]" if finding.get("min_wave") else ""
            lines.append(f"- `{finding['rule_id']}`{location}{min_wave}: {finding['message']}")
    lines.append("")
    return "\n".join(lines)


def _write_report(report_dir: Path, report_stem: str, payload: dict[str, object]) -> None:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / f"{report_stem}.json"
    md_path = report_dir / f"{report_stem}.md"
    json_path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")
    md_path.write_text(_report_markdown(payload), encoding="utf-8")


def _finding(rule_id: str, axis: str, path: Path, message: str) -> Finding:
    return Finding(
        rule_id=rule_id,
        file=_relative(path),
        min_wave=DOC_MIN_WAVES[axis],
        message=message,
    )


def _findings_for_sections(axis: str, path: Path, content: str) -> list[Finding]:
    findings: list[Finding] = []
    for section in REQUIRED_SECTIONS.get(axis, ()):
        if section not in content:
            findings.append(_finding("required-section-missing", axis, path, f"{axis} 缺少必备章节: {section}"))
    return findings


def _findings_for_terms(axis: str, path: Path, content: str) -> list[Finding]:
    findings: list[Finding] = []
    for term in REQUIRED_TERMS_BY_AXIS.get(axis, ()):
        if term not in content:
            findings.append(_finding("required-term-missing", axis, path, f"{axis} 缺少关键术语: {term}"))
    return findings


def _findings_for_terminology_policy(axis: str, path: Path, content: str) -> list[Finding]:
    findings: list[Finding] = []
    for token in TERMINOLOGY_POLICY_TOKENS.get(axis, ()):
        if token not in content:
            findings.append(
                _finding("terminology-policy-missing", axis, path, f"{axis} 缺少规范语义约束: {token}")
            )
    return findings


def _findings_for_stage_chain(path: Path, content: str) -> list[Finding]:
    findings: list[Finding] = []
    positions: dict[str, int] = {}

    for stage in STAGE_CHAIN:
        token = f"| {stage} |"
        index = content.find(token)
        if index < 0:
            findings.append(_finding("stage-chain-stage-missing", "S01", path, f"S01 缺少阶段链节点: {stage}"))
            continue
        positions[stage] = index

    previous_index = -1
    for stage in STAGE_CHAIN:
        current_index = positions.get(stage)
        if current_index is None:
            continue
        if current_index < previous_index:
            findings.append(_finding("stage-chain-order-invalid", "S01", path, f"S01 阶段链顺序错误: {stage}"))
            break
        previous_index = current_index

    if "| S11 |" in content:
        findings.append(
            _finding("stage-chain-s11-merged", "S01", path, "S01 不允许使用合并阶段 S11，必须拆分为 S11A/S11B")
        )
    return findings


def _findings_for_s04(path: Path, content: str) -> list[Finding]:
    return [
        _finding("artifact-name-missing", "S04", path, f"S04 缺少核心对象名: {artifact_name}")
        for artifact_name in ARTIFACT_NAMES
        if artifact_name not in content
    ]


def _findings_for_s05(path: Path, content: str) -> list[Finding]:
    return [
        _finding("module-id-missing", "S05", path, f"S05 缺少模块标识: {module_id}")
        for module_id in MODULE_IDS
        if module_id not in content
    ]


def _findings_for_s06(path: Path, content: str) -> list[Finding]:
    return [
        _finding("root-guide-token-missing", "S06", path, f"S06 缺少关键结构令牌: {token}")
        for token in S06_REQUIRED_TOKENS
        if token not in content
    ]


def _findings_for_s08(path: Path, content: str) -> list[Finding]:
    findings: list[Finding] = []
    for chain_name, tokens in S08_CHAIN_TOKENS.items():
        for token in tokens:
            if token not in content:
                findings.append(
                    _finding("sequence-token-missing", "S08", path, f"S08 缺少 {chain_name} 关键语义: {token}")
                )
    return findings


def _findings_for_s09(path: Path, content: str, scenario_classes: tuple[str, ...]) -> list[Finding]:
    return [
        _finding("scenario-class-missing", "S09", path, f"S09 缺少验收场景类: {scenario_class}")
        for scenario_class in scenario_classes
        if SCENARIO_SECTION_TOKENS[scenario_class] not in content
    ]


def _findings_for_s10(path: Path, content: str, active_axes: tuple[str, ...]) -> list[Finding]:
    return [
        _finding("index-link-missing", axis, path, f"S10 未列出正式轴说明: {S10_AXIS_TOKENS[axis]}")
        for axis in active_axes
        if axis in S10_AXIS_TOKENS and S10_AXIS_TOKENS[axis] not in content
    ]


def _findings_for_readme(path: Path, content: str) -> list[Finding]:
    findings: list[Finding] = []
    for filename, purpose in REQUIRED_SUPPLEMENTAL_DOCS.items():
        if filename not in content:
            findings.append(Finding(
                rule_id="supplemental-doc-index-missing",
                file=_relative(path),
                min_wave=None,
                message=f"README 未注册冻结补充标准: {filename}",
            ))
        if purpose not in content:
            findings.append(Finding(
                rule_id="supplemental-doc-purpose-missing",
                file=_relative(path),
                min_wave=None,
                message=f"README 未注册冻结补充标准用途: {purpose}",
            ))
    return findings


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate dsp-e2e-spec docset coverage and acceptance baseline.")
    parser.add_argument("--report-dir", help="Optional report directory for markdown/json output.")
    parser.add_argument("--report-stem", default="dsp-e2e-spec-docset")
    parser.add_argument(
        "--scenario-class",
        action="append",
        choices=SCENARIO_CLASSES,
        help="Acceptance scenario classes that must be present in S09.",
    )
    parser.add_argument(
        "--wave",
        choices=VALIDATION_WAVES,
        help="Only validate documents whose minimum wave is at or before the requested wave.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    requested_wave_index = WAVE_INDEX.get(args.wave) if args.wave else None
    active_waves = (
        VALIDATION_WAVES
        if requested_wave_index is None
        else VALIDATION_WAVES[: requested_wave_index + 1]
    )
    active_doc_axes = _active_doc_axes(requested_wave_index)
    scenario_classes = tuple(args.scenario_class or SCENARIO_CLASSES)

    layout = load_workspace_layout(ROOT)
    formal_docset_root = layout["SILIGEN_FORMAL_FREEZE_DOCSET_DIR"].absolute
    index_file = layout["SILIGEN_FORMAL_FREEZE_INDEX_FILE"].absolute

    files_payload: dict[str, dict[str, object]] = {}
    findings: list[Finding] = []

    readme_path = formal_docset_root / "README.md"
    if not readme_path.exists():
        findings.append(Finding(
            rule_id="formal-docset-readme-missing",
            file=_relative(readme_path),
            min_wave=None,
            message="正式冻结文档集 README 不存在",
        ))
    else:
        findings.extend(_findings_for_readme(readme_path, _read_text(readme_path)))

    for filename in REQUIRED_SUPPLEMENTAL_DOCS:
        supplemental_path = formal_docset_root / filename
        if not supplemental_path.exists():
            findings.append(Finding(
                rule_id="supplemental-doc-missing",
                file=_relative(supplemental_path),
                min_wave=None,
                message=f"冻结补充标准文件不存在: {filename}",
            ))

    for axis, filename in REQUIRED_DOCS.items():
        path = formal_docset_root / filename
        is_active = axis in active_doc_axes
        exists = path.exists()
        files_payload[axis] = {
            "path": _relative(path),
            "exists": exists,
            "active": is_active,
            "min_wave": DOC_MIN_WAVES[axis],
        }
        if not is_active:
            continue
        if not exists:
            findings.append(_finding("required-doc-missing", axis, path, f"{axis} 冻结文档不存在: {filename}"))
            continue

        content = _read_text(path)
        findings.extend(_findings_for_sections(axis, path, content))
        findings.extend(_findings_for_terms(axis, path, content))
        findings.extend(_findings_for_terminology_policy(axis, path, content))
        if axis == "S01":
            findings.extend(_findings_for_stage_chain(path, content))
        if axis == "S04":
            findings.extend(_findings_for_s04(path, content))
        if axis == "S05":
            findings.extend(_findings_for_s05(path, content))
        if axis == "S06":
            findings.extend(_findings_for_s06(path, content))
        if axis == "S08":
            findings.extend(_findings_for_s08(path, content))
        if axis == "S09":
            findings.extend(_findings_for_s09(path, content, scenario_classes))
        if axis == "S10":
            findings.extend(_findings_for_s10(path, content, active_doc_axes))

    payload = {
        "workspace_root": str(ROOT),
        "formal_docset_root": str(formal_docset_root),
        "index_file": str(index_file),
        "requested_wave": args.wave or "all",
        "active_waves": list(active_waves),
        "active_doc_axes": list(active_doc_axes),
        "report_stem": args.report_stem,
        "scenario_classes": list(scenario_classes),
        "missing_doc_count": sum(
            1
            for item in files_payload.values()
            if item["active"] and not item["exists"]
        ),
        "finding_count": len(findings),
        "files": files_payload,
        "findings": [asdict(item) for item in findings],
    }

    print("[dsp-e2e-spec-docset]")
    print(f"  - formal_docset_root={formal_docset_root}")
    print(f"  - index_file={index_file}")
    print(f"  - requested_wave={args.wave or 'all'}")
    print(f"  - active_waves={','.join(active_waves)}")
    print(f"  - active_doc_axes={','.join(active_doc_axes)}")
    print(f"  - report_stem={args.report_stem}")
    print(f"  - scenario_classes={','.join(scenario_classes)}")
    print(f"  - missing_doc_count={payload['missing_doc_count']}")
    print(f"  - finding_count={payload['finding_count']}")

    if args.report_dir:
        _write_report(Path(args.report_dir), args.report_stem, payload)
        print(f"  - report_dir={Path(args.report_dir).resolve()}")

    if findings:
        print("[issues]")
        for finding in findings:
            location = f" ({finding.file})" if finding.file else ""
            min_wave = f" [min_wave={finding.min_wave}]" if finding.min_wave else ""
            print(f"  - {finding.rule_id}{location}{min_wave}: {finding.message}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
