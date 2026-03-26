# A3 Prompt

## 任务卡

- task_id: `A3`
- wave: `Wave 03`
- chain: `Main Chain A`
- goal: `把 runtime-execution 硬件测试主链从 legacy bridge sources 提升到 canonical owner 面，优先清理 HardwareTestAdapter 公开头和主构建链`
- depends_on: `A2`
- unblocks: `A4`
- build_dir: `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`

## 前置门槛

- 只有在 `A2` 已完成并形成 `phase15-a2-workflow-public-surface-ownerization.md` 后才允许开工。
- 若 `A2` 未完成，直接返回 `blocked`。

## 必守约束

1. 始终使用简体中文。
2. 手工文件编辑必须使用 `apply_patch`。
3. 禁止修改 `tasks.md`、全局状态总表、任意 `Prompts\` 文档。
4. 本任务聚焦 `runtime-execution` 硬件适配 canonical 化，不改现场 SOP 或 observation 脚本。
5. 不得新增对 `process-runtime-core/src` 或 `src/legacy` 的新 public include 回退。

## Allowed Write Scope

- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\include\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\src\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\bootstrap\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\factories\**`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\README.md`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\module.yaml`

## Forbidden Scope

- `D:\Projects\SS-dispense-align\config\machine\**`
- `D:\Projects\SS-dispense-align\apps\runtime-service\**`
- `D:\Projects\SS-dispense-align\apps\runtime-gateway\**`
- `D:\Projects\SS-dispense-align\tests\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`

## Allowed Coordination Writeback

- report_doc: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase15-a3-runtime-execution-hardware-adapter-ownerization.md`
- rule: 只允许新增或更新这份专项报告，记录迁出的 adapter 资产、剩余 legacy bridge sources、验证结果和对 A4 的实际输入。

## 必读上下文

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase8-runtime-execution-bridge-drain.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\phase10-runtime-execution-shell-closeout.md`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\CMakeLists.txt`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\include\siligen\device\adapters\hardware\HardwareTestAdapter.h`
- `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host\CMakeLists.txt`

## 执行动作

1. 优先清理 `HardwareTestAdapter` 的 public header 回指 `src/legacy` 问题，确保首轮硬件 smoke 会触达的公开面来自 canonical owner。
2. 缩小 `SILIGEN_DEVICE_ADAPTERS_LEGACY_BRIDGE_SOURCES`，把真实硬件 smoke 必需的 adapter/source 迁入 canonical source roots。
3. 必要时同步调整 `runtime/host` 的 wiring / factory include 路径，但只处理适配链必需变更。
4. 保持 target 名称稳定，不改动上层 app 入口。
5. 在专项报告中明确列出已从 legacy 移出的文件、仍留在 legacy bridge 的文件，以及这些残留为何不阻塞首轮硬件 smoke。

## 交付物

- runtime-execution 硬件适配主链 canonical 化代码与文档
- `phase15-a3-runtime-execution-hardware-adapter-ownerization.md`

## 验证

- `cmake -S D:\Projects\SS-dispense-align -B C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`
- `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_device_adapters siligen_runtime_host siligen_runtime_host_unit_tests`
- `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "siligen_runtime_host_unit_tests" --output-on-failure`
- `rg -n "src/legacy|process-runtime-core/src" D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\include D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\src D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host`

## Mandatory Final Output

```text
SESSION_SUMMARY_BEGIN
task_id: A3
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
