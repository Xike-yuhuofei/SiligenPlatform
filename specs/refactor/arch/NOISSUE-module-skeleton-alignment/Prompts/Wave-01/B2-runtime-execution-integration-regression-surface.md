# B2 Prompt

## 任务卡

- task_id: `B2`
- wave: `Wave 01`
- chain: `Parallel Chain B`
- goal: `把 runtime-execution 的 integration/regression 承载面从占位目录提升为正式测试入口，并建立不依赖真实硬件的 host 级 smoke 验证`
- depends_on: `none`
- unblocks: `B3`, `A4`
- build_dir: `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`

## 前置门槛

- 直接开工，不等待其他子任务。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止修改 `tasks.md`、全局状态总表、任意 `Prompts\` 文档。
4. 本任务只处理测试承载面、测试注册和轻量 host 级集成验证，不进入 `adapters/device` owner 化。
5. `A3` 将处理硬件适配 canonical 化；不要修改 `modules/runtime-execution/adapters/device\**` 的实现文件。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\tests\README.md`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\tests\integration\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\tests\regression\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\tests\**`

## Forbidden Scope

- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\bootstrap\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\factories\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-b2-runtime-execution-integration-regression-surface.md`
- rule: 只允许新增或更新这份专项报告，记录新增测试根、新增 target、仍缺的真实硬件前置验证项。

## 必读上下文

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase8-runtime-execution-bridge-drain.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase10-runtime-execution-shell-closeout.md`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\README.md`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\tests\README.md`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\tests\CMakeLists.txt`

## 执行动作

1. 为 `modules/runtime-execution/tests` 建立正式 `CMakeLists.txt` 和 integration/regression 子目录入口。
2. 如需让仓库级 `tests` 根识别 `modules/runtime-execution/tests`，允许更新 `tests/CMakeLists.txt`，但只做测试根注册，不改变其他模块逻辑。
3. 在 `runtime/host/tests` 或新建 integration/regression 目录中落地至少一个不依赖真实板卡的 host bootstrap/configuration smoke 场景。
4. 保持现有 `siligen_runtime_host_unit_tests` 正常；新增测试不应引入真实硬件强依赖。
5. 在专项报告中记录新增测试 target、是否已登记到 `ctest -N`，以及哪些硬件相关场景仍需 A3/A4 后补齐。

## 交付物

- runtime-execution 的 integration/regression 正式测试入口
- 至少一个 host 级非硬件 smoke 场景
- `phase15-b2-runtime-execution-integration-regression-surface.md`

## 验证

- `cmake -S D:\Projects\SS-dispense-align -B C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`
- `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_runtime_host_unit_tests`
- 对本任务新增 target 执行定向构建
- `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -N`
- 如新增测试可运行，执行最小集 smoke

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: B2
status: done|partial|blocked
changed_files:
  - <ABS_PATH>
validation:
  - pass|fail|not_run: <detail>
risks_or_blockers:
  - <none|item>
completion_recommendation: complete|needs_followup|cannot_complete
next_ready_task: B3|A4|none
SESSION_SUMMARY_END
```
