# Phase 15 C1 Real Hardware Assets And Config Prep

## 任务结论

- canonical 机器配置入口已冻结为 `config/machine/machine_config.ini`。
- canonical MultiCard vendor 资产目录已冻结为 `modules/runtime-execution/adapters/device/vendor/multicard/`。
- `apps/runtime-service/run.ps1` 与 `apps/runtime-gateway/run.ps1` 已补充真实硬件 bring-up 所需的最小参数入口与启动前检查，不涉及 C++ 实现或 CMake owner 逻辑改动。

## 仓内已存在资产

- `config/machine/machine_config.ini`
- `config/machine/README.md`
- `modules/runtime-execution/adapters/device/vendor/multicard/README.md`
- `modules/runtime-execution/adapters/device/vendor/multicard/MultiCard.def`
- `modules/runtime-execution/adapters/device/vendor/multicard/MultiCard.exp`
- `apps/runtime-service/run.ps1`
- `apps/runtime-service/README.md`
- `apps/runtime-gateway/run.ps1`
- `apps/runtime-gateway/README.md`

## 仓外仍需现场补齐的资产

- `MultiCard.dll`
  - 用途：真实硬件运行时动态加载。
  - 默认放置位置：`modules/runtime-execution/adapters/device/vendor/multicard/`
  - 允许覆盖方式：`-VendorDir <path>` 或当前 shell 会话内 `SILIGEN_MULTICARD_VENDOR_DIR`
- `MultiCard.lib`
  - 用途：真实硬件版本重新构建时的链接输入。
  - 默认放置位置：`modules/runtime-execution/adapters/device/vendor/multicard/`
  - 允许覆盖方式：同上

## bring-up 前必须确认的配置项

- `[Hardware] mode=Real`
- `[Hardware] pulse_per_mm` 与 `[Machine] pulse_per_mm` 一致
- `[Network] control_card_ip`
- `[Network] local_ip`
- `[Network] timeout_ms`
- `[Homing_Axis1]` ~ `[Homing_Axis4]` 的回零方向、距离、输入极性
- `[Interlock] emergency_stop_input`
- `[Interlock] emergency_stop_active_low`
- `[Interlock] safety_door_input`
- `[VelocityTrace] output_path` 指向现场可写目录
- `[Debug] log_file` 与应用默认日志目录策略是否符合现场收集要求

## 启动脚本使用方式

`runtime-service`：

```powershell
./apps/runtime-service/run.ps1 -BuildConfig Debug
./apps/runtime-service/run.ps1 -ConfigPath config/machine/machine_config.ini -VendorDir D:\Vendor\MultiCard
./apps/runtime-service/run.ps1 -DryRun
```

`runtime-gateway`：

```powershell
./apps/runtime-gateway/run.ps1 -BuildConfig Debug
./apps/runtime-gateway/run.ps1 -ConfigPath config/machine/machine_config.ini -VendorDir D:\Vendor\MultiCard -- --port 9527
./apps/runtime-gateway/run.ps1 -DryRun
```

## fail-fast 行为

- 若 canonical config 或覆盖 config 不存在，脚本立即终止。
- 若 `Hardware.mode=Real` 且 vendor 目录不存在，脚本立即终止。
- 若 `Hardware.mode=Real` 且 `MultiCard.dll` 不存在，脚本立即终止。
- 若 `Hardware.mode=Real` 且 `MultiCard.lib` 不存在，脚本发出告警；已构建二进制可继续尝试启动，但真实硬件版本重建不可继续。

## 现场前置条件

- 工控机已安装并可访问真实 MultiCard 板卡驱动环境。
- `control_card_ip` 与本机网卡地址已按现场网络规划完成设置。
- 急停/互锁输入接线和极性已通过现场电气确认。
- 目标主机对日志目录及 `VelocityTrace.output_path` 具有写权限。
- 若使用外部 vendor 覆盖目录，该目录内容与仓内 canonical 目录的文件命名保持一致。
