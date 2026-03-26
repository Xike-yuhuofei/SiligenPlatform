# B3 Prompt

## 任务卡

- task_id: `B3`
- wave: `Wave 02`
- chain: `Parallel Chain B`
- goal: `把 validator 与 test registration 规则同步到新的 workflow/runtime-execution 测试承载面和禁桥约束`
- depends_on: `B1`, `B2`
- unblocks: `A4`
- build_dir: `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`

## 前置门槛

- 只有在 `B1`、`B2` 已完成并形成各自专项报告后才允许开工。
- 若前置任务未完成，直接返回 `blocked`。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止修改 `tasks.md`、全局状态总表、任意 `Prompts\` 文档。
4. 本任务只处理校验脚本、测试根注册和测试登记加固，不修改业务实现。
5. 如果需要新增断言，优先做 required/forbidden snippet 与 canonical path gate，不在本任务做大规模目录重排。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`
- `D:\Projects\SS-dispense-align\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\workflow\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\tests\CMakeLists.txt`

## Forbidden Scope

- 任意 `*.cpp` / `*.h` 业务实现文件
- `D:\Projects\SS-dispense-align\modules\workflow\domain\**`
- `D:\Projects\SS-dispense-align\modules\workflow\application\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\**`
- 任意全局 README / module-checklist / wave-mapping

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-b3-validator-and-test-registration-hardening.md`
- rule: 只允许新增或更新这份专项报告，记录新增断言、测试根注册变化、`ctest -N` 结果和残留缺口。

## 必读上下文

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase14-motion-planning-owner-activation.md`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\include\siligen\device\adapters\hardware\HardwareTestAdapter.h`
- `D:\Projects\SS-dispense-align\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\workflow\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\tests\CMakeLists.txt`

## 执行动作

1. 将新的 workflow/runtime-execution integration/regression 测试承载面纳入正式注册路径。
2. 为以下高风险回退建立硬性断言：
   - public headers 继续命中 `process-runtime-core/src`
   - public headers 继续命中 `src/legacy`
   - 新增 integration/regression 目录没有被 `CMake` / `ctest` 看见
3. 如 `B2` 引入了 `modules/runtime-execution/tests/CMakeLists.txt`，在本任务中把仓库级 `tests` 根注册关系加固到稳定可复验。
4. 不修改业务代码；只做 gate、registration、validation 收口。

## 交付物

- validator 断言增强
- 测试根注册与 `ctest` 登记加固
- `phase15-b3-validator-and-test-registration-hardening.md`

## 验证

- `python D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`
- `cmake -S D:\Projects\SS-dispense-align -B C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`
- `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -N`
- 对新增或修改过的测试根执行定向构建

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: B3
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
