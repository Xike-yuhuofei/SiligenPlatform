from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11


@dataclass(frozen=True)
class ValidationCase:
    name: str
    layer: str
    command: list[str]
    cwd: Path
    description: str
    known_failure_patterns: tuple[str, ...] = ()
    known_failure_exit_codes: tuple[int, ...] = ()
    skipped_exit_codes: tuple[int, ...] = ()
    allow_missing: bool = False


@dataclass
class ValidationResult:
    name: str
    layer: str
    description: str
    status: str
    return_code: int | None
    duration_seconds: float
    command: list[str]
    cwd: str
    stdout: str = ""
    stderr: str = ""
    note: str = ""


@dataclass
class ValidationReport:
    generated_at: str
    workspace_root: str
    metadata: dict[str, object] = field(default_factory=dict)
    results: list[ValidationResult] = field(default_factory=list)

    def counts(self) -> dict[str, int]:
        counts = {"passed": 0, "failed": 0, "known_failure": 0, "skipped": 0}
        for result in self.results:
            counts[result.status] = counts.get(result.status, 0) + 1
        return counts

    def exit_code(self, *, fail_on_known_failure: bool = False) -> int:
        counts = self.counts()
        if counts.get("failed", 0):
            return 1
        if fail_on_known_failure and counts.get("known_failure", 0):
            return 2
        return 0


def _truncate(text: str, limit: int = 4000) -> str:
    stripped = text.strip()
    if len(stripped) <= limit:
        return stripped
    return stripped[:limit] + "\n...[truncated]..."


def _classify_failure(case: ValidationCase, return_code: int, stdout: str, stderr: str) -> tuple[str, str]:
    combined = "\n".join(item for item in (stdout, stderr) if item)
    if return_code in case.skipped_exit_codes:
        return "skipped", "command reported skipped"
    if return_code in case.known_failure_exit_codes:
        return "known_failure", f"matched known failure exit code {return_code}"
    for pattern in case.known_failure_patterns:
        if re.search(pattern, combined, re.S):
            return "known_failure", f"matched known failure pattern: {pattern}"
    return "failed", "command failed"


def _classify_success(case: ValidationCase, stdout: str, stderr: str) -> tuple[str, str]:
    combined = "\n".join(item for item in (stdout, stderr) if item)
    for pattern in case.known_failure_patterns:
        if re.search(pattern, combined, re.S):
            return "known_failure", f"matched known failure pattern: {pattern}"
    return "passed", ""


def run_case(case: ValidationCase) -> ValidationResult:
    start = time.perf_counter()
    executable = case.command[0]
    if case.allow_missing and not Path(executable).exists():
        return ValidationResult(
            name=case.name,
            layer=case.layer,
            description=case.description,
            status="known_failure",
            return_code=None,
            duration_seconds=0.0,
            command=case.command,
            cwd=str(case.cwd),
            note=f"missing executable: {executable}",
        )

    completed = subprocess.run(
        case.command,
        cwd=str(case.cwd),
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        env=os.environ.copy(),
    )
    duration = time.perf_counter() - start
    stdout = _truncate(completed.stdout or "")
    stderr = _truncate(completed.stderr or "")
    if completed.returncode == 0:
        status, note = _classify_success(case, stdout, stderr)
    else:
        status, note = _classify_failure(case, completed.returncode, stdout, stderr)

    return ValidationResult(
        name=case.name,
        layer=case.layer,
        description=case.description,
        status=status,
        return_code=completed.returncode,
        duration_seconds=duration,
        command=case.command,
        cwd=str(case.cwd),
        stdout=stdout,
        stderr=stderr,
        note=note,
    )


def report_to_json(report: ValidationReport, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "generated_at": report.generated_at,
        "workspace_root": report.workspace_root,
        "metadata": report.metadata,
        "counts": report.counts(),
        "results": [asdict(item) for item in report.results],
    }
    path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")


def report_to_markdown(report: ValidationReport, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    counts = report.counts()
    lines = [
        "# Workspace Validation Report",
        "",
        f"- generated_at: `{report.generated_at}`",
        f"- workspace_root: `{report.workspace_root}`",
        f"- passed: `{counts.get('passed', 0)}`",
        f"- failed: `{counts.get('failed', 0)}`",
        f"- known_failure: `{counts.get('known_failure', 0)}`",
        f"- skipped: `{counts.get('skipped', 0)}`",
        "",
    ]

    if report.metadata:
        lines.append("## metadata")
        lines.append("")
        for key in sorted(report.metadata):
            lines.append(f"- {key}: `{report.metadata[key]}`")
        lines.append("")

    preferred_layers = [
        "apps",
        "packages",
        "integration",
        "protocol-compatibility",
        "simulation",
        "unit",
        "sim_hil",
    ]
    seen_layers = []
    for layer in preferred_layers + [item.layer for item in report.results]:
        if layer not in seen_layers:
            seen_layers.append(layer)

    for layer in seen_layers:
        lines.append(f"## {layer}")
        lines.append("")
        layer_results = [item for item in report.results if item.layer == layer]
        if not layer_results:
            lines.append("- none")
            lines.append("")
            continue
        for result in layer_results:
            lines.append(f"- `{result.status}` `{result.name}`: {result.description}")
            lines.append(f"  command: `{subprocess.list2cmdline(result.command)}`")
            lines.append(f"  duration_seconds: `{result.duration_seconds:.3f}`")
            if result.note:
                lines.append(f"  note: {result.note}")
            if result.stdout:
                lines.append("  stdout:")
                lines.append("  ```text")
                lines.extend(f"  {line}" for line in result.stdout.splitlines())
                lines.append("  ```")
            if result.stderr:
                lines.append("  stderr:")
                lines.append("  ```text")
                lines.extend(f"  {line}" for line in result.stderr.splitlines())
                lines.append("  ```")
        lines.append("")

    path.write_text("\n".join(lines), encoding="utf-8")


def python_command(script_path: Path) -> list[str]:
    return [sys.executable, str(script_path)]
