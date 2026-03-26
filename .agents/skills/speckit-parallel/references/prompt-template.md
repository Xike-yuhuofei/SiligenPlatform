# T### Prompt Template

## Task Card

- task_id: `T###`
- phase: `Phase XX - <title>`
- story: `US#|N/A`
- source: `<original tasks.md description>`

## Scope Files

- `<ABS_PATH_1>`
- `<ABS_PATH_2>`

## Allowed Coordination Writeback

- summary_doc: `<ABS_PATH_TO_PHASE_SUMMARY>`
- task_section: `T###`
- slot_markers:
  - `<!-- TASK_SUMMARY_SLOT: T### BEGIN -->`
  - `<!-- TASK_SUMMARY_SLOT: T### END -->`
- rule: 只允许替换当前 task slot，禁止越权修改。

## Phase Context

- current_phase_status: collecting_task_summaries
- phase_end_check_requested_by_user: false
- phase_end_check_status: not_started

## Execution Instructions

1. 仅处理当前 task。
2. 不修改 `tasks.md`。
3. 不修改其他 task slot。
4. 输出并回写同一份 session summary。

## Required Context Files

- `<feature>/spec.md`
- `<feature>/plan.md`
- `<feature>/tasks.md`
- `<feature>/Prompts/00-Multi-Task-Session-Summary-Protocol.md`
- `<feature>/Prompts/Phase-XX-*/Tasks-Execution-Summary.md`

## Done Definition

1. 功能完成并自检。
2. 输出标准化总结 block。
3. 回写本 task slot。

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: T###
status: done|partial|blocked
changed_files:
validation:
risks_or_blockers:
completion_recommendation:
SESSION_SUMMARY_END
```
