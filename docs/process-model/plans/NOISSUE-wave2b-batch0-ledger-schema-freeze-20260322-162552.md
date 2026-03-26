# NOISSUE Wave 2B Batch 0 - Ledger Schema Freeze

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-162552`
- 输入：
  - `docs/process-model/plans/NOISSUE-wave2bC-design-20260322-155805.md`
  - `tools/migration/wave2b_residual_ledger.json`
  - `tools/migration/validate_wave2b_residuals.py`

## 1. 冻结目标

把 residual ledger 从当前粗粒度字符串 allowlist 冻结为可派工、可分阶段收紧的 schema 方向。

## 2. 当前基线

1. 当前 schema 只有 `id`、`pattern`、`allowed_files`。
2. 当前 validator 语义是“防新增 live 引用”，不是“表达 owner 与切换顺序”。
3. 当前 5 条 entry 中，至少有 2 条是混合条目，不能直接派工。

## 3. 冻结后的 schema 方向

后续实现的 ledger entry 至少应表达以下字段：

1. `id`
2. `family`
3. `pattern`
4. `type`
5. `owner`
6. `must_zero_before_cutover`
7. `allowed_files`

其中：

1. `family` 用于把同一 residual 家族的 live debt 与文档 debt 关联起来。
2. `type` 只允许以下枚举：
   - `runtime_build_debt`
   - `doc_boundary`
   - `historical_audit`
3. `owner` 必须引用 Batch 0 已冻结的 canonical owner，而不是开放文本。
4. `must_zero_before_cutover` 明确区分切换前必须清零和可后置收尾的条目。

## 4. 冻结的 family 拆分原则

1. `process-runtime-core-device-hal` 必须拆为：
   - `device-hal-logging-owner`
   - `device-hal-recipes-owner`
   - `device-hal-doc-boundary`
2. `device-adapters-third-party` 必须拆为：
   - `process-runtime-core-third-party-consumer`
   - `recipe-json-third-party-consumer`
   - `third-party-doc-boundary`
   - `hmi-vendor-provenance-doc`
3. `process-runtime-core-infrastructure-runtime` 维持文档边界 family。
4. HMI 两条 probe 维持 `historical_audit` family，不得误当 live blocker。

## 5. Gate 收紧顺序冻结

1. 先拆账显式化 owner，不删 allowlist。
2. 再把 live code/build 命中收敛到 0。
3. 再把文档 residual 归并到单一证据点。
4. 最后才允许移除 ledger 条目。

## 6. 后续实现约束

1. `Batch 1` 只允许实现 schema 扩展和 validator 能力增强，不允许直接删旧 entry。
2. `Batch 2` 允许完成 family 拆分，但仍不做最终清零判定。
3. `Batch 3` 只清必须在 cutover 前归零的 `runtime_build_debt`。
4. `Batch 4` 才清 `doc_boundary` 与 `historical_audit`。

## 7. 验收

1. 本批次是设计冻结，不涉及代码回滚。
2. 唯一验收标准是：后续实现者不需要再决定 ledger 应表达哪些维度、如何拆 family、如何排收紧顺序。
