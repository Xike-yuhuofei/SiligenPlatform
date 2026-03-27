# motion-planning

`M7 motion-planning`

## 模块职责

- 承接 `S9`，把 `ProcessPath` 转成唯一 `MotionPlan`，负责动态约束、插补和时间语义。

## Owner 对象

- `MotionPlan`

## 输入契约

- `PlanMotion`
- `SupersedeOwnedArtifact`

## 输出契约

- `MotionPlanBuilt`
- `TrajectoryConstraintViolated`
- `MotionPlanRejected`
- `ArtifactSuperseded`

## 禁止越权边界

- 不得写阀开关时序。
- 不得声明设备 ready。

## 测试入口

- `S09` motion constraint/rollback to motion
