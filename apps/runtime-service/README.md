# runtime-service

`apps/runtime-service/` 是运行时进程的 app-local composition shell。它负责进程启动、容器装配与 host-local infrastructure 组合，但不新增业务 owner。

## 目录职责

- `bootstrap/`：`BuildContainer(...)` 与 runtime bootstrap 入口实现
- `container/`：`ApplicationContainer*` 进程级组合根
- `factories/`：基础设施适配器工厂
- `runtime/configuration/`：配置解析、`WorkspaceAssetPaths`、interlock/config validator
- `runtime/recipes/`：recipe persistence wiring 与文件仓储适配
- `runtime/status/`：runtime status export assembler；透传 supervision snapshot authority，并通过可选只读 readers 追加 motion / valve enrichment，不重定义 machine/runtime authority
- `runtime/storage/files/`：本地文件存储适配
- `runtime/supervision/`：runtime supervision 的 app-local backend / decorator / terminal-sync wiring；负责进程侧副作用协同，不成为 `ExecutionSession` owner
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

`Siligen::Apps::Runtime::BuildContainer(...)` 的签名、返回类型和行为语义保持不变；调用方只迁移 include / link 落点，不改启动协议。

## 与 M9 的边界

- `modules/runtime-execution/runtime/host` 只保留 host core。
- dispensing execution concrete 已迁入 `modules/runtime-execution`；本目录只在 `ApplicationContainer.Dispensing.cpp` 组合 `CreateDispensingProcessPort(...)`，不保留 app-local execution wrapper。
- recipe persistence、config/storage、security、workflow/job-ingest/DXF wiring 都在本目录的 app-local shell 组合。
- `RuntimeExecutionSupervisionBackend`、`RuntimeSupervisionSyncPort`、`RuntimeStatusExportPort` 都属于本目录的 app-local shell 资产，不得回流到 `modules/runtime-execution` 变成模块 owner。
- `workflow_recipe_*` 仍是 recipe owner surface；本目录只消费这些 public targets 进行 runtime wiring。

## 调用方

以下入口统一通过 `siligen_runtime_process_bootstrap_public` 消费 bootstrap public surface：

- `apps/runtime-service/main.cpp`
- `apps/planner-cli/main.cpp`
- `apps/runtime-gateway/main.cpp`

旧模块路径头只保留最小 deprecated forwarder，不再对应 live owner target。

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
