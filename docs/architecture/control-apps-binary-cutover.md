# Control Apps Binary Cutover

更新时间：`2026-03-19`

## 1. 目标与边界

本轮目标是让以下三个 control app 产出真实 canonical 可执行物，并把默认运行链路从 `control-core/build/bin/**` 切走：

- `apps/control-runtime`
- `apps/control-tcp-server`
- `apps/control-cli`

本轮明确不做的事：

- 不改变已冻结的 TCP/JSON / CLI 协议边界
- 不再做一层 wrapper 伪装迁移完成
- 不把 `runtime-host`、`transport-gateway`、`process-runtime-core` 的库图重新卷回 app 目录

## 2. 三个 control app 的真实构建目标

当前统一构建入口：

```powershell
cmake -S control-core -B <CONTROL_APPS_BUILD_ROOT>
cmake --build <CONTROL_APPS_BUILD_ROOT> --config Debug --target siligen_control_runtime siligen_tcp_server siligen_cli
```

`control-core/apps/CMakeLists.txt` 当前只负责注册根级 canonical app 目录；真实 target owner 已经切到根级 `apps/*`。

| App | 真实构建 target | 真实源码 owner | 真实承载面 |
|---|---|---|---|
| `control-runtime` | `siligen_control_runtime` | `apps/control-runtime` + `packages/runtime-host` | 独立 runtime host exe |
| `control-tcp-server` | `siligen_tcp_server` | `apps/control-tcp-server` + `packages/transport-gateway` + `packages/runtime-host` | 独立 TCP/JSON server exe |
| `control-cli` | `siligen_cli` | `apps/control-cli` | 完整 canonical CLI：连接调试、运动、点胶、DXF、recipe |

## 3. 三个 control app 的真实产物路径

`CONTROL_APPS_BUILD_ROOT` 解析顺序：

1. `SILIGEN_CONTROL_APPS_BUILD_ROOT`
2. `%LOCALAPPDATA%\SiligenSuite\control-apps-build`
3. `D:\Projects\SiligenSuite\build\control-apps`

本次在当前工作区验证到的真实产物路径如下：

| App | 路径模式 | 本次验证到的路径 |
|---|---|---|
| `control-runtime` | `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_control_runtime.exe` | `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_control_runtime.exe` |
| `control-tcp-server` | `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe` | `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_tcp_server.exe` |
| `control-cli` | `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_cli.exe` | `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_cli.exe` |

构建后由 `tools/build/build-validation.ps1` 额外执行落盘校验；本次 build 输出中已记录：

- `artifact: siligen_control_runtime -> C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_control_runtime.exe`
- `artifact: siligen_tcp_server -> C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_tcp_server.exe`
- `artifact: siligen_cli -> C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_cli.exe`

## 4. run.ps1 的 fallback 清理结果

| App | 默认 `run.ps1` 行为 | fallback 清理结果 | 本次验证 |
|---|---|---|---|
| `control-runtime` | 只解析 canonical exe；`-DryRun` 缺产物时非零退出 | 已删除 `control-core/build/bin/**` 执行分支；`-UseLegacyFallback` 直接报错 | `run.ps1 -DryRun -UseLegacyFallback` 输出 `BLOCKED`，退出码 `1` |
| `control-tcp-server` | 只解析 canonical exe；`-DryRun` 缺产物时非零退出 | 已删除 `control-core/build/bin/**` 执行分支；`-UseLegacyFallback` 直接报错 | `run.ps1 -DryRun -UseLegacyFallback` 输出 `BLOCKED`，退出码 `1` |
| `control-cli` | 只解析 canonical exe；`-DryRun` 缺产物时非零退出 | 已删除 `control-core/build/bin/**` 执行分支；不再支持 `-UseLegacyFallback` | `run.ps1 -DryRun` 输出 canonical 路径，退出码 `0` |

CLI 的完整 cutover 已经落地：

- canonical `siligen_cli.exe` 真实承载 `bootstrap-check`
- canonical `siligen_cli.exe` 真实承载连接调试、运动、点胶、DXF、完整 recipe 命令面
- `apps/control-cli/run.ps1` 不再回退到 `control-core/build/bin/**/siligen_cli.exe`

CLI cutover 详情见 `docs/architecture/control-cli-cutover.md`。

## 5. build / test / CI 更新结果

### 5.1 Build

- 根级 `build.ps1` 继续走 `tools/build/build-validation.ps1`
- `build-validation.ps1` 现在会在 build 后验证 canonical exe 已真实落盘，而不是只看 CMake target 成功

### 5.2 Test

