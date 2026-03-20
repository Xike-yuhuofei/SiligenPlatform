# Workspace Onboarding

更新时间：`2026-03-19`

## 1. 当前 canonical 入口

所有命令默认在工作区根执行：

```powershell
Set-Location D:\Projects\SiligenSuite
```

统一入口：

| 入口 | 用途 | 当前状态 |
|---|---|---|
| `.\tools\scripts\install-python-deps.ps1` | 安装 Python 依赖 | 已验证 |
| `.\build.ps1` | 根级构建入口 | 已验证；control apps 与 simulation 都走根级入口 |
| `.\test.ps1` | 根级测试与报告入口 | 已验证 |
| `.\ci.ps1` | 根级严格验证入口 | 已验证 |
| `.\apps\hmi-app\run.ps1` | 开发 / 测试 | 可源码运行 |
| `.\apps\control-runtime\run.ps1` | runtime 宿主入口 | 已验证；真实 exe 为 canonical `siligen_control_runtime.exe` |
| `.\apps\control-tcp-server\run.ps1` | TCP/HIL 入口 | 已验证；真实 exe 为 canonical `siligen_tcp_server.exe` |
| `.\apps\control-cli\run.ps1` | CLI / 排障入口 | 已验证；默认走 canonical `siligen_cli.exe`，不再支持 legacy fallback |
| `config\machine\machine_config.ini` | 机器配置 canonical 源 | 已验证 |
| `data\recipes\` | 配方数据 canonical 源 | 已验证 |

DXF 编辑说明：

- 当前不再提供内建 DXF 编辑器
- 使用方式见 `docs/runtime/external-dxf-editing.md`
- `apps\hmi-app` 自动拉起 gateway 只认 `SILIGEN_GATEWAY_*` 显式契约
- DXF 默认候选目录只认 `uploads\dxf`、`packages\engineering-contracts\fixtures\cases\rect_diag`，或显式 `SILIGEN_DXF_DEFAULT_DIR`

## 2. 第一次接手都要做的事

1. 安装 Python 依赖：

```powershell
.\tools\scripts\install-python-deps.ps1
```

2. 轻量确认 canonical 入口：

```powershell
.\apps\control-runtime\run.ps1 -DryRun
.\apps\control-tcp-server\run.ps1 -DryRun
.\apps\control-cli\run.ps1 -DryRun
.\apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart
.\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\workspace-onboarding-legacy-exit
```

3. 如需处理 DXF，阅读：

```text
docs/runtime/external-dxf-editing.md
```

## 3. 常见误区

1. 误以为 `control-core/build/bin/**` 仍是 `control-runtime`、`control-tcp-server` 的默认产物来源。
2. 误以为 `control-core/config`、`control-core/data/recipes` 仍是默认配置/数据入口。
3. 误以为 `.\build.ps1` 会重新把 `control-core/src/adapters/tcp` 或 `control-core/modules/control-gateway` 加回默认库图。
