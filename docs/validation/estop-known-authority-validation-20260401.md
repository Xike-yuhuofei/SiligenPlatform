# estop_known Authority Validation 2026-04-01

- 任务类型：`incident`
- Ticket：`TASK-003`
- 分支：`fix/protocol/TASK-003-estop-known-authority`
- 验证日期：2026-04-01
- 范围：冻结并修正 `status` 中 `estop_known` 在断线态下的 authority 口径，确保 HMI consumer 不再把“断线且无权威急停来源”误解释为已知。
- 结论：通过。`estop_known` 已不再由 disconnected 场景下的 `machine_execution_snapshot.emergency_stopped` 间接兜底。

## 背景

- `BUG-312` 收口证据显示：在 `connected=false`、`connection_state=disconnected`、`machine_state_reason=hardware_disconnected` 的快照里，`door_open_known=false`，但 `estop_known=true`，且 `sources.estop=system_interlock`。
- 该行为会把执行状态快照误当成急停信号 authority，对 HMI 与诊断消费者构成误导。

## 契约结论

- 本轮语义对象：`status.io.estop_known` 与 `status.effective_interlocks.estop_known`。
- authority 口径：
  - 当后端持有权威急停来源时，`estop_known=true`。
  - 当处于断线态且没有有效急停采样时，`estop_known=false`。
- 非 authority：
  - `machine_execution_snapshot.emergency_stopped` 只表达执行状态，不是急停输入真值，不应在 disconnected 场景下兜底为 `estop_known=true`。

## 代码边界

- `apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
- `apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py`
- `shared/contracts/application/models/states.json`
- `shared/contracts/application/tests/test_protocol_compatibility.py`
- `apps/hmi-app/tests/unit/test_protocol_preview_gate_contract.py`

## 修复摘要

- `TcpCommandDispatcher::HandleStatus` 只在 `connected=true` 时才把 `systemFacade_->IsInEmergencyStop()` 视为 `estop_known` 的有效来源。
- `estop_known` 默认从连接态出发，断线且无 interlock 采样时不再被 `system_interlock` 口径兜底为 true。
- `states.json` 增补 `estop_known` 文案，明确“断线且无有效采样/无权威急停来源时必须为 false”。
- HMI unit test 增加断线未知态样本，锁定 `gate_estop_known()` 与 `gate_estop_active()` 的消费行为。

## 执行验证

执行命令：

```powershell
python apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py
python shared/contracts/application/tests/test_protocol_compatibility.py
python -m unittest apps/hmi-app/tests/unit/test_protocol_preview_gate_contract.py
```

结果：

- `transport-gateway compatibility checks passed`
- `application-contracts compatibility checks passed`
- `test_protocol_preview_gate_contract.py`：28 tests，通过

## 残余风险

- 本轮没有调整 `sources.estop` 在 connected 场景下的命名精度，只修正 disconnected 场景下的已知性兜底问题。
- 本轮没有修改 `disconnect` RPC 语义。
- 若后续需要把 `system_interlock` 与“真实急停输入来源”进一步解耦，应另立任务，不在本轮继续扩散。