- `packages/test-kit/src/test_kit/workspace_validation.py` 的 `apps` suite 现在同时验证：
  - 三个 `run.ps1 -DryRun`
  - `siligen_control_runtime.exe --version`
  - `siligen_tcp_server.exe --help`
  - `siligen_cli.exe --help`
  - `siligen_cli.exe bootstrap-check --config .\config\machine\machine_config.ini`
  - `siligen_cli.exe recipe list --config .\config\machine\machine_config.ini`
  - `siligen_cli.exe dxf-plan --file .\examples\dxf\rect_diag.dxf`
  - `apps/hmi-app/scripts/test.ps1`
- `run.ps1 -DryRun` 在 canonical 产物缺失时现在会返回非零，避免“只打印 BLOCKED 但门禁仍通过”

### 5.3 CI

- `.github/workflows/workspace-validation.yml` 继续显式设置 `SILIGEN_CONTROL_APPS_BUILD_ROOT=${{ github.workspace }}\build\control-apps`
- CI 通过根级 `build.ps1` / `test.ps1` 走与本地相同的 canonical binary 校验链路

### 5.4 README / 文档

已更新：

- `README.md`
- `apps/control-runtime/README.md`
- `apps/control-tcp-server/README.md`
- `apps/control-cli/README.md`
- `docs/architecture/build-and-test.md`
- `docs/architecture/legacy-deletion-gates.md`
- `docs/architecture/workspace-baseline.md`
- `docs/onboarding/developer-workflow.md`
- `docs/validation/README.md`
- `docs/runtime/rollback.md`

## 6. 可执行物验证结果

本次执行：

```powershell
cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --target siligen_cli --config Debug -- /m:1
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile Local -Suite apps -ReportDir D:\Projects\SiligenSuite\integration\reports\verify\control-cli-cutover
```

结果：

- build 成功
- apps suite `10 passed, 0 failed`
- 报告位置：`D:\Projects\SiligenSuite\integration\reports\verify\control-cli-cutover\workspace-validation.md`

关键验证输出：

- `apps/control-cli/run.ps1 -DryRun` -> `source: canonical`
- `siligen_cli.exe --help` -> 正常输出完整命令面帮助
- `siligen_cli.exe bootstrap-check --config D:\Projects\SiligenSuite\config\machine\machine_config.ini` -> `bootstrap-check passed`
- `siligen_cli.exe recipe list --config D:\Projects\SiligenSuite\config\machine\machine_config.ini` -> 正常返回配方列表
- `siligen_cli.exe dxf-plan --file D:\Projects\SiligenSuite\examples\dxf\rect_diag.dxf` -> 正常返回规划结果

补充说明：

- `dxf-augment` 命令已迁入 canonical CLI，但当前本地 build 仍以 `SILIGEN_ENABLE_CGAL=OFF` 构建，因此会命中 `ContourAugmenterAdapter.stub.cpp` 并返回 `NOT_IMPLEMENTED`。这不再是 fallback 依赖问题，而是单独的 CGAL 特性开关问题。

## 7. 删除 `control-core/build` 相关依赖的剩余条件

当前已经完成：

- `control-runtime` 默认链路不再依赖 `control-core/build`
- `control-tcp-server` 默认链路不再依赖 `control-core/build`
- `control-cli` 默认链路不再依赖 `control-core/build`

当前与 CLI 相关的 cutover 已完成；`control-core/build/bin/**/siligen_cli.exe` 不再是 live dependency。

剩余仍需继续清理的是非 CLI 项：

1. 更新部署 / 回滚 / 现场操作口径，确保历史说明不再把 `control-core/build/bin/**/siligen_cli.exe` 写成运行入口
2. 对工作区脚本、历史缓存和本地旧 build 目录做 provenance 审计，确认没有 residual consumer

## 8. 当前结论

- `control-runtime`：已具备真实 canonical exe，且 `run.ps1` fallback 已清理
- `control-tcp-server`：已具备真实 canonical exe，且 `run.ps1` fallback 已清理
- `control-cli`：已具备真实 canonical exe 与完整命令面，且 `run.ps1` fallback 已清理

`control-core` 侧 config/data/HIL/alias/source-root fallback 的后续切换与剩余门禁，见 `docs/architecture/control-core-fallback-removal.md`。

因此：

- 默认 binary cutover 已完成
- `control-core/build/bin/**` 现在不再是三个 control app 的默认 source of truth
- 剩余未删除条件已不再集中在 CLI fallback，而主要是 `control-core` 的库图、`third_party` 与其他 residual consumer

