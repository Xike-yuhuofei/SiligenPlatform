# Motion 子域 - 运动规划

**职责**: 负责运动规划事实、时间/轨迹/CMP 规划与插补算法，不再作为 runtime/control owner。

## 业务范围

- 轨迹规划与约束求解
- 插补算法（直线、圆弧、样条）
- 时间规划、速度规划与触发时间线计算
- `MotionTrajectory`、`TimePlanningConfig`、`MotionPlanningReport` 等规划事实输出
- 不再承接 runtime/control owner 的兼容入口；live consumer 必须直连 canonical owner

## Stage B 边界

- `IMotionRuntimePort`、`IIOControlPort` 的 owner 在 `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/`
- `MotionControlServiceImpl`、`MotionStatusServiceImpl` 的 owner 在 `modules/runtime-execution/application/include/runtime_execution/application/services/motion/`
- `HomingProcess`、`JogController`、`MotionBufferController`、`ReadyZeroDecisionService` 的 owner 在 `modules/runtime-execution/application/include/runtime_execution/application/services/motion/`
- 本目录不再保留 runtime/control owner 的 shim/alias header；live consumer 必须使用 canonical runtime path
- execution-owner 服务禁止在 `modules/motion-planning/domain/motion/domain-services/` 下保留可被 target 误编译的 `.cpp` 或 `.h`

## Execution Owner 审计

- `siligen_runtime_execution_motion_services` 是唯一 execution-owner build target，live source root 固定为 `modules/runtime-execution/application/services/motion/`
- `modules/motion-planning/domain/motion/domain-services/` 下旧 execution shim header 已删除；`modules/workflow/domain/**` 下对应 execution shim 同样已删除

## Planning Owner 审计

- `siligen_motion` 的 live source root 固定为 `modules/motion-planning/domain/motion/`
- `CMPCoordinatedInterpolator`、`TimeTrajectoryPlanner`、`TrajectoryPlanner`、`TriggerCalculator`、`SpeedPlanner`、`GeometryBlender`、`VelocityProfileService`、`SevenSegmentSCurveProfile` 的 owner 固定为 motion-planning
- `BezierCalculator`、`BSplineCalculator`、`CircleCalculator`、`CMPValidator`、`CMPCompensation` 与 interpolation 子目录实现同样固定为 motion-planning owner
- `modules/workflow/domain/domain/motion/` 不再保留 execution 兼容构建入口；planning owner 不再受 workflow fallback 干扰
- workflow domain 不再提供 `siligen_motion` 本地 fallback；缺少 canonical owner target 时构建显式失败

## 目录结构

```
motion/
├── value-objects/                 # 值对象
│   ├── MotionTypes.h
│   ├── HardwareTestTypes.h
│   ├── TrajectoryTypes.h
│   └── TrajectoryAnalysisTypes.h
├── domain-services/               # 领域服务
│   ├── interpolation/             # 插补算法
│   │   ├── InterpolationBase
│   │   ├── LinearInterpolator
│   │   ├── ArcInterpolator
│   │   └── SplineInterpolator
│   │   ├── InterpolationProgramPlanner
│   │   ├── InterpolationCommandValidator
│   │   └── ValidatedInterpolationPort
│   ├── MotionPlanner             # 规划求解 owner
│   ├── TrajectoryPlanner         # 轨迹规划
│   ├── TimeTrajectoryPlanner     # 时间规划
│   ├── TriggerCalculator         # 规划触发计算
│   ├── SpeedPlanner              # 速度规划
│   └── interpolation/            # 插补与程序生成
├── BezierCalculator.*             # 计算器与验证器（根目录文件）
├── BSplineCalculator.*
├── CircleCalculator.*
├── CMPValidator.*
├── CMPCompensation.*
├── CMPCoordinatedInterpolator.*
└── ports/                          # 规划相关端口
    ├── IMotionConnectionPort
    ├── IAxisControlPort
    ├── IPositionControlPort
    ├── IJogControlPort
    ├── IInterpolationPort
    ├── IMotionStatePort
    └── IHomingPort
```

## 命名空间

```cpp
namespace Siligen::Domain::Motion {
    namespace Entities { ... }
    namespace ValueObjects { ... }
    namespace DomainServices { ... }
    namespace Ports { ... }
}
```

## 依赖关系

- ✅ 依赖: `shared/types`, `shared/utils`, `domain/_shared`
- ❌ 不依赖: `infrastructure`, `application`, 其他子域

## 插补规则统一规范

- 插补策略选择、参数校验、插补程序生成统一在本子域领域服务中完成。
- 通过 `ValidatedInterpolationPort` 对外提供统一校验入口；基础设施只负责硬件调用。
- 应用层不得实现插补规则，仅负责流程编排与参数映射。

## 特殊约束

- `M7` public surface 只能承诺规划事实，不得重新吸收 runtime/control owner 语义
- 本子域允许使用 `std::vector` 存储轨迹点（`.claude/rules/DOMAIN.md` 例外）
