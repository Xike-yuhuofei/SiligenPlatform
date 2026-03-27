# dispense-packaging

`M8 dispense-packaging`

## 模块职责

- 承接 `S10-S11A-S11B`，生成 `DispenseTimingPlan`、组装 `ExecutionPackage` 并做离线校验。

## Owner 对象

- `DispenseTimingPlan`
- `ExecutionPackage`

## 输入契约

- `BuildDispenseTimingPlan`
- `AssembleExecutionPackage`
- `ValidateExecutionPackage`
- `SupersedeOwnedArtifact`

## 输出契约

- `DispenseTimingPlanBuilt`
- `DispenseTimingPlanRejected`
- `ExecutionPackageBuilt`
- `ExecutionPackageValidated`
- `ExecutionPackageRejected`
- `OfflineValidationFailed`
- `ArtifactSuperseded`

## 禁止越权边界

- 不得重排 `ProcessPath`。
- 不得重建 `MotionPlan`。
- 不得把 `ExecutionPackageBuilt` 视为可执行。

## 测试入口

- `S09` package-built/package-validated split
- `S09` offline validation failure
