#!/usr/bin/env python3
"""Validate generated parallel prompt workspace integrity."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


SLOT_MARKER_RE = re.compile(r"<!--\s*TASK_SUMMARY_SLOT:\s*(T\d{3})\s+BEGIN\s*-->")


def parse_total_prompt_count(readme_text: str) -> int | None:
    m = re.search(r"total_prompt_count:\s*(\d+)", readme_text)
    return int(m.group(1)) if m else None


def collect_prompt_files(phase_dir: Path) -> list[Path]:
    return sorted(phase_dir.glob("T[0-9][0-9][0-9].md"))


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate parallel prompt workspace")
    parser.add_argument("--feature-dir", required=True, help="Feature directory")
    parser.add_argument("--prompts-dir", default="Prompts", help="Prompts dir under feature")
    args = parser.parse_args()

    feature_dir = Path(args.feature_dir).expanduser().resolve()
    prompts_dir = feature_dir / args.prompts_dir

    errors: list[str] = []

    readme_path = prompts_dir / "README.md"
    protocol_path = prompts_dir / "00-Multi-Task-Session-Summary-Protocol.md"
    if not readme_path.exists():
        errors.append(f"Missing README: {readme_path}")
    if not protocol_path.exists():
        errors.append(f"Missing protocol file: {protocol_path}")

    phase_dirs = sorted([p for p in prompts_dir.iterdir() if p.is_dir() and p.name.startswith("Phase-")]) if prompts_dir.exists() else []
    if not phase_dirs:
        errors.append(f"No phase directories found under: {prompts_dir}")

    total_prompt_files = 0
    for phase_dir in phase_dirs:
        summary_path = phase_dir / "Tasks-Execution-Summary.md"
        if not summary_path.exists():
            errors.append(f"Missing summary file: {summary_path}")
            continue

        summary_text = summary_path.read_text(encoding="utf-8")
        slots = set(SLOT_MARKER_RE.findall(summary_text))

        prompt_files = collect_prompt_files(phase_dir)
        total_prompt_files += len(prompt_files)
        if not prompt_files:
            errors.append(f"No T### prompts found in {phase_dir}")

        for prompt in prompt_files:
            task_id = prompt.stem
            prompt_text = prompt.read_text(encoding="utf-8")

            if "Allowed Coordination Writeback" not in prompt_text:
                errors.append(f"{prompt}: missing 'Allowed Coordination Writeback'")
            if "00-Multi-Task-Session-Summary-Protocol.md" not in prompt_text:
                errors.append(f"{prompt}: missing protocol reference")
            if "Tasks-Execution-Summary.md" not in prompt_text:
                errors.append(f"{prompt}: missing summary reference")
            if task_id not in slots:
                errors.append(f"{summary_path}: missing slot for {task_id}")

    if readme_path.exists():
        readme_text = readme_path.read_text(encoding="utf-8")
        declared_total = parse_total_prompt_count(readme_text)
        if declared_total is None:
            errors.append(f"{readme_path}: missing total_prompt_count")
        elif declared_total != total_prompt_files:
            errors.append(
                f"{readme_path}: total_prompt_count={declared_total}, actual={total_prompt_files}"
            )

    if errors:
        print("Validation failed:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Validation passed.")
    print(f"Feature dir: {feature_dir}")
    print(f"Prompts dir: {prompts_dir}")
    print(f"Phases: {len(phase_dirs)}")
    print(f"Prompt files: {total_prompt_files}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
