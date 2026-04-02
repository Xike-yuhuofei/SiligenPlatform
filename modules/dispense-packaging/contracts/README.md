# dispense-packaging contracts

`modules/dispense-packaging/contracts/` 承载 `M8 dispense-packaging` 的模块专属契约。

## 正式职责

- 当前稳定公开契约以 `ExecutionPackage` / `ExecutionPackageBuilt` / `ExecutionPackageValidated` 为主。
- `DispenseTimingPlan` 的独立公开 contract surface 尚未在 `contracts/include/` 中完全冻结，本阶段不把它误写成已完成事实。
- 明确 `ExecutionPackageBuilt` 与 `ExecutionPackageValidated` 的区别，不允许混用。
- 作为 `M8 -> M9` 交接的最小字段与状态边界说明。

## 边界约束

- 仅放置 `M8 dispense-packaging` owner 专属契约，不放跨模块稳定公共契约。
- `shared/contracts/engineering/` 承载长期稳定公共 schema，本目录只保留 `M8` 特有组包与校验语义。
- `modules/workflow/application/usecases/dispensing/` 是当前 consumer / orchestration 面，不是 `M8` 契约 owner 面。
- 当前契约事实以 `contracts/include/domain/dispensing/contracts/` 和模块内直接 consumer 为准。
