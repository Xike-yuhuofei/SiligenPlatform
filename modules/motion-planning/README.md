# motion-planning

`modules/motion-planning/` 是 `M7 motion-planning` 的 canonical owner 入口。

## Owner 范围

- 运动规划对象与轨迹求解语义
- 规划链路中的运动约束建模与结果产出
- 面向下游执行包装的运动规划事实边界
- canonical owner 类型包括 `TimePlanningConfig`、`MotionPlanningReport`、`MotionTrajectory` 与 `MotionPlanner`

## Owner 入口

- 构建入口：`CMakeLists.txt`（目标：`siligen_module_motion_planning`）。
- 模块契约入口：`contracts/README.md`（模块内运动规划契约与边界说明）。

## 迁移边界

- `modules/motion-planning/` 是 `M7` 的唯一终态 owner 根。
- `MotionPlanningFacade -> MotionPlanner -> MotionTrajectory` 是当前唯一 canonical 规划调用链。
- 业务阶段名保留 `MotionPlan`，但本阶段代码级 canonical payload 固定为 `MotionTrajectory`。
- `IMotionRuntimePort`、`IIOControlPort`、`MotionControlServiceImpl`、`MotionStatusServiceImpl` 的 owner 已迁移到 `modules/runtime-execution`；`M7` 下同名头文件仅保留 shim/alias。
- `workflow` 侧仍可通过 thin compatibility shell 消费 `MotionBufferController`、`JogController`、`HomingProcess`、`ReadyZeroDecisionService` 等残余资产，但这些不再构成 `M7` 的 public owner surface。

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `contracts/` 与 `application/` 已成为当前 `M7` 的 canonical public surface。
- 模块根 target 通过 application/contracts public targets 暴露规划能力，并继续消费 `M6 process-path` 的 `ProcessPath` 事实。
- `siligen_motion` 当前只编译规划相关实现；runtime/control owner 不得回流到 `modules/motion-planning/domain/motion/` 或 `modules/workflow/domain/domain/motion/`。
