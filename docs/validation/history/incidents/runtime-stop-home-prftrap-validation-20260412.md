# Runtime Stop Home PrfTrap Validation 2026-04-12

- 任务类型：`incident`
- 分支：`fix/runtime/NOISSUE-stop-home-prftrap`
- 验证日期：2026-04-12
- 验证范围：把 `stop/cancel -> immediate home.auto` 的现场专项复测固定成受控入口，并完成自动准入、人工门禁确认和真实机台复测。
- 当前结论：`passed`
  - 自动硬件 smoke 已通过。
  - 已完成真实 `dxf.job.start -> dxf.job.stop -> home.auto` 现场专项复测。
  - 已拿到 `3` 个有效通过样本，`0` 个 `skipped`，`0` 个失败样本。

## 本轮已完成

- 新增现场编排入口：
  - `tests/e2e/hardware-in-loop/run_dxf_stop_home_auto_validation.py`
- 新增契约回归：
  - `tests/integration/scenarios/first-layer/test_dxf_stop_home_auto_validation_contract.py`
- 更新 HIL README，固定专项入口、有效样本判定、`skipped` 预算和失败分流口径。
- 编译 `build/bin/Debug/siligen_runtime_service.exe`，补齐 `run-hardware-smoke-observation.ps1` 所需的最小本地产物。

## 自动准入结果

- `run-hardware-smoke-observation.ps1` 结果：
  - `observation_result=ready_for_gate_review`
  - `a4_signal=NeedsManualReview`
  - `dominant_gap=none`
- 证据目录：
  - `tests/reports/hardware-smoke-observation-codex-smoke/20260412-214041/`
- 关键证据：
  - `tests/reports/hardware-smoke-observation-codex-smoke/20260412-214041/hardware-smoke-observation-summary.json`
  - `tests/reports/hardware-smoke-observation-codex-smoke/20260412-214041/hardware-smoke-observation-summary.md`

## 编排入口自检结果

- 执行命令：

```powershell
python .\tests\e2e\hardware-in-loop\run_dxf_stop_home_auto_validation.py `
  --report-root .\tests\reports\online-validation\codex-stop-home-validation
```

- 结果：
  - `overall_status=blocked`
  - `verdict.kind=manual_confirmation_required`
  - `attempted=0`
- 解释：
  - 脚本先跑自动硬件 smoke。
  - 自动检查通过后，因为没有显式传 `--manual-checks-confirmed`，脚本按门禁设计停止，未进入真实动作。
- 证据目录：
  - `tests/reports/online-validation/codex-stop-home-validation/20260412-214121/`
- 关键证据：
  - `tests/reports/online-validation/codex-stop-home-validation/20260412-214121/dxf-stop-home-auto-validation.json`
  - `tests/reports/online-validation/codex-stop-home-validation/20260412-214121/dxf-stop-home-auto-validation.md`
  - `tests/reports/online-validation/codex-stop-home-validation/20260412-214121/logs/hardware-smoke-launcher.log`

## 真实机台专项复测结果

- 执行命令：

```powershell
python .\tests\e2e\hardware-in-loop\run_dxf_stop_home_auto_validation.py `
  --manual-checks-confirmed `
  --report-root .\tests\reports\online-validation\dxf-stop-home-auto-validation
```

- 结果：
  - `overall_status=passed`
  - `verdict.kind=passed`
  - `attempted=3`
  - `valid_passed=3`
  - `skipped=0`
  - `failed=0`
- 固定结论：
  - `stop/cancel -> immediate home.auto` 在当前真实机台和当前修复快照上未再触发 `MC_PrfTrap code=1`
  - 三次样本均满足：
    - `job_terminal_state_after_stop=cancelled`
    - `coord_after_stop.is_moving=false`
    - `coord_after_stop.remaining_segments=0`
    - `coord_after_stop.current_velocity=0.0`
    - `axes_stopped_before_home=true`
    - `home_auto_ok=true`
    - `mc_prftrap_detected=false`
    - `prftrap_hit_count=0`
- 汇总证据目录：
  - `tests/reports/online-validation/dxf-stop-home-auto-validation/20260412-214610/`
- 汇总证据：
  - `tests/reports/online-validation/dxf-stop-home-auto-validation/20260412-214610/dxf-stop-home-auto-validation.json`
  - `tests/reports/online-validation/dxf-stop-home-auto-validation/20260412-214610/dxf-stop-home-auto-validation.md`
  - `tests/reports/online-validation/dxf-stop-home-auto-validation/20260412-214610/hardware-smoke/20260412-214610/hardware-smoke-observation-summary.json`
- 三次样本证据：
  - `tests/reports/online-validation/dxf-stop-home-auto-validation/20260412-214610/probe-attempts/attempt-01/20260412-214611/dxf-stop-home-auto-probe.json`
  - `tests/reports/online-validation/dxf-stop-home-auto-validation/20260412-214610/probe-attempts/attempt-02/20260412-214619/dxf-stop-home-auto-probe.json`
  - `tests/reports/online-validation/dxf-stop-home-auto-validation/20260412-214610/probe-attempts/attempt-03/20260412-214626/dxf-stop-home-auto-probe.json`

## 离线回归

执行命令：

```powershell
python -m pytest .\tests\integration\scenarios\first-layer\test_dxf_stop_home_auto_validation_contract.py -q
python -m pytest .\tests\integration\scenarios\first-layer\test_dxf_stop_home_auto_probe_contract.py -q
```

结果摘要：

- `test_dxf_stop_home_auto_validation_contract.py`：`10 passed`
- `test_dxf_stop_home_auto_probe_contract.py`：`8 passed`

## 当前结论与下一步

- 当前主故障已拿到现场通过证据。
- 下一步不再继续追加同类现场重复样本，转入：
  - 同步 `origin/main`
  - 评估是否需要跑正式 controlled-HIL / limited-HIL 收口
  - 准备 PR / closeout

## 未执行项

- 未在本轮继续同步 `origin/main`。
- 未在本轮执行正式 controlled-HIL / limited-HIL。
- 未在本轮创建 PR 或 closeout。
