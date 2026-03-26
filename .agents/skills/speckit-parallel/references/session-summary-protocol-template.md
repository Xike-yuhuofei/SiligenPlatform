# Multi-Task Session Summary Protocol Template

## Worker Rules

1. 不修改 `tasks.md`。
2. 仅改业务 scope files 与自己的 summary slot。
3. 不修改 `Main Thread Control` 与 `Phase End Check`。

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

## Main Thread Consumption

1. 按 Phase 分发。
2. 等待所有 slot 填充。
3. 等待用户触发 Phase End Check。
4. 主线程统一更新 `tasks.md`。
