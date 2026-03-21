# process-runtime-core 冗余治理冻结文档

当前目录用于沉淀 `packages/process-runtime-core` 在“冗余代码治理”上的冻结产物。

## 当前冻结版本

- 数据模型：`v1.0.0`（冻结日期：`2026-03-21`）
- 接口契约：`v1.0.0`（冻结日期：`2026-03-21`）
- 实体 ID 规范：`v1.0.0`

## 文档索引

- Day 1 边界与实体盘点：`day1-entity-boundary-freeze.md`
- 枚举字典：`model/redundancy-enums.v1.json`
- 数据模型 Schema：`model/redundancy-data-model.schema.v1.json`
- 实体 ID 生成规范：`model/entity-id-spec.v1.md`
- 接口契约（机器可读）：`contracts/redundancy-interface.contract.v1.json`
- 接口契约（说明文档）：`contracts/redundancy-interface.v1.md`
- 里程碑冻结记录：`milestones/m1-data-model-interface-freeze-2026-03-21.md`
- M2 实施进展：`milestones/m2-implementation-progress-2026-03-21.md`

## 变更控制

1. 冻结版本仅允许追加兼容字段，不允许删除或改写现有字段语义。
2. 任何破坏性变更必须升级主版本，并新增冻结记录。
3. 规则、枚举、状态机变更必须同步更新机器可读契约与说明文档。
