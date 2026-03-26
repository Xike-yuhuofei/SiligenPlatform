#!/usr/bin/env python3
"""Generate parallel prompt workspace from tasks.md unfinished [P] tasks."""

from __future__ import annotations

import argparse
import datetime as dt
import re
import shutil
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


PHASE_RE = re.compile(r"^##\s+Phase\s+(\d+)\s*:\s*(.+)$")
TASK_RE = re.compile(
    r"^- \[(?P<done>[ xX])\]\s+(?P<id>T\d{3})\s*(?P<p>\[P\])?\s*(?P<story>\[US\d+\])?\s*(?P<desc>.+)$"
)


@dataclass
class Task:
    task_id: str
    description: str
    story: str
    scope_files: list[str]


@dataclass
class Phase:
    number: int
    title: str
    slug: str
    tasks: list[Task] = field(default_factory=list)

    @property
    def dir_name(self) -> str:
        return f"Phase-{self.number:02d}-{self.slug}" if self.slug else f"Phase-{self.number:02d}"


def slugify(text: str) -> str:
    text = re.sub(r"\(.*?\)", "", text)
    text = re.sub(r"[^A-Za-z0-9]+", "-", text.lower()).strip("-")
    if not text:
        return "phase"
    return text[:48]


def dedupe_keep_order(items: Iterable[str]) -> list[str]:
    seen: set[str] = set()
    out: list[str] = []
    for item in items:
        if item not in seen:
            out.append(item)
            seen.add(item)
    return out


def extract_scope_files(desc: str) -> list[str]:
    from_backticks: list[str] = []
    for token in re.findall(r"`([^`]+)`", desc):
        token = token.strip()
        if re.match(r"^[A-Za-z]:\\", token):
            from_backticks.append(token)
    if from_backticks:
        return dedupe_keep_order(from_backticks)
    raw = re.findall(r"[A-Za-z]:\\[^\s`，,；;]+", desc)
    return dedupe_keep_order(raw)


def parse_unfinished_parallel_tasks(tasks_path: Path) -> list[Phase]:
    phases: list[Phase] = []
    current: Phase | None = None

    for line in tasks_path.read_text(encoding="utf-8").splitlines():
        p = PHASE_RE.match(line.strip())
        if p:
            number = int(p.group(1))
            title = p.group(2).strip()
            current = Phase(number=number, title=title, slug=slugify(title))
            phases.append(current)
            continue

        t = TASK_RE.match(line)
        if not t or current is None:
            continue

        done = t.group("done")
        is_parallel = t.group("p") is not None
        if done.lower() == "x" or not is_parallel:
            continue

        task_id = t.group("id")
        story = (t.group("story") or "").strip("[]")
        desc = t.group("desc").strip()
        current.tasks.append(
            Task(
                task_id=task_id,
                description=desc,
                story=story,
                scope_files=extract_scope_files(desc),
            )
        )

    return [phase for phase in phases if phase.tasks]


def build_protocol_content(feature_dir: Path, prompts_dir: Path) -> str:
    return f"""# Multi-Task Session Summary Protocol

## 目的

统一并行子任务的收口格式，避免多任务并发写面冲突。

## 边界

1. `tasks.md` 仅主线程可修改。
2. 子任务只允许修改自己的 summary slot。
3. 子任务不得触发 Phase End Check。

## Worker Rules

1. 仅修改业务 scope files 与允许写回的 summary slot。
2. 不修改 `Main Thread Control`。
3. 不修改 `Phase End Check`。
4. 不新增第二份总结；重跑时覆盖旧 slot 内容。

## Status 枚举

- `done`
- `partial`
- `blocked`

## Mandatory Final Output Contract

```text
SESSION_SUMMARY_BEGIN
task_id: T###
status: done|partial|blocked
changed_files:
  - <ABS_PATH>
validation:
  - pass|fail|not_run: <detail>
risks_or_blockers:
  - <none|item>
completion_recommendation: complete|needs_followup|cannot_complete
SESSION_SUMMARY_END
```

## 主线程消费规则

1. 读取 `{prompts_dir / 'README.md'}`。
2. 按 Phase 分发 `T###.md`。
3. 等待各 slot 填充。
4. 仅在用户明确触发后执行 Phase End Check。

## 按 Phase 推进规则

1. 一次只推进一个 Phase。
2. 不跨 Phase 并发。
3. 当前 Phase 收口后才推进下一 Phase。

## Feature

- feature_dir: `{feature_dir}`
"""


