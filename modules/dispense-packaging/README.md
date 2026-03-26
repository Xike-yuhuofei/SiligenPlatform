# dispense-packaging

`modules/dispense-packaging/` 是 `M8 dispense-packaging` 的 canonical owner 入口。

## Owner 范围

- 承接 `DispenseTimingPlan`、`ExecutionPackage` 的正式 owner 语义。
- 负责时序构建、执行包组装与离线校验的模块边界。
- 向 `M9 runtime-execution` 输出可消费但不可回写的执行准备事实。

## Owner 入口

- 构建入口：`CMakeLists.txt`（target：`siligen_module_dispense_packaging`）。
- 模块契约入口：`contracts/README.md`。

## 边界约束

- `M6`、`M7` 只提供上游路径与运动结果，不承载 `M8` 终态 owner 事实。
- `M9 runtime-execution` 只能消费 `ExecutionPackage`，不得把 `built` 结果伪装成 `validated`。
- 跨模块稳定工程契约应维护在 `shared/contracts/engineering/`，本目录仅承载 `M8` 专属契约。

## 当前事实来源

- `modules/dispense-packaging/domain/dispensing/`
- `modules/workflow/application/usecases/dispensing/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `domain/dispensing/` 已成为当前唯一真实实现承载面，模块根 target 直接链接 canonical 子域 target。
- 所有 live 实现与构建入口均已收敛到 canonical roots。

