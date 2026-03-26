# Rollback

更新时间：`2026-03-25`

## 1. 适用范围

本文档用于当前工作区回滚操作，适用于开发机、测试机与现场机。

回滚原则：

- runtime/planner 相关回滚对象以 canonical control-apps build root 产物为准。
- 机器配置与配方数据只回滚根级 `config\`、`data\`。
- 已退出目录不参与任何默认回滚流程。

## 2. 当前 canonical 入口

```powershell
Set-Location <repo-root>
```

回滚后确认入口：

| 入口 | 用途 |
|---|---|
| `.\apps\hmi-app\run.ps1` | 验证 HMI 启动 |
| `.\test.ps1` | 产出回滚后报告 |
| `config\machine\machine_config.ini` | canonical 配置源 |
| `data\recipes\` | canonical 配方路径 |

control-apps 产物检查：

```powershell
$ControlAppsBuildRoot = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
} else {
    Join-Path (Get-Location) "build\control-apps"
}

Test-Path (Join-Path $ControlAppsBuildRoot "bin\Debug\siligen_runtime_service.exe")
Test-Path (Join-Path $ControlAppsBuildRoot "bin\Debug\siligen_runtime_gateway.exe")
Test-Path (Join-Path $ControlAppsBuildRoot "bin\Debug\siligen_planner_cli.exe")
```

## 3. 操作步骤（开发/测试）

1. 停掉 HMI 与相关后台进程。
2. 恢复配置：

```powershell
Copy-Item .\backup\config\machine_config.ini .\config\machine\machine_config.ini -Force
```

3. 恢复配方：

```powershell
Copy-Item .\backup\data\recipes .\data\recipes -Recurse -Force
```

4. 如 control-apps 产物被覆盖，恢复产物目录。
5. 如 HMI launcher 配置改动，恢复 `apps\hmi-app\config\gateway-launch.json`。
6. 重新验证：

```powershell
.\apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart
.\test.ps1 -Profile Local -Suite contracts,protocol-compatibility,e2e -ReportDir tests\reports\verify\post-rollback
```

## 4. 常见失败点

1. 回滚了源码但未回滚 control-apps 产物目录。
2. 只恢复 `recipes/recipes`，未恢复 `recipes/versions`。
3. 继续使用已退出目录与别名流程，导致回滚步骤失效。

## 5. 与 legacy 路径的关系

- `packages/`、`integration/`、`tools/`、`examples/` 已退出，不作为回滚对象。
- `control-core/*` 路径不作为默认回滚入口。
- 回滚说明默认从根级 canonical 路径发起。