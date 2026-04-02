# process-planning

`modules/process-planning/` 是 `M4 process-planning` 的 canonical owner 入口。

## Owner 范围

- 工艺配置、规划参数与运行前配置事实的 owner 收敛。
- 为 `M5-M8` 提供 configuration owner 能力，不越权声明完整 `ProcessPlan` owner。
- 不在本轮承担 `FeatureGraph -> ProcessPlan` 全链路规划实现口径。

## Owner 入口

- 构建入口：`modules/process-planning/CMakeLists.txt`（target：`siligen_module_process_planning`）。
- 模块契约入口：`modules/process-planning/contracts/README.md`。

## 边界约束

- `apps/` 层只保留宿主和装配职责，不承载 `M4` 终态 owner 事实。
- `M9 runtime-execution` 只能消费 `M4-M8` 结果，不得回写 `ProcessPlan`。
- `modules/process-planning/` 是 `M4` 唯一 live owner 根。
## 当前事实来源

- `modules/process-planning/domain/configuration/`
- `shared/contracts/engineering/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `domain/configuration/` 已补齐 canonical 子域 target，模块根 target 已从历史聚合 target 切到该 canonical 子域。
- 所有 live 实现与构建入口均已收敛到 canonical roots。
