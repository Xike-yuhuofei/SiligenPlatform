# runtime-service

运行时宿主入口（单轨 canonical）。

- 目录职责：进程启动与宿主装配
- 业务能力来源：`modules/runtime-execution/`、`modules/workflow/`、`shared/`
- 构建产物：`siligen_runtime_service.exe`

## 启动脚本

- 脚本：`apps/runtime-service/run.ps1`
- 默认 machine config：`config/machine/machine_config.ini`
- 默认 vendor 目录：`modules/runtime-execution/adapters/device/vendor/multicard`
- 默认日志参数：`logs/control_runtime.log`（由可执行文件默认值决定，日志目录相对 exe 工作目录）

示例：

```powershell
./apps/runtime-service/run.ps1 -BuildConfig Debug
./apps/runtime-service/run.ps1 -ConfigPath config/machine/machine_config.ini -VendorDir D:\Vendor\MultiCard
./apps/runtime-service/run.ps1 -DryRun
```

脚本行为：

- 默认执行启动前检查，确认 canonical config 存在。
- 当 `config/machine/machine_config.ini` 中 `[Hardware] mode=Real` 时，缺少 `MultiCard.dll` 会立即 fail-fast。
- 若缺少 `MultiCard.lib`，脚本会给出告警，提示真实硬件版本重建仍不可执行。
- 脚本启动时会把 `--config <路径>` 显式转发给 `siligen_runtime_service.exe`，避免依赖隐式工作目录解析。
- 如现场临时跳过检查，只允许使用 `-SkipPreflight`；该选项只用于诊断，不改变 canonical 入口。
