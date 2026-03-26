# Hardware Bring-Up Smoke SOP

更新时间：`2026-03-26`

## 1. 目的与适用范围

本文定义首轮真实硬件 bring-up / smoke 的现场标准操作流程，用于确认以下最小闭环已经成立：

- canonical 机器配置可被 `runtime-service` / `runtime-gateway` 正确加载
- MultiCard vendor 资产可被真实硬件入口识别
- 急停、限位、回零前提、点动、I/O、触发链路具备最小可观测性
- 失败时可立即停止、回滚并保留证据

本文不是正式现场放行依据，也不替代 HIL、长稳、异常恢复和工艺质量签收。

## 2. 执行角色与标记

- `[人工]`：必须由现场人员肉眼、手动或电气方式确认
- `[脚本]`：可直接执行 PowerShell 命令
- `[人工+脚本]`：脚本负责启动/采集，现场人员负责动作与结论

## 3. canonical 入口

- 仓库根：`D:\Projects\SiligenSuite`
- 机器配置：`config/machine/machine_config.ini`
- vendor 目录：`modules/runtime-execution/adapters/device/vendor/multicard`
- 宿主入口：`apps/runtime-service/run.ps1`
- 网关入口：`apps/runtime-gateway/run.ps1`
- 可选操作界面：`apps/hmi-app/run.ps1`
- 回滚基线：`docs/runtime/rollback.md`

## 4. 执行前硬停止条件

满足任一条件，立即停止，不允许继续上板：

- `config/machine/machine_config.ini` 不存在，或内容与当前机型/接线不一致
- `modules/runtime-execution/adapters/device/vendor/multicard/MultiCard.dll` 缺失
- 现场需要重建二进制，但 `modules/runtime-execution/adapters/device/vendor/multicard/MultiCard.lib` 缺失
- 急停、限位、互锁接线或极性未完成现场确认
- 轴体未脱离干涉区，或工作台上存在可能造成碰撞的治具/工件
- 现场无法写入证据目录或默认日志目录

## 5. 证据目录与基础变量

在仓库根 PowerShell 会话执行：

```powershell
Set-Location D:\Projects\SiligenSuite

$EvidenceRoot = Join-Path (Get-Location) ("tests\reports\hardware-smoke\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$ConfigPath = (Resolve-Path ".\config\machine\machine_config.ini").Path
$VendorDir = (Resolve-Path ".\modules\runtime-execution\adapters\device\vendor\multicard").Path
$ControlAppsBuildRoot = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
} else {
    Join-Path (Get-Location) "build\control-apps"
}
```

建议在 `$EvidenceRoot` 中至少保留下列文件：

- `machine_config.snapshot.ini`
- `vendor-assets.txt`
- `runtime-service-dryrun.txt`
- `runtime-gateway-dryrun.txt`
- `smoke-checklist.txt`
- `rollback-notes.txt`

## 6. 通电前检查

### 6.1 机器与环境检查

- `[人工]` 确认工作区域清空，轴体、针头、阀体、治具无干涉风险。
- `[人工]` 确认气源、供胶、供电、网线、地线均已接入且无明显松动。
- `[人工]` 确认当前机型只配置了急停输入，没有安全门输入；`[Interlock] safety_door_input=-1` 与现场一致。
- `[人工]` 确认急停按钮可物理触达，现场所有参与人员已知道停机口令和断电顺序。

### 6.2 机器配置检查

先保存配置快照：

```powershell
Copy-Item $ConfigPath (Join-Path $EvidenceRoot "machine_config.snapshot.ini") -Force
Get-Content $ConfigPath | Set-Content (Join-Path $EvidenceRoot "machine_config.snapshot.txt")
```

必须人工核对以下键值：

- `[Hardware] mode=Real`
- `[Hardware] num_axes=2`
- `[Machine] pulse_per_mm=200`
- `[Hardware] pulse_per_mm=200`
- `[Network] control_card_ip=192.168.0.1`
- `[Network] local_ip=192.168.0.200`
- `[Interlock] emergency_stop_input=7`
- `[Interlock] emergency_stop_active_low=false`
- `[VelocityTrace] output_path=D:\Projects\SiligenSuite\logs\velocity_trace`

若任一值与现场机型、网卡规划或接线不一致，先修正备份策略和审批，再停止本 SOP，不得临场口头忽略。

### 6.3 vendor 资产检查

