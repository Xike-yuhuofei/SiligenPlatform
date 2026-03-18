# SiligenSuite

`D:\Projects\SiligenSuite` 是当前工作区总入口。默认工作方式从仓库根开始，不再把 `control-core\`、`hmi-client\` 或 `dxf-pipeline\` 当作默认入口，也不再提供内建 DXF 编辑器。

## 先看哪里

- 开发：`docs\onboarding\workspace-onboarding.md`
- 测试：`docs\onboarding\workspace-onboarding.md` + `docs\troubleshooting\post-refactor-runbook.md`
- 现场：`docs\runtime\deployment.md` + `docs\runtime\rollback.md` + `docs\troubleshooting\post-refactor-runbook.md`
- 外部 DXF 编辑：`docs\runtime\external-dxf-editing.md`

补充背景：

- 工作区边界：`WORKSPACE.md`
- 当前整仓基线：`docs\architecture\workspace-baseline.md`
- build/test 口径：`docs\architecture\build-and-test.md`
- canonical/legacy 路径划分：`docs\architecture\canonical-paths.md`
- control apps 真实化说明：`docs\architecture\control-apps-realization.md`

## 当前 canonical 入口

根级统一命令：

```powershell
Set-Location D:\Projects\SiligenSuite
.\tools\scripts\install-python-deps.ps1
.\build.ps1
.\test.ps1
.\ci.ps1
```

control apps 产物根解析顺序：

- `SILIGEN_CONTROL_APPS_BUILD_ROOT`
- `%LOCALAPPDATA%\SiligenSuite\control-apps-build`
- `D:\Projects\SiligenSuite\build\control-apps`（仅在 `LOCALAPPDATA` 不可用时退回）

根级应用入口：

| 场景 | canonical 入口 | 当前状态 |
|---|---|---|
| HMI | `apps\hmi-app\run.ps1` | 可直接源码运行 |
| DXF 外部编辑 | `docs\runtime\external-dxf-editing.md` | 纯人工流程；使用 AutoCAD 等外部编辑器 |
| TCP 服务 | `apps\control-tcp-server\run.ps1` | 真实入口；默认只认 canonical `siligen_tcp_server.exe`；`-UseLegacyFallback` 是临时、显式、可审计回退 |
| CLI | `apps\control-cli\run.ps1` | 真实入口；默认只认 canonical `siligen_cli.exe`；当前真实承载面为 `bootstrap-check` + `recipe` 子命令 |
| Runtime | `apps\control-runtime\run.ps1` | 真实入口；默认只认 canonical `siligen_control_runtime.exe`；已具备独立宿主 exe |

## Build / Test / CI

当前 control apps 的真实 build 方式是：

- `cmake -S control-core -B <CONTROL_APPS_BUILD_ROOT>`
- `control-core/apps/CMakeLists.txt` 负责把 `..\apps\control-runtime`、`..\apps\control-tcp-server`、`..\apps\control-cli` 注册为真实 app 入口
- 产物输出到 `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\*.exe`

CI 入口保持根级统一：

- `.github\workflows\workspace-validation.yml` 通过 `.\build.ps1` + `.\test.ps1` 驱动各 suite
- workflow 显式设置 `SILIGEN_CONTROL_APPS_BUILD_ROOT=${{ github.workspace }}\build\control-apps`，让 CI 中的 control app 产物路径可预测、可审计

## 当前已验证与未闭环事项

已验证：

- `apps\control-runtime\run.ps1 -DryRun` 可定位到 canonical `siligen_control_runtime.exe`
- `apps\control-tcp-server\run.ps1 -DryRun` 可定位到 canonical `siligen_tcp_server.exe`
- `apps\control-cli\run.ps1 -DryRun` 可定位到 canonical `siligen_cli.exe`
- `integration\hardware-in-loop\run_hardware_smoke.py` 可直接对 canonical TCP server 做最小启动验证
- `test.ps1 -Profile CI -Suite apps -FailOnKnownFailure` 当前通过；CI 入口也走同一组 canonical app 产物

仍未闭环：

- CLI 的运动 / 点胶 / DXF / 连接调试命令仍未迁入 canonical 实现；当前仍需显式 legacy fallback
- 现场部署是否切离 `control-core` 兼容产物仍需单独做部署口径验证
- `dxf-pipeline` sibling 目录依赖仍未消除
- DXF 编辑已改为外部编辑器人工流程，不再提供 editor app / notify / CLI 协议

## 与 legacy 路径的关系

- `control-core\` 不是默认入口；当前仍承载共享库图、legacy app 迁移源和部分未迁移 CLI 能力。
- `control-core\build\bin\**` 不再是三个 control app 的默认产物来源；只有显式 `-UseLegacyFallback` 才允许临时回退。
- `hmi-client\` 不是默认入口，但仍保留重复源码与兼容脚本。
- `dxf-pipeline\` 不再是默认工程数据入口，但 HMI DXF 预览仍依赖它的 sibling 运行方式。
- `dxf-editor\`、`apps\dxf-editor-app`、`packages\editor-contracts` 已退出工作区默认链路，当前仅应作为历史记录看待。
- `config\`、`data\` 是当前默认说明路径；只有在兼容、回滚、排障时才回到 legacy 路径说明。
