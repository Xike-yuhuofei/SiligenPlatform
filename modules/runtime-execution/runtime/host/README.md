# runtime/host

`modules/runtime-execution/runtime/host/` 现在是 `M9 host core`，不再是广义宿主总装包。

## owner 范围

- 执行域事件发布桥接：`runtime/events/*`
- 执行域任务调度桥接：`runtime/scheduling/*`
- 执行域 diagnostics 适配：`runtime/diagnostics/*`
- 执行域 motion/runtime provider：`runtime/motion/MotionRuntimeServicesProvider.*`
  - canonical provider contract：`runtime_execution/application/services/motion/runtime/IMotionRuntimeServicesProvider.h`
- machine execution state owner 与 planning artifact 导出桥：`runtime/system/*`、`runtime/planning/*`
  - machine execution state store owner：`runtime/system/MachineExecutionStateStore.*`
  - machine execution state backend owner：`runtime/system/MachineExecutionStateBackend.*`（直接实现 `runtime_execution/contracts/system/IMachineExecutionStatePort.h`）
  - machine execution state concrete 已完全留在 `runtime/system/*`，不再通过 workflow / coordinate-alignment 暴露 `DispenserModel` alias
  - consumed planning export port：
    `modules/dispense-packaging/application/include/application/services/dispensing/PlanningArtifactExportPort.h`
- 执行期硬限位 / 软限位监控：`services/motion/*`

## 已迁出到 app-local shell

以下资产不再由 `M9` live 编译或 owner 论证：

- `ContainerBootstrap.*`
- `bootstrap/InfrastructureBindings*`
- `container/ApplicationContainer*`
- `factories/InfrastructureAdapterFactory.*`
- `runtime/configuration/*`
- `runtime/storage/files/*`
- `security/**/*`

其中 bootstrap / container / infrastructure 相关文件已迁到 [`apps/runtime-service`](D:/Projects/SiligenSuite/apps/runtime-service/README.md) 的 app-local 目录；
历史管理持久化面已在 TASK-167 中正式退役，不再保留 `runtime/recipes/*` 的 app-local 替代目录。

## 目标与依赖

- host core target：`siligen_runtime_host`
- 只暴露执行域所需最小依赖：
  - `siligen_runtime_execution_application_public`
  - `siligen_runtime_execution_runtime_contracts`
  - `siligen_trace_diagnostics_contracts_public`
  - `siligen_device_contracts`
  - `siligen_shared`
- 不再 `PUBLIC` 聚合 `job-ingest`、`workflow`、历史管理相关 target、DXF adapter、host storage、已退役持久化兼容层。

## 已摘除旧入口

- `ContainerBootstrap.h`
- `runtime/configuration/WorkspaceAssetPaths.h`

这两个模块内旧路径头已从 `runtime-execution/runtime/host` public surface 删除；真实 process bootstrap public surface 位于 `apps/runtime-service/include/runtime_process_bootstrap/*`。

`runtime-host` 不再依赖 `workflow/application/CMakeLists.txt` 扩散 `../../runtime-execution/application/include`；motion runtime provider 的 canonical consumer surface 固定为 `runtime_execution/application/*`，planning export 仅消费 `dispense-packaging/application` 提供的 owner port。
