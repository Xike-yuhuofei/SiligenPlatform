# process-planning

`modules/process-planning/` 是 `M4 process-planning` 的 canonical owner 入口。

## Owner 范围

- 工艺配置、规划参数与运行前配置事实的 owner 收敛。
- 为 `M5-M8` 提供 configuration owner 能力，不越权声明完整 `ProcessPlan` owner。
- 不在本轮承担 `FeatureGraph -> ProcessPlan` 全链路规划实现口径。

## Owner 入口

- 构建入口：`modules/process-planning/CMakeLists.txt`（target：`siligen_module_process_planning`）。
- 模块契约入口：`modules/process-planning/contracts/README.md`。
- 测试入口：`modules/process-planning/tests/CMakeLists.txt`。

## 边界约束

- `apps/` 层只保留宿主和装配职责，不承载 `M4` 终态 owner 事实。
- `M9 runtime-execution` 只能消费 `M4-M8` 结果，不得回写 `ProcessPlan`。
- `modules/process-planning/` 是 `M4` 唯一 live owner 根。
## 当前事实来源

- `modules/process-planning/domain/configuration/`
- `shared/contracts/engineering/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `domain/configuration/` 保留为模块内 configuration 实现库；`siligen_module_process_planning` 现在固定透传 `siligen_process_planning_application_public + siligen_process_planning_contracts_public`，不再回退到 `domain_configuration` public fallback。
- 所有 live 实现与构建入口均已收敛到 canonical roots。

## 当前测试面

- `tests/unit/`
  - 冻结 `ReadyZeroSpeedResolver` 的 fallback 与 missing-config 错误面
- `tests/contract/`
  - 冻结 `ConfigurationContracts.h` 的公开面与 `ValveSupplyConfig` 边界
- `tests/golden/`
  - 冻结 `DispensingConfig`、`Dxf*Config`、`MachineConfig`、`HomingConfig` 默认快照
