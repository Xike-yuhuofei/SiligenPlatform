# NOISSUE Wave 2B Batch 3A Root Source-Root Flip Plan

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-173019`
- 输入：
  - `docs/process-model/plans/NOISSUE-wave2b-batch2-closeout-20260322-172135.md`
  - `docs/process-model/plans/NOISSUE-wave2bD-integration-20260322-161904.md`
  - `docs/process-model/plans/NOISSUE-wave2b-batch0-source-root-freeze-20260322-162552.md`
  - `integration/reports/verify/wave2b-batch2/apps/workspace-validation.json`
  - `integration/reports/verify/wave2b-batch2/packages/workspace-validation.json`
  - `CMakeLists.txt`
  - `tests/CMakeLists.txt`
  - `tools/build/build-validation.ps1`
  - `tools/scripts/invoke-workspace-tests.ps1`
  - `tools/migration/validate_workspace_layout.py`

## 1. 当前裁决

1. 父线程当前目标是进入 `Batch 3`，不是回做 `Batch 2`。
2. `Batch 2 bridge` 已完成且验证全绿，`3A -> 3B -> 3C` 的串行前提成立。
3. 当前最合理的下一步是先补 `Batch 3A` 的正式计划，并直接实施一个最小、可回滚、不会踩脏工作树的 `3A` 收紧点。
4. 当前不进入 `3B` 或 `3C`；它们都依赖 `3A` 的正式 closeout。

## 2. 为什么先规划再实施 3A

1. 仓内已有 `Batch 0/1/2` 设计与 closeout，但没有 `Batch 3` 或 `Batch 3A` 的单独计划文档。
2. 当前 root selector 事实上已经落地：
   - `cmake/workspace-layout.env` 中 `SILIGEN_SUPERBUILD_SOURCE_ROOT=.`
   - `CMakeLists.txt`、`tests/CMakeLists.txt`、`tools/build/build-validation.ps1` 都已读取同一 selector
   - Batch 2 报告已记录 `source_root_selector_relative=.`、`workspace_source_root=D:\Projects\SS-dispense-align`
3. 现阶段缺的不是再次改 root graph，而是把现有 provenance 从“已打印、可观察”收紧成“失配即阻断”的正式 cutover 证据。
4. 直接去碰 `CMakeLists.txt`、`tests/CMakeLists.txt` 或 `tools/build/build-validation.ps1` 风险更高，因为这些文件已在脏工作树中，有并发修改痕迹。

## 3. 3A 的本批次目标

1. 不重做 selector bridge。
2. 不改变 `workspace root`、`control-apps-build`、`integration/reports`、wrapper 参数面。
3. 只把 root source-root 合同的关键证据补成正式 gate：
   - 若 `control-apps-build/CMakeCache.txt` 存在，则其中 `CMAKE_HOME_DIRECTORY` 必须存在。
   - 若 `control-apps-build/CMakeCache.txt` 存在，则其值必须等于 selector 解析出的 workspace root。
4. 让该 gate 挂在现有 `packages` 根级门禁链上，避免新增独立命令面。

## 4. 实施范围

1. 代码写集：
   - `tools/migration/validate_workspace_layout.py`
2. 文档写集：
   - `docs/process-model/plans/NOISSUE-wave2b-batch3a-root-flip-plan-20260322-173019.md`
3. 非范围：
   - 不修改 `CMakeLists.txt`
   - 不修改 `tests/CMakeLists.txt`
   - 不修改 `tools/build/build-validation.ps1`
   - 不修改 runtime bridge / residual ledger

## 5. 失败模式与回滚

1. 若新增 provenance gate 在当前已存在 build cache 上失败，说明 root source-root 合同与缓存 provenance 失配，`3A` 不得继续推进到 closeout。
2. 若脚本在无 build cache 环境下误报失败，则说明 gate 过紧，需要只在 cache 存在时阻断。
3. 本批次最小回滚单元仅为：
   - `tools/migration/validate_workspace_layout.py`
   - 本计划文档
4. 不允许回滚或覆盖任何与本批次无关的已脏 tracked 文件。

## 6. 验证命令

1. `python -m py_compile tools/migration/validate_workspace_layout.py`
2. `python tools/migration/validate_workspace_layout.py`

## 7. 完成标准

1. `validate_workspace_layout.py` 除 selector/wiring 外，新增 provenance 输出与阻断。
2. 在当前工作区已存在的 `control-apps-build` cache 上，`CMAKE_HOME_DIRECTORY` 与 workspace root 一致并通过验证。
3. 本批次完成后，再决定是否需要把同一证明提升为 `3A closeout`；在没有 closeout 前，不启动 `3B`。

## 8. 串并行裁决

1. 本批次不并行。
2. 原因不是“不能并行”，而是这一步只有一个小写集，而且 `control-apps-build` provenance 只能在单一 build root 上判断；并行没有收益，反而增加混淆。
