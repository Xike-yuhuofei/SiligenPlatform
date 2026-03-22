# DXF 运动执行契约代码映射表（v1）

## 目的

将 [dxf-motion-execution-contract-v1.md](./dxf-motion-execution-contract-v1.md) 中冻结的执行契约，映射到当前代码中的具体对象、入口和责任边界，便于后续评审、实现对齐和缺陷定位。

## 适用范围

- 仅覆盖当前 DXF 点胶主执行链。
- 主链范围：
  `DXF/PB -> PbPathSourceAdapter -> DispensingPlannerService -> InterpolationProgramPlanner -> DispensingProcessService -> InterpolationAdapter -> MultiCard SDK`
- 不覆盖通用运动协调链或其他旁路执行链，例如：
  `MotionCoordinationUseCase`、`DeterministicPathExecutor`、`ValidatedInterpolationPlanner`。

## 当前主链（按代码责任）

1. `engineering_data.processing.dxf_to_pb`
   将 DXF 解析为 PB 几何原语。
2. `PbPathSourceAdapter::LoadFromFile`
   将 `PathBundle.pb` 还原为运行时 `Primitive` 列表。
3. `DispensingPlannerService::Plan`
   组织规划主流程，并产出 `DispensingPlan`。
4. `UnifiedTrajectoryPlannerService::Plan`
   生成 `normalized.path`、`process_path`、`shaped_path`。
5. `DispensingPlannerService::TryPlanWithRuckig`
   基于 `shaped_path` 生成当前主链使用的 `motion_trajectory`。
6. `InterpolationProgramPlanner::BuildProgram`
   将 `shaped_path + motion_trajectory` 合成为 `interpolation_segments`。
7. `DispensingProcessService::ExecuteProcess` / `ExecutePlanInternal`
   将 `interpolation_segments` 作为真实执行程序分批写入板卡缓冲。
8. `InterpolationAdapter`
   将 `InterpolationData` 映射到 `MC_SetCrdPrm`、`MC_LnXY`、`MC_ArcXYC`、`MC_CrdData`、`MC_CrdStart`。

## 契约映射表

