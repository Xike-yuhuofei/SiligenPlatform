# Post-Refactor Runbook

更新时间：`2026-03-19`

## 1. 当前 canonical 入口

排查默认从工作区根开始：

```powershell
Set-Location D:\Projects\SiligenSuite
```

优先使用的入口：

| 入口 | 用途 | 当前状态 |
|---|---|---|
| `integration\reports\workspace-validation.md` | 最近一次统一验证结果 | 已存在 |
| `integration\reports\legacy-exit\legacy-exit-checks.md` | legacy fallback 回流检查 | 已存在 |
| `.\apps\control-runtime\run.ps1 -DryRun` | 查 runtime 宿主 canonical 入口 | 已验证 |
| `.\apps\control-tcp-server\run.ps1 -DryRun` | 查 TCP server canonical 入口 | 已验证 |
| `.\apps\control-cli\run.ps1 -DryRun` | 查 CLI canonical 入口 | 已验证 |
| `python .\integration\hardware-in-loop\run_hardware_smoke.py` | 查 HIL 默认入口 | 已验证 |
| `.\apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart` | 查 HMI 根入口是否存在 | 已验证 |
| `.\test.ps1` | 复跑可重复验证 | 已验证 |

DXF 编辑问题处理：

- 当前不再排查内建 editor app
- 若问题涉及 DXF 文件编辑，按 `docs/runtime/external-dxf-editing.md` 处理人工流程

## 2. 常见失败点

1. canonical control-apps build root 下缺少 exe，导致 `run.ps1 -DryRun` 直接失败。
2. 机器配置只恢复了 `config\machine\machine_config.ini`，忘了同步 `config\machine_config.ini` bridge。
3. 把 `control-core\config`、`control-core\data\recipes` 误当成默认排查入口，导致修改没有进入真实链路。
4. 误以为 `control-cli` 仍支持 `-UseLegacyFallback`，导致按旧文档排障失败。
5. `hardware-smoke` 已通过最小 canonical 启动闭环，却被误判成“已经完成真实机台验收”。

## 3. 与 legacy 路径的关系

- `control-core\build\bin\**` 不再是 `control-runtime`、`control-tcp-server` 的默认排查对象。
- `control-core\config`、`control-core\data\recipes`、`control-core\src\infrastructure\resources\config\files\recipes\schemas` 已退出默认 fallback。
- `control-core\build\bin\**\siligen_cli.exe` 不再是 CLI 的排障入口。
- legacy gateway/tcp alias 已删除；`legacy-exit-check.ps1` 会拦截其回流到 `control-core` 注册。
