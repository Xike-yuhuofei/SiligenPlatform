# Rollback

更新时间：`2026-03-18`

## 1. 适用范围

本文档用于当前重构中期工作区的回滚操作，适用三类场景：

- 开发机：改动后恢复到上一次可用本地状态
- 测试机：恢复测试环境并重新产出可比对报告
- 现场机：在启动失败、配置错配或兼容壳链路失效时快速恢复

回滚原则：

- 当前仓库不是“只回 canonical 路径就够”的状态。
- 只要本次变更涉及配置、配方、旧产物或 HMI 自动拉起，就必须成组回滚。
- `apps\control-runtime` 当前没有独立 exe，不存在“切回 runtime app”这种回滚路径。

## 2. 当前 canonical 入口

回滚的默认入口仍从工作区根发起：

```powershell
Set-Location D:\Projects\SiligenSuite
```

回滚后用于确认状态的 canonical 入口：

| 入口 | 用途 | 当前状态 |
|---|---|---|
| `.\apps\control-tcp-server\run.ps1` | 回滚后重新启动 TCP server | 可用，但真实对象仍在 `control-core\build\bin\**` |
| `.\apps\hmi-app\run.ps1` | 回滚后验证 HMI 启动 | 可用 |
| `.\test.ps1` | 产出回滚后报告 | 可用 |
| `config\machine\machine_config.ini` | canonical 配置源 | 必须与 bridge 一起回滚 |
| `data\recipes\` | canonical 配方路径 | 回滚时建议整组恢复 |

发布前建议最少备份：

- 工作区快照
- `config\`
- `data\recipes\`
- `control-core\build\bin\Debug\` 或现场实际使用的配置目录
- `apps\hmi-app\config\gateway-launch.json`（如果有）

## 3. 操作步骤

### 3.1 开发场景回滚

适用情况：

- 开发机改了配置、脚本或旧产物后，本地链路跑不通
- 需要回到上一次可工作的 mock/simulation 状态

步骤：

1. 停掉正在运行的 HMI、DXF editor、TCP server
2. 恢复配置：

```powershell
Copy-Item .\backup\config\machine_config.ini .\config\machine\machine_config.ini -Force
Copy-Item .\backup\config\machine_config.ini .\config\machine_config.ini -Force
```

3. 如有修改过旧配置 fallback，再一并恢复：

```powershell
Copy-Item .\backup\config\machine_config.ini .\control-core\config\machine_config.ini -Force
```

4. 如改动了配方或模板，恢复 `data\recipes\`：

```powershell
Copy-Item .\backup\data\recipes .\data\recipes -Recurse -Force
```

5. 如 TCP server 旧产物被覆盖，恢复整个目录：

```powershell
if (Test-Path .\control-core\build\bin\Debug) {
    Remove-Item .\control-core\build\bin\Debug -Recurse -Force
}
Copy-Item .\backup\control-core-build-bin\Debug .\control-core\build\bin\Debug -Recurse
```

6. 重新验证：

```powershell
.\apps\control-tcp-server\run.ps1 -DryRun
.\test.ps1 -Profile Local -Suite packages,protocol-compatibility,simulation -ReportDir integration\reports\verify\post-rollback-dev
```

### 3.2 测试场景回滚

适用情况：

- 测试机上的配置、配方或旧产物被新包覆盖
- 需要恢复到上一版可出报告的状态

步骤：

1. 停掉现场/测试相关进程
2. 恢复配置组：

```powershell
Copy-Item .\backup\config\machine_config.ini .\config\machine\machine_config.ini -Force
Copy-Item .\backup\config\machine_config.ini .\config\machine_config.ini -Force
```

3. 恢复配方组：

```powershell
Copy-Item .\backup\data\recipes .\data\recipes -Recurse -Force
```

4. 恢复旧产物组：

```powershell
if (Test-Path .\control-core\build\bin\Debug) {
    Remove-Item .\control-core\build\bin\Debug -Recurse -Force
}
Copy-Item .\backup\control-core-build-bin\Debug .\control-core\build\bin\Debug -Recurse
```

5. 若 HMI 自动拉起配置也改过，恢复：

```powershell
Copy-Item .\backup\gateway-launch.json .\apps\hmi-app\config\gateway-launch.json -Force
```

6. 重新生成测试报告：

```powershell
.\test.ps1 -Profile Local -Suite all -ReportDir integration\reports\verify\post-rollback-test
```

### 3.3 现场场景回滚

适用情况：

- `siligen_tcp_server.exe` 启动即退出
- 改完现场参数后配置失效
- HMI 自动拉起指向了错误 exe 或错误工作目录
- 现场只能靠上一版旧产物恢复

步骤：

1. 停止 HMI 和 TCP server 进程
2. 恢复 canonical 配置与 bridge：

```powershell
Copy-Item .\backup\config\machine_config.ini .\config\machine\machine_config.ini -Force
Copy-Item .\backup\config\machine_config.ini .\config\machine_config.ini -Force
```

3. 如果现场在发布时同步过 legacy fallback，也一起恢复：

```powershell
Copy-Item .\backup\config\machine_config.ini .\control-core\config\machine_config.ini -Force
Copy-Item .\backup\config\machine_config.ini .\control-core\src\infrastructure\resources\config\files\machine_config.ini -Force
```

4. 恢复配方组：

```powershell
Copy-Item .\backup\data\recipes .\data\recipes -Recurse -Force
```

5. 恢复旧产物组：

```powershell
if (Test-Path .\control-core\build\bin\Debug) {
    Remove-Item .\control-core\build\bin\Debug -Recurse -Force
}
Copy-Item .\backup\control-core-build-bin\Debug .\control-core\build\bin\Debug -Recurse
```

6. 若自动拉起配置被改过，恢复 HMI 启动契约并清理临时环境变量：

```powershell
Copy-Item .\backup\gateway-launch.json .\apps\hmi-app\config\gateway-launch.json -Force
Remove-Item Env:SILIGEN_GATEWAY_EXE -ErrorAction SilentlyContinue
Remove-Item Env:SILIGEN_GATEWAY_LAUNCH_SPEC -ErrorAction SilentlyContinue
Remove-Item Env:SILIGEN_DXF_PIPELINE_ROOT -ErrorAction SilentlyContinue
```

7. 从工作区根重启 TCP server：

```powershell
.\apps\control-tcp-server\run.ps1 --config .\config\machine\machine_config.ini
```

8. 若服务已起来，再确认端口：

```powershell
Test-NetConnection -ComputerName 127.0.0.1 -Port 9527
```

9. 重新生成一份回滚后报告：

```powershell
.\test.ps1 -Profile Local -Suite packages,protocol-compatibility,simulation -ReportDir integration\reports\verify\post-rollback-field
```

## 4. 常见失败点

1. 只回滚 `config\machine\machine_config.ini`，忘了 `config\machine_config.ini`。
2. 只替换 `siligen_tcp_server.exe`，没有一起回滚同目录 DLL。
3. 只恢复 `data\recipes\recipes\`，忘了 `data\recipes\versions\`，导致版本链不一致。
4. 现场已改过 `gateway-launch.json`，却没有把它纳入回滚范围。
5. 认为 `apps\control-runtime` 可以作为应急回退入口；当前它没有独立 exe。
6. 回滚后直接双击 exe，而不是从工作区根走 wrapper，导致相对配置路径继续出错。
7. 命中 `IDiagnosticsPort 未注册` 后仍继续现场验收，而不是先停下来恢复已验证版本。

## 5. 与 legacy 路径的关系

- `apps\control-tcp-server` 是回滚后的外部入口，但真实回滚对象是 `control-core\build\bin\**\siligen_tcp_server.exe` 及其同目录依赖。
- `apps\control-cli` 是外部入口，但真实回滚对象是 `control-core\build\bin\**\siligen_cli.exe`。
- `apps\control-runtime` 仅保留 canonical 名称，当前不是可执行回滚目标。
- `control-core\config\machine_config.ini` 与 `control-core\src\infrastructure\resources\config\files\machine_config.ini` 仍是兼容 fallback，配置回滚时不能假装它们不存在。
- `dxf-pipeline\` 不是默认入口，但若现场 HMI 依赖 DXF 预览，它仍是需要被保留和恢复的兼容路径。

结论：

- 回滚说明默认从根级 canonical 路径发起。
- 但当前真正需要恢复的对象，仍包含多条 legacy 路径和兼容壳产物，必须按组恢复。
