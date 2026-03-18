# Control Apps Realization

更新时间：`2026-03-18`

## 1. 目标与边界

本文记录 `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli` 从 wrapper 过渡到真实入口的当前状态。

本轮真实化的重点是“产物来源切换”：

- 默认运行、build、test、CI 必须优先指向根级 `apps/*` 对应的真实 exe
- 不再把 `control-core/build/bin/**` 当作默认产物来源
- 不改变已冻结的协议边界
- 不再新增一层目录包装来伪装 canonical 完成

CLI 的特殊约束：

- 必须拥有明确真实承载面
- 若全量源码尚未迁完，必须给出最小可行承载面与阻塞项
- 对未迁移命令，必须显式阻塞并要求人工启用临时 fallback

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
| `control-runtime` | `apps/control-runtime/CMakeLists.txt` + `apps/control-runtime/main.cpp`，目标名 `siligen_control_runtime` | `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_control_runtime.exe` | 已真实化 | 默认不再回退；只有 `run.ps1 -UseLegacyFallback` 才允许临时审计式回退 |
| `control-tcp-server` | `apps/control-tcp-server/CMakeLists.txt` + `apps/control-tcp-server/main.cpp`，目标名 `siligen_tcp_server` | `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe` | 已真实化 | 默认不再回退；HIL 默认也已切到 canonical 产物；legacy 仅显式 fallback |
| `control-cli` | `apps/control-cli/CMakeLists.txt` + `apps/control-cli/main.cpp`，目标名 `siligen_cli` | `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_cli.exe` | 已有真实承载面，但仍部分阻塞 | 默认不再回退；未迁移命令必须显式走 `-UseLegacyFallback` |

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
- `-UseLegacyFallback` 仅保留作临时、显式、可审计回退

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
- `-UseLegacyFallback` 仅保留给临时排障期

### 4.3 `control-cli`

已完成：

- 根级真实源码入口为 `apps/control-cli/main.cpp`
- 真实 exe 目标为 `siligen_cli`
- canonical CLI 已有最小可行真实承载面，而不是永久 wrapper

当前 canonical 承载面：

- `bootstrap-check`
- `recipe create`
- `recipe list`
- `recipe get`
- `recipe versions`
- `recipe audit`
- `recipe export`
- `recipe import`

真实产物路径：

- 模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_cli.exe`
- 默认 Windows 示例：`%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\Debug\siligen_cli.exe`

最小可行迁移方案：

- 先把 bootstrap 与 recipe 相关能力迁入根级 canonical CLI，形成真实 exe 与真实命令面
- 对未迁移命令不再透明转发，而是明确返回“尚未迁移到 canonical CLI”的阻塞信息
- 仅在用户显式执行 `apps/control-cli/run.ps1 -UseLegacyFallback -- <legacy-args>` 时，才允许调用 legacy 产物

当前阻塞项：

- 运动命令未迁移
- 点胶命令未迁移
- DXF 相关命令未迁移
- 连接调试命令未迁移
- 可信迁移源仍包括旧仓 `D:\Projects\Backend_CPP\src\adapters\cli`

结论：

- `control-cli` 已经摆脱“默认回退到 legacy build”的状态
- 但仍未完成“全量命令面迁移”

## 5. run.ps1、README、build/test/CI 更新

### 5.1 `run.ps1`

已更新：

- `apps/control-runtime/run.ps1`
- `apps/control-tcp-server/run.ps1`
- `apps/control-cli/run.ps1`

统一规则：

- 默认只接受 canonical 产物
- 若 canonical 缺失但检测到 legacy 产物，默认仍报阻塞，不再静默回退
- 只有显式 `-UseLegacyFallback` 才允许临时回退
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
- legacy fallback 仅是临时、显式、可审计路径

### 5.3 build / test / CI

已更新或已对齐：

- `tools/build/build-validation.ps1`
  - 输出真实 `control-apps build root`
  - 不再把路径写死为 `build/control-apps`
- `packages/test-kit/src/test_kit/workspace_validation.py`
  - `dry-run` 与 `process-runtime-core` 本地单测产物都按 canonical control-apps build root 查找
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

剩余关系只剩两类：

1. 显式、临时、可审计的 `-UseLegacyFallback`
2. 迁移期对比、排障或历史产物留存

明确不再允许的情况：

- 默认 `run.ps1` 自动回退到 `control-core/build/bin/**`
- 在文档中把 `control-core/build/bin/**` 继续写成 canonical 产物路径
- 让 CLI 在未迁移命令上继续透明转发，伪装 canonical 已完成

## 8. 删除 `control-core/apps/*` 的前置条件

删除 `control-core/apps/*` 前，至少要满足：

1. 三个 control app 的默认 build、run、test、CI 全部只认根级 `apps/*` 源码与 canonical control-apps build root 产物。
2. `control-cli` 完成剩余命令面的迁移，未迁移命令清零，不再需要 `-UseLegacyFallback`。
3. HIL、workspace validation、根级脚本、CI matrix 与现场最小冒烟不再把 `control-core/build/bin/**` 当默认路径。
4. 仓内 consumer 对 `control-core/apps/control-runtime`、`control-core/apps/control-tcp-server`、`control-core/build/bin/**` 的默认引用清零。
5. README、build/test/CI、部署/回滚/排障文档完成统一切换，避免删目录后文档继续指向 legacy。
6. 保留的 fallback 若尚未删除，必须继续显式、临时、可审计，并附带淘汰计划。

## 9. 当前结论

- `control-runtime`：已摆脱默认 legacy build 回退
- `control-tcp-server`：已摆脱默认 legacy build 回退
- `control-cli`：已摆脱默认 legacy build 回退，但仍被“全量命令面迁移”阻塞

因此，当前可以认为：

- “默认产物来源切换”已经完成
- “彻底删除 `control-core/apps/*`”还需要先收尾 CLI 命令迁移与 residual legacy 文档清理
