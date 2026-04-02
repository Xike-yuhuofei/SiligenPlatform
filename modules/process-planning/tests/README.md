# tests

process-planning 的模块级验证入口应收敛到此目录。

当前最小正式测试面：

- `unit/`
  - `ReadyZeroSpeedResolver` fallback / missing-config 错误面
- `contract/`
  - `ConfigurationContracts.h` umbrella header 公开面
  - `ValveSupplyConfig` 的 DO 边界契约
- `golden/`
  - `DispensingConfig`、`Dxf*Config`、`MachineConfig`、`HomingConfig` 的默认值快照

当前未补：

- `process-planning -> process-path` 邻接 integration
- 更完整的配置加载 / parser / mode-mismatch 回归
- `regression/` 级历史样本
