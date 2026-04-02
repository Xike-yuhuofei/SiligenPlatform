# hardware-in-loop

更新时间：`2026-04-02`

这里统一放硬件冒烟和机台联调入口。

当前入口：

- `run_hardware_smoke.py`
- `run_real_dxf_machine_dryrun.py`
- `run_real_dxf_machine_dryrun_negative_matrix.py`
- `run_real_dxf_preview_snapshot.py`
- `run_hil_closed_loop.py`
- `run_case_matrix.py`
- `run_hil_tcp_recovery.py`
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
- `run_hil_controlled_test.ps1` 是唯一正式的 controlled HIL release path；默认执行顺序固定为 `offline-prereq -> hardware-smoke -> hil-closed-loop -> optional hil-case-matrix -> gate -> release-summary`
- `run_hil_controlled_test.ps1` 在 `passed` 且门禁满足时，会将本次时间戳目录证据同步到固定目录 `tests/reports/hil-controlled-test`
- 任何带 `PublishLatestOnPass=true` 的正式 publish 都必须提供非空 `-Executor`，否则脚本会直接失败，防止生成 unsigned latest authority
- `run_hil_controlled_test.ps1` 在 offline prerequisites 已通过后，即使 hardware-smoke / hil-closed-loop / hil-case-matrix 阻塞，也会继续产出 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md`
- 固定目录会写入 `latest-source.txt`，用于追溯来源时间戳目录（双轨证据）
- 联机测试矩阵基线统一见 `docs/validation/online-test-matrix-v1.md`
- `run_hil_tcp_recovery.py` 是 `P1-01` 的独立 recovery authority，不接 `verify_hil_controlled_gate.py`、`run_hil_controlled_test.ps1` 或 `release-check.ps1`
- `run_hil_closed_loop.py` 与 `run_case_matrix.py` 现在都会额外发布：
  - `case-index.json`
  - `validation-evidence-bundle.json`
  - `report-manifest.json`
  - `report-index.json`
  - `evidence-links.md`
  - `failure-details.json`（存在非通过结论时）
- `run_hil_tcp_recovery.py` 也会发布同套 evidence bundle 工件
- HIL evidence bundle 必须声明前置离线层、`skip_justification` 与 `abort_metadata`
- 根级 `test.ps1 -IncludeHardwareSmoke/-IncludeHil*` 与 `ci.ps1 -IncludeHilCaseMatrix` 只保留为 opt-in surface，不作为正式 controlled HIL release path

推荐命令：

```powershell
python .\tests\e2e\hardware-in-loop\run_hardware_smoke.py
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun.py
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun_negative_matrix.py
```

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py
```

```powershell
python .\tests\e2e\hardware-in-loop\run_case_matrix.py --mode both --rounds 20
```

```powershell
python .\tests\e2e\hardware-in-loop\run_hil_tcp_recovery.py --rounds 10
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

- 预检阶段执行一次 `dxf.load`，循环阶段按 `tcp connect -> status -> supply.open -> dispenser.start/pause/resume -> dispenser.stop -> supply.close -> disconnect` 执行闭环
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
- dry-run preflight 现在同时消费 `effective_interlocks.home_boundary_x_active/home_boundary_y_active`，避免把 HOME boundary 误判成“无 blocker”
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

`run_real_dxf_machine_dryrun_negative_matrix.py` 说明：

- 默认输出 `real-dxf-machine-dryrun-negative-matrix.json` 与 `real-dxf-machine-dryrun-negative-matrix.md`
- 默认报告目录为 `tests/reports/adhoc/real-dxf-machine-dryrun-negative-matrix/<timestamp>/`
- 当前矩阵固定覆盖 `estop`、`door_open`、`home_boundary_x_active`、`home_boundary_y_active`
- 每个 case 会复用 `run_real_dxf_machine_dryrun.py` 生成独立 per-case dry-run 报告，并保留 `launcher.log`
- 当前矩阵只收敛已冻结的 blocker authority；`limit_x_pos/limit_y_pos` 不在本轮范围内

`BUG-312` 受控联机命令建议：

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun.py `
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
- 预览成功后会继续执行 `dxf.preview.confirm`，用于固化 `plan_fingerprint <-> snapshot_hash` 相关性
- 默认用 `--config-mode mock` 生成 mock 硬件配置，但预览数据源必须仍为 `preview_source=planned_glue_snapshot`
- 默认 DXF 为 `samples/dxf/rect_diag.dxf`
- 默认报告目录为 `tests/reports/adhoc/real-dxf-preview-snapshot-canonical/<timestamp>/`
- 输出 `plan-prepare.json`、`snapshot.json`、`glue_points.json`、`motion_preview.json`、`execution_polyline.json`、`preview-verdict.json`、`preview-evidence.md`、`hmi-preview.png`、`online-smoke.log`
- `motion_preview.json` 是正式运动轨迹预览证据；`execution_polyline.json` 仅保留为兼容输出，不改变 `dxf.preview.confirm` / `dxf.job.start` gate
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
- `closed_loop` 路径与 `run_hil_closed_loop.py` 对齐，当前会显式执行 `supply.open -> dispenser.start -> dispenser.stop -> supply.close`
- 已接入根级 `test.ps1` / `ci.ps1` / `run-local-validation-gate.ps1` 的 opt-in 开关 `-IncludeHilCaseMatrix`
- `run_hil_controlled_test.ps1` 与 `release-check.ps1` 当前默认纳入 `hil-case-matrix`
- 如需临时隔离矩阵影响，可显式传 `-IncludeHilCaseMatrix:$false`

`run_hil_tcp_recovery.py` 说明：

- 当前只验证 `TCP session disconnect/reconnect recovery`
- 当前不覆盖物理网线拔插、gateway 进程崩溃/重启、控制卡掉电
- recovery probe 固定为 `dxf.load(rect_diag.dxf)`，不扩展到 `home`、`move`、`jog`、`valve` 类动作
- 默认流程：`connect -> status -> dxf.load -> disconnect -> reconnect -> status -> dxf.load -> disconnect`
- 默认输出 `hil-tcp-recovery-summary.json` 与 `hil-tcp-recovery-summary.md`
- 默认报告目录为 `tests/reports/adhoc/hil-tcp-recovery/`
- 默认轮数为 `10`
- 额外输出：
  - `case-index.json`
  - `validation-evidence-bundle.json`
  - `report-manifest.json`
  - `report-index.json`
  - `evidence-links.md`
  - `failure-details.json`（存在非通过结论时）
- 当前 authority 字段固定聚焦：
  - `rounds[].baseline_status`
  - `rounds[].probe_before_disconnect`
  - `rounds[].disconnect_ack`
  - `rounds[].post_reconnect_status`
  - `rounds[].probe_after_reconnect`
- 当前阶段只作为专项 recovery authority 使用，不进入 controlled gate blocking policy

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
- offline prerequisites 直接以 `-Suite @("contracts","e2e","protocol-compatibility")` 调用根级 `test.ps1`，避免子 `powershell -File` 多 `-Suite` 绑定歧义
- 默认会把 `offline-prereq`、`hardware-smoke`、`hil-closed-loop`、`hil-controlled-gate`、`hil-controlled-release-summary` 聚合到同一报告根
- 默认 `-IncludeHilCaseMatrix:$true`；如需隔离排障可显式关闭，但正式 release path 应优先保留
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