def build_phase_summary(phase: Phase) -> str:
    now = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    slot_blocks = []
    for task in phase.tasks:
        slot_blocks.append(
            "\n".join(
                [
                    f"### {task.task_id}",
                    f"<!-- TASK_SUMMARY_SLOT: {task.task_id} BEGIN -->",
                    "_PENDING_SESSION_SUMMARY_",
                    f"<!-- TASK_SUMMARY_SLOT: {task.task_id} END -->",
                    "",
                ]
            )
        )
    slots = "\n".join(slot_blocks).rstrip()

    return f"""# Phase {phase.number:02d}: {phase.title} - Tasks Execution Summary

- generated_at: {now}
- phase_dir: {phase.dir_name}

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

{slots}
"""


def build_task_prompt(
    task: Task,
    phase: Phase,
    feature_dir: Path,
    summary_doc: Path,
    protocol_doc: Path,
    extra_context_files: list[Path],
) -> str:
    scope_lines = "\n".join([f"- `{path}`" for path in task.scope_files]) or "- _NONE_DETECTED_"

    context_files: list[Path] = [
        feature_dir / "spec.md",
        feature_dir / "plan.md",
        feature_dir / "tasks.md",
    ]
    context_files.extend(extra_context_files)
    context_files.append(summary_doc)
    context_lines = "\n".join([f"- `{p}`" for p in dedupe_keep_order([str(x) for x in context_files])])

    scope_for_context = "\n".join([f"- `{path}`" for path in task.scope_files])
    if scope_for_context:
        context_lines += "\n" + scope_for_context

    return f"""# {task.task_id} Prompt

## Task Card

- task_id: `{task.task_id}`
- phase: `Phase {phase.number:02d} - {phase.title}`
- story: `{task.story or 'N/A'}`
- source: `{task.description}`

## Scope Files

{scope_lines}

## Allowed Coordination Writeback

- summary_doc: `{summary_doc}`
- task_section: `{task.task_id}`
- slot_markers:
  - `<!-- TASK_SUMMARY_SLOT: {task.task_id} BEGIN -->`
  - `<!-- TASK_SUMMARY_SLOT: {task.task_id} END -->`
- rule: 只允许替换当前 task slot markers 之间的内容，禁止修改其它 slot。

## Phase Context

- current_phase_status: collecting_task_summaries
- phase_end_check_requested_by_user: false
- phase_end_check_status: not_started

## Execution Instructions

1. 只实现当前 task 对应变更。
2. 禁止修改 `tasks.md`。
3. 禁止修改 `Main Thread Control` 与 `Phase End Check`。
4. 完成后必须输出标准化总结 block。
5. 将同一份总结 block 回写到本 task slot。

## Required Context Files

{context_lines}

- protocol: `{protocol_doc}`

## Done Definition

1. 业务改动已完成并自检。
2. 总结 block 已输出到会话。
3. 总结 block 已回写到本 slot。
4. 未越权修改其它文件面。

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: {task.task_id}
status: done|partial|blocked
changed_files:
  - <ABS_PATH>
validation:
  - pass|fail|not_run: <detail>
risks_or_blockers:
  - <none|item>
completion_recommendation: complete|needs_followup|cannot_complete
SESSION_SUMMARY_END
```
"""


