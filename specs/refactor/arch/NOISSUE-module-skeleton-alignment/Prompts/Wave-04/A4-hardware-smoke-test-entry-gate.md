# A4 Prompt

## 任务卡

- task_id: `A4`
- wave: `Wave 04`
- chain: `Main Chain A`
- goal: `对首轮真实硬件 smoke test 做 go/no-go 进入门评审，只在条件满足时给出可执行的上板入口`
- depends_on: `A3`, `B2`, `C2`, `C3`
- unblocks: `hardware_smoke_test`
- build_dir: `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`

## 前置门槛

- 只有在 `A3`、`B2`、`C2`、`C3` 全部完成并形成各自专项报告后才允许开工。
- 若任一前置未完成，直接返回 `blocked`，不要替前置任务补做实现。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止修改 `tasks.md`、全局状态总表、任意 `Prompts\` 文档。
4. 本任务默认是 gate review 和证据汇总，不做新的架构性代码改动。
5. 如果发现阻塞项，给出明确 `No-Go` 或 `Needs Follow-up`，不要用模糊表述。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-a4-hardware-smoke-test-entry-gate.md`

## Forbidden Scope

- 任意 `modules\**`、`apps\**`、`scripts\**`、`config\**` 源文件
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\README.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\module-checklist.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\wave-mapping.md`

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-a4-hardware-smoke-test-entry-gate.md`
- rule: 只允许更新这份 gate 报告，产出明确的 go/no-go 结论、阻塞项、建议执行命令和证据链接。

## 必读上下文

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-a3-runtime-execution-hardware-adapter-ownerization.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-b2-runtime-execution-integration-regression-surface.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-c2-bring-up-sop.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-c3-observation-and-recording-scripts.md`
- `D:\Projects\SS-dispense-align\docs\runtime\hardware-bring-up-smoke-sop.md`

## 执行动作

1. 汇总 `A3`、`B2`、`C2`、`C3` 的结果，判断是否达到首轮硬件 smoke 进入门。
2. 至少核查以下 gate：
   - 硬件测试主链是否已退出关键 public legacy include
   - runtime host 非硬件 smoke 是否已稳定
   - bring-up SOP 是否完整
   - 观察/记录脚本是否可用
   - canonical 配置与 vendor 资产路径是否明确
3. 若 gate 通过，给出推荐执行顺序、建议命令、证据收集要求。
4. 若 gate 不通过，给出明确 blocker 列表、所属任务链和建议补救顺序。

## 交付物

- `phase15-a4-hardware-smoke-test-entry-gate.md`

## 验证

- 复核前置专项报告是否存在且内容完整
- 必要时复跑最小命令：
  - `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -N`
  - `python D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`
- gate 结论必须写成 `Go`、`No-Go` 或 `Go with follow-ups`

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: A4
status: done|partial|blocked
changed_files:
  - <ABS_PATH>
validation:
  - pass|fail|not_run: <detail>
risks_or_blockers:
  - <none|item>
completion_recommendation: complete|needs_followup|cannot_complete
next_ready_task: hardware_smoke_test|none
SESSION_SUMMARY_END
```
