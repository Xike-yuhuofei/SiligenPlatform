# Rollback

更新时间：`2026-03-19`

## 1. 适用范围

本文档用于当前工作区的回滚操作，适用三类场景：

- 开发机：改动后恢复到上一次可用本地状态
- 测试机：恢复测试环境并重新产出可比对报告
- 现场机：在启动失败、配置错配或兼容链路失效时快速恢复

回滚原则：

- `control-runtime`、`control-tcp-server`、`control-cli` 的默认回滚对象都是 canonical control-apps build root 下的真实 exe。
- 机器配置与配方数据默认只回滚根级 `config\`、`data\`。
- `control-core\build\bin\**` 不再是任何 control app 的默认回滚对象。

## 2. 当前 canonical 入口

```powershell
Set-Location D:\Projects\SiligenSuite
```

回滚后用于确认状态的入口：

| 入口 | 用途 |
|---|---|
| `.\apps\control-runtime\run.ps1` | 重新启动 runtime 宿主 |
| `.\apps\control-tcp-server\run.ps1` | 重新启动 TCP server |
| `.\apps\control-cli\run.ps1` | 验证 CLI |
| `.\apps\hmi-app\run.ps1` | 验证 HMI 启动 |
| `.\test.ps1` | 产出回滚后报告 |
| `config\machine\machine_config.ini` | canonical 配置源 |
| `data\recipes\` | canonical 配方路径 |

发布前建议最少备份：

- 工作区快照
- `config\`
- `data\recipes\`
- canonical control-apps build root 下的 `bin\Debug\` 或现场实际使用的配置目录
- `apps\hmi-app\config\gateway-launch.json`（如果有）

以下 PowerShell 示例统一使用当前的 control-apps build root 解析顺序：

```powershell
$ControlAppsBuildRoot = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
} else {
    Join-Path (Get-Location) "build\control-apps"
}
```

## 3. 操作步骤

### 3.1 开发 / 测试场景

1. 停掉正在运行的 HMI、TCP server、CLI 会话。
2. 恢复配置：

```powershell
Copy-Item .\backup\config\machine_config.ini .\config\machine\machine_config.ini -Force
Copy-Item .\backup\config\machine_config.ini .\config\machine_config.ini -Force
```

3. 恢复配方：

```powershell
Copy-Item .\backup\data\recipes .\data\recipes -Recurse -Force
```

4. 如 canonical control app 产物被覆盖，恢复整个目录：

```powershell
if (Test-Path (Join-Path $ControlAppsBuildRoot "bin\Debug")) {
    Remove-Item (Join-Path $ControlAppsBuildRoot "bin\Debug") -Recurse -Force
}
Copy-Item .\backup\control-apps-build-bin\Debug (Join-Path $ControlAppsBuildRoot "bin\Debug") -Recurse
```

5. 若 HMI 自动拉起配置也改过，恢复：

```powershell
Copy-Item .\backup\gateway-launch.json .\apps\hmi-app\config\gateway-launch.json -Force
```

6. 重新验证：

```powershell
.\apps\control-runtime\run.ps1 -DryRun
.\apps\control-tcp-server\run.ps1 -DryRun
.\apps\control-cli\run.ps1 -DryRun
.\test.ps1 -Profile Local -Suite packages,protocol-compatibility,simulation -ReportDir integration\reports\verify\post-rollback
```

### 3.2 现场场景

1. 停止 HMI 和 TCP server 进程。
2. 恢复 canonical 配置与 bridge：

```powershell
Copy-Item .\backup\config\machine_config.ini .\config\machine\machine_config.ini -Force
Copy-Item .\backup\config\machine_config.ini .\config\machine_config.ini -Force
```

3. 恢复配方组：

```powershell
Copy-Item .\backup\data\recipes .\data\recipes -Recurse -Force
```

4. 恢复 canonical control app 产物组：

```powershell
if (Test-Path (Join-Path $ControlAppsBuildRoot "bin\Debug")) {
    Remove-Item (Join-Path $ControlAppsBuildRoot "bin\Debug") -Recurse -Force
}
Copy-Item .\backup\control-apps-build-bin\Debug (Join-Path $ControlAppsBuildRoot "bin\Debug") -Recurse
```

5. 若自动拉起配置被改过，恢复 HMI 启动契约并清理临时环境变量：

```powershell
Copy-Item .\backup\gateway-launch.json .\apps\hmi-app\config\gateway-launch.json -Force
Remove-Item Env:SILIGEN_GATEWAY_EXE -ErrorAction SilentlyContinue
Remove-Item Env:SILIGEN_GATEWAY_LAUNCH_SPEC -ErrorAction SilentlyContinue
```

6. 从工作区根重启 TCP server：

```powershell
.\apps\control-tcp-server\run.ps1 --config .\config\machine\machine_config.ini
```

7. 若服务已起来，再确认端口：

```powershell
Test-NetConnection -ComputerName 127.0.0.1 -Port 9527
```

## 4. 常见失败点

1. 只回滚 `config\machine\machine_config.ini`，忘了 `config\machine_config.ini`。
2. 只替换 `siligen_tcp_server.exe`，没有一起回滚同目录 DLL。
3. 只恢复 `data\recipes\recipes\`，忘了 `data\recipes\versions\`，导致版本链不一致。
4. 忘了同步恢复 canonical control-apps build root，只回滚了源码或配置。
5. 回滚后直接双击 exe，而不是从工作区根走 wrapper，导致相对配置路径继续出错。
6. HIL 最小烟测已通过，却没有继续核对真实机台 / 长稳边界，就误判为现场验收完成。

## 5. 与 legacy 路径的关系

- `control-core\build\bin\**` 不再是任何 control app 的补充回滚对象。
- `control-core\config\machine_config.ini`、`control-core\src\infrastructure\resources\config\files\machine_config.ini`、`control-core\data\recipes\` 已退出默认回滚组。
- 回滚说明默认从根级 canonical 路径发起；不要再把 `control-core` 配置/数据副本当成首选恢复面。
