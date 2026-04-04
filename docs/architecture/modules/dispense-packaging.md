# dispense-packaging

`M8 dispense-packaging`

## 模块职责

- 承接 `S10-S11A-S11B`，组装 `ExecutionPackage`、`PlanningArtifactExportRequest`，
  维护 authority/binding 真值并完成离线校验。

## Owner 对象

- `ExecutionPackage`
- `PlanningArtifactExportRequest`
- `AuthorityTriggerLayout`
- `TriggerPlan`
- `DispensingPlan`

## 输入契约

- `AssembleExecutionPackage`
- `ValidateExecutionPackage`
- `BuildPlanningArtifactExportRequest`
- `SupersedeOwnedArtifact`

## workflow-facing seam

- `WorkflowPlanningAssemblyOperationsProvider`
- `WorkflowPlanningAssemblyTypes.h`
- `WorkflowDispensingProcessOperationsProvider`

## 输出契约

- `ExecutionPackageBuilt`
- `ExecutionPackageValidated`
- `ExecutionPackageRejected`
- `PlanningArtifactExportRequestBuilt`
- `OfflineValidationFailed`
- `ArtifactSuperseded`

## 禁止越权边界

- 不得重排 `ProcessPath`。
- 不得重建 `MotionPlan`。
- 不得把 `ExecutionPackageBuilt` 视为可执行。
- 不得在 `workflow` 或宿主层重建 `PlanningArtifactExportRequest`。
- 不得在宿主层直接 include 或构造 `domain/dispensing/DispensingProcessService`；运行时执行入口必须通过 `application/services/dispensing/WorkflowDispensingProcessOperationsProvider` 获取。

## 测试入口

- `S09` package-built/package-validated split
- `S09` offline validation failure
- `siligen_dispense_packaging_boundary_tests`：验证 `M8` owner 边界与 consumer include/target 约束
- `siligen_dispense_packaging_workflow_seam_tests`：验证 `WorkflowPlanningAssemblyOperationsProvider` 三段 seam 的 owner 语义
- workflow-facing seam 只允许暴露 `WorkflowPlanningAssemblyTypes.h`；`PlanningAssemblyTypes.h`
  不再作为跨模块契约面。

## Phase 4 Closeout 注意事项

- 当前 `Phase 4` closeout 以 `M8` owner 独立证据 + workflow/runtime host 回归为准，不以 `siligen_dispense_packaging_unit_tests` 全绿为前提。
- `PreviewSnapshotServiceTest.cpp` 对 `PreviewSnapshotPayload` 旧字段名的编译依赖是既有测试漂移；在未单独修复前，它只应作为任务外阻塞记录。
- `WorkflowPlanningAssemblyTypes.h` 已成为 workflow-facing seam 的稳定 DTO 面；
  `PlanningAssemblyTypes.h` 仅保留 `M8` 内部 stage 组装使用。
