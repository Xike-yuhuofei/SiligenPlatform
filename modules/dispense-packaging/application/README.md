# application

`modules/dispense-packaging/application/` 是 `M8` 对外暴露 packaging / validation consumer surface 的唯一入口。

- `PlanningAssemblyServices.cpp` 当前承接 planning assembly 的 canonical 入口；
  具体组装细节已进一步收拢到
  `PlanningAssemblyCommon.cpp` /
  `PlanningAssemblyAuthorityArtifacts.cpp` /
  `PlanningAssemblyExecutionArtifacts.cpp` /
  `PlanningAssemblyExecutionInterpolation.cpp` /
  `PlanningAssemblyExecutionBinding.cpp` /
  `PlanningAssemblyExecutionPackaging.cpp` 这些 module-local 私有编译单元，并挂到
  module-local `siligen_dispense_packaging_application_planning`。
- canonical application target 当前保留
  `WorkflowPlanningAssemblyOperationsProvider.cpp`、
  `PlanningArtifactExportAssemblyService.cpp` 与 `PreviewSnapshotService.cpp`。
- `PlanningArtifactExportAssemblyService` 只负责组装
  `domain/dispensing/contracts/PlanningArtifactExportRequest.h` 中定义的 canonical request，
  不再导出 application namespace request alias。
- `AuthorityPreviewAssemblyService` 与 `ExecutionAssemblyService` 的 public 签名
  已固定为 workflow DTO：
  `WorkflowAuthorityPreviewRequest -> WorkflowAuthorityPreviewArtifacts`、
  `WorkflowExecutionAssemblyRequest -> WorkflowExecutionAssemblyResult`。
- `WorkflowPlanningAssemblyTypes.h` 当前是 canonical DTO owner；
  `modules/workflow/application/services/dispensing/WorkflowPlanningAssemblyTypes.h`
  仅保留 compat forwarder。
- internal stage 类型与 planning 细节已收回
  `PlanningAssemblyServices.cpp` 与对应 private planning TUs，
  不再保留 `PlanningAssemblyTypes.h` 这类 public 头。
- planning artifact export 的 consumer port / result contract 当前由
  `runtime-execution/application` owner；本目录不再重复声明该稳定 seam。
- 不再保留 `DispensePlanningFacade` 作为 live owner 或兼容入口。
- `siligen_dispense_packaging_application_public` 只导出 application headers /
  contracts 与 link-only `siligen_dispense_packaging_application`；planning concrete
  只能经 `siligen_dispense_packaging_application_planning` 这个 module-local bridge
  由 application target 私有带入，不导出 valve residual，也不直接透传
  motion-planning application target。
- 规划算法实现不得回流到本目录；上游路径/轨迹规划分别由 `M6`、`M7` owner 提供。
