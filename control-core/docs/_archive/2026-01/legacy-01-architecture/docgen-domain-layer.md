# Domain 层文档

## 概述

Domain 层是 Siligen 点胶机控制系统的核心业务逻辑层，采用六边形架构设计原则。该层定义了系统的业务规则、领域模型和端口接口，与基础设施层完全解耦。

## 架构原则

### 核心约束

| 约束项 | 说明 |
|--------|------|
| **依赖方向** | 不依赖基础设施层，仅依赖共享类型 |
| **错误处理** | 返回 `Result<T>`，禁止抛出异常 |
| **命名空间** | `Siligen::Domain::*` |
| **端口定义** | 纯虚函数接口，零实现 |
| **内存管理** | 禁止 `new`/`delete`/`malloc` |
| **STL容器** | 禁止 `std::vector`/`std::map`（motion 模块例外） |
| **IO操作** | 禁止 `std::cout`/`std::thread` |

## 目录结构

```
src/domain/
├── configuration/          # 配置管理模块
├── models/                 # 领域模型类型定义
├── motion/                 # 运动控制核心模块
├── ports/                  # 端口接口定义
├── services/               # 领域业务服务
├── system/                 # 系统管理模块
└── triggering/             # 触发控制模块
```

## 端口接口 (Ports)

端口是 Domain 层与外部世界交互的契约，由 Infrastructure 层的适配器实现。

### 运动控制端口

| 端口接口 | 职责 | 方法数 |
|---------|------|--------|
| `IAdvancedMotionPort` | 高级运动控制 | 16 |
| `IAxisControlPort` | 轴控制和参数设置 | 9 |
| `IHomingPort` | 回零控制 | - |
| `IInterpolationPort` | 插补运动 | 5 |
| `IJogControlPort` | 点动控制 | 3 |
| `IMotionConnectionPort` | 运动连接管理 | 5 |
| `LegacyMotionControllerPort` | ~~主运动控制器~~ (已废弃) | - |
| `IMotionStatePort` | 运动状态查询 | 7 |
| `IPositionControlPort` | 位置控制 | 8 |

### 系统服务端口

| 端口接口 | 职责 |
|---------|------|
| `IClockPort` | 时钟服务 |
| `IConfigurationPort` | 配置管理 |
| `IFileStoragePort` | 文件存储 |
| `IDiagnosticsPort` | 诊断服务 |
| `IDataQueryPort` | 数据查询 |
| `IEnvironmentInfoProviderPort` | 环境信息 |
| `IStacktraceProviderPort` | 堆栈跟踪 |

### 硬件控制端口

| 端口接口 | 职责 |
|---------|------|
| `IHardwareConnectionPort` | 硬件连接 |
| `IIOControlPort` | IO控制 |
| `IValvePort` | 阀门控制 |
| `ITriggerControllerPort` | 触发控制器 |

### 基础设施端口

| 端口接口 | 职责 |
|---------|------|
| `IEventPublisherPort` | 事件发布 |
| `ILoggingPort` | 日志服务 |
| `IVisualizationPort` | 可视化服务 |
| `IDXFFileParsingPort` | DXF文件解析 |

## 领域模型 (Models)

### 基础类型模型

```cpp
// 位置坐标 (3D)
struct Position3D {
    double x, y, z;
};

// 轴状态
struct AxisStatus {
    int axisId;
    bool enabled;
    double position;
    double velocity;
    bool isHomed;
    AxisLimitStatus limitStatus;
    MotorStatus motorStatus;
};

// 运动状态
struct MotionStatus {
    bool isMoving;
    Position3D currentPosition;
    Position3D targetPosition;
    double velocity;
    double acceleration;
    MotionMode motionMode;
    double estimatedTimeToTarget;
};
```

### 配置模型

| 模型 | 说明 |
|------|------|
| `CMPPulseConfig` | CMP 脉冲配置 |
| `DispensingConfig` | 点胶配置 |
| `InterpolationConfig` | 插补配置 |
| `MachineConfig` | 机器配置 |
| `ValveTimingConfig` | 阀门时序配置 |

### 测试模型

| 模型 | 说明 |
|------|------|
| `TestConfiguration` | 测试配置类型 |
| `TestTypes` | 测试类型定义 |
| `TestDataTypes` | 测试数据类型 |
| `TestRecord` | 测试记录类型 |
| `DiagnosticTypes` | 诊断类型定义 |

## 领域服务 (Services)

### CMPService

CMP (Compare Output) 触发业务服务，负责位置触发控制。

```cpp
namespace Siligen::Domain::Services {

class CMPService {
public:
    // 配置 CMP 触发
    Result<void> ConfigureTrigger(const CMPTriggerConfig& config);

    // 启用/禁用 CMP 输出
    Result<void> EnableOutput(int axis, bool enable);

    // 设置触发参数
    Result<void> SetTriggerPosition(int axis, float position, float tolerance);

    // 获取 CMP 状态
    Result<CMPStatus> GetStatus(int axis);
};

} // namespace Siligen::Domain::Services
```

### MotionService

运动控制业务服务，封装核心运动逻辑。

