#!/usr/bin/env python3
"""Refresh per-phase summary slots from generated T### prompt files."""

from __future__ import annotations

import argparse
import datetime as dt
import re
from pathlib import Path


PHASE_TITLE_RE = re.compile(r"^#\s+Phase\s+(\d+)\s*:\s*(.+?)\s+-\s+Tasks Execution Summary", re.M)
SLOT_RE = re.compile(
    r"<!--\s*TASK_SUMMARY_SLOT:\s*(T\d{3})\s+BEGIN\s*-->(.*?)<!--\s*TASK_SUMMARY_SLOT:\s*\1\s+END\s*-->",
    re.S,
)


def build_summary(number: int, title: str, phase_dir_name: str, task_ids: list[str], old_slots: dict[str, str]) -> str:
    now = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    slot_chunks = []
    for task_id in task_ids:
        slot_content = old_slots.get(task_id, "_PENDING_SESSION_SUMMARY_").strip() or "_PENDING_SESSION_SUMMARY_"
        slot_chunks.append(
            "\n".join(
                [
                    f"### {task_id}",
                    f"<!-- TASK_SUMMARY_SLOT: {task_id} BEGIN -->",
                    slot_content,
                    f"<!-- TASK_SUMMARY_SLOT: {task_id} END -->",
                    "",
                ]
            )
        )

    slots_text = "\n".join(slot_chunks).rstrip()
    return f"""# Phase {number:02d}: {title} - Tasks Execution Summary

- generated_at: {now}
- phase_dir: {phase_dir_name}

## Main Thread Control

- phase_status: collecting_task_summaries
- phase_end_check_requested_by_user: false
- phase_end_check_status: not_started
- notes: pending

## Phase End Check

- status: not_started
- gate_result: pending
- blocker_count: 0
- decision: waiting_user_trigger

## Task Slots

{slots_text}
"""


def parse_phase_header(text: str, phase_dir_name: str) -> tuple[int, str]:
    m = PHASE_TITLE_RE.search(text)
    if m:
        return int(m.group(1)), m.group(2).strip()
    fallback = re.match(r"Phase-(\d+)-?(.*)", phase_dir_name)
    if fallback:
        number = int(fallback.group(1))
        raw = fallback.group(2).replace("-", " ").strip()
        title = raw if raw else f"Phase {number:02d}"
        return number, title
    return 0, phase_dir_name


def main() -> int:
    parser = argparse.ArgumentParser(description="Refresh phase summary slots")
    parser.add_argument("--feature-dir", required=True, help="Feature directory")
    parser.add_argument("--prompts-dir", default="Prompts", help="Prompts dir under feature")
    args = parser.parse_args()

    feature_dir = Path(args.feature_dir).expanduser().resolve()
    prompts_dir = feature_dir / args.prompts_dir
    if not prompts_dir.exists():
        raise FileNotFoundError(f"Prompts dir not found: {prompts_dir}")

    phase_dirs = sorted([p for p in prompts_dir.iterdir() if p.is_dir() and p.name.startswith("Phase-")])
    if not phase_dirs:
        print("No phase directories found. Nothing to refresh.")
        return 0

    refreshed = 0
    for phase_dir in phase_dirs:
        task_ids = sorted([path.stem for path in phase_dir.glob("T[0-9][0-9][0-9].md")])
        if not task_ids:
            continue

        summary_path = phase_dir / "Tasks-Execution-Summary.md"
        old_text = summary_path.read_text(encoding="utf-8") if summary_path.exists() else ""
        old_slots = {m.group(1): m.group(2).strip() for m in SLOT_RE.finditer(old_text)}
        number, title = parse_phase_header(old_text, phase_dir.name)

        summary_path.write_text(
            build_summary(number=number, title=title, phase_dir_name=phase_dir.name, task_ids=task_ids, old_slots=old_slots),
            encoding="utf-8",
        )
        refreshed += 1

    print(f"Refreshed summaries: {refreshed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
