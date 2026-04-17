# hardware-in-loop

更新时间：`2026-04-16`

这里统一放硬件冒烟和机台联调入口。

当前入口：

- `run_hardware_smoke.py`
- `run_real_dxf_machine_dryrun.py`
- `run_real_dxf_production_validation.py`
- `run_dxf_stop_home_auto_probe.py`
- `run_dxf_stop_home_auto_validation.py`
- `run_real_dxf_machine_dryrun_negative_matrix.py`
- `run_real_dxf_preview_snapshot.py`
- `run_real_dxf_preview_suite.py`
- `run_real_dxf_machine_dryrun_suite.py`
- `run_hil_closed_loop.py`
- `run_case_matrix.py`
- `run_hil_controlled_test.ps1`
- `verify_hil_controlled_gate.py`
- `render_hil_controlled_release_summary.py`

当前策略：

- 默认验证 canonical `siligen_tcp_server.exe` 的最小启动闭环
- 默认产物根解析顺序与 shared build owner 保持一致：`SILIGEN_CONTROL_APPS_BUILD_ROOT`（显式设置时） -> 携带当前工作区匹配 `CMakeCache.txt` 的 `<repo-root>\build\ca` -> `<repo-root>\build\control-apps` -> `<repo-root>\build` -> 匹配当前工作区且携带匹配 `CMakeCache.txt` 的 `%LOCALAPPDATA%\SS\cab-*` -> 携带当前工作区匹配 `CMakeCache.txt` 的 legacy `%LOCALAPPDATA%\SiligenSuite\control-apps-build`
- 默认目标路径模式为 `<build-root>\bin\<Config>\*.exe`，当前 canonical 命中应为 `<repo-root>\build\ca\bin\<Config>\*.exe`
- 可通过 `SILIGEN_HIL_GATEWAY_EXE` 显式覆盖可执行文件
- 进程 `cwd` 使用仓库根，而不是 `control-core`
- 若启动失败并命中当前已知模式，会归类为 `known_failure`
- 若未来接入真实机台，可通过环境变量替换目标可执行程序
- `run_hil_controlled_test.ps1` 是唯一正式的 controlled HIL release path；默认执行顺序固定为 `offline-prereq -> hardware-smoke -> hil-closed-loop -> optional hil-case-matrix -> gate -> release-summary`
- `run_hil_controlled_test.ps1` 在 `passed` 且门禁满足时，会将本次时间戳目录证据同步到固定目录 `tests/reports/hil-controlled-test`
- 任何带 `PublishLatestOnPass=true` 的正式 publish 都必须提供非空 `-Executor`，否则脚本会直接失败，防止生成 unsigned latest authority
- `run_hil_controlled_test.ps1` 在 offline prerequisites 已通过后，即使 hardware-smoke / hil-closed-loop / hil-case-matrix 阻塞，也会继续产出 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md`
- 固定目录会写入 `latest-source.txt`，用于追溯当前 fixed latest authority 对应的时间戳目录
- 联机测试矩阵基线统一见 `docs/validation/online-test-matrix-v1.md`
- `run_hil_closed_loop.py` 与 `run_case_matrix.py` 的 DXF case 选择统一来自 `shared/contracts/engineering/fixtures/dxf-truth-matrix.json`
- 当前 full-chain canonical producer case 为 `rect_diag`、`bra`、`arc_circle_quadrants`
- `run_real_dxf_preview_suite.py` 会聚合 `rect_diag`、`bra`、`arc_circle_quadrants` 三个 canonical preview case，并补 `case-index.json`、`validation-evidence-bundle.json`、`report-manifest.json`、`report-index.json`
- `run_real_dxf_machine_dryrun_suite.py` 会聚合同一组 canonical dry-run case，并输出 suite summary + per-case launcher log + evidence bundle
- 默认 HIL case 仍是 truth matrix 中标记为 `default_hil_sample` 的 `rect_diag`；`bra`、`arc_circle_quadrants` 只应在显式传 `--dxf-case-id` 时进入真实设备路径
- 所有会触发 `dxf.plan.prepare` 的正式 DXF/HIL 脚本现在都必须显式传入已发布 recipe/version；当前 canonical published context 固定为 `--recipe-id recipe-7d1b00f4-6a99 --version-id version-fea9ce29-f963`
- `limited-hil` 不是“实现完即全量上机”；当前正式口径仍是先过 `full-offline-gate`，再经人工签字进入 `run_hil_controlled_test.ps1` 的受控路径
- preview / dry-run suite 只用于 full-online blocker 汇总与 `G8` 补充证据，不会 publish latest，也不会覆盖 controlled HIL authority
- `run_hil_closed_loop.py` 与 `run_case_matrix.py` 现在都会额外发布：
  - `case-index.json`
  - `validation-evidence-bundle.json`
  - `report-manifest.json`
  - `report-index.json`
  - `evidence-links.md`
  - `failure-details.json`（存在非通过结论时）
- HIL evidence bundle 必须声明前置离线层、`skip_justification` 与 `abort_metadata`
- 根级 `test.ps1 -IncludeHardwareSmoke/-IncludeHil*` 与 `ci.ps1 -IncludeHilCaseMatrix` 只保留为 opt-in surface，不作为正式 controlled HIL release path

推荐命令：

```powershell
python .\tests\e2e\hardware-in-loop\run_hardware_smoke.py
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun.py `
  --recipe-id recipe-7d1b00f4-6a99 `
  --version-id version-fea9ce29-f963
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_production_validation.py `
  --recipe-id recipe-7d1b00f4-6a99 `
  --version-id version-fea9ce29-f963 `
  --dxf-file D:\Projects\SiligenSuite\uploads\dxf\archive\rect_diag.dxf
```