def build_readme(feature_dir: Path, prompts_dir: Path, phases: list[Phase]) -> str:
    total_prompts = sum(len(phase.tasks) for phase in phases)
    rows = []
    for phase in phases:
        task_ids = ", ".join(task.task_id for task in phase.tasks)
        rows.append(
            f"| Phase {phase.number:02d} | `{phase.dir_name}` | {task_ids} | {len(phase.tasks)} | `Tasks-Execution-Summary.md` |"
        )
    table = "\n".join(rows)

    return f"""# Prompts Workspace Index

## 用途

本目录用于当前 feature 的并行子任务分发与收口，不跨 feature 复用。

## 固定规则

1. 仅为未完成 `[P]` 任务生成 prompt。
2. `tasks.md` 仅由主线程维护。
3. 每个 Phase 都有 `Tasks-Execution-Summary.md`。
4. 子任务只允许写回自己的 slot。

## Index

| Phase | Directory | Task IDs | Prompt Count | Summary File |
|---|---|---|---:|---|
{table}

## Stats

- feature_dir: `{feature_dir}`
- prompts_root: `{prompts_dir}`
- total_prompt_count: {total_prompts}
- phase_summary_file: Tasks-Execution-Summary.md
- protocol_file: {prompts_dir / '00-Multi-Task-Session-Summary-Protocol.md'}

## Main Thread Phase Flow

1. 只分发当前 Phase 的 `T###.md`。
2. 等待当前 Phase slot 收口。
3. 等待用户明确触发 Phase End Check。
4. 完成主线程收口后进入下一 Phase。
"""


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate parallel prompts workspace from tasks.md")
    parser.add_argument("--feature-dir", required=True, help="Absolute or repo-relative feature directory")
    parser.add_argument("--prompts-dir", default="Prompts", help="Prompts directory name under feature dir")
    args = parser.parse_args()

    feature_dir = Path(args.feature_dir).expanduser().resolve()
    tasks_path = feature_dir / "tasks.md"
    if not tasks_path.exists():
        raise FileNotFoundError(f"tasks.md not found: {tasks_path}")

    phases = parse_unfinished_parallel_tasks(tasks_path)
    if not phases:
        print("No unfinished parallel tasks ([P]) found. Nothing generated.")
        return 0

    prompts_dir = feature_dir / args.prompts_dir
    prompts_dir.mkdir(parents=True, exist_ok=True)

    desired_phase_names = {phase.dir_name for phase in phases}
    for existing_phase_dir in prompts_dir.iterdir():
        if not existing_phase_dir.is_dir() or not existing_phase_dir.name.startswith("Phase-"):
            continue
        if existing_phase_dir.name not in desired_phase_names:
            shutil.rmtree(existing_phase_dir)

    protocol_path = prompts_dir / "00-Multi-Task-Session-Summary-Protocol.md"
    protocol_path.write_text(build_protocol_content(feature_dir, prompts_dir), encoding="utf-8")

    extra_candidates = [
        feature_dir / "wave-mapping.md",
        feature_dir / "module-cutover-checklist.md",
        feature_dir / "validation-gates.md",
    ]
    extra_files = [path for path in extra_candidates if path.exists()]

    for phase in phases:
        phase_dir = prompts_dir / phase.dir_name
        phase_dir.mkdir(parents=True, exist_ok=True)
        desired_task_files = {f"{task.task_id}.md" for task in phase.tasks}
        for existing_task_doc in phase_dir.glob("T[0-9][0-9][0-9].md"):
            if existing_task_doc.name not in desired_task_files:
                existing_task_doc.unlink()

        summary_path = phase_dir / "Tasks-Execution-Summary.md"
        summary_path.write_text(build_phase_summary(phase), encoding="utf-8")

        for task in phase.tasks:
            task_doc = phase_dir / f"{task.task_id}.md"
            task_doc.write_text(
                build_task_prompt(
                    task=task,
                    phase=phase,
                    feature_dir=feature_dir,
                    summary_doc=summary_path,
                    protocol_doc=protocol_path,
                    extra_context_files=extra_files,
                ),
                encoding="utf-8",
            )

    readme_path = prompts_dir / "README.md"
    readme_path.write_text(build_readme(feature_dir, prompts_dir, phases), encoding="utf-8")

    print(f"Feature dir: {feature_dir}")
    print(f"Prompts dir: {prompts_dir}")
    print(f"Phases generated: {len(phases)}")
    print(f"Prompt files generated: {sum(len(phase.tasks) for phase in phases)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
