# B1 Prompt

## 任务卡

- task_id: `B1`
- wave: `Wave 01`
- chain: `Parallel Chain B`
- goal: `把 workflow 的 integration/regression 承载面从占位目录提升为正式 canonical 测试入口`
- depends_on: `none`
- unblocks: `B3`
- build_dir: `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`

## 前置门槛

- 直接开工，不等待其他子任务。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止修改 `tasks.md`、全局状态总表、任意 `Prompts\` 文档。
4. 本任务只做测试承载面与测试登记，不改 `workflow` owner 实现和 public surface。
5. `A2` 将处理 public header owner 化；不要提前修改 `modules/workflow/CMakeLists.txt`、`domain/include`、`application/include`、`adapters/include`。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\modules\workflow\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\workflow\tests\README.md`
- `D:\Projects\SS-dispense-align\modules\workflow\tests\integration\**`
- `D:\Projects\SS-dispense-align\modules\workflow\tests\regression\**`

## Forbidden Scope

- `D:\Projects\SS-dispense-align\modules\workflow\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\workflow\domain\**`
- `D:\Projects\SS-dispense-align\modules\workflow\application\**`
- `D:\Projects\SS-dispense-align\modules\workflow\adapters\**`
- `D:\Projects\SS-dispense-align\modules\workflow\process-runtime-core\**`
- `D:\Projects\SS-dispense-align\tests\CMakeLists.txt`

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-b1-workflow-integration-regression-surface.md`
- rule: 只允许新增或更新这份专项报告，记录新增测试目标、登记结果、当前仍缺的 workflow 集成场景。

## 必读上下文

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\plan.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase11-workflow-shell-closeout.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase14-motion-planning-owner-activation.md`
- `D:\Projects\SS-dispense-align\modules\workflow\tests\README.md`
- `D:\Projects\SS-dispense-align\modules\workflow\tests\CMakeLists.txt`

## 执行动作

1. 把 `tests/integration`、`tests/regression` 从 `.gitkeep` 占位提升为正式目录结构、说明文档和 `CMakeLists.txt` 入口。
2. 在不改 owner 实现的前提下，优先落地轻量且真实的测试登记面，例如 workflow 初始化、装配、路径执行或 assembly 级 smoke。
3. 如果无法在本任务内引入稳定可运行测试，也必须至少把目录、README、CMake 注册面和后续 target 命名规则一次性定好。
4. 更新 `modules/workflow/tests/CMakeLists.txt`，使新的 integration/regression 面能被 canonical tests root 看见。
5. 在专项报告中明确列出新增 target、暂未覆盖的场景和为何这些场景需要后续任务继续补齐。

## 交付物

- workflow 的 integration/regression 正式测试承载面
- 新增或更新的测试注册入口
- `phase15-b1-workflow-integration-regression-surface.md`

## 验证

- `cmake -S D:\Projects\SS-dispense-align -B C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`
- `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -N`
- 对本任务新增 target 执行定向构建
- 如新增目标可执行，再定向运行至少一个最小 smoke 测试

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: B1
status: done|partial|blocked
changed_files:
  - <ABS_PATH>
validation:
  - pass|fail|not_run: <detail>
risks_or_blockers:
  - <none|item>
completion_recommendation: complete|needs_followup|cannot_complete
next_ready_task: B3|none
SESSION_SUMMARY_END
```
