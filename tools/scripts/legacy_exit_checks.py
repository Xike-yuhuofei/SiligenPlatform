from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from fnmatch import fnmatch
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
EXCLUDED_DIR_NAMES = {
    ".git",
    ".venv",
    "__pycache__",
    "build",
    "node_modules",
    "reports",
    "tmp",
}
CODE_EXTENSIONS = {".cmake", ".json", ".ps1", ".py", ".toml", ".yaml", ".yml"}
ACTIVE_DOC_ROOTS = (
    ROOT / "docs" / "onboarding",
    ROOT / "docs" / "runtime",
    ROOT / "docs" / "troubleshooting",
    ROOT / "docs" / "validation",
)
ROOT_CODE_FILES = (
    ROOT / "build.ps1",
    ROOT / "ci.ps1",
    ROOT / "legacy-exit-check.ps1",
    ROOT / "release-check.ps1",
    ROOT / "test.ps1",
)
ROOT_DOC_FILES = (
    ROOT / "README.md",
    ROOT / "WORKSPACE.md",
)


@dataclass(frozen=True)
class Finding:
    rule_id: str
    rule_title: str
    message: str
    file: str | None = None
    line: int | None = None
    pattern: str | None = None
    snippet: str | None = None


@dataclass(frozen=True)
class SearchRule:
    id: str
    title: str
    description: str
    scope: str
    patterns: tuple[str, ...]
    allowlist: tuple[str, ...] = ()


@dataclass(frozen=True)
class MissingPathRule:
    id: str
    title: str
    description: str
    paths: tuple[str, ...]


