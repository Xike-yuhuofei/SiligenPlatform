# Motion 子域 - 运动规划

**职责**: 负责运动规划事实、时间/轨迹/CMP 规划、插补算法与插补程序生成，不再作为 runtime/control owner 或 interpolation runtime wrapper owner。

## 业务范围

- 轨迹规划与约束求解
- 插补算法（直线、圆弧、样条）
- 时间规划、速度规划与触发时间线计算
- `MotionTrajectory`、`TimePlanningConfig`、`MotionPlanningReport` 等规划事实输出
- 仅保留 motion-planning 自身 owner 规划事实与内部 residual seam；workflow compat shell 已物理删除

## Stage B 边界

- `IMotionRuntimePort`、`IIOControlPort` 的 owner 在 `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/`
- `MotionControlServiceImpl`、`MotionStatusServiceImpl`、`ValidatedInterpolationPort` 的 owner 在 `modules/runtime-execution/application/include/runtime_execution/application/services/motion/`
- 本目录不得重新声明 runtime/control owner 类型，也不再保留对应 shim/alias 头
- `MotionBufferController`、`JogController`、`HomingProcess`、`ReadyZeroDecisionService` 的 live implementation owner 已冻结到 `modules/runtime-execution/application/services/motion/execution/`
- 上述四类 execution-owner 服务禁止在 `modules/motion-planning/domain/motion/domain-services/` 下保留可被 target 误编译的 `.cpp`

## Execution Owner 审计

- `siligen_runtime_execution_motion_execution_services` 的 live source root 固定为 `modules/runtime-execution/application/services/motion/execution/`
- `modules/motion-planning/domain/motion/domain-services/` 下这四个旧路径已删除，不再承担历史 include 兼容

## Planning Owner 审计

- `siligen_motion` 的 live source root 固定为 `modules/motion-planning/domain/motion/`
- `CMPCoordinatedInterpolator`、`TimeTrajectoryPlanner`、`TrajectoryPlanner`、`TriggerCalculator`、`SpeedPlanner`、`GeometryBlender`、`VelocityProfileService`、`SevenSegmentSCurveProfile` 的 owner 固定为 motion-planning
- `BezierCalculator`、`BSplineCalculator`、`CircleCalculator`、`CMPValidator`、`CMPCompensation` 与 interpolation 子目录实现同样固定为 motion-planning owner
- `modules/workflow/domain/domain/motion/` 与 `modules/workflow/domain/include/domain/motion/` 下旧 compat residue 已物理删除，不再保留 thin shell
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
│   │   └── InterpolationCommandValidator
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
    ├── IPositionControlPort
    ├── IMotionStatePort
    └── IVelocityProfilePort
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

- ✅ 依赖: `process-path/contracts`, `runtime-execution/contracts/motion`（仅限插补程序生成 DTO）、`shared/types`, `shared/utils`
- ❌ 不依赖: `infrastructure`, `application`, 其他子域

## 插补规则统一规范

- 插补策略选择、参数校验、插补程序生成统一在本子域领域服务中完成。
- 插补程序生成只消费 `runtime-execution/contracts/motion/IInterpolationPort.h` 中的 DTO 定义，不再在 M7 内持有 runtime wrapper 实现。
- 应用层不得实现插补规则，仅负责流程编排与参数映射。

## 特殊约束

- `M7` public surface 只能承诺规划事实与 CMP / 轨迹插补 facade，不得重新吸收 runtime/control owner 语义
- `InterpolationProgramPlanner` 虽继续留在 `domain/motion/`，但仅作为 internal residual seam 供直接 consumer 调用，不构成稳定 public surface
- `InterpolationAlgorithm` 的对外真值固定为 `motion_planning/contracts/InterpolationTypes.h`
- 本子域允许使用 `std::vector` 存储轨迹点（`.claude/rules/DOMAIN.md` 例外）
