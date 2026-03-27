# process-path

`M6 process-path`

## 模块职责

- 承接 `S8`，生成唯一 `ProcessPath`，负责路径顺序、连接和进退刀组织。

## Owner 对象

- `ProcessPath`

## 输入契约

- `BuildProcessPath`
- `SupersedeOwnedArtifact`

## 输出契约

- `ProcessPathBuilt`
- `ProcessPathRejected`
- `UncoveredFeatureDetected`
- `ArtifactSuperseded`

## 禁止越权边界

- 不得写 jerk、加速度或时间规划。
- 不得生成 `MotionPlan`。

## 测试入口

- `S09` path coverage/order/rollback to path
