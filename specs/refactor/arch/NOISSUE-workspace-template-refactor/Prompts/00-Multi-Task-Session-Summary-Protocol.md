# Multi-Task Session Summary Protocol

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

1. 读取 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\Prompts\README.md`。
2. 按 Phase 分发 `T###.md`。
3. 等待各 slot 填充。
4. 仅在用户明确触发后执行 Phase End Check。

## 按 Phase 推进规则

1. 一次只推进一个 Phase。
2. 不跨 Phase 并发。
3. 当前 Phase 收口后才推进下一 Phase。

## Feature

- feature_dir: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor`
