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

