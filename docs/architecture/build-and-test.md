# Build And Test

更新时间：`2026-03-18`

## 1. 目标

本文档定义 `SiligenSuite` 当前统一的根级 build/test/CI 入口。

原则：

- 优先统一根级入口，不强行统一所有底层构建技术。
- 默认调用入口统一收敛到仓库根。
- DXF 编辑不再纳入应用测试面，当前改为外部编辑器人工流程。

## 2. 根级入口

### 2.1 Python 依赖安装

```powershell
.\tools\scripts\install-python-deps.ps1
```

当前会安装：

- `packages/test-kit`
- `packages/engineering-data`
- `apps/hmi-app` 依赖

### 2.2 Build 入口

```powershell
.\build.ps1
.\build.ps1 -Profile CI -Suite simulation
```

当前根级 build 真实接入的命令：

- control apps 的 CMake configure/build
  - source root：`control-core`
  - canonical app source root：`apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli`
  - canonical binary root：`SILIGEN_CONTROL_APPS_BUILD_ROOT` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build` -> `D:\Projects\SiligenSuite\build\control-apps`
  - app 产物：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_control_runtime.exe`、`siligen_tcp_server.exe`、`siligen_cli.exe`
- `packages/simulation-engine` 的 CMake configure/build

### 2.3 Test 入口

```powershell
.\test.ps1
.\test.ps1 -Profile Local -Suite apps
.\test.ps1 -Profile CI -Suite simulation -FailOnKnownFailure
```

### 2.4 CI 入口

```powershell
.\ci.ps1
.\ci.ps1 -Suite protocol-compatibility
```

当前默认 CI 工作流：

- `.github/workflows/workspace-validation.yml`
- 所有 suite 都通过根级 `.\build.ps1` + `.\test.ps1` 执行
- workflow 显式设置 `SILIGEN_CONTROL_APPS_BUILD_ROOT=${{ github.workspace }}\build\control-apps`，保证 CI 中的 control app 产物路径固定、可审计

## 3. 当前测试矩阵

| Suite | 根级命令 | 覆盖对象 | 当前真实命令 |
|---|---|---|---|
| `apps` | `.\build.ps1 -Suite apps` + `.\test.ps1 -Suite apps` | canonical apps | build 产出 `siligen_control_runtime.exe` / `siligen_tcp_server.exe` / `siligen_cli.exe`；test 同时执行三个 app 的 dry-run 与 `apps/hmi-app/scripts/test.ps1`。 |
| `packages` | `.\test.ps1 -Suite packages` | 可独立验证的 canonical packages | `application-contracts`、`engineering-contracts`、`engineering-data`、`transport-gateway`；本地模式额外收集 canonical control-apps build root 暴露的 `siligen_unit_tests.exe` / `siligen_pr1_tests.exe`。 |
| `integration` | `.\test.ps1 -Suite integration` | 场景回归与联调 | `integration/scenarios/run_engineering_regression.py`；本地模式默认同时执行指向 canonical TCP server 的 `hardware-in-loop/run_hardware_smoke.py`。 |
| `protocol-compatibility` | `.\test.ps1 -Suite protocol-compatibility` | 契约/协议闭环 | `integration/protocol-compatibility/run_protocol_compatibility.py` |
| `simulation` | `.\build.ps1 -Suite simulation` + `.\test.ps1 -Suite simulation` | 仿真与仿真回归 | `simulation_engine_smoke_test.exe`、`simulation_engine_json_io_test.exe`、`integration/simulated-line/run_simulated_line.py` |

## 4. 当前纳入门禁的范围

当前 CI 门禁纳入：

- `apps`: `control-runtime`、`control-tcp-server`、`control-cli`、`hmi-app`
- `packages`: `application-contracts`、`engineering-contracts`、`engineering-data`、`transport-gateway`
- `integration`: `engineering-regression`
- `protocol-compatibility`
- `simulation`

当前未纳入 CI 门禁，但有根级入口或本地检查方式：

- `integration/hardware-in-loop/run_hardware_smoke.py`
- `process-runtime-core` 通过 canonical control-apps build root `bin\*.exe` 暴露的本地单测

## 5. 当前与 legacy 的剩余关系

- `control-core/build/bin/**` 不再是三个 control app 的默认产物路径。
- 三个 app 的 `run.ps1` 默认只认 canonical 产物；只有显式 `-UseLegacyFallback` 才允许临时审计式回退。
- control apps 的 exe owner 已切到根级 `apps/*`，但 `runtime-host`、`process-runtime-core`、`device-adapters`、`device-contracts`、`shared-kernel` 仍通过 `control-core` 的库图编译。

## 6. 推荐命令

### 6.1 本地日常验证

```powershell
.\tools\scripts\install-python-deps.ps1
.\build.ps1 -Profile Local -Suite apps,simulation
.\test.ps1 -Profile Local -Suite apps,packages,protocol-compatibility,integration,simulation
```

### 6.2 外部 DXF 编辑

DXF 编辑当前不再有测试入口或 app 入口，统一参考：

```text
docs/runtime/external-dxf-editing.md
```
