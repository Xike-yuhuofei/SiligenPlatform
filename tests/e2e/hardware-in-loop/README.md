# hardware-in-loop

更新时间：`2026-03-22`

这里统一放硬件冒烟和机台联调入口。

当前入口：

- `run_hardware_smoke.py`
- `run_real_dxf_machine_dryrun.py`
- `run_hil_closed_loop.py`
- `run_hil_controlled_test.ps1`
- `verify_hil_controlled_gate.py`
- `render_hil_controlled_release_summary.py`

当前策略：

- 默认验证 canonical `siligen_tcp_server.exe` 的最小启动闭环
- 默认优先使用当前工作区 `build\bin\<Config>\...` 的二进制；只有工作区未构建时才回退到 `SILIGEN_CONTROL_APPS_BUILD_ROOT`
- 默认产物根按 `<repo-root>\build` -> `SILIGEN_CONTROL_APPS_BUILD_ROOT` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build` 解析
- 默认目标路径模式：`<repo-root>\build\bin\<Config>\*.exe`
- 可通过 `SILIGEN_HIL_GATEWAY_EXE` 显式覆盖可执行文件
- 进程 `cwd` 使用仓库根，而不是 `control-core`
- 若启动失败并命中当前已知模式，会归类为 `known_failure`
- 若未来接入真实机台，可通过环境变量替换目标可执行程序
- `run_hil_controlled_test.ps1` 在 `passed` 且门禁满足时，会将本次时间戳目录证据同步到固定目录 `tests/reports/hil-controlled-test`
- 固定目录会写入 `latest-source.txt`，用于追溯来源时间戳目录（双轨证据）

推荐命令：

```powershell
python .\tests\e2e\hardware-in-loop\run_hardware_smoke.py
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun.py
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py
```

```powershell
python .\tests\e2e\hardware-in-loop\run_hil_closed_loop.py --report-dir .\tests\reports\hil-controlled-test --duration-seconds 1800
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -PublishLatestReportDir tests\reports\hil-controlled-test
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -SkipBuild `
  -UseTimestampedReportDir `
  -PublishLatestReportDir tests\reports\hil-controlled-test
```

`run_hil_closed_loop.py` 说明：

- 预检阶段执行一次 `dxf.load`，循环阶段按 `tcp connect -> status -> dispenser start/pause/resume -> dispenser.stop -> disconnect` 执行闭环
- 默认对 `Running/Paused/Running` 状态做等待门控，避免 pause/resume 竞态误判
- `dispenser.start` 未观测到 `Running` 视为失败，不再按 `skipped` 兼容通过
- 默认输出 `hil-closed-loop-summary.json` 与 `hil-closed-loop-summary.md`
- 报告包含 `dispenser_start_params` 与 `state_transition_checks` 字段
- 报告在失败/已知失败时会追加 `failure_context`（iteration/step_name/method/error_message/last_success_step/socket_connected/recent_status_snapshot）
- 默认退出码与 workspace validation 对齐：`0=pass`、`10=known_failure`、`11=skipped`、`1=failed`

`run_real_dxf_machine_dryrun.py` 说明：

- 明确只走 canonical 链：`dxf.artifact.create -> dxf.plan.prepare -> dxf.preview.snapshot -> dxf.preview.confirm -> dxf.job.start -> dxf.job.status`
- 默认 DXF 为 `samples/dxf/rect_diag.dxf`
- 默认报告目录为 `tests/reports/adhoc/real-dxf-machine-dryrun-canonical/<timestamp>/`
- 运行前会检查急停/门/限位，并在需要时先执行回零
- 默认参数为 `dispensing=10mm/s`、`dry_run=10mm/s`、`rapid=20mm/s`、`velocity_trace_interval_ms=50`

`run_real_dxf_preview_snapshot.py` 说明：

- 只走 preview-only 主链：`dxf.artifact.create -> dxf.plan.prepare -> dxf.preview.snapshot`
- 预览成功后会继续执行 `dxf.preview.confirm`，用于固化 `plan_fingerprint <-> snapshot_hash` 相关性
- 默认用 `--config-mode mock` 生成 mock 硬件配置，但预览数据源必须仍为 `preview_source=planned_glue_snapshot`
- 默认 DXF 为 `samples/dxf/rect_diag.dxf`
- 默认报告目录为 `tests/reports/adhoc/real-dxf-preview-snapshot-canonical/<timestamp>/`
- 输出 `plan-prepare.json`、`snapshot.json`、`glue_points.json`、`execution_polyline.json`、`preview-verdict.json`、`preview-evidence.md`、`hmi-preview.png`、`online-smoke.log`
- 同时保留兼容报告 `real-dxf-preview-snapshot.json`、`real-dxf-preview-snapshot.md`
- 若返回 `mock_synthetic`、`runtime_snapshot` 或缺少 `preview_source/preview_kind`，脚本直接失败，禁止把结果当作真实规划胶点预览证据
- 若 `plan_id / plan_fingerprint / snapshot_hash` 无法回链，`preview-verdict.json` 必须落为 `mismatch` 或 `incomplete`
- `rect_diag` 的当前几何基线固定在 `tests/baselines/preview/rect_diag.preview-snapshot-baseline.json`
- 自动 evidence 会追加一次 HMI 在线 smoke，把真实 `snapshot.json` 注入主窗口并截取 `hmi-preview.png`
- 自动回归入口为 `python -m pytest tests/e2e/first-layer/test_real_preview_snapshot_geometry.py -q`

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
