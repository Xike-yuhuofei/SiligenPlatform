# Developer Workflow

更新时间：`2026-03-25`

## 1. 唯一推荐工作方式

- 从仓库根目录开始。
- 优先使用根级 `build.ps1`、`test.ps1`、`ci.ps1`，验收默认使用 `scripts/validation/run-local-validation-gate.ps1`。
- 优先使用 `apps/`、`modules/`、`shared/` 下的 canonical 路径。
- DXF 编辑统一走外部编辑器人工流程，不再有内建 editor app。

## 2. 默认启动命令

```powershell
Set-Location <repo-root>
```

| 目标 | 推荐入口 | 备注 |
|---|---|---|
| HMI | `.\apps\hmi-app\run.ps1` | 官方默认入口；负责生成/注入 online gateway launch contract |
| Runtime Service | `siligen_runtime_service.exe` | 构建产物入口（`control-apps-build/bin/<Config>/`） |
| Runtime Gateway | `siligen_runtime_gateway.exe` | 构建产物入口（`control-apps-build/bin/<Config>/`） |
| Planner CLI | `siligen_planner_cli.exe` | 构建产物入口（`control-apps-build/bin/<Config>/`） |

DXF 编辑：

- 读取 `docs/runtime/external-dxf-editing.md`
- 使用 AutoCAD 等外部编辑器人工处理 DXF

## 3. 构建与测试入口

```powershell
.\build.ps1
.\test.ps1
.\ci.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

说明：

- 仓库默认门禁已切换为本地验证链路。
- HMI 的开发态在线启动默认允许 `apps/hmi-app/run.ps1` 临时生成 gateway launch contract；验证/CI/release 入口要求正式 `apps/hmi-app/config/gateway-launch.json` 已落地，否则会 fail fast。

## 4. 不再作为默认入口的旧路径

- `packages/`
- `integration/`
- `tools/`
- `examples/`
- `control-core/*`
- `apps/hmi-app/scripts/run.ps1` 直接 online 启动

## 5. 代码审查要求

1. PR 描述中的运行命令、测试命令、验证步骤，必须先给根级入口。
2. 新文档和新脚本不得再写已退出旧根为默认能力。
3. 若需求涉及 DXF 编辑，默认引用 `docs/runtime/external-dxf-editing.md`。
