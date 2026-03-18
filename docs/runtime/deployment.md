# Deployment

更新时间：`2026-03-18`

## 1. 当前 canonical 入口

部署与验证默认从工作区根发起：

```powershell
Set-Location D:\Projects\SiligenSuite
```

部署相关 canonical 入口：

| 入口 | 用途 | 当前状态 |
|---|---|---|
| `.\tools\scripts\install-python-deps.ps1` | 安装 Python 依赖 | 已验证 |
| `.\build.ps1 -Profile Local -Suite simulation` | 构建仿真相关产物 | 已验证 |
| `.\test.ps1` | 生成交付前/交付后报告 | 已验证 |
| `.\apps\control-tcp-server\run.ps1` | 启动 TCP server | 根级入口已验证；真实 exe 仍在 `control-core\build\bin\**` |
| `.\apps\hmi-app\run.ps1` | 启动 HMI | 可源码运行 |
| `.\apps\control-cli\run.ps1` | CLI 排查入口 | 根级入口已验证；真实 exe 仍在 `control-core\build\bin\**` |

DXF 编辑：

- 当前不部署内建 editor app
- 使用方式见 `docs/runtime/external-dxf-editing.md`

## 2. 发布前统一检查

```powershell
Set-Location D:\Projects\SiligenSuite
.\tools\scripts\install-python-deps.ps1
.\build.ps1 -Profile Local -Suite simulation
.\test.ps1 -Profile Local -Suite apps,packages,protocol-compatibility,integration,simulation -ReportDir integration\reports\verify\pre-release
.\ci.ps1 -Suite all
.\apps\control-tcp-server\run.ps1 -DryRun
.\apps\control-cli\run.ps1 -DryRun
.\apps\control-runtime\run.ps1 -DryRun
```

## 3. 现场部署说明

- 现场当前推荐交付“完整工作区”，不要默认裁成只含 `apps\` 的瘦包。
- 若现场需要 DXF 文件修改，使用 AutoCAD 等外部编辑器人工处理，不再交付内建 editor 组件。
- 若现场需要 HMI DXF 预览，仍需随包带上 `dxf-pipeline`。
