# workflow

`modules/workflow/` 是 `M0 workflow` 的 canonical owner 入口。

## Owner 范围

- 工作流编排与阶段推进语义
- `WorkflowRun / StageTransitionRecord / RollbackDecision` owner 事实
- 阶段失败、回退决策与归档握手编排

## 当前边界真值

- `workflow` 只承载 `M0` 编排职责，不再承载 recipe owner、runtime concrete owner 或跨模块稳定事件契约 owner。
- recipe family 的 canonical owner 已迁到 [`modules/recipe-lifecycle`](D:/Projects/SiligenSuite/modules/recipe-lifecycle/README.md)。
- recipe JSON serialization canonical utility 已收口到 `recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h`；`workflow` 不再导出 serializer surface。
- 跨模块稳定事件发布契约 `IEventPublisherPort` 的 canonical owner 已迁到 [`shared/contracts/runtime`](D:/Projects/SiligenSuite/shared/contracts/runtime/README.md)，代码统一从 `runtime/contracts/system/IEventPublisherPort.h` 引入。
- generic diagnostics sink 的 canonical owner 已收口到 `trace_diagnostics/contracts/{IDiagnosticsPort,DiagnosticTypes}.h`；`workflow/domain/include/domain/diagnostics/**` 旧 public headers 已删除。
- hardware-test diagnostics contracts 当前 live landing 位于 `apps/runtime-service/include/runtime_process_bootstrap/diagnostics/**`，属于 app-local quarantine surface，不属于 `workflow` owner。
- runtime service 装配 concrete 与 planning 工件落盘 concrete 固定由 `modules/runtime-execution/` 或 `apps/runtime-service/` 承接；`workflow` 只保留端口、请求/结果与 orchestration call site。
- 代码冗余治理 `Candidate/Evidence/DecisionLog` 语义不属于 `workflow` rollback；相关 `recovery-control / redundancy` surface 已退出 canonical build/test bundle。
- `application/` 当前只保留 `commands/queries/facade` 的 headers-only M0 skeleton；
  planning / execution foreign surface 已迁回 `dispense-packaging` 与 `runtime-execution`，且不再进入模块根 canonical bundle。
- `application/CMakeLists.txt` 仅作为模块根 canonical bundle 的内部自校验面；
  `modules/workflow/CMakeLists.txt` 会显式纳入该自校验，但不会再导出 `siligen_workflow_application_headers`。
- `domain/` 现只保留 `include/workflow/domain/**` 这组 M0 owner model；旧 `domain/domain/**`、`domain/include/domain/**` 与 `motion-core/**` 已退出。

## 目录职责

- `contracts/`：`M0` 专属稳定契约
- `services/`：M0 lifecycle / rollback / archive / projection skeleton
- `application/`：`commands/queries/facade` headers-only skeleton
- `runtime/`：M0 command/event/archive orchestration skeleton
- `domain/`：只暴露 `include/workflow/domain/**` 的 M0 owner model
- `adapters/`：workflow owner adapter root；当前仅保留 `persistence / messaging / projection_store` shell roots
- `tests/`：模块级验证入口；`canonical` 只承载 workflow owner 测试，`integration` 与 `regression` 仅保留 M0 登记面
- shell-only 保留目录：当前仅 `examples` 目录保留说明与空壳，不承载 live code；`services/` 已实化为 M0 skeleton

## 禁止事项

- 不允许把 recipe、motion execution、system concrete、valve concrete 或新的 engineering concrete 回流到 `workflow`。
- 不允许新增 compat / bridge / facade shell 来替代真实 owner 迁移。
- 不允许把 live 代码写回 shell-only 目录。
- 不允许在 `tests/canonical/` 之外新增 source-bearing workflow 测试。

## 迁移来源

- `legacy-archive` 中已归档的历史 workflow application/domain 快照
- `legacy-archive` 中已归档的历史 process-runtime-core 相关快照
