# runtime-gateway

运行时网关与 TCP 入口（单轨 canonical）。

- 目录职责：网关进程入口与装配
- 业务能力来源：`apps/runtime-gateway/transport-gateway/`、`modules/runtime-execution/`、`shared/`
- 构建产物：`siligen_runtime_gateway.exe`

## 启动脚本

- 脚本：`apps/runtime-gateway/run.ps1`
- 默认 machine config：`config/machine/machine_config.ini`
- 默认 vendor 目录：`modules/runtime-execution/adapters/device/vendor/multicard`
- 默认 vendor 目录现在跟踪 `MultiCard.dll` / `MultiCard.lib`，fresh clone 不应再要求额外手工补 SDK 才能走默认 dry-run / build 链路
- 默认日志参数：`logs/tcp_server.log`（由可执行文件固定传入，日志目录相对 exe 工作目录）

示例：

```powershell
./apps/runtime-gateway/run.ps1 -BuildConfig Debug
./apps/runtime-gateway/run.ps1 -ConfigPath config/machine/machine_config.ini -VendorDir D:\Vendor\MultiCard -- --port 9527
./apps/runtime-gateway/run.ps1 -DryRun
```

脚本行为：

- 默认执行启动前检查，确认 canonical config 存在。
- 当 `config/machine/machine_config.ini` 中 `[Hardware] mode=Real` 时，缺少 `MultiCard.dll` 会立即 fail-fast。
- 若缺少 `MultiCard.lib`，脚本会给出告警，提示真实硬件版本重建仍不可执行。
- 脚本启动时会把 `--config <路径>` 显式转发给 `siligen_runtime_gateway.exe`。
- 网关额外参数通过 `--` 后转发，例如 `-- --port 9527`。
