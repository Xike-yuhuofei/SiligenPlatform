# workflow

`modules/workflow/` 是 `M0 workflow` 的 canonical owner 入口。

## Owner 范围

- 工作流编排与阶段推进语义
- 规划链（`M4-M8`）触发与编排边界
- 流程状态、阶段失败与调度决策事实
- recovery governance 的最小 metadata、ports 与 scoring 逻辑

## 当前边界真值

- `workflow` 只承载 `M0` 编排职责，不再承载 recipe owner、runtime concrete owner 或跨模块稳定事件契约 owner。
- recipe family 的 canonical owner 已迁到 [`modules/recipe-lifecycle`](D:/Projects/SiligenSuite/modules/recipe-lifecycle/README.md)。
- recipe JSON serialization canonical utility 已收口到 `recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h`；`workflow` 不再导出 serializer surface。
- 跨模块稳定事件发布契约 `IEventPublisherPort` 的 canonical owner 已迁到 [`shared/contracts/runtime`](D:/Projects/SiligenSuite/shared/contracts/runtime/README.md)，代码统一从 `runtime/contracts/system/IEventPublisherPort.h` 引入。
- generic diagnostics sink 的 canonical owner 已收口到 `trace_diagnostics/contracts/{IDiagnosticsPort,DiagnosticTypes}.h`；`workflow/domain/include/domain/diagnostics/**` 旧 public headers 已删除。
- hardware-test diagnostics contracts 当前 live landing 位于 `apps/runtime-service/include/runtime_process_bootstrap/diagnostics/**`，属于 app-local quarantine surface，不属于 `workflow` owner。
- runtime service 装配 concrete 与 planning 工件落盘 concrete 固定由 `modules/runtime-execution/` 或 `apps/runtime-service/` 承接；`workflow` 只保留端口、请求/结果与 orchestration call site。
- `application/include/`、`domain/include/`、`adapters/include/` 仍是 workflow 当前 public roots，但它们还没有完全收口，不能被当作 foreign owner surface 的长期分发器。

## 目录职责

- `contracts/`：`M0` 专属稳定契约
- `application/`：canonical application 入口，终态只应保留 `planning-trigger/`、`phase-control/`、`recovery-control/`
- `domain/`：workflow 自有领域事实与少量仍待退出的 bridge residue
- `adapters/`：workflow owner ports 的最小 concrete adapter
- `tests/`：模块级验证入口；`tests/canonical/` 是唯一 source-bearing workflow 测试承载面
- shell-only 保留目录：仅保留说明与空壳，不承载 live code

## 当前 live residue

- `modules/workflow/domain/domain/dispensing/**` 仍保留一组 dormant foreign files / compat 痕迹，但 workflow live build graph 已不再编译 `siligen_triggering` 或 `PositionTriggerController.cpp`。
- `modules/workflow/domain/include/domain/{motion,safety}/**` 仍保留一组 compat public headers，尚未完全收口到 canonical owner surface。
- `siligen_workflow_application_headers` 仍透传部分 foreign owner headers/contracts，application public surface 还没有完全瘦身。
- `tests/canonical/` 仍直接依赖部分 foreign owner targets，测试边界尚未完全收口。

## 禁止事项

- 不允许把 recipe、motion execution、system concrete、valve concrete 或新的 engineering concrete 回流到 `workflow`。
- 不允许新增 compat / bridge / facade shell 来替代真实 owner 迁移。
- 不允许把 live 代码写回 shell-only 目录。
- 不允许在 `tests/canonical/` 之外新增 source-bearing workflow 测试。

## 迁移来源

- `legacy-archive` 中已归档的历史 workflow application/domain 快照
- `legacy-archive` 中已归档的历史 process-runtime-core 相关快照
