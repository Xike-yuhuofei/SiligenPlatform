# runtime-execution

`modules/runtime-execution/` 是 `M9 runtime-execution` 的 canonical owner 根，但本阶段 owner 范围已收紧为执行域与 host core，不再承担进程级 bootstrap 或非执行基础设施聚合。

## 当前 owner 范围

- 执行链 application public surface：消费已验证执行输入、驱动执行、收敛执行状态与失败归责。
- runtime contracts：执行域运行时契约、motion/runtime bridge contracts、device/runtime consumer contracts。
- runtime host core：事件桥接、任务调度桥接、执行期 diagnostics、执行期 motion/runtime provider、执行态适配与执行边界监控。

## 不再属于 M9 owner 的 live 事实

- 进程级 `BuildContainer(...)` bootstrap / composition root。
- `WorkspaceAssetPaths` 与进程启动期配置路径解析。
- recipe persistence wiring、runtime file storage、security 子系统、DXF / workflow / job-ingest 宿主装配。

这些职责已迁到 [`apps/runtime-service`](D:/Projects/SiligenSuite/apps/runtime-service/README.md) 的 app-local composition / infrastructure shell；`M9` 不再以 `siligen_runtime_host` 对外聚合这些能力。

## Canonical 入口

- 模块构建根：`modules/runtime-execution/CMakeLists.txt`
- application public：`modules/runtime-execution/application/CMakeLists.txt`
  target: `siligen_runtime_execution_application_public`
  runtime provider contract: `application/include/runtime_execution/application/services/motion/runtime/IMotionRuntimeServicesProvider.h`
  planning export contract: `application/include/runtime_execution/application/services/dispensing/PlanningArtifactExportPort.h`
- runtime contracts：`modules/runtime-execution/contracts/runtime/CMakeLists.txt`
  target: `siligen_runtime_execution_runtime_contracts`
- host core：`modules/runtime-execution/runtime/host/CMakeLists.txt`
  target: `siligen_runtime_host`

## Host Core 边界

`siligen_runtime_host` 现在只保留：

- `runtime/events/*`
- `runtime/scheduling/*`
- `runtime/diagnostics/*`
- `runtime/motion/WorkflowMotionRuntimeServicesProvider.*`
- `runtime/system/DispenserModelMachineExecutionStateBackend.*`
  app-facing neutral alias: `runtime/system/WorkflowMachineExecutionStateBackend.h`
- `runtime/system/LegacyMachineExecutionStateAdapter.*`
- `runtime/planning/PlanningArtifactExportPortAdapter.*`
- `services/motion/HardLimitMonitorService.*`
- `services/motion/SoftLimitMonitorService.*`

`siligen_runtime_host` 不再 `PUBLIC` 聚合 `job-ingest`、`workflow`、`workflow_recipe`、DXF adapter、host storage 或 recipe persistence。

## App 边界

- `apps/runtime-service` 持有 runtime process bootstrap public surface：
  target: `siligen_runtime_process_bootstrap_public`
  include root: `runtime_process_bootstrap/*`
- `apps/planner-cli`、`apps/runtime-gateway`、`apps/runtime-service` 对 `BuildContainer(...)` / `WorkspaceAssetPaths` 的消费统一经 `runtime_process_bootstrap/*`。
- `modules/runtime-execution/runtime/host/ContainerBootstrap.h` 与 `modules/runtime-execution/runtime/host/runtime/configuration/WorkspaceAssetPaths.h` 只保留 deprecated forwarder 兼容壳，不再承载 live owner 实现。
