# Post-Refactor Runbook

更新时间：`2026-03-18`

## 1. 当前 canonical 入口

排查默认从工作区根开始：

```powershell
Set-Location D:\Projects\SiligenSuite
```

优先使用的入口：

| 入口 | 用途 | 当前状态 |
|---|---|---|
| `integration\reports\workspace-validation.md` | 最近一次统一验证结果 | 已存在 |
| `docs\architecture\system-acceptance-report.md` | 当前已验证事实与已知阻塞 | 已存在 |
| `.\apps\control-tcp-server\run.ps1 -DryRun` | 查 TCP server 入口是否能定位旧产物 | 已验证 |
| `.\apps\control-cli\run.ps1 -DryRun` | 查 CLI 入口是否能定位旧产物 | 已验证 |
| `.\apps\control-runtime\run.ps1 -DryRun` | 确认 runtime 当前仍是 `BLOCKED` | 已验证 |
| `.\apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart` | 查 HMI 根入口是否存在 | 已验证 |
| `.\test.ps1` | 复跑可重复验证 | 已验证 |

DXF 编辑问题处理：

- 当前不再排查内建 editor app
- 若问题涉及 DXF 文件编辑，按 `docs/runtime/external-dxf-editing.md` 处理人工流程

## 2. 常见失败点

1. `apps\control-runtime\run.ps1 -DryRun` 返回 `BLOCKED` 被误判成新故障。
2. `hardware-smoke` 脚本直接返回 0 被误判成真实现场链路通过。
3. 误以为仓库仍提供 `apps/dxf-editor-app`、`dxf-editor` 或 `packages/editor-contracts`。
4. 误以为系统仍支持 notify / CLI 编辑器协作协议。
5. HMI DXF 预览失败，其实是 `dxf-pipeline` 没跟着交付或 `SILIGEN_DXF_PIPELINE_ROOT` 指错。

## 3. 与 legacy 路径的关系

- `apps\control-tcp-server`、`apps\control-cli` 是当前排查入口，但真正的问题常落在 `control-core\build\bin\**` 产物或相关配置 fallback。
- `apps\control-runtime` 当前只有 canonical 名称，不是独立可执行排查目标。
- `dxf-pipeline\` 仍是 HMI DXF 预览的实际兼容依赖。
