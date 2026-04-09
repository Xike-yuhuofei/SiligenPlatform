# runtime-service

`apps/runtime-service/` 是运行时进程的 app-local composition shell。它负责进程启动、容器装配与 host-local infrastructure 组合，但不新增业务 owner。

## 目录职责

- `bootstrap/`：`BuildContainer(...)` 与 runtime bootstrap 入口实现
- `container/`：`ApplicationContainer*` 进程级组合根
- `factories/`：基础设施适配器工厂
- `runtime/configuration/`：配置解析、`WorkspaceAssetPaths`、interlock/config validator
- `runtime/recipes/`：recipe repository / file storage / schema provider 的 app-local wiring
- `runtime/status/`：runtime status export assembler；透传 supervision snapshot authority，并追加只读 enrichment
- `runtime/storage/files/`：本地文件存储适配
- `runtime/supervision/`：runtime supervision 的 app-local backend / sync wiring
- `security/`：安全相关 host-local infrastructure
- `include/runtime_process_bootstrap/`：canonical public surface
- `tests/`：bootstrap / infra 的 app-local tests

## Public Surface

- public target：`siligen_runtime_process_bootstrap_public`
- implementation target：`siligen_runtime_process_bootstrap`
- canonical include root：`runtime_process_bootstrap/*`
- 当前稳定入口：
  - `runtime_process_bootstrap/ContainerBootstrap.h`
  - `runtime_process_bootstrap/WorkspaceAssetPaths.h`
  - `runtime_process_bootstrap/diagnostics/{aggregates,ports,value-objects}/*`（hardware-test diagnostics app-local quarantine surface）
  - `runtime_process_bootstrap/storage/ports/IFileStoragePort.h`（upload storage app-local quarantine surface）

`runtime_process_bootstrap/*` 当前承载 bootstrap 入口、hardware-test diagnostics 与 upload storage 的 app-local quarantine surface；recipe serializer 的 canonical public surface 来自 `recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h`，不在本目录下暴露。

## 边界约束

- recipe 规则 owner 与 serializer owner 已迁到 [`modules/recipe-lifecycle`](D:/Projects/SiligenSuite/modules/recipe-lifecycle/README.md)；`runtime-service` 只消费其 public surface，不持有 recipe 规则或 serializer owner。
- 跨模块稳定事件发布契约统一来自 [`shared/contracts/runtime`](D:/Projects/SiligenSuite/shared/contracts/runtime/README.md)，代码统一从 `runtime/contracts/system/IEventPublisherPort.h` 引入。
- generic diagnostics sink 统一来自 `modules/trace-diagnostics/contracts/include/trace_diagnostics/contracts/{IDiagnosticsPort,DiagnosticTypes}.h`；本目录下的 `runtime_process_bootstrap/diagnostics/**` 仅承接 hardware-test diagnostics 的 app-local bootstrap surface。
- `runtime_process_bootstrap/storage/ports/IFileStoragePort.h` 只承接 runtime-service host-local upload storage wiring；不得再把该 seam 放回 `modules/process-planning/**` 或 `shared/**`。
- `modules/runtime-execution/runtime/host` 只保留 host core；本目录只负责 app-local composition / infrastructure wiring。
- `workflow`、`recipe-lifecycle`、`runtime-execution`、`job-ingest`、`dxf-geometry` 等模块能力只允许通过各自 canonical public surface 接入，不得再回退到旧 workflow compat path。

## 调用方

以下入口统一通过 `siligen_runtime_process_bootstrap_public` 消费 bootstrap public surface：

- `apps/runtime-service/main.cpp`
- `apps/planner-cli/main.cpp`
- `apps/runtime-gateway/main.cpp`

## 启动脚本

- 脚本：`apps/runtime-service/run.ps1`
- 默认 machine config：`config/machine/machine_config.ini`
- 默认 vendor 目录：`modules/runtime-execution/adapters/device/vendor/multicard`
- 默认日志参数：`logs/control_runtime.log`

示例：

```powershell
./apps/runtime-service/run.ps1 -BuildConfig Debug
./apps/runtime-service/run.ps1 -ConfigPath config/machine/machine_config.ini -VendorDir D:\Vendor\MultiCard
./apps/runtime-service/run.ps1 -DryRun
```
