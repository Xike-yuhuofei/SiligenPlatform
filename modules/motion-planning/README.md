# motion-planning

`modules/motion-planning/` 是 `M7 motion-planning` 的 canonical owner 入口。

## Owner 范围

- 运动规划对象与轨迹求解语义
- 规划链路中的运动约束建模与结果产出
- 面向下游执行包装的运动规划事实边界
- `scripts/engineering-data/path_to_trajectory.py` 背后的离线 trajectory Python owner
- canonical owner 类型包括 `TimePlanningConfig`、`MotionPlanningReport`、`MotionTrajectory` 与 `MotionPlanner`

## Owner 入口

- 构建入口：`CMakeLists.txt`（目标：`siligen_module_motion_planning`）。
- 模块契约入口：`contracts/README.md`（模块内运动规划契约与边界说明）。
- Python owner 入口：`application/motion_planning/trajectory_generation.py`。

## 迁移边界

- `modules/motion-planning/` 是 `M7` 的唯一终态 owner 根。
- 当前冻结的对外 surface 只有 `motion_planning/contracts/*`、`application/services/motion_planning/MotionPlanningFacade.h`、`application/services/motion_planning/CmpInterpolationFacade.h`、`application/services/motion_planning/TrajectoryInterpolationFacade.h`。
- `scripts/engineering-data/path_to_trajectory.py` 是稳定 workspace 入口；其实现 owner 已迁到 `modules/motion-planning/application/motion_planning/`，不再由 `dxf-geometry` 长期承载 trajectory 语义。
- `MotionPlanningFacade -> MotionPlanner -> MotionTrajectory` 是当前 canonical 规划调用链；CMP / 轨迹插补通过对应 facade 暴露；`InterpolationProgramPlanner` 仅保留为 `domain/motion` internal seam，不构成 M7 public surface。
- 业务阶段名保留 `MotionPlan`，但本阶段代码级 canonical payload 固定为 `MotionTrajectory`。
- `InterpolationAlgorithm` 的 contract 真值固定在 `contracts/include/motion_planning/contracts/InterpolationTypes.h`，外部消费者不得再引用旧 `Siligen::Domain::Motion::InterpolationAlgorithm`。
- `IMotionRuntimePort`、`IIOControlPort`、`MotionControlServiceImpl`、`MotionStatusServiceImpl`、`ValidatedInterpolationPort`、`InterpolationPlanningUseCase` 的 owner 已迁移到 `modules/runtime-execution`；`M7` 下对应 live owner 路径已退场。
- `MotionBufferController`、`JogController`、`HomingProcess`、`ReadyZeroDecisionService` 的 execution owner 已固定在 `modules/runtime-execution/application/services/motion/execution/`；`M7` 下对应旧兼容头已删除。

## 统一骨架状态

- `contracts/` 与 `application/services` 已成为当前 `M7` 的 canonical public surface；application public layer 当前只保留 3 个 facade。
- `application/motion_planning/` 负责离线 trajectory Python owner；这是 workspace tool owner，不新增 shared contract authority。
- 模块根 target 通过 application/contracts public targets 暴露规划能力，并继续消费 `M6 process-path` 的 `ProcessPath` 事实。
- `siligen_motion` 当前只编译规划相关实现与插补程序生成；runtime/control owner 不得回流到 `modules/motion-planning/domain/motion/`。
- `InterpolationPlanningUseCase` 与 `ValidatedInterpolationPort` 已从 `modules/motion-planning` live build 迁出，不再作为 M7 stable seam。
