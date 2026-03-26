# motion-planning

`modules/motion-planning/` 是 `M7 motion-planning` 的 canonical owner 入口。

## Owner 范围

- 运动规划对象与轨迹求解语义
- 规划链路中的运动约束建模与结果产出
- 面向下游执行包装的运动规划事实边界

## Owner 入口

- 构建入口：`CMakeLists.txt`（目标：`siligen_module_motion_planning`）。
- 模块契约入口：`contracts/README.md`（模块内运动规划契约与边界说明）。

## 迁移边界

- `modules/motion-planning/` 是 `M7` 的唯一终态 owner 根。
- 运行时与控制相关实现已并入 canonical domain/application/adapters surfaces。

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- 模块根 target 已稳定转发到 canonical `siligen_motion` owner target，`workflow` 与 canonical tests 已通过该 owner target 闭环构建。
- 所有 live 规划、运行时与控制相关实现均已收敛到 canonical roots。

