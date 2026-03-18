# Workspace Onboarding

更新时间：`2026-03-18`

## 1. 当前 canonical 入口

所有命令默认在工作区根执行：

```powershell
Set-Location D:\Projects\SiligenSuite
```

统一入口：

| 入口 | 用途 | 当前状态 |
|---|---|---|
| `.\tools\scripts\install-python-deps.ps1` | 安装 Python 依赖 | 已验证 |
| `.\build.ps1` | 根级构建入口 | 已验证；当前只真正接入 `packages/simulation-engine` 构建 |
| `.\test.ps1` | 根级测试与报告入口 | 已验证 |
| `.\ci.ps1` | 根级严格验证入口 | 已验证 |
| `.\apps\hmi-app\run.ps1` | 开发 / 测试 | 可源码运行 |
| `.\apps\control-tcp-server\run.ps1` | 测试 / 现场 | 已验证 DryRun；真实 exe 仍来自 `control-core\build\bin\**` |
| `.\apps\control-cli\run.ps1` | 开发 / 测试 / 现场支持排查 | 已验证 DryRun；真实 exe 仍来自 `control-core\build\bin\**` |
| `.\apps\control-runtime\run.ps1` | 仅用于确认状态 | 当前 `-DryRun` 返回 `BLOCKED` |

DXF 编辑说明：

- 当前不再提供内建 DXF 编辑器
- 使用方式见 `docs/runtime/external-dxf-editing.md`

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
```

3. 如需处理 DXF，阅读：

```text
docs/runtime/external-dxf-editing.md
```

## 3. 常见误区

1. 误以为仓库仍提供 `apps/dxf-editor-app` 或 `dxf-editor`。
2. 误以为 `notify / CLI` 编辑器协作协议仍然有效。
3. 误以为 `.\build.ps1` 会构建 `control-core` 全量旧产物。
