# HMI Manual Dispense Feedback Validation 2026-04-12

- 任务类型：`incident`
- Ticket：`NOISSUE`
- 分支：`fix/hmi/NOISSUE-manual-dispense-feedback`
- 验证日期：`2026-04-12`
- 范围：修正 HMI 手动点胶默认参数过弱导致“点胶阀无明显动作”的现场体验，并让 HMI 状态栏透传后端 `message/code`。
- 结论：通过。真机已确认默认 `20/40/40` 可驱动阀控链路，`SUPPLY_NOT_OPEN` 失败详情也已透传到 HMI。

## 背景

- 现场现象：HMI 点击点胶阀“启动”无明显硬件动作，但供胶阀控制有响应。
- 已收敛根因：`apps/hmi-app` 手动点胶默认参数为 `10/1000/15`，对当前现场机型的手动感知过弱；主链 `runtime-gateway` 和阀控后端并未损坏。
- 本轮不扩展到 DXF、运动轴、长稳或工艺验收，只验证 HMI 手动阀控链路与失败反馈。

## 契约结论

- HMI 手动点胶默认参数冻结为：
  - `count=20`
  - `interval_ms=40`
  - `duration_ms=40`
- `dispenser.start`
  - 成功时 HMI 必须展示实际下发参数。
  - 失败时 HMI 必须展示后端返回的 `message/code`。
- `dispenser.stop` / `supply.open` / `supply.close`
  - 失败时 HMI 也必须透传后端返回的 `message/code`，而不是只显示泛化失败。

## 代码边界

- [apps/hmi-app/src/hmi_client/ui/main_window.py](/D:/Projects/SiligenSuite/apps/hmi-app/src/hmi_client/ui/main_window.py)
- [apps/hmi-app/src/hmi_client/client/protocol.py](/D:/Projects/SiligenSuite/apps/hmi-app/src/hmi_client/client/protocol.py)
- [apps/hmi-app/tests/unit/test_main_window.py](/D:/Projects/SiligenSuite/apps/hmi-app/tests/unit/test_main_window.py)
- [apps/hmi-app/tests/unit/test_protocol_preview_gate_contract.py](/D:/Projects/SiligenSuite/apps/hmi-app/tests/unit/test_protocol_preview_gate_contract.py)

## 修复摘要

- `MainWindow` 手动点胶默认值从 `10/1000/15` 调整为 `20/40/40`。
- `CommandProtocol` 为 `dispenser.start`、`dispenser.stop`、`supply.open`、`supply.close` 统一返回 `(ok, error_message, error_code)`。
- `MainWindow` 状态栏失败文案统一走 `action + code + message` 格式。
- HMI 单元测试与协议契约测试增加成功/失败样本，锁定默认参数与错误透传行为。

## 执行验证

执行命令：

```powershell
python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q
python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q
```

结果：

- `test_main_window.py`：`79 passed`
- `test_protocol_preview_gate_contract.py`：`43 passed`

已知未收口项：

- `python -m pytest .\apps\hmi-app\tests\unit -q` 仍有 1 个无关失败：
  - [apps/hmi-app/tests/unit/test_preview_flow_integration.py](/D:/Projects/SiligenSuite/apps/hmi-app/tests/unit/test_preview_flow_integration.py)
  - 用例：`PreviewFlowIntegrationTest.test_offline_dxf_load_uses_same_source_preview_builder`
  - 该失败与本轮阀控修复无直接关系，本轮未继续扩散处理。

## 真机证据

### 1. HMI 在线启动准入

- [online-startup.stdout.log](/D:/Projects/SiligenSuite/tests/reports/hmi-valve-hil-20260412-140343/online-startup.stdout.log)
- 结论：
  - `SUPERVISOR_EVENT` 阶段序列完整进入 `online_ready`
  - `SUPERVISOR_DIAG ... online_ready=true`

### 2. 最小阀控正常链路

- 证据目录：
  - [tests/reports/hmi-valve-hil-20260412-140615/hmi-regression.stdout.log](/D:/Projects/SiligenSuite/tests/reports/hmi-valve-hil-20260412-140615/hmi-regression.stdout.log)
  - [tests/reports/hmi-valve-hil-20260412-140615/tcp_server.delta.log](/D:/Projects/SiligenSuite/tests/reports/hmi-valve-hil-20260412-140615/tcp_server.delta.log)
  - [tests/reports/hmi-valve-hil-20260412-140615/success-chain.png](/D:/Projects/SiligenSuite/tests/reports/hmi-valve-hil-20260412-140615/success-chain.png)
- HMI 观测：
  - `供料阀已打开`
  - `点胶已启动 (次数:20, 间隔:40ms, 持续:40ms)`
  - `点胶已停止`
  - `供料阀已关闭`
- TCP / 硬件观测：
  - `supply.open` 成功，`supply_open=true`
  - `dispenser.start` 实际下发参数为 `count=20, intervalMs=40, durationMs=40`
  - 点胶过程中多次观测到 `CallMC_CmpPulseOnce ... PulseWidth=40ms`
  - `dispenser.stop` 和 `supply.close` 后状态收敛回 `supply_open=false`、`valve_open=false`

### 3. 失败详情透传

- 证据目录：
  - [tests/reports/hmi-valve-hil-20260412-140615/failure-message.png](/D:/Projects/SiligenSuite/tests/reports/hmi-valve-hil-20260412-140615/failure-message.png)
- HMI 观测：
  - 在供料关闭基线下直接点胶，状态栏显示：
    - `点胶启动失败(code=2701): failure_stage=check_valve_safety_supply_open;failure_code=SUPPLY_NOT_OPEN;message=supply_valve_not_open`
- TCP 观测：
  - `dispenser.start` 返回：
    - `error.code=2701`
    - `error.message=failure_stage=check_valve_safety_supply_open;failure_code=SUPPLY_NOT_OPEN;message=supply_valve_not_open`

## 残余风险

- 本轮只验证了手动阀控最小动作，没有覆盖 `pause/resume`、门禁/急停阻断、长时间重复点胶或工艺胶量签收。
- 本轮没有把这条真机专项纳入正式 gate，只补了历史证据与单测锁定。
- 若后续需要长期防回归，建议把“供料未开直接点胶，HMI 必须透传 `SUPPLY_NOT_OPEN`”补成稳定自动化在线专项。
