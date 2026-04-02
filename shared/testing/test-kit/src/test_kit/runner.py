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
    case_id: str = ""
    owner_scope: str = "shared/testing"
    primary_layer: str = ""
    suite_ref: str = ""
    risk_tags: tuple[str, ...] = ()
    required_assets: tuple[str, ...] = ()
    required_fixtures: tuple[str, ...] = ()
    evidence_profile: str = "workspace-validation"
    stability_state: str = "stable"
    size_label: str = ""
    label_refs: tuple[str, ...] = ()
    known_failure_patterns: tuple[str, ...] = ()
    known_failure_exit_codes: tuple[int, ...] = ()
    skipped_exit_codes: tuple[int, ...] = ()
    allow_missing: bool = False
    timeout_seconds: int | None = None
    retry_budget: int = 0


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
    case_id: str = ""
    owner_scope: str = "shared/testing"
    primary_layer: str = ""
    suite_ref: str = ""
    risk_tags: tuple[str, ...] = ()
    required_assets: tuple[str, ...] = ()
    required_fixtures: tuple[str, ...] = ()
    evidence_profile: str = "workspace-validation"
    stability_state: str = "stable"
    size_label: str = ""
    label_refs: tuple[str, ...] = ()


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
            case_id=case.case_id,
            owner_scope=case.owner_scope,
            primary_layer=case.primary_layer,
            suite_ref=case.suite_ref,
            risk_tags=case.risk_tags,
            required_assets=case.required_assets,
            required_fixtures=case.required_fixtures,
            evidence_profile=case.evidence_profile,
            stability_state=case.stability_state,
            size_label=case.size_label,
            label_refs=case.label_refs,
        )

    total_duration = 0.0
    attempts = max(1, case.retry_budget + 1)
    attempt_notes: list[str] = []

    for attempt_index in range(1, attempts + 1):
        start = time.perf_counter()
        try:
            completed = subprocess.run(
                case.command,
                cwd=str(case.cwd),
                check=False,
                capture_output=True,
                text=True,
                encoding="utf-8",
                env=os.environ.copy(),
                timeout=case.timeout_seconds,
            )
            total_duration += time.perf_counter() - start
            stdout = _truncate(completed.stdout or "")
            stderr = _truncate(completed.stderr or "")
            if completed.returncode == 0:
                status, note = _classify_success(case, stdout, stderr)
            else:
                status, note = _classify_failure(case, completed.returncode, stdout, stderr)
            if attempt_index < attempts and status == "failed":
                attempt_notes.append(f"attempt {attempt_index}/{attempts} failed; retrying")
                continue
            final_note = "; ".join(item for item in [*attempt_notes, note] if item)
            return ValidationResult(
                name=case.name,
                layer=case.layer,
                description=case.description,
                status=status,
                return_code=completed.returncode,
                duration_seconds=total_duration,
                command=case.command,
                cwd=str(case.cwd),
                stdout=stdout,
                stderr=stderr,
                note=final_note,
                case_id=case.case_id,
                owner_scope=case.owner_scope,
                primary_layer=case.primary_layer,
                suite_ref=case.suite_ref,
                risk_tags=case.risk_tags,
                required_assets=case.required_assets,
                required_fixtures=case.required_fixtures,
                evidence_profile=case.evidence_profile,
                stability_state=case.stability_state,
                size_label=case.size_label,
                label_refs=case.label_refs,
            )
        except subprocess.TimeoutExpired as exc:
            total_duration += time.perf_counter() - start
            stdout = _truncate((exc.stdout or "") if isinstance(exc.stdout, str) else "")
            stderr = _truncate((exc.stderr or "") if isinstance(exc.stderr, str) else "")
            timeout_note = f"command timed out after {case.timeout_seconds}s"
            if attempt_index < attempts:
                attempt_notes.append(f"attempt {attempt_index}/{attempts} timed out; retrying")
                continue
            final_note = "; ".join(item for item in [*attempt_notes, timeout_note] if item)
            return ValidationResult(
                name=case.name,
                layer=case.layer,
                description=case.description,
                status="failed",
                return_code=124,
                duration_seconds=total_duration,
                command=case.command,
                cwd=str(case.cwd),
                stdout=stdout,
                stderr=stderr,
                note=final_note,
                case_id=case.case_id,
                owner_scope=case.owner_scope,
                primary_layer=case.primary_layer,
                suite_ref=case.suite_ref,
                risk_tags=case.risk_tags,
                required_assets=case.required_assets,
                required_fixtures=case.required_fixtures,
                evidence_profile=case.evidence_profile,
                stability_state=case.stability_state,
                size_label=case.size_label,
                label_refs=case.label_refs,
            )

    raise RuntimeError(f"unreachable validation result for case {case.name}")


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
        "freeze",
        "apps",
        "contracts",
        "e2e",
        "protocol-compatibility",
        "performance",
        "unit",
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
            case_suffix = f" [{result.case_id}]" if result.case_id else ""
            descriptor = result.description
            if result.primary_layer:
                descriptor = f"{descriptor} | primary_layer={result.primary_layer}"
            if result.owner_scope:
                descriptor = f"{descriptor} | owner_scope={result.owner_scope}"
            if result.suite_ref:
                descriptor = f"{descriptor} | suite={result.suite_ref}"
            if result.size_label:
                descriptor = f"{descriptor} | size={result.size_label}"
            if result.label_refs:
                descriptor = f"{descriptor} | labels={','.join(result.label_refs)}"
            lines.append(f"- `{result.status}` `{result.name}`{case_suffix}: {descriptor}")
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
