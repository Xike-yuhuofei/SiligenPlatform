# dispense-packaging contracts

`modules/dispense-packaging/contracts/` 承载 `M8 dispense-packaging` 的模块 owner 专属契约。

## 正式职责

- 定义 `ExecutionPackage` 及其校验语义。
- 定义 `PlanningArtifactExportRequest` 这类 `M8 -> M0/M9` 规划工件交接载荷。
- 明确 `ExecutionPackageBuilt` 与 `ExecutionPackageValidated` 的区别，不允许混用。
- 作为 `M8 -> M9` 交接的最小字段与状态边界说明。
- `DispenseTimingPlan` 为历史命名残留，不再在本目录新增同名 live 契约。

## 边界约束

- 仅放置 `M8 dispense-packaging` owner 专属契约，不放跨模块稳定公共契约。
- `shared/contracts/engineering/` 承载长期稳定公共 schema，本目录只保留 `M8` 特有组包与校验语义。
- `modules/dispense-packaging/domain/dispensing/` 与
  `modules/dispense-packaging/application/services/dispensing/` 是当前 canonical 契约实现面。
- `workflow` 与 `runtime-execution` 只能消费这些契约，不再自行定义同语义 request/result 组装逻辑。
