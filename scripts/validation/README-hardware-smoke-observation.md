# Hardware Smoke Observation

## 目的

这组脚本用于在首轮真实硬件 smoke 前快速生成结构化观察报告，帮助区分三类阻塞：

- 配置问题
- vendor 资产问题
- owner / build / dry-run 边界问题

它们只做观察、记录和证据整理，不直接执行真实硬件动作。

## 脚本

- `scripts/validation/run-hardware-smoke-observation.ps1`
  - 生成当前工作区的硬件 smoke 观察报告
  - 默认执行 `runtime-service` / `runtime-gateway` 的严格 dry-run 与诊断 dry-run
  - 输出 `summary.md` / `summary.json` 和 dry-run 日志
- `scripts/validation/register-hardware-smoke-observation.ps1`
  - 将一次观察结果登记为 intake 记录
  - 供 `A4` 汇总或人工现场归档使用

## 默认输入

- machine config：`config/machine/machine_config.ini`
- vendor 目录：`modules/runtime-execution/adapters/device/vendor/multicard`
- runtime-service 脚本：`apps/runtime-service/run.ps1`
- runtime-gateway 脚本：`apps/runtime-gateway/run.ps1`
- build root：显式设置 `$env:SILIGEN_CONTROL_APPS_BUILD_ROOT` 时使用该路径；否则只认携带当前工作区匹配 `CMakeCache.txt` 的 `build\ca`

## 默认输出

观察报告默认输出到：

- `tests/reports/hardware-smoke-observation/<timestamp>/`

其中至少包含：

- `hardware-smoke-observation-summary.md`
- `hardware-smoke-observation-summary.json`
- `logs/runtime-service-dryrun-strict.log`
- `logs/runtime-service-dryrun-diagnostic.log`
- `logs/runtime-gateway-dryrun-strict.log`
- `logs/runtime-gateway-dryrun-diagnostic.log`

登记记录默认输出到：

- `tests/reports/hardware-smoke-observation/intake/<timestamp>-<machine-id>.md`
- `tests/reports/hardware-smoke-observation/intake/<timestamp>-<machine-id>.json`

## 典型用法

最小观察：

```powershell
Set-Location D:\Projects\SS-dispense-align
.\scripts\validation\run-hardware-smoke-observation.ps1
```

指定 vendor 覆盖目录：

```powershell
Set-Location D:\Projects\SS-dispense-align
.\scripts\validation\run-hardware-smoke-observation.ps1 `
  -BuildConfig Debug `
  -VendorDir D:\Vendor\MultiCard
```

观察后立即登记：

```powershell
Set-Location D:\Projects\SS-dispense-align
.\scripts\validation\run-hardware-smoke-observation.ps1 `
  -BuildConfig Debug `
  -RegisterObservation `
  -Operator xike `
  -MachineId x7-lab-01
```

单独登记已有观察结果：

```powershell
Set-Location D:\Projects\SS-dispense-align
.\scripts\validation\register-hardware-smoke-observation.ps1 `
  -EvidenceDir tests\reports\hardware-smoke-observation\<timestamp> `
  -Operator xike `
  -MachineId x7-lab-01
```

## 输出语义

`run-hardware-smoke-observation.ps1` 的核心结论字段：

- `observation_result`
  - `blocked`：当前存在自动化阻塞项
  - `ready_for_gate_review`：自动检查通过，但仍需要按 SOP 完成人工安全检查
- `a4_signal`
  - `No-Go`：不应进入 `A4`
  - `NeedsManualReview`：可以交由 `A4` 结合 SOP 与现场条件继续评审
- `dominant_gap`
  - `configuration`
  - `vendor-assets`
  - `owner-boundary`
  - `none`

## 与 SOP 的关系

- 本脚本负责自动化观察与证据整理
- `docs/runtime/hardware-bring-up-smoke-sop.md` 负责现场人工检查、停止条件和回滚动作
- 推荐顺序：
  1. 先执行 `run-hardware-smoke-observation.ps1`
  2. 处理自动化阻塞项
  3. 再按 SOP 进行人工安全检查
  4. 将结果交给 `A4` 做 go/no-go 进入门评审
