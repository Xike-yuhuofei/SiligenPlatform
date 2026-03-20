# hardware-in-loop

这里统一放硬件冒烟和机台联调入口。

当前入口：

- `run_hardware_smoke.py`
- `run_hil_closed_loop.py`
- `run_hil_controlled_test.ps1`

当前策略：

- 默认验证 canonical `siligen_tcp_server.exe` 的最小启动闭环
- 默认产物根按 `SILIGEN_CONTROL_APPS_BUILD_ROOT` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build` -> `D:\Projects\SiligenSuite\build\control-apps` 解析
- 默认目标路径模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe`
- 可通过 `SILIGEN_HIL_GATEWAY_EXE` 显式覆盖可执行文件
- 进程 `cwd` 使用仓库根，而不是 `control-core`
- 若启动失败并命中当前已知模式，会归类为 `known_failure`
- 若未来接入真实机台，可通过环境变量替换目标可执行程序
- `run_hil_controlled_test.ps1` 在 `passed` 且门禁满足时，会将本次时间戳目录证据同步到固定目录 `integration/reports/hil-controlled-test`
- 固定目录会写入 `latest-source.txt`，用于追溯来源时间戳目录（双轨证据）

推荐命令：

```powershell
python .\integration\hardware-in-loop\run_hardware_smoke.py
```

```powershell
python .\integration\hardware-in-loop\run_hil_closed_loop.py --report-dir .\integration\reports\hil-controlled-test --duration-seconds 1800
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\integration\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local
```

`run_hil_closed_loop.py` 说明：

- 预检阶段执行一次 `dxf.load`，循环阶段按 `tcp connect -> status -> dispenser start/pause/resume -> dispenser.stop -> disconnect` 执行闭环
- 默认对 `Running/Paused/Running` 状态做等待门控，避免 pause/resume 竞态误判
- `dispenser.start` 未观测到 `Running` 视为失败，不再按 `skipped` 兼容通过
- 默认输出 `hil-closed-loop-summary.json` 与 `hil-closed-loop-summary.md`
- 报告包含 `dispenser_start_params` 与 `state_transition_checks` 字段
- 默认退出码与 workspace validation 对齐：`0=pass`、`10=known_failure`、`11=skipped`、`1=failed`

关键参数：

- `--pause-resume-cycles`（默认 `3`）
- `--dispenser-count`（默认 `300`）
- `--dispenser-interval-ms`（默认 `200`）
- `--dispenser-duration-ms`（默认 `80`）
- `--state-wait-timeout-seconds`（默认 `8`）

对应环境变量：

- `SILIGEN_HIL_PAUSE_RESUME_CYCLES`
- `SILIGEN_HIL_DISPENSER_COUNT`
- `SILIGEN_HIL_DISPENSER_INTERVAL_MS`
- `SILIGEN_HIL_DISPENSER_DURATION_MS`
- `SILIGEN_HIL_STATE_WAIT_TIMEOUT_SECONDS`
