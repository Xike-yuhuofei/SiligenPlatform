# Runtime Passive Disconnect Validation 2026-04-01

- 任务类型：`incident`
- Ticket：`BUG-312`
- Closeout branch：`fix/runtime/BUG-312-passive-disconnect-state`
- 验证日期：2026-04-01
- 验证范围：真实被动断线后 `runtime-gateway` 的连接态收敛、`machine_state_reason` 收敛、interlock monitor 生命周期收敛，以及对应离线回归与受限 L5 证据闭环。
- 结论：通过。主故障“被动断线后长期停在 `degraded`，且 interlock known 状态不收敛”已在离线回归和受限 L5 中闭环。

## 事件与根因

- 现场现象：真实被动断线后，`runtime-gateway status` 长期返回 `connection_state=degraded`，`machine_state_reason` 不能稳定收敛到 `hardware_disconnected`，同时 interlock monitor 未随断线停止，`door_open_known` 仍可能表现为已知。
- 根因 1：`MotionRuntimeFacade::MonitoringLoop()` 在 heartbeat degraded 后跳过断线检查，导致底层连接态无法进一步退到 `Disconnected`。
- 根因 2：`TcpCommandDispatcher::HandleStatus()` 在 heartbeat degraded 时优先对外暴露 `degraded`，会掩盖底层已经断开的真实连接快照。

## 代码边界

- `modules/runtime-execution/adapters/device/src/adapters/motion/controller/runtime/MotionRuntimeFacade.cpp`
- `modules/runtime-execution/adapters/device/src/adapters/motion/controller/runtime/MotionRuntimeFacade.h`
- `apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
- `apps/runtime-service/bootstrap/InfrastructureBindingsBuilder.cpp`
- `apps/runtime-service/security/InterlockMonitor.cpp`
- `apps/runtime-service/security/InterlockMonitor.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h`
- `modules/runtime-execution/adapters/device/src/drivers/multicard/MockMultiCardWrapper.cpp`
- `modules/runtime-execution/runtime/host/tests/unit/runtime/motion/MotionRuntimeFacadeTest.cpp`
- `apps/runtime-service/tests/integration/HostBootstrapSmokeTest.cpp`
- `apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py`

## 修复摘要

- `MotionRuntimeFacade` 将“连接宽限期”和“状态监控宽限期”拆开，只在短状态监控宽限期内跳过断线检查，不再因 heartbeat degraded 永久跳过被动断线收敛。
- `MotionRuntimeFacade` 在 connect/disconnect 与状态变化路径上统一刷新 `connection_info_` 并触发 connection state callback，保证 consumer 能收到真实连接态变更。
- `InfrastructureBindingsBuilder` 将 interlock monitor 生命周期绑定到 device connection snapshot；连接建立时启动监控，断线时停止监控，并在启用 interlock 时显式启动连接态监控。
- `InterlockMonitor` 区分“监控未运行/采样不可用”和“真实安全触发”，在断线或采样失败时返回 unavailable/error，不再把 unknown/unavailable 偷换成 fail-safe estop/door known。
- `TcpCommandDispatcher::HandleStatus` 只在“底层快照仍为 `Connected` 且 heartbeat degraded”时对外返回 `degraded`；若底层快照已断开，则优先暴露 `disconnected` 和 `hardware_disconnected`。

## 离线验证

执行命令：

```powershell
cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_runtime_gateway siligen_runtime_host_unit_tests runtime_service_integration_host_bootstrap_smoke

C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_runtime_host_unit_tests.exe --gtest_filter=MotionRuntimeFacadeTest.*

C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\runtime_service_integration_host_bootstrap_smoke.exe

python apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py

C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_runtime_host_unit_tests.exe --gtest_filter=MotionRuntimeFacadeTest.PassiveDisconnectStillTransitionsToDisconnectedAfterHeartbeatDegrades --gtest_repeat=3

C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\runtime_service_integration_host_bootstrap_smoke.exe --gtest_filter=RuntimeExecutionIntegrationHostBootstrapSmokeTest.InterlockMonitorStopsAfterPassiveDisconnectWhenHeartbeatDegradesFirst --gtest_repeat=3
```

结果摘要：

- `MotionRuntimeFacadeTest.*` 通过。
- `RuntimeExecutionIntegrationHostBootstrapSmokeTest` 通过。
- `test_transport_gateway_compatibility.py` 通过。
- 两条关键回归用例均已执行 `--gtest_repeat=3`，通过。

## 受限 L5 证据

- 证据目录：`tests/reports/online-validation/passive-disconnect-postfix-20260401-131941`
- 主结论证据：
  - 未连接基线：`connection_state=disconnected`
  - 未连接基线：`door_open_known=false`，`door_open source=unknown`
  - 被动断线后首次采样：`connection_state=disconnected`
  - 被动断线后：`machine_state_reason=hardware_disconnected`
  - 被动断线后：日志出现 `连锁监控器已停止`
- 证据文件：
  - `tests/reports/online-validation/passive-disconnect-postfix-20260401-131941/l5-summary.json`
  - `tests/reports/online-validation/passive-disconnect-postfix-20260401-131941/tcp_server.tail.after-disconnect.log`
  - `tests/reports/online-validation/passive-disconnect-postfix-20260401-131941/tcp-status-trace.json`

## 契约影响

- 未修改 `status` 响应结构和字段名。
- 未修改 `disconnect` RPC 的幂等确认语义。
- 修改的是“连接态与 interlock 可用性如何收敛到既有字段”的行为，而不是新增协议面字段。

## 残余风险

- `estop_known` 当前仍来自 `system_interlock` 口径；本轮未治理 disconnected 与 fail-safe estop 的更细粒度语义区分。
- `disconnect` RPC 仍是幂等确认语义，不代表真实硬件已经断开；该语义需独立治理。
- 恢复阶段观测到额外 localhost 客户端活动，因此恢复后的 `Idle/homed` 快照只作为次级观察，不作为本轮主结论依据。

## 未执行项

- 未额外做性能验证；本轮目标不是性能问题。
- 未扩展到真实轴运动或真实阀输出验证；本轮联机门禁保持“单宿主、限网卡、无运动、无阀输出”。
- 未在本次 closeout 中重复执行已通过的离线回归与 L5 复核；直接复用 2026-04-01 已生成的验证证据。
