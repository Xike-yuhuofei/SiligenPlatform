# Field Acceptance

更新时间：`2026-03-21`

## 1. 结论摘要

本轮验收覆盖：

- mock：HMI client -> TCP mock server -> mock core 行为
- simulation：工程数据导出、`packages/simulation-engine` fixture 回归、直接 `simulate_dxf_path.exe` 运行
- protocol compatibility：application / engineering / transport 契约兼容
- HIL：`integration/hardware-in-loop/run_hil_controlled_test.ps1`（60s 快速门禁 + 1800s 全流程）
- real hardware：`2026-03-20` 目标机台执行 `1800s` 闭环长稳并通过

当前结论：

- `mock`、`simulation`、`protocol compatibility` 维持通过。
- `HIL` 闭环/长稳在 `pause/resume=3` 门槛下通过，状态门禁无 `skipped/failed`。
- `real hardware` 本轮已执行并纳入验收证据，但该结论不等同工艺质量签收。
- `2026-03-21` 已补做一次新 `artifact / plan / job` 主链的真机真实点胶回归，记录见 [DXF 真机真实点胶回归记录 2026-03-21](/D:/Projects/SiligenSuite/docs/validation/dxf-real-dispense-field-regression-2026-03-21.md)。

## 2. 分层结果

| 层级 | 结果 | 说明 |
|---|---|---|
| mock | `partial pass` | HMI client + mock TCP server 跑通；`dxf.resume`、`recipe.list` 后端方法不存在。 |
| simulation | `pass` | engineering regression、simulation smoke/json-io、fixture regression、100 次 soak 均通过。 |
| protocol compatibility | `pass` | application / engineering / transport 契约兼容通过。 |
| HIL | `pass` | `run_hil_controlled_test.ps1` 在 `60s` 与 `1800s` 均通过，`pause/resume=3`。 |
| real hardware | `pass` | `2026-03-20` 目标机台完成 `1800s` 闭环长稳，纳入验收证据。 |

## 3. HIL 当前事实

- `apps/control-tcp-server/run.ps1 -DryRun` 只验证 canonical `siligen_tcp_server.exe`。
- `run_hardware_smoke.py` 与 `run_hil_closed_loop.py` 均以工作区根为 `cwd` 执行。
- `1800s` 报告：`integration/reports/hil-controlled-test-20260320-1800-gate3-01`
  - `overall_status=passed`
  - `iterations=599`
  - `timeout_count=0`
  - `state_transition_checks=4193/4193 passed`
- 固定目录：`integration/reports/hil-controlled-test` 已同步最新通过证据，来源由 `latest-source.txt` 记录。

## 4. 关键业务链路结论

- `HMI -> TCP -> Core`：mock 验证通过；HIL 闭环链路已通过。
- DXF 加载 / 执行：mock、simulation、HIL（含真实机台口径）均有通过证据。
- DXF 新主链真实点胶：`2026-03-21` 已完成一次 `dry_run=false` 真机闭环，供料与点胶阀运行中均被观测到打开，任务最终 `completed`。
- 暂停/恢复状态转换：已在 `pause/resume=3` 门槛下持续通过。
- 报警 / 异常恢复：mock 已验证；真实机台专项恢复场景仍建议补充。

## 5. 当前验收结论

- 可以确认的通过范围：mock、simulation、protocol compatibility、HIL 闭环长稳、real hardware 本轮长稳执行。
- 当前更准确的结论是：
  - `mock/simulation 替代验收通过`
  - `HIL 受控闭环长稳通过（门禁已收紧）`
  - `real hardware 已执行并纳入证据`
  - `DXF 新主链真机真实点胶已完成一次通过性回归`
  - `整机/工艺签收仍需独立工艺验收流程`