```powershell
python .\tests\e2e\hardware-in-loop\run_dxf_stop_home_auto_probe.py `
  --recipe-id recipe-7d1b00f4-6a99 `
  --version-id version-fea9ce29-f963
```

```powershell
python .\tests\e2e\hardware-in-loop\run_dxf_stop_home_auto_validation.py `
  --recipe-id recipe-7d1b00f4-6a99 `
  --version-id version-fea9ce29-f963
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun_negative_matrix.py `
  --recipe-id recipe-7d1b00f4-6a99 `
  --version-id version-fea9ce29-f963
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py `
  --recipe-id recipe-7d1b00f4-6a99 `
  --version-id version-fea9ce29-f963
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_preview_suite.py `
  --recipe-id recipe-7d1b00f4-6a99 `
  --version-id version-fea9ce29-f963
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun_suite.py `
  --recipe-id recipe-7d1b00f4-6a99 `
  --version-id version-fea9ce29-f963
```

```powershell
python .\tests\e2e\hardware-in-loop\run_case_matrix.py --mode both --rounds 20
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite e2e -IncludeHilCaseMatrix -ReportDir .\tests\reports\verify\hil-case-matrix -FailOnKnownFailure
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local -UseTimestampedReportDir -PublishLatestOnPass:$false
```

```powershell
python .\tests\e2e\hardware-in-loop\run_hil_closed_loop.py --report-dir .\tests\reports\hil-controlled-test --duration-seconds 1800
```

```powershell
python .\tests\e2e\hardware-in-loop\run_hil_closed_loop.py --dxf-case-id bra --report-dir .\tests\reports\adhoc\hil-bra --duration-seconds 300
```

```powershell
python .\tests\e2e\hardware-in-loop\run_case_matrix.py --mode closed_loop --dxf-case-id arc_circle_quadrants --rounds 5
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local -PublishLatestOnPass:$false
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -Executor "Codex + Xike" `
  -PublishLatestReportDir tests\reports\hil-controlled-test
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -SkipBuild `
  -UseTimestampedReportDir `
  -PublishLatestOnPass:$false
