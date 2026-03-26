# process-path

`modules/process-path/` 是 `M6 process-path` 的 canonical owner 入口。

## Owner 范围

- 基于 `CoordinateTransformSet` 与上游规则构建 `ProcessPath`。
- 承接路径段、路径序列与路径校验的模块 owner 边界。
- 为 `M7 motion-planning` 提供正式路径事实，不回退为执行层的临时中间态。

## Owner 入口

- 构建入口：`CMakeLists.txt`（target：`siligen_module_process_path`）。
- 模块契约入口：`contracts/README.md`。

## 边界约束

- `M4`、`M5` 只提供规划和对齐输入，不承载 `M6` 终态 owner 事实。
- `M8`、`M9` 只能消费 `ProcessPath` 结果，不得反向重排或修补路径事实。
- 跨模块稳定工程契约应维护在 `shared/contracts/engineering/`，本目录只承载 `M6` 专属契约。

## 当前事实来源

- `modules/process-path/domain/trajectory/`
- `shared/contracts/engineering/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `domain/trajectory/` 已成为当前唯一真实实现承载面，模块根 target 直接链接 canonical 子域 target。
- 所有 live 实现与构建入口均已收敛到 canonical roots。

