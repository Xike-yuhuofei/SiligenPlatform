from __future__ import annotations

import argparse
import ast
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[2]
SCAN_ROOTS = (
    REPO_ROOT / "apps" / "hmi-app" / "tests",
    REPO_ROOT / "modules" / "hmi-application" / "tests",
    REPO_ROOT / "tests" / "contracts",
    REPO_ROOT / "tests" / "integration",
    REPO_ROOT / "tests" / "e2e",
    REPO_ROOT / "tests" / "performance",
)
CORE_KEYWORDS = (
    "commandprotocol",
    "gateway",
    "startup",
    "launch",
    "preview",
    "snapshot",
    "execution",
    "preflight",
    "request",
    "response",
    "schema",
    "online",
    "state",
)


@dataclass(frozen=True)
class Violation:
    rule_id: str
    file: str
    line: int
    column: int
    message: str

    def to_dict(self) -> dict[str, object]:
        return asdict(self)


def _relative(path: Path) -> str:
    return path.resolve().relative_to(REPO_ROOT).as_posix()


def _iter_python_files() -> Iterable[Path]:
    for root in SCAN_ROOTS:
        if not root.exists():
            continue
        yield from sorted(root.rglob("*.py"))


def _is_core_boundary_file(path: Path, text: str) -> bool:
    haystacks = (_relative(path).lower(), text.lower())
    return any(keyword in haystack for haystack in haystacks for keyword in CORE_KEYWORDS)


def _call_name(node: ast.Call) -> str:
    func = node.func
    if isinstance(func, ast.Name):
        return func.id
    if isinstance(func, ast.Attribute):
        owner = ""
        if isinstance(func.value, ast.Name):
            owner = func.value.id + "."
        return owner + func.attr
    return ""


def _has_strict_mock_kwargs(node: ast.Call) -> bool:
    strict_keys = {"autospec", "spec", "spec_set"}
    for keyword in node.keywords:
        if keyword.arg in strict_keys:
            return True
    return False


def _has_explicit_replacement(node: ast.Call) -> bool:
    if len(node.args) >= 3:
        return True
    explicit_keys = {"new", "new_callable"}
    return any(keyword.arg in explicit_keys for keyword in node.keywords)


def _is_bare_mock_call(name: str) -> bool:
    return name in {"Mock", "MagicMock", "mock.Mock", "mock.MagicMock", "unittest.mock.Mock", "unittest.mock.MagicMock"}


def _is_patch_call(name: str) -> bool:
    return name in {"patch", "patch.object", "mock.patch", "mock.patch.object", "unittest.mock.patch", "unittest.mock.patch.object"}


def _function_looks_like_test_double(name: str) -> bool:
    lowered = name.lower()
    return any(token in lowered for token in ("fake", "stub", "mock"))


class LooseMockVisitor(ast.NodeVisitor):
    def __init__(self, path: Path) -> None:
        self.path = path
        self.violations: list[Violation] = []

    def visit_Call(self, node: ast.Call) -> None:
        name = _call_name(node)
        location = {
            "file": _relative(self.path),
            "line": node.lineno,
            "column": node.col_offset + 1,
        }

        if _is_bare_mock_call(name) and not _has_strict_mock_kwargs(node):
            self.violations.append(
                Violation(
                    rule_id="loose-mock",
                    message="禁止在核心边界测试中使用无约束 Mock/MagicMock；请改为 spec/spec_set 或 create_autospec。",
                    **location,
                )
            )

        if _is_patch_call(name) and not _has_strict_mock_kwargs(node) and not _has_explicit_replacement(node):
            self.violations.append(
                Violation(
                    rule_id="loose-patch",
                    message="禁止在核心边界测试中使用无 autospec/spec/spec_set 的 patch/patch.object。",
                    **location,
                )
            )

        self.generic_visit(node)

    def visit_FunctionDef(self, node: ast.FunctionDef) -> None:
        if _function_looks_like_test_double(node.name) and (node.args.vararg or node.args.kwarg):
            self.violations.append(
                Violation(
                    rule_id="wide-fake-signature",
                    file=_relative(self.path),
                    line=node.lineno,
                    column=node.col_offset + 1,
                    message="核心边界 fake/stub 不允许使用 *args/**kwargs 通吃调用面。",
                )
            )
        self.generic_visit(node)

    def visit_AsyncFunctionDef(self, node: ast.AsyncFunctionDef) -> None:
        self.visit_FunctionDef(node)  # type: ignore[arg-type]


def _scan_file(path: Path) -> list[Violation]:
    text = path.read_text(encoding="utf-8")
    if not _is_core_boundary_file(path, text):
        return []

    try:
        module = ast.parse(text, filename=str(path))
    except SyntaxError as exc:
        return [
            Violation(
                rule_id="syntax-error",
                file=_relative(path),
                line=exc.lineno or 1,
                column=(exc.offset or 1),
                message=f"无法解析测试文件，无法完成 loose mock 审计: {exc.msg}",
            )
        ]

    visitor = LooseMockVisitor(path)
    visitor.visit(module)
    return visitor.violations


def _write_reports(report_dir: Path, violations: list[Violation], scanned_files: list[str]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "no-loose-mock-report.json"
    md_path = report_dir / "no-loose-mock-report.md"

    payload = {
        "status": "passed" if not violations else "failed",
        "scanned_file_count": len(scanned_files),
        "scanned_files": scanned_files,
        "violation_count": len(violations),
        "violations": [item.to_dict() for item in violations],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# No Loose Mock Report",
        "",
        f"- status: `{payload['status']}`",
        f"- scanned_file_count: `{payload['scanned_file_count']}`",
        f"- violation_count: `{payload['violation_count']}`",
        "",
    ]
    if violations:
        lines.extend(
            [
                "| rule_id | file | line | column | message |",
                "|---|---|---:|---:|---|",
            ]
        )
        for item in violations:
            message = item.message.replace("\n", " ")
            lines.append(f"| {item.rule_id} | {item.file} | {item.line} | {item.column} | {message} |")
    else:
        lines.append("- no violations")

    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Block loose Mock/MagicMock/patch usage on core boundary tests.")
    parser.add_argument("--report-dir", default=str(REPO_ROOT / "tests" / "reports" / "static"))
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    scanned_files: list[str] = []
    violations: list[Violation] = []

    for path in _iter_python_files():
        text = path.read_text(encoding="utf-8")
        if not _is_core_boundary_file(path, text):
            continue
        scanned_files.append(_relative(path))
        violations.extend(_scan_file(path))

    json_path, md_path = _write_reports(report_dir, violations, scanned_files)
    print(f"no-loose-mock report json: {json_path}")
    print(f"no-loose-mock report md:   {md_path}")
    if violations:
        for item in violations:
            print(f"{item.file}:{item.line}:{item.column}: {item.rule_id}: {item.message}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
