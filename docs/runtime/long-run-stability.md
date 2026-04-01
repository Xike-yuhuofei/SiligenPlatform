# Long Run Stability

更新时间：`2026-04-01`

## 1. 长稳策略

目标机台 HIL 长稳采用双阶段：

- 快速门禁：`60s`
- 正式门禁：`1800s`
- 默认门槛：`pause/resume=3`

## 2. 推荐执行方式

```powershell
.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -HilDurationSeconds 1800 `
  -HilPauseResumeCycles 3
```

当前默认把多轮 `home + closed_loop` 稳定性矩阵纳入同一批 gate：

```powershell
.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -HilDurationSeconds 1800 `
  -HilPauseResumeCycles 3
```

如需临时隔离矩阵影响，可显式传：`-IncludeHilCaseMatrix:$false`

## 3. 报告与发布

- 时间戳报告：`tests/reports/hil-controlled-test/<timestamp>/`
- 固定最新目录：`tests/reports/hil-controlled-test/`
- 发布清单：`tests/reports/hil-controlled-test/latest-source.txt`

固定目录至少包含：

- `workspace-validation.json/.md`
- `hil-closed-loop-summary.json/.md`
- `hil-controlled-gate-summary.json/.md`
- `hil-controlled-release-summary.md`
- 默认额外包含 `hil-case-matrix/case-matrix-summary.json/.md`

## 4. 判定基线

- `workspace-validation`：`failed=0`、`known_failure=0`、`skipped=0`
- `hil-closed-loop`：`overall_status=passed`
- `pause_resume_cycles`：达到预设门槛（默认 `3`）
- `timeout_count`：`0`
- `hil-case-matrix` workspace case 为 `passed`，且 `case-matrix-summary.overall_status=passed`

## 5. 下一阶段建议

1. 在不同班次重复至少 `2` 轮 `1800s`，确认跨时段稳定性。
2. 增加报警注入/异常恢复受控场景，补齐恢复闭环证据。
