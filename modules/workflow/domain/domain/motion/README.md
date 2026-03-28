# Motion 兼容壳

**职责**: `modules/workflow/domain/domain/motion/` 不再是完整 motion owner；当前只保留 workflow execution-owner 的 live 实现根，以及面向历史 include 的兼容壳。

## 当前 owner 边界

- execution-owner live `.cpp` 仅允许保留在本目录下的 `MotionBufferController`、`JogController`、`HomingProcess`、`ReadyZeroDecisionService`
- planning owner 固定在 `modules/motion-planning/domain/motion/`
- runtime concrete owner 固定在 `modules/runtime-execution/application/services/motion/`

## 兼容规则

- `domain/motion/*` 的 workflow 旧路径可以继续存在，但只能是 thin shim header
- `BezierCalculator`、`BSplineCalculator`、`CircleCalculator`、`CMPValidator`、`CMPCompensation` 与 interpolation 目录下各 planning 头，全部只允许转发到 motion-planning owner
- `MotionControlServiceImpl`、`MotionStatusServiceImpl` 只允许转发到 runtime-execution owner
- workflow 本目录下禁止再新增 planning/runtime concrete duplicate `.cpp`

## 构建约束

- `modules/workflow/domain/domain/motion/CMakeLists.txt` 继续保持非 live 占位，不提供 planning fallback
- workflow live graph 中的 planning 能力必须经 `siligen_motion` 消费 canonical owner
- 任何新代码若需要 planning 类型，优先直接包含 motion-planning canonical public surface；legacy include 仅用于兼容旧调用方