```

`run_hil_closed_loop.py` 说明：

- 预检阶段执行一次 `dxf.artifact.create` 上传当前 DXF，循环阶段按 `tcp connect -> status -> supply.open -> dispenser.start/pause/resume -> dispenser.stop -> supply.close -> disconnect` 执行闭环
- 支持 `--dxf-case-id <case>` 或 `--dxf-file <path>` 二选一；`--dxf-case-id` 会从共享 truth matrix 解析 case 与资产引用
- 当前可选 `--dxf-case-id` 为 `rect_diag`、`bra`、`arc_circle_quadrants`
- 省略 `--dxf-case-id` 时使用 truth matrix 默认 HIL case `rect_diag`
- 默认对 `Running/Paused/Running` 状态做等待门控，避免 pause/resume 竞态误判
- `dispenser.start` 未观测到 `Running` 视为失败，不再按 `skipped` 兼容通过
- 默认输出 `hil-closed-loop-summary.json` 与 `hil-closed-loop-summary.md`
- 报告包含 `dispenser_start_params` 与 `state_transition_checks` 字段
- 报告在失败/已知失败时会追加 `failure_context`（iteration/step_name/method/error_message/last_success_step/socket_connected/recent_status_snapshot）
- 默认退出码与 workspace validation 对齐：`0=pass`、`10=known_failure`、`11=skipped`、`1=failed`

`run_real_dxf_machine_dryrun.py` 说明：

- 明确只走 canonical 链：`dxf.artifact.create -> dxf.plan.prepare -> dxf.preview.snapshot -> dxf.preview.confirm -> dxf.job.start -> dxf.job.status`
- 必须显式传入已发布 `--recipe-id/--version-id`，禁止回退到 active recipe / activeVersionId / 最近 artifact 推断
- 默认 DXF 为 `samples/dxf/rect_diag.dxf`
- 默认报告目录为 `tests/reports/adhoc/real-dxf-machine-dryrun-canonical/<timestamp>/`
- 运行前会检查急停/门/限位，并在需要时先执行回零
- dry-run preflight 现在同时消费 `effective_interlocks.home_boundary_x_active/home_boundary_y_active`，避免把 HOME boundary 误判成“无 blocker”
- canonical `machine_config.ini` 的 `[Homing_Axis1..4]` 已收敛到 validator 允许范围：`ready_zero_speed_mm_s=8.0`、`rapid_velocity=10.0`
- 当未使用 `--mock-io-json` 时，dry-run preflight 会自动归一化真实机台的 HOME boundary 起始态：若未回零或 `home_boundary_x/y_active=true` 会先 `home`；若 `home` 成功后仍停在 HOME boundary，脚本会把该状态记录为“post-home HOME signal latched”并继续 canonical 主链，不再执行物理逃逸去平移整条轨迹
- 上述自动归一化只对真实 preflight 生效；negative matrix 里由 `--mock-io-json` 注入的 `home_boundary_x_active/home_boundary_y_active` 仍必须直接阻断 preflight
- gateway 监听端口默认改为自动分配空闲端口，并在 dry-run 报告根字段 `gateway_port` 中固化本次实际端口
- 支持 `--mock-io-json`，用于在 `Hardware.mode=Mock` 下对受控 negative case 注入 `estop`、`door`、`limit_x_neg`、`limit_y_neg`
- 默认参数为 `dispensing=10mm/s`、`dry_run=10mm/s`、`rapid=20mm/s`、`velocity_trace_interval_ms=50`
- `--job-timeout` 现在表示最小超时下限，不再直接等于最终等待时长；脚本会基于 `dxf.plan.prepare.estimated_time_s` 计算动态预算：`max(job_timeout, estimated_time_s * job_timeout_scale + job_timeout_buffer_seconds)`
- 当前动态超时默认参数为：`--job-timeout=120`、`--job-timeout-scale=2.0`、`--job-timeout-buffer-seconds=15`
- 从 `BUG-312` 起，dry-run 报告除兼容保留的 `job_status_history` 外，还会并列输出 `machine_status_history`、`coord_status_history`、`phase_timeline`、`verdict`、`evidence_contract`
- 报告 `artifacts.job_timeout_budget` 和 `observation_summary.effective_job_timeout_seconds` 会落本次实际采用的等待预算，便于区分“真实执行超时”与“预算过紧”
- 当前 authority 口径固定为：`status=safety gate only`、`dxf.job.status=display progress only`、`motion.coord.status + axis feedback=physical execution`
- 当前阶段枚举固定为：`prepare -> binding -> start -> coord_running -> axis_motion -> terminal`
- 当前 verdict 枚举固定为：`completed`、`canonical_step_failed`、`motion_timeout_unclassified`、`state_contradiction`
- 若连续窗口内出现“`dxf.job.status` 高进度/高分段推进，但 `motion.coord.status` idle-empty，且轴速度与位移保持近零”，脚本会直接落 `state_contradiction` 并输出 `first_contradiction_sample`
- 当前 `state_contradiction` 默认阈值参数为：`--contradiction-progress-threshold-percent=95`、`--contradiction-consecutive-samples=5`、`--contradiction-grace-seconds=0.5`、`--position-epsilon-mm=0.001`、`--velocity-epsilon-mm-s=0.001`
- 当前能力先作为 `BUG-312` 专项诊断入口使用，默认不直接接入 `verify_hil_controlled_gate.py`
- 自动离线回归入口为 `python -m pytest tests/integration/scenarios/first-layer/test_real_dxf_machine_dryrun_observation_contract.py -q`

`run_real_dxf_production_validation.py` 说明：

- 当前用于 BUG-318 这类“生产模式 path-trigger 真机专项验证”，固定走 `dxf.artifact.create -> dxf.plan.prepare(dry_run=false,use_hardware_trigger=true) -> dxf.preview.snapshot -> dxf.preview.confirm -> dxf.job.start -> dxf.job.status`
- 必须显式传入已发布 `--recipe-id/--version-id`
- 默认 DXF 固定为仓内 canonical sample `samples/dxf/rect_diag.dxf`
- 不再允许 `uploads archive -> repo sample` 静默回退；若显式传入 `--dxf-file` 且文件不存在，脚本直接失败
- 会在真实连接前做最小 preflight：`status -> 必要时 home -> safety gate`
- 会同时冻结 `job-status-history.json`、`machine-status-history.json` 与本次 gateway 追加日志切片 `tcp_server.log`
- 当前 blocking authority 固定为 `dxf.job.status` 单轨，固定要求：
  - `preview_source=planned_glue_snapshot`
  - `preview_kind=glue_points`
  - `job.state=completed`
  - `overall_progress_percent=100`
  - `completed_count == target_count`
- `tcp_server.log` 中的 `PathTriggeredDispenserLoop/StopDispenser` 计数与在线轴覆盖率仅保留为 diagnostics，不再作为第二条 blocker
- 若 runtime 发现 path-trigger 计数未完整收口，必须把 `job.state` 直接收口为 `failed`，并在 `error_message` 中返回 `failure_stage=path_trigger_reconcile;failure_code=DISPENSER_TRIGGER_INCOMPLETE;...`
- 当前离线回归入口为 `python -m pytest tests/integration/scenarios/first-layer/test_real_dxf_production_validation_contract.py -q`

`run_dxf_stop_home_auto_probe.py` 说明：

- 当前用于 `MC_PrfTrap` 现场专项复测，固定执行 `dxf.job.start -> dxf.job.stop -> 等 cancel 终态 -> immediate home.auto`
- 必须显式传入已发布 `--recipe-id/--version-id`
- 默认报告目录为 `tests/reports/adhoc/dxf-stop-home-auto-probe/<timestamp>/`
- 会同时落盘：
  - `dxf-stop-home-auto-probe.json`
  - `dxf-stop-home-auto-probe.md`
  - `gateway-stdout.log`
  - `gateway-stderr.log`
- 报告会强制保留 stop 前观测、stop 后直到终态的观测，以及 `pre_home_snapshot` / `post_home_snapshot`
- `observation_summary` 会直接给出：
  - `job_terminal_state_after_stop`
  - `coord_after_stop.is_moving / remaining_segments / current_velocity / raw_status_word / raw_segment`
  - `axes_stopped_before_home`
  - `home_auto_ok / home_auto_summary_state / home_auto_message`
- `mc_prftrap_detected / prftrap_hit_count`
- 若 `dxf.job.stop` 过晚导致 job 已 `completed`，脚本会返回 `skipped` 口径，不把该次结果误判为“stop/cancel 主链已验证”
- 当前离线回归入口为 `python -m pytest tests/integration/scenarios/first-layer/test_dxf_stop_home_auto_probe_contract.py -q`

`run_dxf_stop_home_auto_validation.py` 说明：

- 当前用于把 `MC_PrfTrap` 现场专项复测固定成一个正式编排入口，不替代单次 probe
- 必须显式传入已发布 `--recipe-id/--version-id`
- 固定先执行 `run-hardware-smoke-observation.ps1`，再根据 `--manual-checks-confirmed` 决定是否允许进入真实动作
- 默认成功条件为累计 `3` 个有效通过样本；有效样本必须同时满足：
  - `overall_status=passed`
  - `job_terminal_state_after_stop != completed`
  - `axes_stopped_before_home=true`
  - `home_auto_ok=true`
  - `mc_prftrap_detected=false`
  - `prftrap_hit_count=0`
- 默认 `skipped` 预算为 `3`；超过预算直接阻塞本阶段，避免现场反复“晚停一次再试”
- 首个非 `skipped` 失败样本会立即停止后续 probe，并归类为：
  - `stop_drain_incomplete`
  - `axis_stopped_but_prftrap`
  - `probe_failed_unclassified`
- 默认报告目录为 `tests/reports/online-validation/dxf-stop-home-auto-validation/<timestamp>/`
- 当前会同时落盘：
  - `dxf-stop-home-auto-validation.json`
  - `dxf-stop-home-auto-validation.md`
  - `logs/hardware-smoke-launcher.log`
  - `probe-attempts/attempt-*/launcher.log`
- 当前离线回归入口为 `python -m pytest tests/integration/scenarios/first-layer/test_dxf_stop_home_auto_validation_contract.py -q`

`run_real_dxf_machine_dryrun_negative_matrix.py` 说明：

- 默认输出 `real-dxf-machine-dryrun-negative-matrix.json` 与 `real-dxf-machine-dryrun-negative-matrix.md`
- 必须显式传入已发布 `--recipe-id/--version-id`
- 默认报告目录为 `tests/reports/adhoc/real-dxf-machine-dryrun-negative-matrix/<timestamp>/`
- 当前矩阵固定覆盖 `estop`、`door_open`、`home_boundary_x_active`、`home_boundary_y_active`
- 每个 case 会复用 `run_real_dxf_machine_dryrun.py` 生成独立 per-case dry-run 报告，并保留 `launcher.log`
- 当前矩阵只收敛已冻结的 blocker authority；`limit_x_pos/limit_y_pos` 不在本轮范围内

`BUG-312` 受控联机命令建议：

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun.py `
  --recipe-id recipe-7d1b00f4-6a99 `
  --version-id version-fea9ce29-f963 `
  --report-root .\tests\reports\adhoc\live-hmi-bug312 `
  --contradiction-progress-threshold-percent 95 `
  --contradiction-consecutive-samples 5 `
  --contradiction-grace-seconds 0.5 `
  --position-epsilon-mm 0.001 `
  --velocity-epsilon-mm-s 0.001
```

