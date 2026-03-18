# InterpolationPlanningUseCase

## 职责

基于控制点与插补算法生成轨迹点序列，输出长度/时间/触发统计。

## 典型调用链

`DXFDispensingPlanner` → `InterpolationPlanningUseCase` → `TrajectoryInterpolatorFactory` → 具体插补器

## 说明

- 仅负责插补轨迹生成，不直接接触硬件端口
- 算法由 `InterpolationAlgorithm` 指定，参数由 `InterpolationConfig` 提供
- 硬件插补程序生成与参数校验由 Domain 统一入口负责（`InterpolationProgramPlanner` / `ValidatedInterpolationPort`）
