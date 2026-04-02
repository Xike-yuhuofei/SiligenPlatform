# dxf-geometry

目标 owner：`M2 dxf-geometry`

当前正式事实来源：`modules/dxf-geometry/{application,adapters,contracts}/` 与 `shared/contracts/engineering/`

## Owner 入口

- 构建入口：`CMakeLists.txt`（目标：`siligen_module_dxf_geometry`）。
- 模块契约入口：`contracts/README.md`（模块内几何契约与输入引用规则说明）。

## 迁移边界

- `modules/dxf-geometry/` 是 `M2` 的 canonical owner 根。
- `modules/dxf-geometry/application/engineering_data/` 是当前 canonical Python owner 面。

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- 历史 bridge 与 owner-local 目录已退出 live owner graph；新增终态实现不得回落到桥接路径。

## 测试基线

- `modules/dxf-geometry/tests/` 负责 `dxf-geometry` owner 级 `unit + contracts + golden + integration` 证明。
- 当前最小正式矩阵聚焦 `DxfPbPreparationService` 的命令装配、脚本解析与 PB 生成闭环。
- 仓库级 `tests/` 只消费跨 owner 场景，不替代本模块内 contract。
