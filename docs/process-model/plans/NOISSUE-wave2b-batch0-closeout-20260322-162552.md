# NOISSUE Wave 2B Batch 0 Closeout

- 状态：Pass
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-162552`
- 关联输入：
  - `docs/process-model/plans/NOISSUE-wave2b-batch0-source-root-freeze-20260322-162552.md`
  - `docs/process-model/plans/NOISSUE-wave2b-batch0-runtime-root-contract-20260322-162552.md`
  - `docs/process-model/plans/NOISSUE-wave2b-batch0-owner-freeze-20260322-162552.md`
  - `docs/process-model/plans/NOISSUE-wave2b-batch0-ledger-schema-freeze-20260322-162552.md`

## 1. 本批次目标

冻结 `Wave 2B` 后续实现必须共享的四类合同：

1. source-root 终态与 selector 语义
2. runtime workspace-root 判定、兼容窗口与 fail-fast 规则
3. recipes / logging / third-party 最终 owner
4. residual ledger 的 schema 方向、family 拆分原则和收紧顺序

## 2. 本批次结论

1. `workspace root` 已冻结为长期唯一 superbuild root。
2. runtime 仓库根判定合同已冻结为“必须证明仓库根”，不接受模块级 `CMakeLists.txt` 误判。
3. 旧路径兼容窗口已冻结为保守策略，只保留两类只读 fallback：
   - `config/machine_config.ini`
   - `data/recipes/templates.json`
4. diagnostics logging canonical owner 已冻结为 `packages/traceability-observability`。
5. recipes persistence / serializer canonical owner 已冻结为 `packages/runtime-host`。
6. third-party live consumer ownership 已冻结为真实消费者显式声明，核心 owner 为 `packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway`。
7. residual ledger schema 已冻结为带 `family/type/owner/must_zero_before_cutover` 的方向，并明确了 family 拆分和 gate 收紧顺序。

## 3. Batch 1 Go / No-Go 判定

当前结论：`Batch 1 = Go`

理由：

1. `Batch 1` 不再需要决定长期 source-root 终态。
2. `Batch 1` 不再需要决定 runtime 仓库根判定与兼容窗口边界。
3. `Batch 1` 不再需要决定 recipes/logging/third-party 的最终 owner。
4. `Batch 1` 不再需要决定 residual ledger 的 schema 方向与 family 拆分原则。

## 4. 对 Batch 1 的硬约束

1. 只允许实现 selector/provenance、resolver/CLI gate、ledger schema/gate 三条并行流。
2. 不允许提前进入 Batch 2 bridge 或 Batch 3 flip。
3. 不允许修改 `build.ps1` / `test.ps1` 参数面、artifact 根或 `integration/reports` 根。
4. 不允许引入新的 compatibility alias 或新的过渡 owner。

## 5. 未执行事项

1. 本批次未修改任何代码或脚本。
2. 本批次未运行新的 build/test。
3. 本批次结论完全基于现有设计文档和静态事实冻结。
