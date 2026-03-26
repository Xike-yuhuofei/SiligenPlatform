# InterpolationPlanningUseCase

## 职责

基于控制点与插补算法生成轨迹点序列，输出长度/时间/触发统计。

## 典型调用链

`InterpolationPlanningUseCase` → `TrajectoryInterpolatorFactory` → 具体插补器

## 说明

- 仅负责插补轨迹生成，不直接接触硬件端口
- 点胶规划主链路中的插补已下沉到 Domain 服务内直接调用 Domain 插补组件；本 UseCase 保留为应用层独立能力
- 算法由 `InterpolationAlgorithm` 指定，参数由 `InterpolationConfig` 提供
- 硬件插补程序生成与参数校验由 Domain 统一入口负责（`InterpolationProgramPlanner` / `ValidatedInterpolationPort`）
