# 冗余治理接口契约说明 v1.0.0

对应机器可读契约：`redundancy-interface.contract.v1.json`。

## 1. 协议范围

本契约用于 `packages/process-runtime-core` 内“冗余治理闭环”的接口边界，覆盖：

1. 代码实体登记
2. 静态/动态证据上报
3. 候选计算与查询
4. 工作流状态迁移
5. 决策日志追加

## 2. 版本策略

1. 当前协议标识：`siligen.process-runtime-core.redundancy/1.0`。
2. minor 版本仅允许追加可选字段或新增方法。
3. 删除字段、改写字段语义、调整状态迁移规则均属于破坏性变更，必须升级主版本。

## 3. 幂等语义

以下方法要求幂等：

1. `redundancy.entity.register`
2. `redundancy.evidence.static.report`
3. `redundancy.evidence.runtime.report`
4. `redundancy.decision.log.append`

幂等判定键：

1. `entity_id`
2. `evidence_id`
3. `decision_id`

## 4. 状态机约束

允许迁移集合：

1. `NEW -> REVIEWED`
2. `REVIEWED -> DEPRECATED`
3. `DEPRECATED -> OBSERVED`
4. `OBSERVED -> REMOVED`
5. `REMOVED -> ROLLED_BACK`
6. `DEPRECATED -> ROLLED_BACK`
7. `OBSERVED -> ROLLED_BACK`

禁止迁移示例：

1. `NEW -> REMOVED`
2. `REVIEWED -> REMOVED`
3. `ROLLED_BACK -> REMOVED`

违反约束返回 `7102 invalid_state_transition`。

## 5. 错误码约定

1. `7100 invalid_payload`：请求体不满足 JSON Schema。
2. `7101 entity_or_candidate_not_found`：关联主实体不存在。
3. `7102 invalid_state_transition`：不允许的状态迁移。
4. `7104 duplicate_decision_id`：日志去重冲突。
5. `7199 internal_error`：内部错误。

## 6. 与数据模型的一致性

1. 枚举与数据模型一致来源：`model/redundancy-enums.v1.json`。
2. 字段定义与数据模型一致来源：`model/redundancy-data-model.schema.v1.json`。
3. 任何契约变更必须同时更新上述两个文件。
