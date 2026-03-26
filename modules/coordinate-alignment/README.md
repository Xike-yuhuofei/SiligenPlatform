# coordinate-alignment

`modules/coordinate-alignment/` 是 `M5 coordinate-alignment` 的 canonical owner 入口。

## Owner 范围

- 规划链中的坐标系基准对齐语义
- 坐标变换与对齐参数的模块 owner 边界
- 面向 `M6 process-path` 消费的对齐结果口径

## 边界约束

- `workflow`、`process-planning` 仅提供上游输入，不承载 `M5` 终态 owner 事实。
- 运行时入口与执行链不得回写 `M5` owner 事实。
- 跨模块稳定工程契约应放在 `shared/contracts/engineering/`，本模块仅承载 `M5` 专属契约。

## Owner 入口

- 构建入口：`CMakeLists.txt`（目标：`siligen_module_coordinate_alignment`）。
- 契约入口：`contracts/README.md`。

## 当前事实来源

- `modules/coordinate-alignment/domain/machine/`
- `shared/contracts/engineering/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `domain/machine/` 已成为当前唯一真实实现承载面，模块根 target 直接链接 canonical 子域 target。
- 所有 live 实现与构建入口均已收敛到 canonical roots。

