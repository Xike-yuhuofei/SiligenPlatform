# Motion 子域 - 运动控制

**职责**: 负责多轴运动控制、回零/点动、轨迹规划与插补等核心运动逻辑

## 业务范围

- 轴控制（使能、参数设置）
- 位置控制（绝对/相对移动）
- 运动缓冲管理
- 点动（JOG）运动
- 回零控制（Homing）
- 插补算法（直线、圆弧、样条）
- 轨迹规划和执行

## 点动统一入口规范

- `DomainServices::JogController` 是点动规则唯一入口
- `IJogControlPort` 仅承载硬件动作与参数下发，不包含业务校验
- 应用层/适配层不得在点动路径重复实现校验或互锁判断

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
│   ├── TrajectoryPlanner          # 轨迹规划
│   ├── BufferManager              # 缓冲管理
│   ├── TriggerCalculator          # 触发计算
│   ├── JogController              # JOG 控制
│   ├── MotionControlService        # 运动控制服务（接口）
│   ├── MotionControlServiceImpl    # 运动控制实现
│   ├── MotionStatusService         # 状态服务（接口）
│   ├── MotionStatusServiceImpl     # 状态服务实现
│   └── MotionValidationService     # 运动校验服务
├── BezierCalculator.*             # 计算器与验证器（根目录文件）
├── BSplineCalculator.*
├── CircleCalculator.*
├── CMPValidator.*
├── CMPCompensation.*
├── CMPCoordinatedInterpolator.*
└── ports/                          # 端口接口（8个）
    ├── IMotionConnectionPort
    ├── IAxisControlPort
    ├── IPositionControlPort
    ├── IJogControlPort
    ├── IInterpolationPort
    ├── IMotionStatePort
    ├── IHomingPort
    └── IAdvancedMotionPort
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

本子域允许使用 `std::vector` 存储轨迹点（`.claude/rules/DOMAIN.md` 例外）
