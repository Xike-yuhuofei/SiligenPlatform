# Workspace Onboarding

更新时间：`2026-03-25`

## 1. 当前 canonical 入口

所有命令默认在工作区根执行：

```powershell
Set-Location <repo-root>
```

统一入口：

| 入口 | 用途 | 当前状态 |
|---|---|---|
| `.\scripts\validation\install-python-deps.ps1` | 安装 Python 依赖 | 已验证 |
| `.\build.ps1` | 根级构建入口 | 已验证 |
| `.\test.ps1` | 根级测试与报告入口 | 已验证 |
| `.\ci.ps1` | 根级严格验证入口 | 已验证 |
| `.\apps\hmi-app\run.ps1` | HMI 启动入口 | 已验证 |
| `config\machine\machine_config.ini` | 机器配置 canonical 源 | 已验证 |
| `data\recipes\` | 配方数据 canonical 源 | 已验证 |

control-apps 产物（构建后）：

- `siligen_runtime_service.exe`
- `siligen_runtime_gateway.exe`
- `siligen_planner_cli.exe`

DXF 编辑说明：

- 当前不再提供内建 DXF 编辑器。
- 使用方式见 `docs/runtime/external-dxf-editing.md`。
- `apps\hmi-app` 自动拉起 gateway 只认 `SILIGEN_GATEWAY_*` 显式契约。
- DXF 默认候选目录只认 `uploads\dxf`、`shared\contracts\engineering\fixtures\cases\rect_diag`，或显式 `SILIGEN_DXF_DEFAULT_DIR`。

## 2. 第一次接手都要做的事

1. 安装 Python 依赖：

```powershell
.\scripts\validation\install-python-deps.ps1
```

2. 轻量确认 canonical 入口：

```powershell
$controlAppsRoot = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} else {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
}

Test-Path (Join-Path $controlAppsRoot "bin\Debug\siligen_runtime_service.exe")
Test-Path (Join-Path $controlAppsRoot "bin\Debug\siligen_runtime_gateway.exe")
Test-Path (Join-Path $controlAppsRoot "bin\Debug\siligen_planner_cli.exe")

.\apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart
.\legacy-exit-check.ps1 -Profile Local -ReportDir tests\reports\verify\workspace-onboarding-legacy-exit
```

3. 如需处理 DXF，阅读：

```text
docs/runtime/external-dxf-editing.md
```

## 3. 常见误区

1. 误以为已删除的 `packages/`、`integration/`、`tools/`、`examples/` 仍可作为默认入口。
2. 误以为 `control-core/*` 子树仍是默认配置/数据/二进制来源。
3. 误以为 `build.ps1` 会重新允许 legacy alias 回流默认链路。