# Motion 子域 - 运动规划

**职责**: 负责运动规划事实、时间/轨迹/CMP 规划与插补算法，不再作为 runtime/control owner。

## 业务范围

- 轨迹规划与约束求解
- 插补算法（直线、圆弧、样条）
- 时间规划、速度规划与触发时间线计算
- `MotionTrajectory`、`TimePlanningConfig`、`MotionPlanningReport` 等规划事实输出
- 面向后续阶段保留的少量兼容残余（仅供 thin compatibility shell 使用，不构成 owner public surface）

## Stage B 边界

- `IMotionRuntimePort`、`IIOControlPort` 的 owner 在 `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/`
- `MotionControlServiceImpl`、`MotionStatusServiceImpl` 的 owner 在 `modules/runtime-execution/application/include/runtime_execution/application/services/motion/`
- 本目录下同名头文件仅允许保留 shim/alias，禁止重新声明 runtime/control owner 类型
- `JogController`、`HomingProcess`、`ReadyZeroDecisionService` 当前仍作为兼容残余存在，但后续阶段应继续收瘦，不得扩大 public 依赖面

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
│   ├── MotionBufferController    # 兼容残余，仅供 thin shell 使用
│   ├── JogController             # 兼容残余，仅供 thin shell 使用
│   ├── HomingProcess             # 兼容残余，仅供 thin shell 使用
│   ├── ReadyZeroDecisionService  # 兼容残余，仅供 thin shell 使用
│   └── interpolation/            # 插补与程序生成
├── BezierCalculator.*             # 计算器与验证器（根目录文件）
├── BSplineCalculator.*
├── CircleCalculator.*
├── CMPValidator.*
├── CMPCompensation.*
├── CMPCoordinatedInterpolator.*
└── ports/                          # 规划相关端口；runtime/control 端口仅保留 shim
    ├── IMotionConnectionPort
    ├── IAxisControlPort
    ├── IPositionControlPort
    ├── IJogControlPort
    ├── IInterpolationPort
    ├── IMotionStatePort
    ├── IHomingPort
    ├── IMotionRuntimePort  # shim -> M9 owner
    └── IIOControlPort      # shim -> M9 owner
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
