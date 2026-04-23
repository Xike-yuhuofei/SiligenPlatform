# runtime-execution

`modules/runtime-execution/` 是 `M9 runtime-execution` 的 canonical owner 根，但本阶段 owner 范围已收紧为执行域与 host core，不再承担进程级 bootstrap 或非执行基础设施聚合。

## 当前 owner 范围

- 执行链 application public surface：消费已验证执行输入、驱动执行、收敛执行状态与失败归责。
- deterministic-path runtime 装配：通过 `MotionRuntimeAssemblyFactory` 消费 `modules/motion-planning/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.h` 提供的 M7 concrete seam，不再在 M9 本地持有路径构建与插补程序生成 owner。
- Python simulation export concrete：`application/runtime_execution/*.py` 持有 simulation input 的 runtime defaults、trigger CSV/投影、payload 组装、JSON 导出与 CLI concrete；schema authority 仍留在 `shared/contracts/engineering/`，geometry helper 仍由 `modules/dxf-geometry/` 提供。
- runtime contracts：仅保留 runtime-owned 执行态契约、motion/runtime bridge contracts、device/runtime consumer contracts；不再透传 configuration / task scheduler / event publisher 等 foreign-owner public surface。
- 跨模块稳定事件发布契约 `IEventPublisherPort` 现由 `shared/contracts/runtime` 持有；`runtime-execution` 只消费 `runtime/contracts/system/IEventPublisherPort.h`。
- runtime host core：事件桥接、任务调度桥接、执行期 diagnostics、执行期 motion/runtime provider、machine execution state owner、执行边界监控。

## 不再属于 M9 owner 的 live 事实

- 进程级 `BuildContainer(...)` bootstrap / composition root。
- `WorkspaceAssetPaths` 与进程启动期配置路径解析。
- 已退役历史管理持久化 wiring、runtime file storage、security 子系统、DXF / workflow / job-ingest 宿主装配。

这些职责已迁到 [`apps/runtime-service`](D:/Projects/SiligenSuite/apps/runtime-service/README.md) 的 app-local composition / infrastructure shell；`M9` 不再以 `siligen_runtime_host` 对外聚合这些能力。

## Canonical 入口

- 模块构建根：`modules/runtime-execution/CMakeLists.txt`
- application public：`modules/runtime-execution/application/CMakeLists.txt`
  target: `siligen_runtime_execution_application_public`
  runtime provider contract: `application/include/runtime_execution/application/services/motion/runtime/IMotionRuntimeServicesProvider.h`
  dispensing workflow seam:
  `application/include/runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h`
  与
  `application/include/runtime_execution/application/usecases/dispensing/WorkflowExecutionPort.h`
  模式/宿主接线回归哨兵：
  `tests/unit/dispensing/DispensingWorkflowModeResolutionTest.cpp`
- Python simulation export owner：`modules/runtime-execution/application/runtime_execution/`
  stable compat surface 仍通过 `engineering_data.contracts.simulation_input`、`engineering_data.cli.export_simulation_input` 与 `scripts/engineering-data/export_simulation_input.py` 暴露。
- runtime contracts：`modules/runtime-execution/contracts/runtime/CMakeLists.txt`
  target: `siligen_runtime_execution_runtime_contracts`
- host core：`modules/runtime-execution/runtime/host/CMakeLists.txt`
  target: `siligen_runtime_host`

## Host Core 边界

`siligen_runtime_host` 现在只保留：

- `runtime/events/*`
- `runtime/scheduling/*`
- `runtime/diagnostics/*`
- `runtime/motion/MotionRuntimeServicesProvider.*`
- `runtime/system/MachineExecutionStateStore.*`
- `runtime/system/MachineExecutionStateBackend.*`（直接实现 `IMachineExecutionStatePort`）
- `runtime/planning/PlanningArtifactExportPortAdapter.*`
- `services/motion/HardLimitMonitorService.*`
- `services/motion/SoftLimitMonitorService.*`

其中 machine execution state 的 canonical concrete 已收口为 `runtime/system/MachineExecutionStateStore.*` + `runtime/system/MachineExecutionStateBackend.*`。`runtime-execution` 不再依赖 workflow machine aggregate alias，也不再把 `DispenserModel` / `DispensingTask` 暴露为 live public/test surface。
`runtime/planning/PlanningArtifactExportPortAdapter.*` 仅实现
`modules/dispense-packaging/application/include/application/services/dispensing/PlanningArtifactExportPort.h`
定义的 owner port。

deterministic-path 的 `ProcessPath` 重建、轨迹规划与插补程序生成现由 `M7 motion-planning` 持有；`runtime-execution` 仅负责 runtime port 装配、tick 推进调用与执行态消费。

`siligen_runtime_host` 不再 `PUBLIC` 聚合 `job-ingest`、`workflow`、DXF adapter、host storage 或已退役持久化兼容层。

## App 边界

- `apps/runtime-service` 持有 runtime process bootstrap public surface：
  target: `siligen_runtime_process_bootstrap_public`
  include root: `runtime_process_bootstrap/*`
- `apps/planner-cli`、`apps/runtime-gateway`、`apps/runtime-service` 对 `BuildContainer(...)` / `WorkspaceAssetPaths` 的消费统一经 `runtime_process_bootstrap/*`。
- `modules/runtime-execution/runtime/host/` 已不再暴露 `ContainerBootstrap.h` 与 `runtime/configuration/WorkspaceAssetPaths.h` 旧入口；相关 process bootstrap public surface 已完全迁到 `apps/runtime-service/include/runtime_process_bootstrap/*`。
