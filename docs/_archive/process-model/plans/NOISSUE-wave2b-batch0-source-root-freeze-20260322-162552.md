# NOISSUE Wave 2B Batch 0 - Source-Root Freeze

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-162552`
- 输入：
  - `docs/process-model/plans/NOISSUE-wave2bA-design-20260322-155701.md`
  - `docs/process-model/plans/NOISSUE-wave2bD-integration-20260322-161904.md`

## 1. 冻结目标

冻结 root build/test/source-root 的长期终态与 selector 合同，使后续 `Batch 1` 只实现既定合同，不再重新决定物理根。

## 2. 冻结结论

1. `workspace root` 冻结为长期唯一 superbuild root。
2. `CMakeLists.txt`、`tests/CMakeLists.txt`、`tools/build/build-validation.ps1`、`build.ps1`、`test.ps1` 共同构成 root source-root 对外合同。
3. 后续允许新增单一 selector，但 selector 的语义必须是“证明 workspace root 是唯一 root”，而不是“允许在多个 root 之间切换”。
4. `controlAppsBuild` 产物根、target 名称、`integration/reports` 证据根、根级 wrapper 参数面在 `Wave 2B` 全周期内视为冻结合同。
5. 任何 future cutover 都不得再讨论“是否改成别的长期 superbuild root”；这已经在本批次关闭。

## 3. Selector 合同

1. selector 必须只有一个来源。
2. selector 必须被 root CMake、root tests、build provenance、test provenance 共用。
3. selector 解析结果必须与 `CMAKE_HOME_DIRECTORY` 一致。
4. selector 的默认值必须指向当前 workspace root。
5. selector 不提供第二个候选 root，也不提供自动 fallback 到 legacy root 的能力。

## 4. 后续实现约束

1. `Batch 1` 只允许实现 selector/provenance 可观测性，不允许借此引入新的 root 终态讨论。
2. `Batch 2` 只允许把各入口统一到同一 selector，不允许改变 wrapper 参数面或报告根。
3. `Batch 3` 的 root flip 实际上是“把所有入口改为依赖同一 selector 合同”，而不是迁移到新的物理根。

## 5. 回滚与验收

1. 本批次是设计冻结，不涉及代码回滚。
2. 唯一验收标准是：后续实现者不需要再决定“长期 root 是什么”或“selector 是否多 root”。

## 6. 明确禁止

1. 禁止把 `workspace root` 与 package root 并列为长期可选 canonical root。
2. 禁止把 selector 设计成双 root fallback。
3. 禁止在后续实现中同时修改产物根、报告根和 source-root 合同。
