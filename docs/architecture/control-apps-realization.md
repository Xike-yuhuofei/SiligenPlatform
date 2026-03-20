# Control Apps Realization

更新时间：`2026-03-19`

## 1. 目标与边界

本文记录 `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli` 从 wrapper 过渡到真实入口的当前状态。

权威 cutover 记录已转移到 `docs/architecture/control-apps-binary-cutover.md`；本文保留为补充说明，并与其保持一致。

本轮真实化的重点是“产物来源切换”：

- 默认运行、build、test、CI 必须优先指向根级 `apps/*` 对应的真实 exe
- 不再把 `control-core/build/bin/**` 当作默认产物来源
- 不改变已冻结的协议边界
- 不再新增一层目录包装来伪装 canonical 完成

CLI 的特殊约束：

- 必须拥有明确真实承载面
- 不允许继续通过 wrapper 伪装迁移完成
- 新命令不得继续落在 `control-core`

## 2. 当前构建拓扑

当前 control apps 的统一 build 方式是：

```powershell
cmake -S control-core -B <CONTROL_APPS_BUILD_ROOT>
cmake --build <CONTROL_APPS_BUILD_ROOT> --config Debug --target siligen_control_runtime siligen_tcp_server siligen_cli
```

其中：

- `control-core` 仍是当前 CMake source root，因为共享库图、shared-kernel 与若干底层包仍在这里编译
- `control-core/apps/CMakeLists.txt` 只负责把根级真实 app 目录注册进来
- 三个真实 app owner 已切换到根级目录：
  - `apps/control-runtime`
  - `apps/control-tcp-server`
  - `apps/control-cli`

`CONTROL_APPS_BUILD_ROOT` 的解析顺序为：

1. `SILIGEN_CONTROL_APPS_BUILD_ROOT`
2. `%LOCALAPPDATA%\SiligenSuite\control-apps-build`
3. `D:\Projects\SiligenSuite\build\control-apps`（仅当 `LOCALAPPDATA` 不可用时）

## 3. 三个 control app 的真实入口与产物

| App | 真实构建入口 | 真实产物路径 | 当前状态 | 与 `control-core/build/bin/**` 的剩余关系 |
|---|---|---|---|---|
| `control-runtime` | `apps/control-runtime/CMakeLists.txt` + `apps/control-runtime/main.cpp`，目标名 `siligen_control_runtime` | `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_control_runtime.exe` | 已真实化 | 默认与显式 legacy fallback 都已切除；传 `-UseLegacyFallback` 直接阻塞 |
| `control-tcp-server` | `apps/control-tcp-server/CMakeLists.txt` + `apps/control-tcp-server/main.cpp`，目标名 `siligen_tcp_server` | `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe` | 已真实化 | 默认与显式 legacy fallback 都已切除；HIL 默认也已切到 canonical 产物 |
| `control-cli` | `apps/control-cli/CMakeLists.txt` + `apps/control-cli/main.cpp`，目标名 `siligen_cli` | `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_cli.exe` | 已真实化 | 默认与显式 legacy fallback 都已切除；连接调试、运动、点胶、DXF、recipe 已迁入 canonical CLI |

## 4. 分项说明与推进结果

### 4.1 `control-runtime`

已完成：

- 根级真实源码入口为 `apps/control-runtime/main.cpp`
- 真实 exe 目标为 `siligen_control_runtime`
- 入口直接调用 `Siligen::Apps::Runtime::BuildContainer(...)`
- `run.ps1` 默认只解析 canonical 产物

真实产物路径：

- 模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_control_runtime.exe`
- 默认 Windows 示例：`%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\Debug\siligen_control_runtime.exe`

剩余关系：

- `control-core/build/bin/**/siligen_control_runtime.exe` 不再是默认来源
- `control-core` 仍只在库图层面参与构建
- `apps/control-runtime/run.ps1` 已删除 legacy 执行分支；`-UseLegacyFallback` 直接报错

### 4.2 `control-tcp-server`

已完成：

- 根级真实源码入口为 `apps/control-tcp-server/main.cpp`
- 真实 exe 目标为 `siligen_tcp_server`
- 入口直接调用 `BuildContainer(...)`、`BuildTcpFacadeBundle(...)`、`TcpServerHost`
- `run.ps1` 默认只解析 canonical 产物
- `integration/hardware-in-loop/run_hardware_smoke.py` 默认已切换到 canonical TCP server

真实产物路径：

- 模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe`
- 默认 Windows 示例：`%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\Debug\siligen_tcp_server.exe`

剩余关系：

- `control-core/build/bin/**/siligen_tcp_server.exe` 不再是默认运行或 HIL 路径
- `apps/control-tcp-server/run.ps1` 已删除 legacy 执行分支；`-UseLegacyFallback` 直接报错

### 4.3 `control-cli`

已完成：

- 根级真实源码入口为 `apps/control-cli/main.cpp`
- 真实 exe 目标为 `siligen_cli`
- `apps/control-cli` 已承接连接调试、运动、点胶、DXF、recipe 的真实命令实现
- `run.ps1` 已删除显式 `-UseLegacyFallback`

当前 canonical 承载面：

