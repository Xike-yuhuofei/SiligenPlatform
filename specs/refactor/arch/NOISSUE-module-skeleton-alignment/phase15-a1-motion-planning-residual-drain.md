# Phase 15 A1: motion-planning residual drain

更新时间：`2026-03-25`

## 目标

在不宣称 `motion-planning` 达成 shell-only closeout 的前提下，继续缩小 `modules/motion-planning/src` 的保留面，只保留仍必须冻结的 runtime/control residual surface。

## 本次迁出资产

本次从 `modules/motion-planning/src/motion` 清理的 pure-planning bridge 副本包括：

- 值对象
  - `MotionTrajectory.h`
  - `MotionTypes.h`
  - `SemanticPath.h`
  - `TimePlanningConfig.h`
  - `TrajectoryAnalysisTypes.h`
  - `TrajectoryTypes.h`
  - `HardwareTestTypes.h`
- 领域服务与算法
  - `TriggerCalculator*`
  - `TimeTrajectoryPlanner*`
  - `TrajectoryPlanner*`
  - `SpeedPlanner*`
  - `VelocityProfileService*`
  - `MotionValidationService.h`
  - `GeometryBlender*`
  - `SevenSegmentSCurveProfile*`
  - `interpolation/*`
- 计算器与插补支持类型
  - `BezierCalculator*`
  - `BSplineCalculator*`
  - `CircleCalculator*`
  - `CMPValidator*`
  - `CMPCompensation*`
  - `CMPCoordinatedInterpolator*`
- 纯规划端口
  - `IInterpolationPort.h`
  - `IVelocityProfilePort.h`

这些资产的 canonical owner 继续由 `modules/motion-planning/domain/motion` 下的 `siligen_motion` 承载。

## 仍冻结的 residual 清单

本次未继续清排，仍保留在 bridge 中的 residual runtime/control surface：

- `domain-services/HomingProcess*`
- `domain-services/JogController*`
- `domain-services/MotionBufferController*`
- `domain-services/MotionControlService*`
- `domain-services/MotionStatusService*`
- `ports/IAxisControlPort.h`
- `ports/IPositionControlPort.h`
- `ports/IJogControlPort.h`
- `ports/IHomingPort.h`
- `ports/IMotionConnectionPort.h`
- `ports/IMotionRuntimePort.h`
- `ports/IMotionStatePort.h`
- `ports/IIOControlPort.h`
- `ports/IAdvancedMotionPort.h`

## 不能继续清排的原因

- 这些类型承载真实运行态控制、回零、点动和状态查询语义，不属于本轮允许迁入的纯 `M7` 规划 owner 面。
- `MotionBufferController`、`HomingProcess`、`MotionControlService*`、`MotionStatusService*` 仍直接表达 runtime/control surface，若误迁会扩大 `motion-planning` canonical owner 的职责边界。
- 当前任务只允许收缩 bridge，不允许改写 `workflow` 或 `runtime-execution` owner 分配，也不允许把 residual runtime 接口包装成“已 closeout”。

## 模块局部文档写回

- `modules/motion-planning/README.md` 已补充 `src/motion` 仅保留 residual runtime/control surface 的事实。
- `modules/motion-planning/module.yaml` 已同步记录 pure-planning 副本已排出 bridge。
- `modules/motion-planning/src/README.md`
- `modules/motion-planning/src/motion/README.md`
  - 已改写为 residual bridge 口径，避免把 `src/motion` 继续描述为完整 owner 实现面。

## 验证结果

- 已通过：
  - `cmake -S D:\Projects\SS-dispense-align -B C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`
  - `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_unit_tests process_runtime_core_motion_runtime_assembly_test process_runtime_core_deterministic_path_execution_test`
  - `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "process_runtime_core_motion_runtime_assembly_test|process_runtime_core_deterministic_path_execution_test" --output-on-failure`

## 未决风险

- `src/motion` 仍保留 runtime/control residual surface，因此模块口径仍然是 `bridge-backed; canonical owner target active; residual runtime/control surface frozen`。
- 后续若要继续推进，需要先明确这些 residual 类型的真实 owner 落点，而不是把它们直接并入纯规划 canonical 面。
