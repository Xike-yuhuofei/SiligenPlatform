# tests

runtime-execution 的模块级验证入口应收敛到此目录。

- `tests/unit/` 是模块内部 unit 正式入口；当前承接 `DispensingExecutionUseCaseInternalTest`，用于验证 execution-session 内部状态收口、活动 task 约束与终态提交规则。
- `tests/runtime-host/` 仅作为模块级测试索引入口；真实 runtime host 测试实现已迁到 `runtime/host/tests/`。
- `tests/integration/` 是模块级 integration 正式入口；当前登记 `runtime_execution_integration_host_bootstrap_smoke`，用于验证 host bootstrap/configuration 在 `Mock` 配置下可完成最小装配且不依赖真实硬件。
- `tests/regression/` 是模块级 regression 正式入口；当前先保留 canonical 回归承载面与 target 聚合点，后续硬件相关回归矩阵在 A3/A4 清障后继续补齐。