```cpp
namespace Siligen::Domain::Services {

class MotionService {
public:
    // 启动点动运动
    Result<void> StartJog(int axis, JogDirection direction, float velocity);

    // 停止点动
    Result<void> StopJog(int axis);

    // 移动到绝对位置
    Result<void> MoveToPosition(int axis, float position, const MotionParameters& params);

    // 紧急停止
    Result<void> EmergencyStop();

    // 获取轴状态
    Result<AxisStatus> GetAxisStatus(int axis);
};

} // namespace Siligen::Domain::Services
```

### TestStateManager

测试状态管理服务，跟踪测试执行状态。

## 运动控制模块 (Motion)

### 插补器

| 插补器类型 | 说明 |
|-----------|------|
| `LinearInterpolator` | 线性插补 |
| `CircularInterpolator` | 圆弧插补 |
| `BezierInterpolator` | 贝塞尔曲线插补 |
| `BSplineInterpolator` | B样条插补 |
| `HelicalInterpolator` | 螺旋插补 |
| `EllipseInterpolator` | 椭圆插补 |

### 核心组件

| 组件 | 说明 |
|------|------|
| `BufferManager` | 缓冲管理器 |
| `TrajectoryExecutor` | 轨迹执行器 |
| `TrajectoryGenerator` | 轨迹生成器 |
| `TriggerCalculator` | 触发计算器 |
| `CMPCompensation` | CMP 补偿 |
| `CMPCoordinatedInterpolator` | CMP 协同插补 |

### 约束例外

```cpp
// CLAUDE_SUPPRESS: DOMAIN_NO_STL_CONTAINERS
// Reason: 轨迹点数量取决于路径长度和插补精度，运行时确定。
//         FixedCapacityVector 会导致栈溢出或容量不足。
// Approved-By: 架构设计
// Date: 2025-12-30
std::vector<TrajectoryPoint> GenerateTrajectory() noexcept;
```

## 触发控制模块 (Triggering)

### CMPulseGenerator

CMP 脉冲生成器，负责产生硬件触发脉冲。

```cpp
namespace Siligen::Domain::Triggering {

class CMPulseGenerator {
public:
    // 生成单次脉冲
    Result<void> GeneratePulse(const CMPulseConfig& config);

    // 生成重复脉冲
    Result<void> GenerateRepeatPulse(const CMPulseConfig& config);

    // 停止脉冲生成
    Result<void> StopPulse(int outputChannel);
};

} // namespace Siligen::Domain::Triggering
```

### PositionTriggerController

位置触发控制器，基于轴位置触发输出。

## 系统管理模块 (System)

### DiagnosticSystem

诊断系统，监控系统健康状态。

### StatusMonitor

状态监控器，跟踪系统状态变化。

## 类型定义

### 枚举类型

```cpp
// 点动方向
enum class JogDirection { Positive, Negative };

// 运动模式
enum class MotionMode {
    Idle, Jog, Position, Trajectory, Homing, Stopped, Error
};

// 回零阶段
enum class HomingStage {
    Idle, SearchingHome, BackingOff, FindingIndex,
    SettingPosition, Completed, Failed
};

// IO信号类型
enum class IOType {
    GENERAL_INPUT, GENERAL_OUTPUT,
    LIMIT_POSITIVE, LIMIT_NEGATIVE,
    ALARM, HOME, ENABLE, EMERGENCY_STOP
};
```

## 接口隔离原则 (ISP)

`LegacyMotionControllerPort` 已废弃，违反接口隔离原则。请使用细粒度接口：

| 原 LegacyMotionControllerPort 方法 | 替代接口 |
|------------------------------|---------|
| 连接管理 (5 methods) | `IMotionConnectionPort` |
| 轴控制 (9 methods) | `IAxisControlPort` |
| 位置控制 (8 methods) | `IPositionControlPort` |
| 点动 (3 methods) | `IJogControlPort` |
| 插补 (5 methods) | `IInterpolationPort` |
| IO控制 (5 methods) | `IIOControlPort` |
| 状态查询 (7 methods) | `IMotionStatePort` |
| 高级功能 (16 methods) | `IAdvancedMotionPort` |

## 使用示例

### 定义端口接口

```cpp
// src/domain/<subdomain>/ports/IAxisControlPort.h
#pragma once

#include "../../shared/types/Result.h"

namespace Siligen::Domain::Ports {

class IAxisControlPort {
public:
    virtual ~IAxisControlPort() = default;

    virtual Result<void> EnableAxis(int axisId) = 0;
    virtual Result<void> DisableAxis(int axisId) = 0;
    virtual Result<void> ClearPosition(int axisId) = 0;
    virtual Result<void> SetVelocity(int axisId, float velocity) = 0;
    virtual Result<void> SetAcceleration(int axisId, float acceleration) = 0;
};

} // namespace Siligen::Domain::Ports
```

### 定义领域模型

```cpp
// src/domain/models/MotionTypes.h
#pragma once

namespace Siligen::Domain::Models {

struct AxisStatus {
    int axisId;
    bool enabled;
    double position;
    double velocity;
    bool isHomed;
    // ...
};

} // namespace Siligen::Domain::Models
```

## 迁移指南

从旧的 `LegacyMotionControllerPort` 迁移到细粒度接口：

1. **识别使用的方法** - 确定代码实际调用的方法
2. **注入细粒度接口** - 替换为对应的专用接口
3. **更新构造函数** - 修改依赖注入参数
4. **验证功能** - 确保行为一致

详细迁移指南参见：`docs/architecture/interface-segregation-migration.md`

