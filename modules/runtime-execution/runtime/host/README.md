# runtime/host

`runtime/host` 当前仍是运行时宿主总装包；第二阶段目标不是继续扩张，而是把它收缩为执行域 host core。

## 第二阶段目标

- `M9` 只保留执行会话管理、任务调度桥接、恢复逻辑、执行域必要 diagnostics 适配。
- 进程级 container / bootstrap / 上游模块装配迁移到 `apps/runtime-service`。
- recipe persistence、config/storage 文件适配、security 子树迁出 `M9 runtime-execution`。

## 文件级归类

### 保留在 M9 host core

- `runtime/events/InMemoryEventPublisherAdapter.*`
- `runtime/scheduling/TaskSchedulerAdapter.*`
- `runtime/diagnostics/DiagnosticsPortAdapter.*`
- `services/motion/HardLimitMonitorService.*`
- `services/motion/SoftLimitMonitorService.*`

说明：这些资产直接服务于执行域事件桥接、任务调度或执行时 diagnostics，不承担 broad app 装配 owner 责任。

### 迁出到 `apps/runtime-service`

- `ContainerBootstrap.*`
- `bootstrap/InfrastructureBindings*`
- `container/ApplicationContainer.*`
- `container/ApplicationContainer.System.*`
- `container/ApplicationContainer.Motion.*`
- `container/ApplicationContainer.Dispensing.*`
- `container/ApplicationContainer.Recipes.*`
- `factories/InfrastructureAdapterFactory.*`

说明：这些文件的核心职责是进程级 wiring、依赖解析与 broad host 装配，不应继续由 `M9` 作为 owner 聚合。

### 迁出到非 M9 owner / 独立基础设施层

- `runtime/recipes/*`
- `runtime/configuration/*`
- `runtime/storage/files/*`
- `security/**/*`

说明：这些资产分别属于 recipe persistence、配置/文件存储适配和安全子系统，不应继续绑定为执行域 owner 事实。

## 目标依赖图

- 第二阶段完成后，`siligen_runtime_host` 收缩为新的执行域 host core target，只暴露执行域所需最小依赖。
- `siligen_runtime_host` 不再 `PUBLIC` 聚合：
  - `siligen_job_ingest_application_public`
  - `siligen_workflow_application_public`
  - `siligen_workflow_recipe_application_public`
- `apps/runtime-service` 显式装配上游模块与 host core：
  - app 负责 wiring `job_ingest`、`workflow`、`workflow_recipe`
  - M9 只提供 `runtime_execution_application_public` 与 host core

## 实施顺序

1. 先提取新的 host core target，只包含事件、调度、diagnostics、执行期 monitor。
2. 再把 `ContainerBootstrap`、`InfrastructureBindingsBuilder`、`ApplicationContainer.*` 迁到 `apps/runtime-service`。
3. 最后移出 `runtime/recipes`、`runtime/configuration`、`runtime/storage` 与 `security` 子树，并更新依赖图。

## 第二阶段不变项

- 不改外部协议。
- 不改任务状态语义。
- 不引入新的执行结果持久化 schema。
- 仍保留 `workflow/application` 作为 legacy DXF compatibility owner，host 拆分不回退第一阶段边界。
