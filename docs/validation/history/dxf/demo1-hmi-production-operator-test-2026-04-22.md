# Demo-1 HMI Production Operator Test 2026-04-22

更新时间：`2026-04-22`

## 1. 测试目标

- 通过 HMI 主链路模拟人工完成 `Demo-1.dxf` 真实生产。
- 优先发现人工操作时会遇到的阻塞、误导、低效和恢复问题。
- 将“操作问题排查”与“轴/阀日志和规划数据严格一一对应”分开结论，不混写。

## 2. 操作范围

- DXF / 生产对象：`D:\Projects\SiligenSuite\samples\dxf\Demo-1.dxf`
- HMI 入口：`apps/hmi-app/scripts/online-smoke.ps1`
- 是否允许绕过 HMI：`不允许`
- 设备前提：用户已确认“已经具备直接联机条件”
- 必要证据：`runtime/gateway` 日志、报告目录、`coord-status-history`、异常截图或录像

## 3. 控制脚本能力结论

- 入口脚本：`apps/hmi-app/scripts/online-smoke.ps1`
- 当前已确认覆盖：
  - `operator_preview`
  - `home_move`
  - `jog`
  - `supply_dispenser`
  - `estop_reset`
  - `door_interlock`
- 当前未确认覆盖：
  - 通过 HMI 自动化脚本执行 `production start`
  - 生产执行中状态观察
  - 执行完成后的收尾与下一单恢复
- 结论：`BLOCK`
- 阻断原因：
  - 当前仓库内可见的 HMI UI 自动化入口已覆盖在线预览和若干运行时动作，但未提供“导入 Demo-1 -> prepare -> production start -> 完整执行 -> 收尾”的自动化生产路径。
  - 在“脚本必须控制 UI、不能跳过 HMI 接口”的前提下，不能用 `run_real_dxf_production_validation.py` 之类的接口型入口替代。

## 4. 操作主线

目标主线应为：

1. 启动 HMI
2. 确认联机与设备状态
3. 导入 `Demo-1.dxf`
4. 执行 preview / prepare
5. 发起 production start
6. 观察执行中状态、按钮状态与报错
7. 观察完成或中止后的界面、日志、报告
8. 尝试再次发起下一单或恢复操作

当前实际状态：

- `1` 到 `4` 已有部分自动化能力锚点
- `5` 到 `8` 缺少 HMI 自动化生产入口

## 5. 操作问题清单

- 类型：`自动化能力缺口`
  现象：当前 HMI 自动化脚本未暴露 production start 全链路，无法按“脚本控制 UI”的口径执行本轮专项。
  影响：无法在不绕过 HMI 的前提下，稳定复现人工生产主线，也无法系统收集操作问题清单。
  复现步骤：查看 `apps/hmi-app/scripts/online-smoke.ps1`、`apps/hmi-app/src/hmi_client/tools/ui_qtest.py`、`apps/hmi-app/scripts/run_online_runtime_action_matrix.py` 的已支持 profile。
  当前证据：
  - `ui_qtest.py` 当前支持的 runtime action profile 为 `full`、`operator_preview`、`snapshot_render`、`home_move`、`jog`、`supply_dispenser`、`estop_reset`、`door_interlock`
  - 未发现面向 DXF 生产启动的 HMI UI 自动化 profile
  建议归属层：`apps/hmi-app` UI 自动化 / online smoke orchestration

- 类型：`证据边界`
  现象：已有真机生产正例证明 `Demo-1.dxf` 可以完成生产执行，但该证据来自接口型生产验证入口，不是 HMI 控制脚本。
  影响：可以证明“机器可执行生产”，但不能证明“人工通过 HMI 操作时不会遇到问题”。
  复现步骤：检查 `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/`
  当前证据：
  - `real-dxf-production-validation.md`
  - `real-dxf-production-validation.json`
  - `gateway-stdout.log`
  - `gateway-stderr.log`
  - `tcp_server.log`
  - `coord-status-history.json`
  建议归属层：`tests/e2e/hardware-in-loop` 与 `apps/hmi-app` 之间的联机自动化边界

## 6. 追溯一致性结论

- 生产执行结论：`已有外部正例，但不属于本次 HMI operator test 的直接通过证据`
- 严格一一对应结论：`未证明`
- 证据边界：
  - `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/real-dxf-production-validation.md` 显示：
    - `overall_status=passed`
    - `validation_mode=production_execution`
    - `job_terminal_completed=passed`
    - `profile_compare_business_trigger_matches_glue_count=passed`
    - `profile_compare_immediate_pulse_matches_start_boundary_count=passed`
    - `execution_complete_logged=passed`
    - `supply_close_logged=passed`
    - `observed_axis_coverage=passed`
  - 但 `coord-status-history.json` 当前内容为 `[]`
  - 因此，这批证据最多证明 `Demo-1.dxf` 在真机上可完成一次生产执行，以及 summary-level alignment 通过；不能升级成“轴/阀日志与规划数据严格一一对应”

## 7. 关键证据

- HMI：
  - `apps/hmi-app/scripts/online-smoke.ps1`
  - `apps/hmi-app/src/hmi_client/tools/ui_qtest.py`
  - `apps/hmi-app/scripts/run_online_runtime_action_matrix.py`
- runtime / gateway：
  - `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/gateway-stdout.log`
  - `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/gateway-stderr.log`
  - `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/tcp_server.log`
- report：
  - `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/real-dxf-production-validation.md`
  - `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/real-dxf-production-validation.json`
- coord-status-history：
  - `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/coord-status-history.json`
- 其他状态历史：
  - `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/job-status-history.json`
  - `tests/reports/online-validation/dxf-production-path-trigger/20260422-095754/machine-status-history.json`
- 异常截图/录像：
  - 本轮尚未生成，因为 HMI 自动化生产入口缺失，测试在准入阶段被阻断

## 8. 中止条件与风险

- 若继续以当前入口执行，本轮会被迫退化成“接口型 production validation”，不再满足“脚本必须控制 UI”的前提，应立即中止。
- 若在 `coord-status-history` 仍为空的情况下直接宣称“严格一一对应”，会把 summary-level alignment 误写成 strict correspondence。

## 9. 建议下一步

- 先补一个新的 HMI runtime action profile，使 `online-smoke.ps1` / `ui_qtest.py` 真正覆盖：
  - DXF browse
  - preview / prepare
  - production start
  - 执行中观察
  - 完成后恢复或下一单
- 该 profile 的输出必须至少沉淀：
  - HMI 时间线
  - 截图或录像
  - 对应 run 的 `runtime/gateway` 日志路径
  - 报告目录
  - `coord-status-history`
- 在新增 profile 之前，本专项按 `BLOCK` 处理，而不是按 `FAIL` 处理；阻断点是 HMI 自动化能力不足，不是 `Demo-1.dxf` 生产能力不足。