联机观察点：

- 先看 `verdict.kind` 是否直接落为 `state_contradiction`
- 再看 `phase_timeline` 是否停在 `start` 之后且长期未进入 `coord_running` / `axis_motion`
- 若 `verdict.kind=state_contradiction`，优先看 `first_contradiction_sample` 中 `job.current_segment`、`job.cycle_progress_percent`、`coord.raw_status_word`、`coord.raw_segment`、`coord.axes.X/Y.velocity`
- 若最终仍是 timeout 但未落矛盾，则按 `motion_timeout_unclassified` 处理，不要直接宣称根因已闭环

`run_real_dxf_preview_snapshot.py` 说明：

- 只走 preview-only 主链：`dxf.artifact.create -> dxf.plan.prepare -> dxf.preview.snapshot`
- 必须显式传入已发布 `--recipe-id/--version-id`
- 预览成功后会继续执行 `dxf.preview.confirm`，用于固化 `plan_fingerprint <-> snapshot_hash` 相关性
- 默认用 `--config-mode mock` 生成 mock 硬件配置，但预览数据源必须仍为 `preview_source=planned_glue_snapshot`
- 默认 DXF 为 `samples/dxf/rect_diag.dxf`
- 默认报告目录为 `tests/reports/adhoc/real-dxf-preview-snapshot-canonical/<timestamp>/`
- 输出 `plan-prepare.json`、`snapshot.json`、`glue_points.json`、`motion_preview.json`、`preview-verdict.json`、`preview-evidence.md`、`hmi-preview.png`、`online-smoke.log`
- 同时保留兼容报告 `real-dxf-preview-snapshot.json`、`real-dxf-preview-snapshot.md`
- 若返回 `mock_synthetic`、`runtime_snapshot` 或缺少 `preview_source/preview_kind`，脚本直接失败，禁止把结果当作真实规划胶点预览证据
- 若 `plan_id / plan_fingerprint / snapshot_hash` 无法回链，`preview-verdict.json` 必须落为 `mismatch` 或 `incomplete`
- `rect_diag` 的当前几何基线固定在 `tests/baselines/preview/rect_diag.preview-snapshot-baseline.json`
- 自动 evidence 会追加一次 HMI 在线 smoke，把真实 `snapshot.json` 注入主窗口并截取 `hmi-preview.png`
- 自动回归入口为 `python -m pytest tests/integration/scenarios/first-layer/test_real_preview_snapshot_geometry.py -q`

