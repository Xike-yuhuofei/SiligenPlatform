# C1 Prompt

## 任务卡

- task_id: `C1`
- wave: `Wave 01`
- chain: `Parallel Chain C`
- goal: `完成真实硬件 bring-up 所需的 canonical 资产、配置和启动脚本准备，不改动 owner 代码逻辑`
- depends_on: `none`
- unblocks: `C2`, `C3`, `A4`

## 前置门槛

- 直接开工，不等待其他子任务。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止修改 `tasks.md`、全局状态总表、任意 `Prompts\` 文档。
4. 本任务只处理配置、脚本、vendor 资产说明和运行文档，不修改 C++ 实现与 CMake 所有权。
5. 不得引入新的 bridge 路径；机器配置必须继续以 `config/machine/machine_config.ini` 为唯一 canonical 入口。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\config\machine\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\vendor\multicard\**`
- `D:\Projects\SS-dispense-align\apps\runtime-service\run.ps1`
- `D:\Projects\SS-dispense-align\apps\runtime-service\README.md`
- `D:\Projects\SS-dispense-align\apps\runtime-gateway\run.ps1`
- `D:\Projects\SS-dispense-align\apps\runtime-gateway\README.md`

## Forbidden Scope

- 任意 `CMakeLists.txt`
- 任意 `*.cpp`、`*.h` 运行时实现文件
- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\src\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\**`

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-c1-real-hardware-assets-and-config-prep.md`
- rule: 只允许新增或更新这份专项报告，记录外部依赖清单、缺失资产、现场前置条件和脚本用法。

## 必读上下文

- `D:\Projects\SS-dispense-align\cmake\workspace-layout.env`
- `D:\Projects\SS-dispense-align\config\machine\README.md`
- `D:\Projects\SS-dispense-align\config\machine\machine_config.ini`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\README.md`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\vendor\multicard\README.md`
- `D:\Projects\SS-dispense-align\apps\runtime-service\README.md`
- `D:\Projects\SS-dispense-align\apps\runtime-gateway\README.md`

## 执行动作

1. 冻结真实硬件 bring-up 所需的 canonical 配置面，确认 `machine_config.ini`、vendor 资产、运行脚本参数、日志输出路径的当前事实。
2. 如果 `run.ps1` 脚本缺少对 canonical config 路径、vendor 资产路径或启动前检查的明确入口，可做最小参数化/说明性增强，但不要改动运行逻辑。
3. vendor 资产说明必须写清真实板卡启动所需文件、允许的外部覆盖方式以及缺失时的 fail-fast 行为。
4. 在专项报告中列出当前已在仓内的资产、仍需现场补齐的仓外资产以及 bring-up 前必须确认的配置项。

## 交付物

- 配置/脚本/资产说明更新
- `phase15-c1-real-hardware-assets-and-config-prep.md`

## 验证

- 对修改过的 PowerShell 脚本执行语法解析或最小 dry-run
- `rg -n "config/machine_config.ini|device-adapters/vendor/multicard" D:\Projects\SS-dispense-align\apps\runtime-service D:\Projects\SS-dispense-align\apps\runtime-gateway D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\vendor\multicard`
- 使用 `Get-Item` / `Test-Path` 自检文档中声明的 canonical 路径

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: C1
status: done|partial|blocked
changed_files:
  - <ABS_PATH>
validation:
  - pass|fail|not_run: <detail>
risks_or_blockers:
  - <none|item>
completion_recommendation: complete|needs_followup|cannot_complete
next_ready_task: C2|C3|A4|none
SESSION_SUMMARY_END
```
