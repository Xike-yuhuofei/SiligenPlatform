# HIL 闭环/长稳发布结论模板 V1

更新时间：`2026-03-20`

## 1. 执行信息

- 执行日期：`2026-03-20`
- 执行人：`Codex + Xike`
- Profile：`Local`
- 固定报告目录：`integration/reports/hil-controlled-test`
- 来源时间戳目录：`integration/reports/hil-controlled-test-20260320-1800-gate3-01`
- 时长参数：`--duration-seconds=1800`
- pause/resume 周期：`--pause-resume-cycles=3`
- dispenser 参数：`--dispenser-count=300 --dispenser-interval-ms=200 --dispenser-duration-ms=80`
- 状态等待参数：`--state-wait-timeout-seconds=8`

## 2. 自动化证据

- `workspace-validation.json`：`integration/reports/hil-controlled-test/workspace-validation.json`（generated_at=`2026-03-20T06:26:23.074514+00:00`）
- `workspace-validation.md`：`integration/reports/hil-controlled-test/workspace-validation.md`
- `hil-closed-loop-summary.json`：`integration/reports/hil-controlled-test/hil-closed-loop-summary.json`（generated_at=`2026-03-20T06:56:30.595963+00:00`）
- `hil-closed-loop-summary.md`：`integration/reports/hil-controlled-test/hil-closed-loop-summary.md`
- 来源追溯：`integration/reports/hil-controlled-test/latest-source.txt`

## 3. 判定摘要

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| hardware smoke | `通过` | TCP endpoint 可达 |
| hil-closed-loop | `通过` | 1800s 执行 `599` 次迭代，`overall_status=passed` |
| long soak duration | `通过` | `duration_seconds=1800`，`elapsed_seconds=1803.111` |
| state transition checks | `通过` | `4193/4193` 均为 `passed`，无 `skipped/failed` |
| timeout_count | `通过` | `0` |
| reconnect_count | `通过` | `599`（每轮 `connect/disconnect` 设计下的连接计数） |
| known failure / skipped | `通过` | workspace-validation 统计为 `0 / 0` |

## 4. 结论

只允许填写以下三种结论之一：

- `通过`
- `有条件通过`
- `阻塞`

填写规则：

- 闭环动作完整通过，报告完整，且满足长稳时长门槛：`通过`
- 关键动作通过，但存在非阻断说明项（如受控允许的单点波动）：`有条件通过`（不适用于状态转换门禁项）
- 任一关键动作失败、报告缺失或时长不达标：`阻塞`

新增门禁（`2026-03-20` 起生效）：

- `dispenser.start` 必须在状态窗口内观测到 `Running`，不再接受“阻塞完成后视为通过”。
- `pause/resume` 状态转换检查不允许 `skipped`，出现即 `阻塞`。

本轮结论：`通过`

说明：

- 长稳目标、报告产物、状态转换门禁与失败计数门槛均满足。
- 证据已按“双轨”发布：时间戳目录保留审计，固定目录用于对外引用。

## 5. 范围声明

本结论只代表：

- `HIL 闭环与长稳验证结果`
- `真实机台口径下的闭环控制稳定性证据已补齐`

本结论不代表：

- 工艺质量签收完成
- 整机产能签收完成
