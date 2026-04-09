# application

runtime-execution 的 facade、handler、query/command 入口应收敛到此目录。

当前规则：

- app / host consumer 统一从 `application/include/runtime_execution/application/**` 引入 M9 执行链 public headers。
- job/session/control 当前 live owner 仍位于 `application/usecases/**`；在 owner 定稿迁移前，不得把这组职责误记为 `services/` 或 `runtime/` 已接管。
- `usecases/dispensing/DispensingExecutionUseCase*.cpp` 的真实编译 owner 位于 `modules/runtime-execution/application/`，不得再由 `workflow/application` 回编。
- `DispensingExecutionUseCase` 的 canonical API 只接受已规划执行输入；DXF/PB 准备与 planning facade 调用必须经 `workflow/application/**` 过渡编排入口完成。
- `DispensingExecutionUseCase` 的跨模块结果类型统一为中立命名 `DispensingExecutionResult`；不得再恢复 `DispensingMVP*` public 语义。
- 上传归 M1、规划与编排归 M0；这些 owner 头必须分别从 `job_ingest/application/**`、`workflow/application/**` 引入。
- runtime-workflow compat 只允许保留 `domain` 级 residual bridge，禁止恢复 broad runtime compat target。

回归检查清单：

- 禁止在 `modules/runtime-execution/application/**` 中重新引入 legacy planning facade 或直接依赖 M8 planning concrete。
- 禁止在 `modules/runtime-execution/application/**` 中重新包含 `DxfPbPreparationService`。
- 禁止在 `DispensingExecutionUseCase*.cpp` 中恢复 `planner_->Plan(...)`、`BuildPlanRequest(...)`、`EnsurePbReady(...)`。
- 禁止恢复 `DispensingExecutionWorkflowUseCase`、`DispensingExecutionCompatibilityService` 或 `SetLegacyExecutionForwarders(...)` 这类 legacy execute adapter/wiring。
- `DispensingExecutionUseCase` 对跨模块公开的入口只允许保留 canonical `DispensingExecutionRequest` 与 job API；内部 task 状态机可继续作为 job engine 使用，但不得重新暴露到 gateway/workflow public contract。
- internal task runtime 测试必须下沉到 `modules/runtime-execution/**` 自有 test target；跨模块 consumer 不得通过 public header 探测 task 私有状态。