def relative_path(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def is_excluded(path: Path) -> bool:
    rel_parts = path.relative_to(ROOT).parts
    return any(part in EXCLUDED_DIR_NAMES for part in rel_parts[:-1])


def iter_code_files() -> list[Path]:
    files: dict[str, Path] = {}
    for root_file in ROOT_CODE_FILES:
        if root_file.exists():
            files[relative_path(root_file)] = root_file

    for base in (ROOT / "apps", ROOT / "packages", ROOT / "integration", ROOT / "tools", ROOT / ".github"):
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or is_excluded(path):
                continue
            if path.name == "CMakeLists.txt" or path.suffix.lower() in CODE_EXTENSIONS:
                rel_path = relative_path(path)
                if rel_path == "tools/scripts/legacy_exit_checks.py":
                    continue
                files[rel_path] = path
    return [files[key] for key in sorted(files)]


def iter_python_files() -> list[Path]:
    return [path for path in iter_code_files() if path.suffix.lower() == ".py"]


def iter_powershell_files() -> list[Path]:
    return [path for path in iter_code_files() if path.suffix.lower() == ".ps1"]


def iter_cmake_files() -> list[Path]:
    return [
        path
        for path in iter_code_files()
        if path.name == "CMakeLists.txt" or path.suffix.lower() == ".cmake"
    ]


def iter_active_docs() -> list[Path]:
    files: dict[str, Path] = {}
    for root_file in ROOT_DOC_FILES:
        if root_file.exists():
            files[relative_path(root_file)] = root_file

    for base in ACTIVE_DOC_ROOTS:
        if not base.exists():
            continue
        for path in base.rglob("*.md"):
            if not path.is_file() or is_excluded(path):
                continue
            files[relative_path(path)] = path
    return [files[key] for key in sorted(files)]


def get_scope_files(scope: str) -> list[Path]:
    if scope == "active_docs":
        return iter_active_docs()
    if scope == "cmake":
        return iter_cmake_files()
    if scope == "powershell":
        return iter_powershell_files()
    if scope == "python":
        return iter_python_files()
    if scope == "code":
        return iter_code_files()
    raise ValueError(f"Unsupported scope: {scope}")


def matches_allowlist(rel_path: str, allowlist: tuple[str, ...]) -> bool:
    return any(fnmatch(rel_path, pattern) for pattern in allowlist)


def compile_patterns(patterns: tuple[str, ...]) -> list[re.Pattern[str]]:
    return [re.compile(pattern) for pattern in patterns]


def scan_rule(rule: SearchRule) -> list[Finding]:
    compiled = compile_patterns(rule.patterns)
    findings: list[Finding] = []
    for path in get_scope_files(rule.scope):
        rel_path = relative_path(path)
        if matches_allowlist(rel_path, rule.allowlist):
            continue
        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            content = path.read_text(encoding="utf-8", errors="ignore")
        for line_no, line in enumerate(content.splitlines(), start=1):
            for pattern in compiled:
                if pattern.search(line):
                    findings.append(
                        Finding(
                            rule_id=rule.id,
                            rule_title=rule.title,
                            file=rel_path,
                            line=line_no,
                            pattern=pattern.pattern,
                            snippet=line.strip(),
                            message=f"{rule.title} 命中未白名单 legacy 引用。",
                        )
                    )
                    break
    return findings


def check_missing_paths(rule: MissingPathRule) -> list[Finding]:
    findings: list[Finding] = []
    for rel_path in rule.paths:
        target = ROOT / rel_path
        if target.exists():
            findings.append(
                Finding(
                    rule_id=rule.id,
                    rule_title=rule.title,
                    file=rel_path,
                    message=f"{rule.title} 失败：目录仍存在。",
                )
            )
    return findings


def render_markdown(
    *,
    generated_at: str,
    profile: str,
    rules: list[SearchRule | MissingPathRule],
    grouped_findings: dict[str, list[Finding]],
) -> str:
    passed = sum(1 for rule in rules if not grouped_findings.get(rule.id))
    failed = len(rules) - passed
    lines = [
        "# Legacy Exit Checks",
        "",
        f"- generated_at: `{generated_at}`",
        f"- profile: `{profile}`",
        f"- workspace_root: `{ROOT}`",
        f"- passed_rules: `{passed}`",
        f"- failed_rules: `{failed}`",
        "",
        "## Rule Summary",
        "",
        "| Rule | Result |",
        "|---|---|",
    ]
    for rule in rules:
        result = "PASS" if not grouped_findings.get(rule.id) else "FAIL"
        lines.append(f"| `{rule.id}` | `{result}` |")

    for rule in rules:
        findings = grouped_findings.get(rule.id, [])
        lines.extend(
            [
                "",
                f"## {rule.id}",
                "",
                f"- title: `{rule.title}`",
                f"- description: {rule.description}",
            ]
        )
        if isinstance(rule, SearchRule) and rule.allowlist:
            lines.append(f"- allowlist: `{', '.join(rule.allowlist)}`")
        if not findings:
            lines.append("- result: pass")
            continue
        lines.append(f"- result: fail ({len(findings)} findings)")
        for finding in findings[:20]:
            location = finding.file or "<repo>"
            if finding.line is not None:
                location = f"{location}:{finding.line}"
            lines.append(f"- {location} | {finding.message}")
            if finding.snippet:
                lines.append(f"  snippet: `{finding.snippet}`")
        if len(findings) > 20:
            lines.append(f"- ... omitted {len(findings) - 20} additional findings")
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate legacy deletion and freeze gates.")
    parser.add_argument("--profile", choices=("local", "ci"), default="local")
    parser.add_argument("--report-dir", default=str(ROOT / "integration" / "reports" / "legacy-exit"))
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    report_dir.mkdir(parents=True, exist_ok=True)
    generated_at = datetime.now(timezone.utc).isoformat()

    rules: list[SearchRule | MissingPathRule] = [
        MissingPathRule(
            id="removed-directories-absent",
            title="已删除目录必须保持不存在",
            description="已声明移除的 legacy 目录重新出现时立即失败。",
            paths=(
                "control-core",
                "hmi-client",
                "apps/dxf-editor-app",
                "dxf-editor",
                "packages/editor-contracts",
                "simulation-engine",
                "control-core/modules/shared-kernel",
                "control-core/src/shared",
                "control-core/src/domain",
                "control-core/src/application",
                "control-core/src/adapters",
                "control-core/src/infrastructure",
                "control-core/modules/process-core",
                "control-core/modules/motion-core",
                "control-core/modules/control-gateway",
                "control-core/modules/device-hal",
                "control-core/modules/device-hal/src/adapters/diagnostics/logging",
                "control-core/modules/device-hal/src/adapters/recipes",
            ),
        ),
        SearchRule(
            id="removed-paths-blocked-in-code",
            title="构建/测试/CI 不得重新引用已删除目录",
            description="阻止 build/test/CI、自动化和源码把已删除目录重新拉回执行链路。",
            scope="code",
            patterns=(
                r"hmi-client[/\\]",
                r"apps[/\\]dxf-editor-app",
                r"packages[/\\]editor-contracts",
                r"(?<![A-Za-z0-9_-])dxf-editor(?=$|[/\\])",
                r"(?<!packages/)(?<!packages\\)simulation-engine(?=[/\\])",
            ),
        ),
        SearchRule(
            id="control-core-removed-subtrees-blocked-in-code",
            title="已删除 control-core 子树不得重新回流到代码/脚本/CMake",
            description="阻止已删除的 process-runtime-core legacy 镜像与 shared-kernel 目录重新进入构建、脚本或自动化链路。",
            scope="code",
            patterns=(
                r"control-core[/\\]modules[/\\]shared-kernel(?=$|[/\\])",
                r"control-core[/\\]src[/\\]shared(?=$|[/\\])",
                r"control-core[/\\]src[/\\]domain(?=$|[/\\])",
                r"control-core[/\\]src[/\\]application(?=$|[/\\])",
                r"control-core[/\\]src[/\\]adapters(?=$|[/\\])",
                r"control-core[/\\]src[/\\]infrastructure(?=$|[/\\])",
                r"control-core[/\\]modules[/\\]process-core(?=$|[/\\])",
                r"control-core[/\\]modules[/\\]motion-core(?=$|[/\\])",
                r"control-core[/\\]modules[/\\]control-gateway(?=$|[/\\])",
                r"control-core[/\\]modules[/\\]device-hal(?=$|[/\\])",
                r"control-core[/\\]modules[/\\]device-hal[/\\]src[/\\]adapters[/\\]diagnostics[/\\]logging(?=$|[/\\])",
                r"control-core[/\\]modules[/\\]device-hal[/\\]src[/\\]adapters[/\\]recipes(?=$|[/\\])",
            ),
        ),
        SearchRule(
            id="control-core-root-blocked-in-live-code",
            title="live 代码/脚本/CMake 不得再直接引用 control-core 根目录",
            description="最终删除后，除删除门禁自身外，不允许任何 live 代码、脚本、测试或 CMake 再直接指向 control-core。",
            scope="code",
            patterns=(
                r"control-core(?=$|[/\\])",
            ),
        ),
        SearchRule(
            id="dxf-pipeline-shell-blocked-in-code",
            title="代码/自动化不得新增 dxf-pipeline legacy 依赖",
            description="阻止新的 dxf-pipeline src/proto/import 回流。",
            scope="code",
            patterns=(
                r"dxf-pipeline[/\\]src",
                r"dxf-pipeline[/\\]proto",
                r"^\s*from\s+dxf_pipeline(\.|$)",
                r"^\s*import\s+dxf_pipeline(\.|$)",
                r"[\"']-m[\"']\s*,\s*[\"']dxf_pipeline\.",
                r"python\s+-m\s+dxf_pipeline\.",
            ),
        ),
        SearchRule(
            id="control-core-shell-direct-paths-allowlist",
            title="直接 control-core 兼容壳路径必须收敛在白名单",
            description="最终删除后，脚本、测试和自动化不得再指向任何 control-core 兼容壳路径。",
            scope="code",
            patterns=(
                r"control-core[/\\]build[/\\]bin",
                r"control-core[/\\]apps[/\\]control-runtime",
                r"control-core[/\\]apps[/\\]control-tcp-server",
                r"control-core[/\\]src[/\\]adapters[/\\]tcp",
                r"control-core[/\\]modules[/\\]control-gateway",
            ),
        ),
        SearchRule(
            id="control-core-config-fallback-blocked-in-code",
            title="代码中不得重新回退到 control-core 配置路径",
            description="阻止 runtime、脚本和自动化继续把 control-core 配置文件当默认 fallback。",
            scope="code",
            patterns=(
                r"control-core[/\\]config[/\\]machine_config\.ini",
                r"control-core[/\\]src[/\\]infrastructure[/\\]resources[/\\]config[/\\]files[/\\]machine_config\.ini",
            ),
        ),
        SearchRule(
            id="control-core-recipe-fallback-blocked-in-code",
            title="代码中不得重新回退到 control-core recipes/schema 路径",
            description="阻止 runtime、脚本和自动化继续把 control-core data/recipes 当默认来源。",
            scope="code",
            patterns=(
                r"control-core[/\\]data[/\\]recipes",
                r"control-core[/\\]src[/\\]infrastructure[/\\]resources[/\\]config[/\\]files[/\\]recipes[/\\]schemas",
            ),
        ),
        SearchRule(
            id="control-core-powershell-root-allowlist",
            title="PowerShell 中 control-core 根目录拼接必须收敛在白名单",
            description="最终删除后，PowerShell 不得再拼接 control-core 根目录。",
            scope="powershell",
            patterns=(
                r"Join-Path\s+\$\w+\s+[\"']control-core[\"']",
            ),
        ),
        SearchRule(
            id="control-cli-legacy-fallback-blocked",
            title="control-cli run.ps1 不得重新引入 legacy fallback",
            description="canonical CLI 已完成 cutover，run.ps1 不得再显式依赖 control-core build/bin 或外部旧仓 CLI 源。",
            scope="powershell",
            patterns=(
                r"UseLegacyFallback",
                r"control-core[/\\]build[/\\]bin.*siligen_cli\.exe",
                r"Backend_CPP[/\\]src[/\\]adapters[/\\]cli",
            ),
            allowlist=(
                "apps/control-runtime/run.ps1",
                "apps/control-tcp-server/run.ps1",
            ),
        ),
        SearchRule(
            id="control-core-python-root-allowlist",
            title="Python 中 control-core 根目录路径必须收敛在白名单",
            description="最终删除后，Python 不得再直接依赖 control-core 目录。",
            scope="python",
            patterns=(
                r"[\"']control-core[\"']",
            ),
        ),
        SearchRule(
            id="control-core-hmi-launcher-env-blocked",
            title="HMI 代码不得重新接受 control-core legacy 启动环境变量",
            description="阻止 HMI 或自动化重新读取 SILIGEN_CONTROL_CORE_ROOT / SILIGEN_TCP_SERVER_EXE。",
            scope="code",
            patterns=(
                r"SILIGEN_CONTROL_CORE_ROOT",
                r"SILIGEN_TCP_SERVER_EXE",
            ),
        ),
        SearchRule(
            id="control-core-dxf-default-path-blocked",
            title="HMI 代码不得重新把 control-core/uploads/dxf 当默认候选路径",
            description="默认 DXF 路径只能来自显式环境变量或 workspace canonical 路径。",
            scope="code",
            patterns=(
                r"control-core[/\\]uploads[/\\]dxf",
            ),
        ),
        SearchRule(
            id="control-core-cmake-allowlist",
            title="CMake legacy include/link 只能出现在已登记阻塞项",
            description="最终删除后，packages/apps/CMake 不得再新增任何 control-core 硬编码 include 或 source 依赖。",
            scope="cmake",
            patterns=(
                r"control-core[/\\]src[/\\]domain",
                r"control-core[/\\]src[/\\]application",
                r"control-core[/\\]modules[/\\]process-core",
                r"control-core[/\\]modules[/\\]motion-core",
                r"control-core[/\\]modules[/\\]device-hal",
                r"control-core[/\\]modules[/\\]shared-kernel",
                r"control-core[/\\]third_party",
                r"\.\./\.\./control-core",
                r"\.\.\\\.\.\\control-core",
            ),
        ),
        SearchRule(
            id="device-hal-mirrored-source-include-blocked",
            title="已迁出的 device-hal 镜像源码不得再从 legacy 目录 include",
            description="health/motion/dispensing/multicard 镜像源码已切到 packages/device-adapters，禁止 runtime-host 等消费者继续从 modules/device-hal/src 获取这些头。",
            scope="code",
            patterns=(
                r'#include\s+"modules/device-hal/src/adapters/diagnostics/health/testing/',
                r'#include\s+"modules/device-hal/src/adapters/dispensing/dispenser/',
                r'#include\s+"modules/device-hal/src/adapters/motion/controller/',
                r'#include\s+"modules/device-hal/src/drivers/multicard/(IMultiCardWrapper|MockMultiCardWrapper|RealMultiCardWrapper)\.h"',
            ),
        ),
        SearchRule(
            id="control-core-gateway-shell-registration-blocked",
            title="control-core 默认库图不得继续注册 gateway/tcp alias 壳",
            description="legacy gateway/tcp alias 必须由 canonical transport-gateway package 暴露，而不是由 control-core 子目录注册。",
            scope="cmake",
            patterns=(
                r"add_subdirectory\(adapters/tcp\)",
                r"add_subdirectory\(control-gateway\)",
            ),
        ),
        SearchRule(
            id="canonical-package-source-root-fallback-blocked",
            title="canonical package/app 不得继续隐式回落到 CMAKE_SOURCE_DIR",
            description="阻止 apps/runtime-host/process-runtime-core/transport-gateway 继续把 CMAKE_SOURCE_DIR 当 control-core 兼容 source root。",
            scope="cmake",
            patterns=(
                r"\$\{CMAKE_SOURCE_DIR\}/src",
                r"\$\{CMAKE_SOURCE_DIR\}/third_party",
            ),
        ),
        SearchRule(
            id="active-docs-legacy-entry-allowlist",
            title="活动文档中的 legacy 默认入口必须显式白名单",
            description="root/onboarding/runtime/troubleshooting/validation 文档中出现 legacy 路径时，必须是已登记的兼容说明或历史说明。",
            scope="active_docs",
            patterns=(
                r"hmi-client[/\\]src",
                r"hmi-client[/\\]scripts",
                r"(?<![A-Za-z0-9_-])hmi-client(?=$|[/\\])",
                r"dxf-pipeline[/\\]src",
                r"dxf-pipeline[/\\]proto",
                r"(?<![A-Za-z0-9_-])dxf-pipeline(?=$|[/\\])",
                r"control-core[/\\]build[/\\]bin",
                r"control-core[/\\]apps[/\\]control-runtime",
                r"control-core[/\\]apps[/\\]control-tcp-server",
                r"control-core[/\\]src[/\\]adapters[/\\]tcp",
                r"control-core[/\\]modules[/\\]control-gateway",
                r"apps[/\\]dxf-editor-app",
                r"packages[/\\]editor-contracts",
                r"(?<![A-Za-z0-9_-])dxf-editor(?=$|[/\\])",
                r"(?<!packages/)(?<!packages\\)simulation-engine(?=[/\\])",
            ),
            allowlist=(
                "README.md",
                "WORKSPACE.md",
                "docs/onboarding/developer-workflow.md",
                "docs/onboarding/workspace-onboarding.md",
                "docs/runtime/deployment.md",
                "docs/runtime/external-dxf-editing.md",
                "docs/runtime/field-acceptance.md",
                "docs/runtime/long-run-stability.md",
                "docs/runtime/release-process.md",
                "docs/runtime/rollback.md",
                "docs/troubleshooting/post-refactor-runbook.md",
            ),
        ),
    ]

    grouped_findings: dict[str, list[Finding]] = {}
    for rule in rules:
        if isinstance(rule, MissingPathRule):
            findings = check_missing_paths(rule)
        else:
            findings = scan_rule(rule)
        grouped_findings[rule.id] = findings

    all_findings = [finding for findings in grouped_findings.values() for finding in findings]
    markdown_path = report_dir / "legacy-exit-checks.md"
    json_path = report_dir / "legacy-exit-checks.json"
    payload = {
        "generated_at": generated_at,
        "profile": args.profile,
        "workspace_root": str(ROOT),
        "rule_count": len(rules),
        "failed_rule_count": sum(1 for rule in rules if grouped_findings.get(rule.id)),
        "finding_count": len(all_findings),
        "rules": [
            {
                "id": rule.id,
                "title": rule.title,
                "description": rule.description,
                "allowlist": list(rule.allowlist) if isinstance(rule, SearchRule) else [],
                "findings": [finding.__dict__ for finding in grouped_findings.get(rule.id, [])],
            }
            for rule in rules
        ],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    markdown_path.write_text(
        render_markdown(
            generated_at=generated_at,
            profile=args.profile,
            rules=rules,
            grouped_findings=grouped_findings,
        ),
        encoding="utf-8",
    )

    failed_rule_count = payload["failed_rule_count"]
    print(
        "legacy exit checks complete: "
        f"profile={args.profile} "
        f"rules={len(rules)} "
        f"failed_rules={failed_rule_count} "
        f"findings={len(all_findings)}"
    )
    print(f"json report: {json_path}")
    print(f"markdown report: {markdown_path}")
    return 1 if failed_rule_count else 0


if __name__ == "__main__":
    raise SystemExit(main())
