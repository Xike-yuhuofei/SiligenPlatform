# HIL 闭环/长稳发布结论模板 V1

更新时间：`2026-03-19`

## 1. 执行信息

- 执行日期：`2026-03-20`
- 执行人：`Codex + Xike`
- Profile：`Local`
- 报告目录：`integration/reports/hil-controlled-test`
- 时长参数：`--duration-seconds=1800`
- pause/resume 周期：`--pause-resume-cycles=3`
- dispenser 参数：`--dispenser-count=300 --dispenser-interval-ms=200 --dispenser-duration-ms=80`
- 状态等待参数：`--state-wait-timeout-seconds=8`

## 2. 自动化证据

- `workspace-validation.json`：`integration/reports/hil-controlled-test/workspace-validation.json`（generated_at=`2026-03-19T16:53:03.650664+00:00`）
- `workspace-validation.md`：`integration/reports/hil-controlled-test/workspace-validation.md`
- `hil-controlled-test/hil-closed-loop-summary.json`：`integration/reports/hil-controlled-test/hil-closed-loop-summary.json`（generated_at=`2026-03-19T17:24:12.285952+00:00`）
- `hil-controlled-test/hil-closed-loop-summary.md`：`integration/reports/hil-controlled-test/hil-closed-loop-summary.md`

## 3. 判定摘要

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| hardware smoke | `通过` | TCP endpoint 可达 |
| hil-closed-loop | `通过` | 1800s 执行 22 次迭代，`overall_status=passed` |
| long soak duration | `通过` | `duration_seconds=1800`，`elapsed_seconds=1865.32` |
| state transition checks | `阻塞（门禁项）` | 任一 `state_transition_checks.status=skipped/failed` 直接判定阻塞 |
| timeout_count | `通过` | `0` |
| reconnect_count | `通过` | `0` |
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

本轮结论：`有条件通过`

说明：

- 长稳目标、报告产物与失败计数门槛均满足。
- 当前环境下 `dispenser start` 命令呈“阻塞式完成”语义，导致 pause/resume 状态转换在长稳脚本中按兼容策略跳过并留痕为 `state_transition_checks.status=skipped`。

## 5. 范围声明

本结论只代表：

- `HIL 闭环与长稳验证结果`

本结论不代表：

- 真机 smoke / 试运行 / 现场签收完成
- 整机或工艺真实性签收完成
