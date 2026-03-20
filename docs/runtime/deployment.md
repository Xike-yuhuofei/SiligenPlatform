# Deployment

更新时间：`2026-03-19`

## 1. 当前 canonical 入口

部署与验证默认从工作区根发起：

```powershell
Set-Location D:\Projects\SiligenSuite
```

部署相关 canonical 入口：

| 入口 | 用途 | 当前状态 |
|---|---|---|
| `.\tools\scripts\install-python-deps.ps1` | 安装 Python 依赖 | 已验证 |
| `.\build.ps1 -Profile Local -Suite apps,simulation` | 构建交付产物 | 已验证 |
| `.\test.ps1` | 生成交付前/交付后报告 | 已验证 |
| `.\legacy-exit-check.ps1` | 验证 legacy fallback 未回流 | 已验证 |
| `.\apps\control-runtime\run.ps1` | 启动 runtime 宿主 | 根级入口已验证；真实 exe 为 canonical `siligen_control_runtime.exe` |
| `.\apps\control-tcp-server\run.ps1` | 启动 TCP server / HIL 入口 | 根级入口已验证；真实 exe 为 canonical `siligen_tcp_server.exe` |
| `.\apps\control-cli\run.ps1` | CLI 排查入口 | 根级入口已验证；默认真实 exe 为 canonical `siligen_cli.exe` |
| `config\machine\machine_config.ini` | 机器配置 canonical 源 | 已验证 |
| `data\recipes\` | 配方数据 canonical 源 | 已验证 |

DXF 编辑：

- 当前不部署内建 editor app
- 使用方式见 `docs/runtime/external-dxf-editing.md`

## 2. 发布前统一检查

```powershell
Set-Location D:\Projects\SiligenSuite
.\tools\scripts\install-python-deps.ps1
.\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\pre-release-legacy-exit
.\build.ps1 -Profile Local -Suite apps,simulation
.\test.ps1 -Profile Local -Suite apps,packages,protocol-compatibility,integration,simulation -ReportDir integration\reports\verify\pre-release
.\ci.ps1 -Suite all
.\apps\control-runtime\run.ps1 -DryRun
.\apps\control-tcp-server\run.ps1 -DryRun
.\apps\control-cli\run.ps1 -DryRun
python .\integration\hardware-in-loop\run_hardware_smoke.py
```

## 3. 现场部署说明

- 现场当前推荐交付“完整工作区”或等价的 canonical 发布包，不要默认裁成只含 `apps\` 的瘦包。
- 机器配置与配方数据默认从根级 `config\`、`data\` 发布；不要再同步 `control-core\config` 或 `control-core\data\recipes` 作为默认副本。
- HIL/烟测默认入口是 `integration\hardware-in-loop\run_hardware_smoke.py`，其目标可执行文件为 `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe`。
- CLI 只随包发布 canonical `siligen_cli.exe`；不得把 `control-core\build\bin\**` 重新写回默认启动说明。
