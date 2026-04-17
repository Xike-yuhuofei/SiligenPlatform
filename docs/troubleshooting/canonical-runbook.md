# Canonical Runbook

更新时间：`2026-04-17`

## 1. 当前排障入口

排查默认从工作区根开始：

```powershell
Set-Location <repo-root>
```

优先使用的入口：

| 入口 | 用途 | 当前状态 |
|---|---|---|
| `tests\reports\workspace-validation.md` | 最近一次统一验证结果 | 已存在 |
| `tests\reports\ci\legacy-exit\legacy-exit-checks.md` | legacy 回流检查 | 已存在 |
| `.\test.ps1 -Suite contracts` | 复跑核心回归 | 已验证 |
| `.\apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart` | 检查 HMI 启动链 | 已验证 |
| `python .\tests\e2e\hardware-in-loop\run_hardware_smoke.py` | 检查 HIL 最小链路 | 已验证 |

control-apps 产物检查：

当前约束：`SILIGEN_CONTROL_APPS_BUILD_ROOT` 只能显式指向当前 worktree 仓内 `.\build` 树；联机链路不得解析或使用任何 `%LOCALAPPDATA%` 下的 build root。

```powershell
. .\scripts\validation\tooling-common.ps1
$controlAppsRoot = Get-ControlAppsBuildRoot -WorkspaceRoot (Get-Location)

Test-Path (Join-Path $controlAppsRoot "bin\Debug\siligen_runtime_service.exe")
Test-Path (Join-Path $controlAppsRoot "bin\Debug\siligen_runtime_gateway.exe")
Test-Path (Join-Path $controlAppsRoot "bin\Debug\siligen_planner_cli.exe")
```

## 2. 常见失败点

1. canonical control-apps build root 下缺少 exe，导致运行链直接失败。
2. 误按旧文档恢复已删除目录或旧别名，导致排障步骤与仓内事实冲突。
3. 把历史仓路径误当默认入口，导致修改没有进入真实链路。
4. `hardware-smoke` 仅覆盖最小启动闭环，被误判为完整现场验收。
5. 直接调用 `apps/hmi-app/scripts/run.ps1` 进入 online 模式，绕过官方入口的 launch contract 生成/注入，现已被显式阻断。

## 3. 与 legacy 路径的关系

- `packages/`、`integration/`、`tools/`、`examples/` 已删除，不再作为排障对象。
- `control-core/*` 路径不再是默认排障对象。
- legacy gateway/tcp alias 已删除；`legacy-exit-check.ps1` 会拦截其回流。

## 4. 多 Worktree Build Root 约束

多 worktree 同机联机时，build root 只能绑定到“当前 worktree 自己仓内的构建目录”：

- 每个 worktree 只允许使用自己仓内的 `build\` 树。
- 禁止使用任何 `%LOCALAPPDATA%` 下的 build root，包括 `LOCALAPPDATA\SS\cab-*` 与 `LOCALAPPDATA\SiligenSuite\control-apps-build`。
- 联机、HMI、gateway、HIL 只允许解析并启动当前 worktree 仓内 `build\` 树下的 exe。

建议在当前 worktree shell 内显式固定：

```powershell
Set-Location <current-worktree-root>
. .\scripts\validation\tooling-common.ps1

$env:SILIGEN_CONTROL_APPS_BUILD_ROOT = (Join-Path (Get-Location) "build\\ca")
$controlAppsRoot = Get-ControlAppsBuildRoot -WorkspaceRoot (Get-Location)
$controlAppsRoot
```

默认解析顺序固定为：`build\ca -> build\control-apps -> build`。