```powershell
@(
    (Join-Path $VendorDir "MultiCard.def"),
    (Join-Path $VendorDir "MultiCard.exp"),
    (Join-Path $VendorDir "MultiCard.dll"),
    (Join-Path $VendorDir "MultiCard.lib")
) | ForEach-Object {
    [pscustomobject]@{
        Path = $_
        Exists = Test-Path $_
    }
} | Tee-Object -FilePath (Join-Path $EvidenceRoot "vendor-assets.txt")
```

判定规则：

- `[脚本]` `MultiCard.dll` 缺失：硬停止，不允许继续。
- `[脚本]` `MultiCard.lib` 缺失：若本次需要现场重建或替换二进制，硬停止；若只验证既有 `Debug` 二进制，可记录偏差后提交 `A4` 审核。

### 6.4 二进制与 dry-run 检查

```powershell
Test-Path (Join-Path $ControlAppsBuildRoot "bin\Debug\siligen_runtime_service.exe")
Test-Path (Join-Path $ControlAppsBuildRoot "bin\Debug\siligen_runtime_gateway.exe")

.\apps\runtime-service\run.ps1 -BuildConfig Debug -VendorDir $VendorDir -DryRun 2>&1 |
    Tee-Object -FilePath (Join-Path $EvidenceRoot "runtime-service-dryrun.txt")

.\apps\runtime-gateway\run.ps1 -BuildConfig Debug -VendorDir $VendorDir -DryRun 2>&1 |
    Tee-Object -FilePath (Join-Path $EvidenceRoot "runtime-gateway-dryrun.txt")
```

判定规则：

- `[脚本]` dry-run 返回非零：停止，先处理配置或 vendor 资产问题。
- `[脚本]` dry-run 输出中的 `config=` 必须指向 `D:\Projects\SiligenSuite\config\machine\machine_config.ini`。

## 7. 上电后安全检查

### 7.1 急停与互锁

- `[人工+脚本]` 启动前保持急停可用但不按下。
- `[人工]` 手动按下急停，确认驱动/板卡进入预期保护态。
- `[人工]` 释放急停，确认机械、电气和面板状态恢复到可继续检测的安全待机态。
- `[人工]` 若急停动作后 X/Y 轴仍可被驱动、状态灯异常或需要多次复位才恢复，立即停止并记录。

当前配置口径：

- 急停输入位：`X7`
- 极性：`高电平=正常，低电平=触发`

### 7.2 限位与运动边界

- `[人工]` 核对 X/Y 当前初始位置在软限位内。
- `[人工]` 确认 `[Machine]` 与 `[Hardware]` 的运动量纲一致后，才允许进入点动或回零前检查。
- `[人工]` 若任何轴已接近机械端点，不做回零和大步长点动，先人工退到安全区。

## 8. 回零前检查

当前配置要求重点核对：

- 轴数量：2 轴
- `Homing_Axis1.direction=0`
- `Homing_Axis2.direction=0`
- `Homing_Axis1.search_distance=485.0`
- `Homing_Axis2.search_distance=400.0`
- `Homing_Axis1.enable_limit_switch=true`
- `Homing_Axis2.enable_limit_switch=true`

执行要求：

- `[人工]` 回零前确认轴正负方向认知一致，且回零方向不会直接撞向机械硬限位。
- `[人工]` 回零速度、搜索距离、逃逸距离必须与现场丝杆、行程和传感器安装位置匹配。
- `[人工]` 若未完成该项，不允许进行回零相关 smoke。

## 9. 首轮 smoke 执行顺序

### 9.1 启动服务

建议使用两个独立 PowerShell 窗口。

窗口 A：

```powershell
Set-Location D:\Projects\SiligenSuite
.\apps\runtime-service\run.ps1 -BuildConfig Debug -VendorDir $VendorDir
```

窗口 B：

```powershell
Set-Location D:\Projects\SiligenSuite
.\apps\runtime-gateway\run.ps1 -BuildConfig Debug -VendorDir $VendorDir -- --port 9527
```

可选操作界面：

```powershell
Set-Location D:\Projects\SiligenSuite
.\apps\hmi-app\run.ps1 -Mode online -BuildConfig Debug -DisableGatewayAutostart
```

### 9.2 点动 smoke

- `[人工]` 先做最小步长、低速、单轴点动，只验证 X/Y 方向正确、运动可停、无异常振动。
- `[人工]` 点动顺序建议：`X+`、`X-`、`Y+`、`Y-`。
- `[人工]` 每次点动后确认当前位置仍在软限位内。
- `[人工]` 若方向相反、无法停止、运动抖动明显或出现意外冲击，立即执行停止条件。

