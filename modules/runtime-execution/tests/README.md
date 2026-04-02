# tests

runtime-execution 的模块级验证入口应收敛到此目录。

- `tests/runtime-host/` 仅作为模块级测试索引入口；真实 runtime host 测试实现已迁到 `runtime/host/tests/`。
- `tests/integration/` 是模块级 integration 正式入口；当前 canonical target `runtime_execution_integration_host_bootstrap_smoke` 由 `runtime/host/tests/integration/` 提供，覆盖 host core 端口最小装配与 `MotionRuntimeFacade -> MotionRuntimeConnectionAdapter -> RuntimeSupervisionPortAdapter` 的 supervision 闭环。
- `tests/regression/` 是模块级 regression 正式入口；当前 canonical target 由 `runtime/host/tests/regression/` 提供，覆盖 machine execution 状态机映射、`estop/recover` 以及 passive disconnect 后的 reconnect recovery。
