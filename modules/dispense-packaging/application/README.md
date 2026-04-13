# application

`modules/dispense-packaging/application/` 是 `M8` 对外暴露 packaging / validation consumer surface 的唯一入口。

- planning owner 实现收敛在 `PlanningAssemblyServices.cpp`、
  `WorkflowPlanningAssemblyOperationsProvider.cpp`、
  `usecases/dispensing/PlanningUseCase.cpp` 与
  `usecases/dispensing/PlanningPortAdapters.cpp`。
- `PlanningArtifactExportAssemblyService` 只负责组装
  `domain/dispensing/contracts/PlanningArtifactExportRequest.h` 中定义的 canonical request，
  不再导出 application namespace request alias。
- `AuthorityPreviewAssemblyService` 与 `ExecutionAssemblyService` 的 public 签名
  已固定为 workflow DTO：
  `WorkflowAuthorityPreviewRequest -> WorkflowAuthorityPreviewArtifacts`、
  `WorkflowExecutionAssemblyRequest -> WorkflowExecutionAssemblyResult`。
- `WorkflowPlanningAssemblyTypes.h` 当前是 canonical DTO owner；
  `workflow` 不再保留 compat forwarder。
- internal stage 类型已收回 `PlanningAssemblyServices.cpp` 本地实现，
  不再保留 `PlanningAssemblyTypes.h` 这类 public 头。
- planning artifact export 的 consumer port / result contract 由
  `application/services/dispensing/PlanningArtifactExportPort.h` 持有；
  `runtime-execution/runtime/host` 仅实现 adapter。
- `PlanningUseCase` 当前直接暴露 `PrepareAuthorityPreview(...)` 与
  `AssembleExecutionFromAuthority(...)` 作为 runtime-execution 消费的最小 public seam，
  不再通过 workflow private/internal 头泄漏。
- 不再保留 `DispensePlanningFacade` 作为 live owner 或兼容入口。
- `siligen_dispense_packaging_application_public` 只导出 application headers /
  contracts 与 link-only implementation，不导出 valve residual，也不透传
  motion-planning application target。
- 规划算法实现不得回流到本目录；上游路径/轨迹规划分别由 `M6`、`M7` owner 提供。
