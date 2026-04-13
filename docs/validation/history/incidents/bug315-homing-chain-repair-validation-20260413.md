# BUG-315 Homing Chain Repair Validation 2026-04-13

- 任务类型：`incident`
- 分支：`fix/motion/BUG-315-homing-chain-repair`
- 验证日期：`2026-04-13`
- 验证范围：确认 BUG-315 补丁在当前 worktree 二进制上完成离线回归、真实机台单轮 dry-run、`target_count=2` 多轮 dry-run，以及 `stop -> home.auto` 定向复测。
- 当前结论：`passed with known HIL infrastructure limitation`
  - 当前补丁二进制已完成真实机台单轮 patched dry-run。
  - 当前补丁二进制已完成真实机台 `target_count=2` patched 多轮 dry-run。
  - 当前补丁二进制已完成 `stop/cancel -> immediate home.auto` 定向复测，未复现 `MC_PrfTrap`。
  - formal `limited-hil` 仍被仓库基础设施问题阻塞，本轮未得到 canonical HIL PASS。

## 本轮代码边界

- `apps/hmi-app`
  - HMI 默认 `target_count` 从 `100` 收敛为 `1`。
- `modules/runtime-execution`
  - 点胶坐标系配置显式固定到机台零点，不再沿用当前规划位置作为原点。
  - 板卡包装层补齐 `MC_SetCrdPrm` origin flag / origin position 透传。
- 定向单元测试
  - 补充 HMI 默认值断言。
  - 补充点胶坐标系原点策略断言。
  - 补充插补适配层 origin flag 行为断言。

## 已确认事实

- 当前补丁落地后的关键行为是：
  - `MainWindow` 默认 `target_count=1`
  - 点胶场景 `use_current_planned_position_as_origin=false`
- 离线回归已通过：

```powershell
python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q
ctest --test-dir .\build -C Debug --output-on-failure -R "siligen_runtime_execution_unit_tests|siligen_runtime_host_unit_tests"
```

- 手工离线 HIL 前置报告中，HIL required offline cases 为通过，但 formal HIL 仍不成立：
  - `protocol-compatibility`
  - `engineering-regression`
  - `simulated-line`
- 现场现成 `9527` gateway 不是当前补丁树，不能作为本次补丁验证证据。

## 真实机台验证结果

### 1. 单轮 patched dry-run

- 执行命令：

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun.py `
  --gateway-exe D:\Projects\wt-bug315\build\bin\siligen_runtime_gateway.exe `
  --report-root D:\Projects\wt-bug315\tests\reports\online-validation\bug315-single-cycle-rerun
```

- 结果：
  - `overall_status=passed`
  - `verdict.kind=completed`
  - `target_count=1`
  - `elapsed_seconds=99.8516`
- 关键证据：
  - `tests/reports/online-validation/bug315-single-cycle-rerun/20260413-133653/real-dxf-machine-dryrun-canonical.json`
  - `tests/reports/online-validation/bug315-single-cycle-rerun/20260413-133653/real-dxf-machine-dryrun-canonical.md`

### 2. 双轮 patched dry-run

- 诊断样本：
  - 第一次双轮尝试复用 canonical dry-run 逻辑并把 `dxf.job.start.target_count` 提升到 `2`，但仍沿用单轮预算。
  - 结果是 `overall_status=failed`、`verdict.kind=motion_timeout_unclassified`。
  - 该样本超时点已经进入第 `2` 轮，`completed_count=1`、`current_cycle=2`、`overall_progress_percent=71`。
  - 该样本用于证明“第一次失败更像验证预算不足”，不作为补丁行为回归结论。
- 诊断样本证据：
  - `tests/reports/online-validation/bug315-two-cycle-rerun/20260413-134041/real-dxf-machine-dryrun-canonical.json`
  - `tests/reports/online-validation/bug315-two-cycle-rerun/20260413-134041/real-dxf-machine-dryrun-canonical.md`

- 放宽预算后的有效样本：

```powershell
python <one-off launcher importing run_real_dxf_machine_dryrun.py>  # 运行时覆写 dxf.job.start.target_count=2，并将 --job-timeout 提升到 260
```

- 结果：
  - `overall_status=passed`
  - `verdict.kind=completed`
  - `target_count=2`
  - `completed_count=2`
  - `elapsed_seconds=196.9893`
- 关键证据：
  - `tests/reports/online-validation/bug315-two-cycle-rerun-timeout260/20260413-134423/real-dxf-machine-dryrun-canonical.json`
  - `tests/reports/online-validation/bug315-two-cycle-rerun-timeout260/20260413-134423/real-dxf-machine-dryrun-canonical.md`

### 3. `stop -> home.auto` 定向复测

- 执行命令：

```powershell
python .\tests\e2e\hardware-in-loop\run_dxf_stop_home_auto_probe.py `
  --gateway-exe D:\Projects\wt-bug315\build\bin\siligen_runtime_gateway.exe `
  --report-root D:\Projects\wt-bug315\tests\reports\online-validation\bug315-stop-home-auto-probe `
  --target-count 2
```

- 结果：
  - `overall_status=passed`
  - `verdict.kind=passed`
  - `job_terminal_state_after_stop=cancelled`
  - `axes_stopped_before_home=true`
  - `home_auto_ok=true`
  - `mc_prftrap_detected=false`
  - `prftrap_hit_count=0`
- 关键证据：
  - `tests/reports/online-validation/bug315-stop-home-auto-probe/20260413-134832/dxf-stop-home-auto-probe.json`
  - `tests/reports/online-validation/bug315-stop-home-auto-probe/20260413-134832/dxf-stop-home-auto-probe.md`

## 离线与 HIL 前置证据

- 手工离线 HIL 前置报告：
  - `tests/reports/hil-manual-offline-prereq/20260413T042129Z/workspace-validation.json`
  - `tests/reports/hil-manual-offline-prereq/20260413T042129Z/workspace-validation.md`
- 当前已知限制：
  - 根构建 / formal HIL 基础设施仍存在参数与离线门禁阻塞。
  - 本轮不能把“未跑通 controlled-HIL / limited-HIL”改写为“补丁未通过现场验证”。
  - 本轮也不能把当前 targeted machine evidence 改写为“canonical HIL PASS”。

## 当前结论与下一步

- 当前 BUG-315 主链修复已经拿到足够的 targeted evidence：
  - 单轮 patched dry-run 通过。
  - 双轮 patched dry-run 通过。
  - `stop/home.auto` probe 通过。
- 当前分支下一步进入 closeout：
  - 提交当前代码与测试。
  - 推送分支并创建 PR。
  - 在 PR 中明确区分：
    - `真实机台 targeted evidence = passed`
    - `formal limited-hil = infrastructure blocked`

## 未完成项

- 未在本轮修复 `run_real_dxf_machine_dryrun.py` 的正式 `--target-count` 参数化能力。
- 未在本轮修复 formal `limited-hil` 的仓库基础设施阻塞。
