# application

runtime-execution 的 facade、handler、query/command 入口应收敛到此目录。

当前规则：

- app / host consumer 统一从 `application/include/runtime_execution/application/**` 引入 M9 执行链 public headers。
- `usecases/dispensing/DispensingExecutionUseCase*.cpp` 的真实编译 owner 位于 `modules/runtime-execution/application/`，不得再由 `workflow/application` 回编。
- `DispensingExecutionUseCase` 的 canonical API 只接受已规划执行输入；DXF/PB 准备与 planning facade 调用必须经 `workflow/application/**` 过渡编排入口完成。
- 上传归 M1、规划与编排归 M0；这些 owner 头必须分别从 `job_ingest/application/**`、`workflow/application/**` 引入。
- runtime-workflow compat 只允许保留 `domain` 级 residual bridge，禁止恢复 broad runtime compat target。

回归检查清单：

- 禁止在 `modules/runtime-execution/application/**` 中重新包含 `application/services/dispensing/DispensePlanningFacade.h`。
- 禁止在 `modules/runtime-execution/application/**` 中重新包含 `DxfPbPreparationService`。
- 禁止在 `DispensingExecutionUseCase*.cpp` 中恢复 `planner_->Plan(...)`、`BuildPlanRequest(...)`、`EnsurePbReady(...)`。
- legacy DXF 请求只能经 `workflow/application/services/dispensing/DispensingExecutionCompatibilityService` 与 `DispensingExecutionWorkflowUseCase` 进入 M9 canonical execution API。
