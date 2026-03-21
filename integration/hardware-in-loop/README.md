# hardware-in-loop

这里统一放硬件冒烟和机台联调入口。

当前入口：

- `run_hardware_smoke.py`
- `run_hil_closed_loop.py`
- `run_hil_controlled_test.ps1`
- `verify_hil_controlled_gate.py`
- `render_hil_controlled_release_summary.py`

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

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\integration\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -PublishLatestReportDir integration\reports\hil-controlled-test
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\integration\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -SkipBuild `
  -UseTimestampedReportDir `
  -PublishLatestReportDir integration\reports\hil-controlled-test
```

`run_hil_closed_loop.py` 说明：

- 预检阶段执行一次 `dxf.load`，循环阶段按 `tcp connect -> status -> dispenser start/pause/resume -> dispenser.stop -> disconnect` 执行闭环
- 默认对 `Running/Paused/Running` 状态做等待门控，避免 pause/resume 竞态误判
- `dispenser.start` 未观测到 `Running` 视为失败，不再按 `skipped` 兼容通过
- 默认输出 `hil-closed-loop-summary.json` 与 `hil-closed-loop-summary.md`
- 报告包含 `dispenser_start_params` 与 `state_transition_checks` 字段
- 报告在失败/已知失败时会追加 `failure_context`（iteration/step_name/method/error_message/last_success_step/socket_connected/recent_status_snapshot）
- 默认退出码与 workspace validation 对齐：`0=pass`、`10=known_failure`、`11=skipped`、`1=failed`

`verify_hil_controlled_gate.py` 说明：

- 校验 `workspace-validation` 的 `failed/known_failure/skipped` 必须为 `0`
- 校验 `hardware-smoke` 与 `hil-closed-loop` 两个必需 case 必须 `passed`
- 校验 `hil-closed-loop-summary` 的 `overall_status=passed`
- 校验 `state_transition_checks` 非空且全部 `status=passed`
- 校验 `timeout_count=0` 且 `elapsed_seconds >= duration_seconds`
- 输出 `hil-controlled-gate-summary.json` 与 `hil-controlled-gate-summary.md`
- 退出码：`0=pass`、`1=gate fail`、`10=缺少输入工件`

`render_hil_controlled_release_summary.py` 说明：

- 读取 Gate / workspace / hil 报告
- 自动生成 `hil-controlled-release-summary.md`
- 结论规则：Gate 为 `passed` 则 `通过`，否则 `阻塞`

`run_hil_controlled_test.ps1` 说明：

- 支持 `-SkipBuild` 跳过 `build.ps1 -Suite apps`（用于诊断回合提速）
- `-SkipBuild` 只影响构建步骤，不改变 test/gate/release/publish 语义

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
