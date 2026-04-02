# Motion 兼容壳

`modules/workflow/domain/domain/motion/` 不再持有 motion execution / runtime concrete 的 live 实现。

## 当前 owner 边界

- execution-owner concrete 固定在 `modules/runtime-execution/application/services/motion/`
- planning owner 固定在 `modules/motion-planning/domain/motion/`
- workflow 不再为 execution-owner motion 暴露 compatibility target 或 shim header

## 兼容规则

- `MotionBufferController`、`JogController`、`HomingProcess`、`ReadyZeroDecisionService` 的 workflow shim 已删除，不允许恢复
- `MotionControlServiceImpl`、`MotionStatusServiceImpl` 的 workflow shim 已删除；live consumer 只允许使用 runtime-execution canonical owner
- `IMotionRuntimePort`、`IIOControlPort` 的 workflow port alias 已删除；不得恢复 `domain/motion/ports/*` runtime alias
- `BezierCalculator`、`BSplineCalculator`、`CircleCalculator`、`CMPValidator`、`CMPCompensation` 与 interpolation 目录下 planning 头只允许转发到 motion-planning owner
- 禁止在本目录重新新增 execution/runtime live `.cpp`

## 构建约束

- `modules/workflow/domain/domain/CMakeLists.txt` 不再定义 motion execution compatibility target
- 缺少 canonical owner target 时必须显式失败，禁止回退到 workflow 本地实现
