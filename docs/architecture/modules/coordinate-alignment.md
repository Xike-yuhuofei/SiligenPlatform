# coordinate-alignment

`M5 coordinate-alignment`

## 模块职责

- 承接 `S6-S7`，解析坐标链、对位补偿、无补偿显式化和超限判定。

## Owner 对象

- `CoordinateTransformSet`
- `AlignmentCompensation`

## 输入契约

- `ResolveCoordinateTransforms`
- `RunAlignment`
- `MarkAlignmentNotRequired`
- `SupersedeOwnedArtifact`

## 输出契约

- `CoordinateTransformResolved`
- `AlignmentSucceeded`
- `AlignmentNotRequired`
- `AlignmentFailed`
- `CompensationOutOfRange`
- `ArtifactSuperseded`

## 禁止越权边界

- 不得改写源图或工艺模板。
- 不得生成 `ProcessPath` 或 `MotionPlan`。

## 测试入口

- `S09` alignment success/not-required/out-of-range
