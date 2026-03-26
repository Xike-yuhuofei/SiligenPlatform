# A2 Prompt

## 任务卡

- task_id: `A2`
- wave: `Wave 02`
- chain: `Main Chain A`
- goal: `把 workflow 的 wrapper-backed public surface 收敛到真正 owner-backed 的 canonical 实现目录`
- depends_on: `A1`
- unblocks: `A3`
- build_dir: `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`

## 前置门槛

- 只有在 `A1` 已完成并形成 `phase15-a1-motion-planning-residual-drain.md` 后才允许开工。
- 若 `A1` 未收口，直接返回 `blocked`，不要提前修改 `workflow` public surface。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止修改 `tasks.md`、全局状态总表、任意 `Prompts\` 文档。
4. 本任务聚焦 public surface owner 化，不做 runtime-execution 设备适配迁移。
5. 不允许重新把 public headers 回指到 `process-runtime-core/src` 或其他 bridge payload。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\modules\workflow\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\workflow\domain\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\workflow\application\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\workflow\adapters\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\workflow\domain\include\**`
- `D:\Projects\SS-dispense-align\modules\workflow\application\include\**`
- `D:\Projects\SS-dispense-align\modules\workflow\adapters\include\**`
- `D:\Projects\SS-dispense-align\modules\workflow\domain\domain\**`
- `D:\Projects\SS-dispense-align\modules\workflow\application\**`
- `D:\Projects\SS-dispense-align\modules\workflow\adapters\**`
- `D:\Projects\SS-dispense-align\modules\workflow\process-runtime-core\CMakeLists.txt`

## Forbidden Scope

- `D:\Projects\SS-dispense-align\modules\runtime-execution\**`
- `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`
- `D:\Projects\SS-dispense-align\modules\workflow\tests\integration\**`
- `D:\Projects\SS-dispense-align\modules\workflow\tests\regression\**`

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-a2-workflow-public-surface-ownerization.md`
- rule: 只允许新增或更新这份专项报告，记录已 owner 化的 public headers、残留 wrapper、构建验证结果。

## 必读上下文

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase8-runtime-execution-bridge-drain.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase11-workflow-shell-closeout.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase14-motion-planning-owner-activation.md`
- `D:\Projects\SS-dispense-align\modules\workflow\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\workflow\module.yaml`

## 执行动作

1. 盘点 `domain/include`、`application/include`、`adapters/include` 里当前仍回指 bridge 的 wrapper headers。
2. 优先把稳定 owner 面改成真正由 canonical 实现目录承载，而不是长期透传 `process-runtime-core`。
3. 保持 `siligen_workflow_*_public` 和 `siligen_workflow_runtime_consumer_public` target 名称不变，避免下游消费者改名。
4. 如必须保留极少量 bridge wrapper，需在专项报告中明确列出并说明为什么还不能移除。
5. 不修改 `runtime-execution` 或外部 app 消费者；这些兼容性由现有 target 闭环验证。

## 交付物

- workflow owner-backed public surface 更新
- 最小 bridge wrapper 残留清单
- `phase15-a2-workflow-public-surface-ownerization.md`

## 验证

- `cmake -S D:\Projects\SS-dispense-align -B C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`
- `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_unit_tests process_runtime_core_motion_runtime_assembly_test process_runtime_core_pb_path_source_adapter_test siligen_runtime_host_unit_tests`
- `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "process_runtime_core_motion_runtime_assembly_test|process_runtime_core_pb_path_source_adapter_test|siligen_runtime_host_unit_tests" --output-on-failure`
- `rg -n "process-runtime-core/src|src/legacy" D:\Projects\SS-dispense-align\modules\workflow\domain\include D:\Projects\SS-dispense-align\modules\workflow\application\include D:\Projects\SS-dispense-align\modules\workflow\adapters\include`

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: A2
status: done|partial|blocked
changed_files:
  - <ABS_PATH>
validation:
  - pass|fail|not_run: <detail>
risks_or_blockers:
  - <none|item>
completion_recommendation: complete|needs_followup|cannot_complete
next_ready_task: A3|none
SESSION_SUMMARY_END
```
