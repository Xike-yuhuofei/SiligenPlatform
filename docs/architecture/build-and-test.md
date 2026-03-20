# Build And Test

更新时间：`2026-03-19`

## 1. 目标

本文档定义 `SiligenSuite` 当前统一的根级 build/test/CI 入口。

原则：

- 默认调用入口统一收敛到仓库根。
- 默认验证链路必须使用 canonical app/config/data 路径。
- `legacy-exit` 门禁负责拦截 `control-core` config/data/HIL/alias/source-root 回流。

## 2. 根级入口

### 2.1 Python 依赖安装

```powershell
.\tools\scripts\install-python-deps.ps1
```

### 2.2 Build 入口

```powershell
.\build.ps1
.\build.ps1 -Profile CI -Suite simulation
```

当前根级 build 真实接入的命令：

- control apps 的 CMake configure/build
  - top-level source root：`control-core`
  - canonical app source root：`apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli`
  - canonical binary root：`SILIGEN_CONTROL_APPS_BUILD_ROOT` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build` -> `D:\Projects\SiligenSuite\build\control-apps`
  - app 产物：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_control_runtime.exe`、`siligen_tcp_server.exe`、`siligen_cli.exe`
  - build 后验证：`tools/build/build-validation.ps1` 会校验上述 exe 已真实落盘
- `packages/simulation-engine` 的 CMake configure/build
  - `simulation` suite 走 fresh configure，要求空 build root 下也能独立生成全部示例与测试产物

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
- workflow 显式设置 `SILIGEN_CONTROL_APPS_BUILD_ROOT=${{ github.workspace }}\build\control-apps`
- `.\ci.ps1` 会先执行 `.\legacy-exit-check.ps1 -Profile CI`

## 3. 当前测试矩阵

| Suite | 根级命令 | 覆盖对象 | 当前真实命令 |
|---|---|---|---|
| `apps` | `.\build.ps1 -Suite apps` + `.\test.ps1 -Suite apps` | canonical apps | build 产出并校验 `siligen_control_runtime.exe` / `siligen_tcp_server.exe` / `siligen_cli.exe`；test 执行三个 app 的 dry-run、`siligen_control_runtime.exe --version`、`siligen_tcp_server.exe --help`、`siligen_cli.exe bootstrap-check --config .\config\machine\machine_config.ini`，以及 `apps/hmi-app/scripts/test.ps1`。 |
| `packages` | `.\test.ps1 -Suite packages` | 可独立验证的 canonical packages | `application-contracts`、`engineering-contracts`、`engineering-data`、`transport-gateway`；本地模式额外收集 canonical control-apps build root 暴露的 `siligen_unit_tests.exe` / `siligen_pr1_tests.exe`。 |
| `integration` | `.\test.ps1 -Suite integration` | 场景回归与联调 | `integration/scenarios/run_engineering_regression.py`；本地模式默认同时执行指向 canonical TCP server 的 `hardware-in-loop/run_hardware_smoke.py`。 |
| `protocol-compatibility` | `.\test.ps1 -Suite protocol-compatibility` | 契约/协议闭环 | `integration/protocol-compatibility/run_protocol_compatibility.py` |
| `simulation` | `.\build.ps1 -Suite simulation` + `.\test.ps1 -Suite simulation` | 仿真与仿真回归 | `simulation_engine_smoke_test.exe`、`simulation_engine_json_io_test.exe`、scheme C `ctest` 子集、`integration/simulated-line/run_simulated_line.py` |

## 4. 当前纳入门禁的范围

当前 CI 门禁纳入：

- `apps`: `control-runtime`、`control-tcp-server`、`control-cli`、`hmi-app`
- `packages`: `application-contracts`、`engineering-contracts`、`engineering-data`、`transport-gateway`
- `integration`: `engineering-regression`
- `protocol-compatibility`
- `simulation`
- `legacy-exit`: config/data fallback、HIL 默认入口、gateway/tcp alias 注册点、canonical package source-root fallback

当前未纳入 CI 强制门禁，但有根级入口或本地检查方式：

- `integration/hardware-in-loop/run_hardware_smoke.py`
- `process-runtime-core` 通过 canonical control-apps build root `bin\*.exe` 暴露的本地单测

## 5. 当前与 legacy 的剩余关系

- `control-core/build/bin/**` 不再是三个 control app 的默认产物路径。
- `control-runtime`、`control-tcp-server`、`control-cli` 的 `run.ps1` 都已移除 legacy fallback；三者默认只解析 canonical build root 下的真实产物。
- `control-core/config/*`、`control-core/data/recipes/*`、`control-core/src/infrastructure/resources/config/files/recipes/schemas/*` 已退出默认解析链路。
- `runtime-host`、`process-runtime-core`、`device-adapters`、`device-contracts`、`shared-kernel` 仍通过 `control-core` 的真实库图编译，但不再通过隐式 `CMAKE_SOURCE_DIR` fallback 取用 canonical app/package 配置。
