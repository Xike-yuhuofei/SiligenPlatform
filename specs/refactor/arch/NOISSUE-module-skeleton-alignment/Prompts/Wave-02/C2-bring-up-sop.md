# C2 Prompt

## 任务卡

- task_id: `C2`
- wave: `Wave 02`
- chain: `Parallel Chain C`
- goal: `形成首轮真实硬件 bring-up / smoke 的现场 SOP，覆盖启动前检查、停止条件、回滚和日志采集`
- depends_on: `C1`
- unblocks: `A4`

## 前置门槛

- 只有在 `C1` 已完成并形成 `phase15-c1-real-hardware-assets-and-config-prep.md` 后才允许开工。
- 若 `C1` 未完成，直接返回 `blocked`。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止修改 `tasks.md`、全局状态总表、任意 `Prompts\` 文档。
4. 本任务是 SOP 文档工作，不改动 C++、CMake 或设备适配逻辑。
5. SOP 中所有路径、配置、资产名必须与当前 canonical 路径一致。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\docs\runtime\hardware-bring-up-smoke-sop.md`

## Forbidden Scope

- 任意 `*.cpp`、`*.h`、`CMakeLists.txt`
- `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`
- `D:\Projects\SS-dispense-align\scripts\validation\**`
- `D:\Projects\SS-dispense-align\config\machine\**`

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-c2-bring-up-sop.md`
- rule: 只允许新增或更新这份专项报告，记录 SOP 覆盖面、已知前置缺口和对 A4 的输入。

## 必读上下文

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase10-runtime-execution-shell-closeout.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase14-motion-planning-owner-activation.md`
- `D:\Projects\SS-dispense-align\docs\runtime\rollback.md`
- `D:\Projects\SS-dispense-align\config\machine\machine_config.ini`
- `D:\Projects\SS-dispense-align\apps\runtime-service\README.md`
- `D:\Projects\SS-dispense-align\apps\runtime-gateway\README.md`

## 执行动作

1. 编写首轮 bring-up / smoke SOP，至少覆盖通电前检查、机器配置检查、vendor 资产检查、急停与限位检查、回零前检查、点动 / I/O / 触发 smoke、停止条件、失败回滚、日志与证据收集。
2. 所有命令和路径使用当前 canonical 路径，不得重新写回 bridge/alias。
3. 明确哪些步骤是必须人工确认、哪些可以脚本化。
4. 在专项报告中说明 SOP 与 C1/C3/A4 的衔接关系。

## 交付物

- `docs/runtime/hardware-bring-up-smoke-sop.md`
- `phase15-c2-bring-up-sop.md`

## 验证

- `rg -n "config/machine_config.ini|device-adapters/vendor|runtime-host" D:\Projects\SS-dispense-align\docs\runtime\hardware-bring-up-smoke-sop.md`
- 文档自检，确认路径与命令可回链到仓内真实文件或脚本

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: C2
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
