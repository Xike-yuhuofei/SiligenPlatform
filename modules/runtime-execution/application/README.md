# application

runtime-execution 的 facade、handler、query/command 入口应收敛到此目录。

当前规则：

- app / host consumer 统一从 `application/include/runtime_execution/application/**` 引入 M9 执行链 public headers。
- `application/runtime_execution/*.py` 是 simulation-input runtime concrete owner；负责 runtime defaults、trigger 映射、payload 组装和 `export_simulation_input` CLI `main()`。
- 上述 Python concrete 只消费 `engineering_data.processing.simulation_geometry` 提供的 geometry helper；不得在本模块内重定义 `PathBundle` geometry conversion 或 `shared/contracts/engineering` schema authority。
- 稳定 import / CLI 名称继续由 `dxf-geometry` compat shell 与 workspace wrapper 暴露；本模块不新增第二套 stable public package 名。
- job/session/control 当前 live owner 仍位于 `application/usecases/**`；在 owner 定稿迁移前，不得把这组职责误记为 `services/` 或 `runtime/` 已接管。
- `usecases/dispensing/DispensingExecutionUseCase*.cpp` 的真实编译 owner 位于 `modules/runtime-execution/application/`，不得再由 `workflow/application` 回编。
- `DispensingExecutionUseCase` 的 canonical API 只接受已规划执行输入；DXF/PB 准备与 planning facade 调用必须经 `workflow/application/**` 过渡编排入口完成。
- `DispensingExecutionUseCase` 的跨模块结果类型统一为中立命名 `DispensingExecutionResult`；不得再恢复 `DispensingMVP*` public 语义。
- 上传归 M1、规划与编排归 M0；这些 owner 头必须分别从 `job_ingest/application/**`、`workflow/application/**` 引入。
- runtime-workflow 不得恢复 broad compat target；若仍存在跨模块 residual bridge，只允许停留在非 public 的 `domain` 级残留面，且不得回流为 M9 live truth。
- deterministic-path 的 concrete owner 已迁至 `modules/motion-planning/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.*`；本目录只允许通过 `MotionRuntimeAssemblyFactory` 装配并消费该 seam，不得再在 `runtime-execution/application` 内本地重建 `ProcessPath`、时间规划结果或插补程序。

回归检查清单：

- 禁止在 `modules/runtime-execution/application/**` 中重新引入 legacy planning facade 或直接依赖 M8 planning concrete。
- 禁止恢复 `usecases/motion/trajectory/DeterministicPathExecutionUseCase.*` 到 `modules/runtime-execution/application/**`。
- 禁止在 `modules/runtime-execution/application/**` 中重新包含 `DxfPbPreparationService`。
- 禁止在 `DispensingExecutionUseCase*.cpp` 中恢复 `planner_->Plan(...)`、`BuildPlanRequest(...)`、`EnsurePbReady(...)`。
- 禁止恢复 `DispensingExecutionWorkflowUseCase`、`DispensingExecutionCompatibilityService` 或 `SetLegacyExecutionForwarders(...)` 这类 legacy execute adapter/wiring。
- `DispensingExecutionUseCase` 对跨模块公开的入口只允许保留 canonical `DispensingExecutionRequest` 与 job API；内部 task 状态机可继续作为 job engine 使用，但不得重新暴露到 gateway/workflow public contract。
- internal task runtime 测试必须下沉到 `modules/runtime-execution/**` 自有 test target；跨模块 consumer 不得通过 public header 探测 task 私有状态。
