# Long Run Stability

更新时间：`2026-03-20`

## 1. 本轮长稳策略

本轮在目标机台上执行 HIL 闭环/长稳双阶段验证，默认门槛统一为 `pause/resume=3`：

- 快速门禁：`60s` 受控回归（验证参数与发布流程）
- 正式门禁：`1800s` 全流程闭环长稳（真实机台口径）
- 证据双轨：时间戳目录保留审计，固定目录同步最新通过结果

## 2. 已执行结果

### 2.1 HIL 快速门禁（60s）

- 报告目录：`integration/reports/hil-controlled-test-20260320-smoke60-01`
- `overall_status=passed`
- `duration_seconds=60`
- `pause_resume_cycles=3`
- `iterations=21`
- `timeout_count=0`
- `state_transition_checks`: 全部 `passed`

### 2.2 HIL 正式长稳（1800s）

- 报告目录：`integration/reports/hil-controlled-test-20260320-1800-gate3-01`
- `workspace validation`: `passed=3 failed=0 known_failure=0 skipped=0`
- `hil-closed-loop`：
  - `overall_status=passed`
  - `duration_seconds=1800`
  - `elapsed_seconds=1803.111`
  - `pause_resume_cycles=3`
  - `iterations=599`
  - `timeout_count=0`
  - `state_transition_checks`: `4193/4193` 为 `passed`

### 2.3 固定目录发布（双轨）

- 固定目录：`integration/reports/hil-controlled-test`
- 同步结果：
  - `workspace-validation.json/.md`
  - `hil-closed-loop-summary.json/.md`
- 追溯清单：`integration/reports/hil-controlled-test/latest-source.txt`
  - `source_report_dir=integration/reports/hil-controlled-test-20260320-1800-gate3-01`

### 2.4 real hardware 归类

- 本轮 `1800s` 长稳已纳入真实机台验收证据口径。
- 该结论代表“闭环控制与状态转换门禁在目标机台通过”，不替代工艺质量/产能签收。

## 3. 当前结论

- `mock/simulation` 稳定性：通过
- `HIL 60s + 1800s`：通过（门禁参数与状态门禁均满足）
- `real hardware`：已执行并纳入验收证据

## 4. 下一阶段建议

1. 在不同班次重复至少 `2` 轮 `1800s`，确认跨时段稳定性一致。  
2. 在 HIL 脚本上增加“报警注入/异常恢复”受控场景，补齐现场恢复闭环证据。  
