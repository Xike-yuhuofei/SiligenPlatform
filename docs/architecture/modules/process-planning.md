# process-planning

`M4 process-planning`

## 模块职责

- 承接 `S5`，把 `FeatureGraph` 映射为 `ProcessPlan`。

## Owner 对象

- `ProcessPlan`

## 输入契约

- `PlanProcess`
- `ReplanProcess`
- `SupersedeOwnedArtifact`

## 输出契约

- `ProcessPlanBuilt`
- `ProcessRuleRejected`
- `ArtifactSuperseded`

## 禁止越权边界

- 不得输出轨迹或设备指令。
- 不得直接查询设备实时门禁。

## 测试入口

- `S09` process rule mapping/process rejection
