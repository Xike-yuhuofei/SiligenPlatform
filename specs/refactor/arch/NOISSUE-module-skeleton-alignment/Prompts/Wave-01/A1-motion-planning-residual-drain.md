# A1 Prompt

## 任务卡

- task_id: `A1`
- wave: `Wave 01`
- chain: `Main Chain A`
- goal: `继续推进 motion-planning residual runtime/control surface drain，缩小 bridge 保留面，但不宣称 shell-only closeout`
- depends_on: `none`
- unblocks: `A2`
- build_dir: `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`

## 前置门槛

- 直接开工，不等待其他子任务。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止使用破坏性 git 命令，不回滚无关改动。
4. 禁止修改 `tasks.md`、`specs/refactor/arch/NOISSUE-module-skeleton-alignment/module-checklist.md`、`specs/refactor/arch/NOISSUE-module-skeleton-alignment/wave-mapping.md`、`specs/refactor/arch/NOISSUE-module-skeleton-alignment/README.md`。
5. 禁止宣称 `motion-planning` 已 shell-only closeout。
6. validator 加固属于 `B3`，不要修改 `scripts/migration/validate_workspace_layout.py`。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\modules\motion-planning\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\motion-planning\README.md`
- `D:\Projects\SS-dispense-align\modules\motion-planning\module.yaml`
- `D:\Projects\SS-dispense-align\modules\motion-planning\domain\**`
- `D:\Projects\SS-dispense-align\modules\motion-planning\application\**`
- `D:\Projects\SS-dispense-align\modules\motion-planning\adapters\**`
- `D:\Projects\SS-dispense-align\modules\motion-planning\tests\**`
- `D:\Projects\SS-dispense-align\modules\motion-planning\src\**`

## Forbidden Scope

- `D:\Projects\SS-dispense-align\modules\workflow\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\**`
- `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`
- 任意 `Prompts\` 文档
- 任意全局状态总表或波次映射文档

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-a1-motion-planning-residual-drain.md`
- rule: 只允许新增或更新这份专项报告，记录已迁出资产、仍冻结 residual 清单、验证结果和未决风险。

## 必读上下文

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\plan.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\module-checklist.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase13-planning-chain-shell-closeout.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase14-motion-planning-owner-activation.md`
- `D:\Projects\SS-dispense-align\modules\motion-planning\README.md`
- `D:\Projects\SS-dispense-align\modules\motion-planning\module.yaml`

## 执行动作

1. 盘点 `modules/motion-planning/src` 中当前 residual runtime/control surface 与纯规划 owner 资产，明确哪些仍必须冻结、哪些可以继续迁入 canonical 面。
2. 只迁移纯 `M7` 规划事实、值对象、领域服务或 adapter glue；不要把真实运行态控制、回零、手动控制接口误迁入错误 owner。
3. 保持模块根 `siligen_motion` owner target 的稳定性，不引入对 `workflow` 或 `runtime-execution` 的新反向依赖。
4. 如需改动 `README.md` 或 `module.yaml`，只更新模块局部事实，不要写入全局完成口径。
5. 在专项报告中明确列出本次迁出的文件/类型、仍冻结 residual 文件/类型以及不能继续清排的原因。

## 交付物

- motion-planning 残余面缩小后的代码与构建文件
- 模块局部文档更新
- `phase15-a1-motion-planning-residual-drain.md`

## 验证

- `cmake -S D:\Projects\SS-dispense-align -B C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`
- `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_unit_tests process_runtime_core_motion_runtime_assembly_test process_runtime_core_deterministic_path_execution_test`
- `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "process_runtime_core_motion_runtime_assembly_test|process_runtime_core_deterministic_path_execution_test" --output-on-failure`
- 如果某项无法执行，必须在总结中说明原因。

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: A1
status: done|partial|blocked
changed_files:
  - <ABS_PATH>
validation:
  - pass|fail|not_run: <detail>
risks_or_blockers:
  - <none|item>
completion_recommendation: complete|needs_followup|cannot_complete
next_ready_task: A2|none
SESSION_SUMMARY_END
```