### 9.3 I/O smoke

- `[人工]` 在不带胶、不接触产品的条件下，仅验证阀输出与关断语义。
- `[人工]` 核对 `[ValveSupply] do_bit_index=2` 对应现场 DO 线序。
- `[人工]` 核对 `[ValveDispenser] cmp_channel=1`、`cmp_axis_mask=3` 与当前 X/Y 主链一致。
- `[人工]` 若任何输出与预期对象不一致，立即停止，不做后续触发 smoke。

### 9.4 触发 smoke

- `[人工]` 仅在点动、I/O、急停检查都通过后进行。
- `[人工]` 使用最小风险空跑路径，确认触发能按预期建立、触发后可停止、停止后无残留输出。
- `[人工]` 若出现持续输出、重复触发、停机后仍有阀动作，立即断开相关执行链。

## 10. 停止条件

满足任一条件，立即停止当前 smoke，并记录时间点、动作和现象：

- 急停按下后设备未进入安全态
- 轴运动方向与预期相反
- 轴体接近硬限位、发生干涉、异响、剧烈振动或温升异常
- 日志出现持续超时、连接失败、互锁错误、硬件初始化失败
- I/O 输出对象错误、阀动作异常、停止后仍持续输出
- 任何人对当前配置、接线或安全状态失去把握

建议最小停机动作：

1. 立即停止手头点动/触发动作。
2. 按下急停。
3. 记录当前时间、轴位置、触发前最后一个动作。
4. 关闭 `runtime-gateway` 与 `runtime-service` 进程。
5. 若怀疑配置污染，转入回滚流程。

## 11. 失败回滚

本 SOP 的回滚以 `docs/runtime/rollback.md` 为基线，最小动作如下：

```powershell
Set-Location D:\Projects\SiligenSuite

Get-Process siligen_runtime_gateway,siligen_runtime_service -ErrorAction SilentlyContinue |
    Stop-Process -Force

Copy-Item .\backup\config\machine_config.ini .\config\machine\machine_config.ini -Force
```

若本次 smoke 修改过以下内容，也必须恢复：

- `$env:SILIGEN_MULTICARD_VENDOR_DIR`
- `apps/hmi-app/config/gateway-launch.json` 或临时 launch contract
- `control-apps-build` 下现场替换过的可执行文件或 DLL

回滚后至少执行以下确认：

```powershell
Set-Location D:\Projects\SiligenSuite
.\apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart
```

如需完整回滚，请继续执行 `docs/runtime/rollback.md` 中的配方恢复、产物恢复和验证步骤。

## 12. 日志与证据采集

最少采集项：

- `$EvidenceRoot\machine_config.snapshot.ini`
- `$EvidenceRoot\vendor-assets.txt`
- `$EvidenceRoot\runtime-service-dryrun.txt`
- `$EvidenceRoot\runtime-gateway-dryrun.txt`
- `logs\control_runtime.log`
- `logs\tcp_server.log`
- `D:\Projects\SiligenSuite\logs\velocity_trace\`
- 现场照片/视频：急停状态、轴初始位置、I/O 指示灯、异常现象

建议命令：

```powershell
Set-Location D:\Projects\SiligenSuite

if (Test-Path ".\logs\control_runtime.log") {
    Copy-Item ".\logs\control_runtime.log" (Join-Path $EvidenceRoot "control_runtime.log") -Force
}

if (Test-Path ".\logs\tcp_server.log") {
    Copy-Item ".\logs\tcp_server.log" (Join-Path $EvidenceRoot "tcp_server.log") -Force
}

if (Test-Path "D:\Projects\SiligenSuite\logs\velocity_trace") {
    Copy-Item "D:\Projects\SiligenSuite\logs\velocity_trace" (Join-Path $EvidenceRoot "velocity_trace") -Recurse -Force
}
```

## 13. 与后续任务的衔接

- `C1` 提供 canonical 配置、vendor 资产约束和运行脚本 preflight，本 SOP 直接消费这些事实。
- `C3` 将补充观察/记录脚本；在其完成前，本文默认人工创建 `$EvidenceRoot` 并手工归档证据。
- `A4` 将基于本文、`C1` 资产准备状态和 `C3` 观察脚本状态做 go/no-go 进入门评审。
