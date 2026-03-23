# Developer Workflow

更新时间：`2026-03-23`

## 1. 唯一推荐工作方式

- 从仓库根目录开始。
- 优先使用根级 `build.ps1`、`test.ps1`、`ci.ps1`，验收默认使用 `tools/scripts/run-local-validation-gate.ps1`。
- 优先使用 `apps/`、`packages/` 下的 canonical 路径。
- DXF 编辑统一走外部编辑器人工流程，不再有 editor app。

## 2. 默认启动命令

```powershell
Set-Location <repo-root>
```

| 目标 | 推荐命令 | 备注 |
|---|---|---|
| HMI | `.\apps\hmi-app\run.ps1` | 官方默认入口。 |
| TCP server | `.\apps\control-tcp-server\run.ps1` | 官方默认入口；默认只认 canonical `siligen_tcp_server.exe`，已移除 legacy fallback。 |
| CLI | `.\apps\control-cli\run.ps1` | 官方默认入口；默认只认 canonical `siligen_cli.exe`，不再支持 legacy fallback。 |
| Runtime | `.\apps\control-runtime\run.ps1` | 官方默认入口；默认只认 canonical `siligen_control_runtime.exe`，已移除 legacy fallback。 |

DXF 编辑：

- 读取 `docs/runtime/external-dxf-editing.md`
- 使用 AutoCAD 等外部编辑器人工处理 DXF

## 3. 构建与测试入口

```powershell
.\build.ps1
.\test.ps1
.\ci.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-local-validation-gate.ps1
```

说明：

- 仓库默认门禁已切换为本地验证链路，不再依赖 GitHub Actions checks。

## 4. 不再作为默认入口的旧路径

- `dxf-editor`
- `apps/dxf-editor-app`
- `packages/editor-contracts`
- `hmi-client`（已删除；历史材料见 `docs/_archive/2026-03/hmi-client/`）
- `dxf-pipeline`
- `control-core/apps/*`
- `control-core/build/bin/*`

## 5. 代码审查要求

1. PR 描述中的运行命令、测试命令、验证步骤，必须先给根级入口。
2. 新文档和新脚本不得再写 `dxf-editor`、`apps/dxf-editor-app`、`packages/editor-contracts` 为默认能力。
3. 若需求涉及 DXF 编辑，默认引用 `docs/runtime/external-dxf-editing.md`。