`run_case_matrix.py` 说明：

- 当前用于 `home` / `closed_loop` 的多轮补充矩阵，不替代 `run_hil_controlled_test.ps1`
- 默认输出 `case-matrix-summary.json` 与 `case-matrix-summary.md`
- 默认报告目录为 `tests/reports/adhoc/hil-case-matrix/`
- 当前支持 `--mode home|closed_loop|both` 与 `--rounds <N>`
- 支持 `--dxf-case-id <case>` 或 `--dxf-file <path>` 二选一；省略时同样回落到 truth matrix 默认 HIL case `rect_diag`
- 若进入真实设备扩样，推荐顺序为：先默认 `rect_diag` 走 controlled path，再按计划扩到 `--dxf-case-id bra` 或 `--dxf-case-id arc_circle_quadrants`
- `closed_loop` 路径与 `run_hil_closed_loop.py` 对齐，当前会显式执行 `supply.open -> dispenser.start -> dispenser.stop -> supply.close`
- 已接入根级 `test.ps1` / `ci.ps1` / `run-local-validation-gate.ps1` 的 opt-in 开关 `-IncludeHilCaseMatrix`
- `run_hil_controlled_test.ps1` 与 `release-check.ps1` 当前默认纳入 `hil-case-matrix`
- 如需临时隔离矩阵影响，可显式传 `-IncludeHilCaseMatrix:$false`

