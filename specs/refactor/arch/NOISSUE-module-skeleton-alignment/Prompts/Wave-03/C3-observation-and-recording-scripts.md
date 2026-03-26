# C3 Prompt

## 任务卡

- task_id: `C3`
- wave: `Wave 03`
- chain: `Parallel Chain C`
- goal: `补齐硬件 smoke 观察、记录和证据收集脚本，使现场能快速区分配置问题、装配问题和 owner 边界问题`
- depends_on: `C1`
- unblocks: `A4`

## 前置门槛

- 只有在 `C1` 已完成并形成 `phase15-c1-real-hardware-assets-and-config-prep.md` 后才允许开工。
- 若 `C1` 未完成，直接返回 `blocked`。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止修改 `tasks.md`、全局状态总表、任意 `Prompts\` 文档。
4. 本任务只处理 `scripts/validation` 下的观察与记录脚本，不改业务实现或 migration validator。
5. 新脚本应优先复用现有 `scripts/validation` 能力，不重复造完整框架。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\scripts\validation\run-hardware-smoke-observation.ps1`
- `D:\Projects\SS-dispense-align\scripts\validation\register-hardware-smoke-observation.ps1`
- `D:\Projects\SS-dispense-align\scripts\validation\README-hardware-smoke-observation.md`

## Forbidden Scope

- `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`
- `D:\Projects\SS-dispense-align\docs\runtime\hardware-bring-up-smoke-sop.md`
- 任意 `modules\**` 源码或 `CMakeLists.txt`

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-c3-observation-and-recording-scripts.md`
- rule: 只允许新增或更新这份专项报告，记录脚本入口、输出位置、证据格式和已知限制。

## 必读上下文

- `D:\Projects\SS-dispense-align\scripts\validation\run-external-migration-observation.ps1`
- `D:\Projects\SS-dispense-align\scripts\validation\register-external-observation-intake.ps1`
- `D:\Projects\SS-dispense-align\scripts\validation\run-local-validation-gate.ps1`
- `D:\Projects\SS-dispense-align\apps\runtime-service\run.ps1`
- `D:\Projects\SS-dispense-align\apps\runtime-gateway\run.ps1`

## 执行动作

1. 在 `scripts/validation` 下补齐硬件 smoke 观察脚本，至少覆盖启动命令、配置路径、日志路径、关键检查点和结果摘要输出。
2. 如果需要登记外部观察输入，可提供对应 register 脚本，但不要修改既有 migration observation 流程。
3. 输出格式必须有利于 A4 做 go/no-go 汇总，优先使用结构化文本或 markdown 报告。
4. 在专项报告中写清脚本入口、依赖参数、默认输出目录以及如何与 SOP 配套使用。

## 交付物

- 新增或更新的 hardware smoke 观察脚本
- `README-hardware-smoke-observation.md`
- `phase15-c3-observation-and-recording-scripts.md`

## 验证

- 对新增或修改的 PowerShell 脚本做语法解析
- 如脚本支持 help / dry-run，执行最小自检
- 自检脚本输出是否能包含配置路径、运行命令、结果摘要、日志位置

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: C3
status: done|partial|blocked
changed_files:
  - <ABS_PATH>
validation:
  - pass|fail|not_run: <detail>
risks_or_blockers:
  - <none|item>
completion_recommendation: complete|needs_followup|cannot_complete
next_ready_task: A4|none
SESSION_SUMMARY_END
```