- `bootstrap-check`
- `connect` / `disconnect` / `status`
- `home` / `jog` / `move` / `stop-all` / `estop`
- `dispenser start|purge|stop|pause|resume`
- `supply open|close`
- `dxf-plan` / `dxf-dispense` / `dxf-augment`
- `recipe create|update|draft|draft-update|publish|list|get|versions|archive|version-create|compare|rollback|activate|audit|export|import`

真实产物路径：

- 模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_cli.exe`
- 默认 Windows 示例：`%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\Debug\siligen_cli.exe`

当前结论：

- `control-cli` 已完成命令面 cutover
- `control-core/build/bin/**/siligen_cli.exe` 不再是默认或显式 CLI 入口
- `dxf-augment` 已迁入 canonical CLI，但当前本地构建若关闭 `SILIGEN_ENABLE_CGAL`，仍会命中 `DXFContourAugmenter.stub.cpp`
- 详见 `docs/architecture/control-cli-cutover.md`

## 5. run.ps1、README、build/test/CI 更新

### 5.1 `run.ps1`

已更新：

- `apps/control-runtime/run.ps1`
- `apps/control-tcp-server/run.ps1`
- `apps/control-cli/run.ps1`

统一规则：

- 默认只接受 canonical 产物
- `apps/control-runtime/run.ps1`、`apps/control-tcp-server/run.ps1`、`apps/control-cli/run.ps1` 都已不再支持 legacy fallback
- `-DryRun` 会直接输出当前目标路径与来源，便于审计

### 5.2 README

已更新：

- 根级 `README.md`
- `apps/control-runtime/README.md`
- `apps/control-tcp-server/README.md`
- `apps/control-cli/README.md`
- `integration/hardware-in-loop/README.md`
- `docs/validation/README.md`

这些文档现在统一说明：

- 真实源码 owner 是根级 `apps/*`
- 真实产物路径来自 canonical control-apps build root
- legacy fallback 已从三个 control app 的默认与显式入口中切除

### 5.3 build / test / CI

已更新或已对齐：

- `tools/build/build-validation.ps1`
  - 输出真实 `control-apps build root`
  - 不再把路径写死为 `build/control-apps`
- `packages/test-kit/src/test_kit/workspace_validation.py`
  - `dry-run` 与 `process-runtime-core` 本地单测产物都按 canonical control-apps build root 查找
  - `apps` suite 新增 `control-cli-help`、`control-cli-recipe-list`、`control-cli-dxf-augment`
- `integration/hardware-in-loop/run_hardware_smoke.py`
  - 默认可执行文件已切到 canonical TCP server
- `.github/workflows/workspace-validation.yml`
  - CI 显式设置 `SILIGEN_CONTROL_APPS_BUILD_ROOT=${{ github.workspace }}\build\control-apps`
  - 所有 suite 继续通过根级 `build.ps1` / `test.ps1` 驱动

## 6. 产物路径验证

建议使用以下命令验证默认目标：

```powershell
.\apps\control-runtime\run.ps1 -DryRun
.\apps\control-tcp-server\run.ps1 -DryRun
.\apps\control-cli\run.ps1 -DryRun
python .\integration\hardware-in-loop\run_hardware_smoke.py
.\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure
```

当前预期结果：

- 三个 `run.ps1 -DryRun` 都应输出 `source: canonical`
- `hardware_smoke` 应默认从 canonical `siligen_tcp_server.exe` 启动
- `apps` suite 不再要求默认存在 `control-core/build/bin/**`

## 7. 与 `control-core/build/bin/**` 的剩余关系

剩余关系只剩迁移期对比、排障或历史产物留存；不再存在 live CLI fallback。

明确不再允许的情况：

- `control-runtime`、`control-tcp-server` 的 `run.ps1` 继续提供 legacy fallback
- 在文档中把 `control-core/build/bin/**` 继续写成 canonical 产物路径
- 让 CLI 再次回退到 `control-core/build/bin/**/siligen_cli.exe`

## 8. `control-core/apps/*` 当前状态

`2026-03-18` 已完成以下删除：

1. `control-core/apps/control-runtime`
2. `control-core/apps/control-tcp-server`

删除依据：

1. 三个 control app 的默认 build、run、test、CI 已只认根级 `apps/*` 源码与 canonical control-apps build root 产物。
2. HIL、workspace validation、根级脚本与默认 dry-run 已不再把上述 legacy app 目录当默认入口。
3. 仓内源码级 consumer 已清零，只剩历史报告和归档/阶段性文档文本。

当前仍保留：

1. `control-core/apps/CMakeLists.txt`，因为 `control-core` 仍是当前 control app 的 CMake source root，需要继续注册根级 `apps/*`。
2. `control-core/apps/CMakeLists.txt`，因为当前 control apps 仍由 `control-core` 顶层 CMake 统一注册。

## 9. 当前结论

- `control-runtime`：已摆脱默认 legacy build 回退
- `control-tcp-server`：已摆脱默认 legacy build 回退
- `control-cli`：已摆脱默认 legacy build 回退，但仍被“全量命令面迁移”阻塞
- `control-core/apps/control-runtime`、`control-core/apps/control-tcp-server`：已删除

因此，当前可以认为：

- “默认产物来源切换”已经完成
- “删除 legacy app 子目录”已经完成
- “彻底删除 `control-core` 作为 app build source root 的角色”还需要先收尾 CLI 命令迁移与 `control-core` source root 剥离