| 契约项 | 权威对象 | 代码文件 | 核心函数/类 | 当前责任 | 风险/备注 |
| --- | --- | --- | --- | --- | --- |
| `process_path` 是几何/工艺权威源 | `ProcessPath`；执行态对外暴露为 `DispensingPlan.process_path` | `packages/process-runtime-core/src/domain/trajectory/value-objects/ProcessPath.h`；`packages/process-runtime-core/src/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.cpp`；`packages/process-runtime-core/src/domain/dispensing/planning/domain-services/DispensingPlannerService.h`；`packages/process-runtime-core/src/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp` | `ProcessAnnotator::Annotate`；`TrajectoryShaper::Shape`；`DispensingPlannerService::Plan` | 定义段几何、`dispense_on`、`flow_rate`、局部约束和 `ProcessTag`；后续触发规划、速度规划和程序合成都以其段顺序为基准。 | 代码里同时存在 `UnifiedTrajectoryPlanResult.process_path` 和 `shaped_path` 两级对象；当前真正继续下传到 `DispensingPlan.process_path` 的是 `shaped_path`，不是更早阶段的原始 `process_path`。评审时不能只看名字。 |
| `motion_trajectory` 是内部时间律参考源 | `MotionTrajectory` | `packages/process-runtime-core/src/domain/motion/value-objects/MotionTrajectory.h`；`packages/process-runtime-core/src/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.h`；`packages/process-runtime-core/src/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.cpp`；`packages/process-runtime-core/src/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp` | `UnifiedTrajectoryPlannerService::Plan`；`DispensingPlannerService::TryPlanWithRuckig` | 保存带时间的内部轨迹采样：`points[t, position, velocity, process_tag, dispense_on, flow_rate]`，以及 `total_time`、`total_length`、`planning_report`。 | 概念上它属于规划层时间律结果；但当前主执行链中，`BuildUnifiedPlanRequest` 显式设置 `generate_motion_trajectory=false`，随后 `DispensingPlannerService::TryPlanWithRuckig(shaped_path, request)` 覆盖写入 `plan.motion_trajectory`。因此“概念权威源”和“当前实现权威入口”并不完全重合。 |
| `InterpolationProgramPlanner` 是执行语义合成层 | `InterpolationProgramPlanner` | `packages/process-runtime-core/src/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h`；`packages/process-runtime-core/src/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.cpp` | `InterpolationProgramPlanner::BuildProgram` | 将 `ProcessPath` 的几何/工艺语义与 `MotionTrajectory` 的速度语义投影为板卡可执行的 `InterpolationData`，填充段类型、目标点、速度、末速度、加速度。 | 这一步不是“点转段”的机械改壳，而是段级约束投影。它保留几何段语义，但不是对 `motion_trajectory` 的逐点无损复制。代码中若采样轨迹长度与几何长度偏差过大，会在 `DispensingPlannerService` 记录告警。 |
| `interpolation_segments` 是下发前唯一真源 | `std::vector<InterpolationData>`；执行态为 `DispensingPlan.interpolation_segments` 和 `DispensingExecutionPlan.interpolation_segments` | `packages/process-runtime-core/src/domain/dispensing/planning/domain-services/DispensingPlannerService.h`；`packages/process-runtime-core/src/domain/dispensing/value-objects/DispensingExecutionTypes.h`；`packages/process-runtime-core/src/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`；`packages/process-runtime-core/src/domain/dispensing/domain-services/DispensingProcessService.cpp` | `DispensingPlannerService::Plan`；`DispensingProcessService::ExecuteProcess`；`DispensingProcessService::ExecutePlanInternal` | 作为板卡下发前的最终程序对象，被 `DispensingProcessService` 逐段取出并调用 `interpolation_port_->AddInterpolationData(...)` 发送。 | `DispensingController::Build(...)` 也会构造插补相关输出，但当前主链里它用于触发位置生成，不是板卡主执行程序来源。这个区分必须保留，否则会把“触发构建”误认成“执行真源”。 |
| 控制卡执行层不得越权改写上位机语义 | `IInterpolationPort` / `InterpolationAdapter` | `packages/process-runtime-core/src/domain/motion/ports/IInterpolationPort.h`；`packages/process-runtime-core/src/domain/dispensing/domain-services/DispensingProcessService.cpp`；`packages/device-adapters/src/legacy/adapters/motion/controller/interpolation/InterpolationAdapter.cpp` | `ConfigureCoordinateSystem`；`AddInterpolationData`；`FlushInterpolationData`；`StartCoordinateSystemMotion` | 负责协议适配、单位换算、缓冲区调度和 SDK 调用，不负责重排路径、不负责改写段类型、不负责重建工艺语义。 | 当前未发现适配层重排路径或改写工艺标签的逻辑；但板卡仍会对段内速度与插补细节进行自身实现，因此设备实际轨迹不应直接等同于上位机 `motion_trajectory` 的逐点结果。 |
| 圆弧语义保留链必须贯通到 SDK | `SegmentType::Arc -> InterpolationType::CIRCULAR_CW/CCW -> MC_ArcXYC` | `packages/process-runtime-core/src/domain/trajectory/value-objects/Path.h`；`packages/process-runtime-core/src/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.cpp`；`packages/device-adapters/src/legacy/adapters/motion/controller/interpolation/InterpolationAdapter.cpp` | `InterpolationProgramPlanner::BuildProgram`；`InterpolationAdapter::AddInterpolationData` | 在可表示为圆弧的执行段上，尽量保留圆弧语义，而不是一律退化为折线点流。 | 当前主链中圆弧会被生成 `CIRCULAR_CW/CCW` 并映射到 `MC_ArcXYC`；完整圆会被拆成两段圆弧命令。需要注意，DXF 导入阶段的样条/椭圆近似仍可能更早发生几何降阶，因此“圆弧保留”只针对进入执行规划后的弧段。 |
| 工艺 `tag` 应保持可追溯 | `ProcessTag`；显式存在于 `ProcessSegment` 和 `MotionTrajectoryPoint` | `packages/process-runtime-core/src/domain/trajectory/value-objects/ProcessPath.h`；`packages/process-runtime-core/src/domain/motion/value-objects/MotionTrajectory.h`；`packages/process-runtime-core/src/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`；`packages/process-runtime-core/src/domain/motion/ports/IInterpolationPort.h` | `ProcessAnnotator::Annotate`；`DispensingPlannerService::TryPlanWithRuckig` 附近的轨迹元数据回填 | 当前 `ProcessTag` 和 `dispense_on` 会显式写入 `MotionTrajectoryPoint`，供预览、统计和触发相关逻辑使用。 | `InterpolationData` 结构体当前不包含 `ProcessTag` 字段，因此从 `process_path` 到板卡段命令的工艺语义追溯，主要依赖段顺序与构建时的隐式对应，而不是显式字段透传。这是后续若要增强诊断与回放时最值得收紧的点之一。 |

## 当前实现上的关键结论

1. `process_path` 与 `motion_trajectory` 当前已经在对象层面分离。
2. `InterpolationProgramPlanner` 当前确实承担“执行语义合成”职责，而不是简单格式转换。
3. 板卡真实下发对象是 `interpolation_segments`，不是 `motion_trajectory.points`。
4. `DispensingController` 当前更接近触发/辅助输出生成层，不是板卡执行程序权威源。
5. 当前实现里最值得继续跟踪的偏差点有两个：
   - `DispensingPlan.process_path` 实际承载的是 `shaped_path`。
   - `motion_trajectory` 的当前主入口是 `TryPlanWithRuckig`，而不是 `UnifiedTrajectoryPlannerService` 内部的 `MotionPlanner`。

## 建议的后续使用方式

- 评审契约是否被实现满足时，优先按本表逐项核对，不要只根据类名做口头判断。
- 若后续要补“一页评审摘要版”，应以本表为底稿压缩，而不是重新抽象一版新说法。
- 若后续要做代码整改，优先检查以下三项：
  - `process_path` / `shaped_path` 命名与对外暴露是否需要收敛。
  - `motion_trajectory` 的生成权是否需要回归统一规划入口。
  - `InterpolationData` 是否需要显式承载工艺追溯字段。
