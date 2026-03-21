# M1 里程碑冻结记录：数据模型 + 接口契约

## 基本信息

- 里程碑：`M1`
- 冻结日期：`2026-03-21`
- 作用范围：`packages/process-runtime-core`
- 负责人：`Codex`

## 冻结产物

1. Day 1 盘点与边界：`../day1-entity-boundary-freeze.md`
2. 枚举字典：`../model/redundancy-enums.v1.json`
3. 数据模型：`../model/redundancy-data-model.schema.v1.json`
4. 实体 ID 规范：`../model/entity-id-spec.v1.md`
5. 接口契约：`../contracts/redundancy-interface.contract.v1.json`
6. 契约说明：`../contracts/redundancy-interface.v1.md`

## 冻结决策

1. 模型版本冻结为 `v1.0.0`，主实体保持 5 类，不新增第 6 类。
2. 接口版本冻结为 `v1.0.0`，方法集冻结为 7 个。
3. 状态机冻结为 `NEW/REVIEWED/DEPRECATED/OBSERVED/REMOVED/ROLLED_BACK`。
4. 评分结果口径冻结为 `redundancy_score` 与 `deletion_risk_score` 双分值。

## 一致性检查结果

1. 枚举字典与数据模型枚举一致。
2. 接口入参与模型字段命名一致。
3. 状态机迁移约束已在契约说明中固定。

## 后续变更门禁

1. 冻结后若需修改字段语义，必须升级主版本并新增冻结记录。
2. 新增方法或可选字段需保持向后兼容，并更新契约说明。
3. 任何删除动作必须经过 `DEPRECATED -> OBSERVED -> REMOVED` 流程，不可跳步。

## 下一里程碑输入

1. 依据冻结契约实现最小采集管线（静态证据与动态证据入库）。
2. 实现候选计算任务与评审查询入口。
3. 实现状态迁移与决策日志持久化。