`verify_hil_controlled_gate.py` 说明：

- 默认读取 `offline-prereq/workspace-validation.json`、`hardware-smoke/hardware-smoke-summary.json`、`hil-closed-loop-summary.json`、`validation-evidence-bundle.json`
- 当显式传 `--require-hil-case-matrix` 时，还会校验 `hil-case-matrix/case-matrix-summary.json`
- 校验离线前置层 `failed/known_failure/skipped` 必须为 `0`
- 校验 required offline cases：`protocol-compatibility`、`engineering-regression`、`simulated-line`
- 校验 `hardware-smoke` 与 `hil-closed-loop` 必须 `overall_status=passed`
- 校验 evidence bundle schema/version、`report-manifest.json`、`report-index.json`
- 校验 `metadata.admission`、`safety_preflight_passed=true`，以及 override 使用时必须带非空 reason
- 输出 `hil-controlled-gate-summary.json` 与 `hil-controlled-gate-summary.md`
- 退出码：`0=pass`、`1=gate fail`、`10=缺少输入工件`

`render_hil_controlled_release_summary.py` 说明：

- 读取 Gate / offline-prereq / hardware-smoke / hil 报告
- 若 gate 要求 `hil-case-matrix`，还会读取 `hil-case-matrix/case-matrix-summary.json`
- 自动生成 `hil-controlled-release-summary.md`
- 结论规则：offline prerequisites、hardware smoke、hil-closed-loop、admission/preflight、schema compatibility 全部通过时为 `通过`，否则 `阻塞`

`run_hil_controlled_test.ps1` 说明：

- 支持 `-SkipBuild` 跳过 `build.ps1 -Suite apps`（用于诊断回合提速）
- `-SkipBuild` 只影响构建步骤，不改变 test/gate/release/publish 语义
- 当 `PublishLatestOnPass=true` 时，必须提供非空 `-Executor`，latest publish 才会被允许
- 支持 `-OperatorOverrideReason "<reason>"`；只有离线前置层不满足且现场负责人明确批准时才允许使用
- offline prerequisites 直接以 `-Suite @("contracts","integration","e2e","protocol-compatibility")` 调用根级 `test.ps1`，覆盖 `engineering-regression`，并避免子 `powershell -File` 多 `-Suite` 绑定歧义
- 默认会把 `offline-prereq`、`hardware-smoke`、`hil-closed-loop`、`hil-controlled-gate`、`hil-controlled-release-summary` 聚合到同一报告根
- 默认 `-IncludeHilCaseMatrix:$true`；如需隔离排障可显式关闭，但正式 release path 应优先保留
- 当前 controlled path 不会自动把 truth matrix 全量 case 一次性上机；若要扩到 `bra` / `arc_circle_quadrants`，应先保留默认 `rect_diag` 完成受控回合与人工签字
- 若 offline prerequisites 已通过，但后续 HIL 步骤阻塞，脚本仍会继续渲染 gate/release summary，并保留原始失败退出码

Phase 12 blocked evidence 示例：

- `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-controlled-gate-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-controlled-release-summary.md`

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
