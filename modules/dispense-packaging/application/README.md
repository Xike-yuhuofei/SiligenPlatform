# application

`modules/dispense-packaging/application/` 是 `M8` 对外暴露 packaging / validation consumer surface 的唯一入口。

- planning owner 实现收敛在 `PlanningAssemblyServices.cpp` 与
  `WorkflowPlanningAssemblyOperationsProvider.cpp`。
- `AuthorityPreviewAssemblyService` 与 `ExecutionAssemblyService` 的 public 签名
  已固定为 workflow DTO：
  `WorkflowAuthorityPreviewRequest -> WorkflowAuthorityPreviewArtifacts`、
  `WorkflowExecutionAssemblyRequest -> WorkflowExecutionAssemblyResult`。
- `WorkflowPlanningAssemblyTypes.h` 当前是 canonical DTO owner；
  `modules/workflow/application/services/dispensing/WorkflowPlanningAssemblyTypes.h`
  仅保留 compat forwarder。
- internal stage 类型已收回 `PlanningAssemblyServices.cpp` 本地实现，
  不再保留 `PlanningAssemblyTypes.h` 这类 public 头。
- 不再保留 `DispensePlanningFacade` 作为 live owner 或兼容入口。
- `siligen_dispense_packaging_application_public` 只导出 application headers /
  contracts 与 link-only implementation，不导出 valve residual，也不透传
  motion-planning application target。
- 规划算法实现不得回流到本目录；上游路径/轨迹规划分别由 `M6`、`M7` owner 提供。
