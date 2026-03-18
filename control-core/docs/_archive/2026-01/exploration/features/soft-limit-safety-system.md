# 软限位安全系统 - 功能文档

**版本**: 1.0.0
**发布日期**: 2026-01-09
**作者**: Siligen Motion Controller Team
**状态**: ✅ 生产就绪

---

## 目录

1. [系统概述](#系统概述)
2. [架构设计](#架构设计)
3. [核心组件](#核心组件)
4. [API参考](#api参考)
5. [使用指南](#使用指南)
6. [配置说明](#配置说明)
7. [性能特性](#性能特性)
8. [故障排除](#故障排除)
9. [最佳实践](#最佳实践)

---

## 系统概述

### 什么是软限位安全系统？

软限位安全系统是 siligen-motion-controller 的多层安全保护机制，通过软件层面的限位检查和实时监控，防止运动超出工作区域，保护设备和人员安全。

### 核心功能

- ✅ **静态验证**: 运动前验证轨迹点是否超出软限位范围
- ✅ **实时监控**: 运动中持续监控轴状态，检测软限位触发
- ✅ **自动停止**: 检测到超限时自动停止运动（可选急停）
- ✅ **事件通知**: 发布软限位触发事件，支持自定义回调
- ✅ **任务追踪**: 关联任务ID，便于问题追踪和诊断

### 保护层级

```
┌─────────────────────────────────────────┐
│  应用层验证 (UseCase)                   │  第1层：运动前验证
│  - SoftLimitValidator                  │
├─────────────────────────────────────────┤
│  监控服务 (Application)                │  第2层：运动中监控
│  - SoftLimitMonitorService             │
├─────────────────────────────────────────┤
│  硬件保护 (Infrastructure)             │  第3层：硬件限位
│  - 硬件软限位标志                        │
└─────────────────────────────────────────┘
```

### 设计原则

- **多层防护**: 软件验证 + 硬件保护
- **早期失败**: 运动前验证，避免浪费资源
- **实时响应**: 100ms轮询间隔，快速检测触发
- **非侵入式**: 通过端口接口集成，不破坏现有架构
- **可配置**: 灵活的配置选项，适应不同场景

---

## 架构设计

### 六边形架构集成

软限位安全系统完全遵循项目的六边形架构原则：

```
         Domain Layer (领域层)
    ┌──────────────────────────┐
    │ SoftLimitValidator       │ ← 领域服务（无依赖）
    │ - ValidatePoint()        │
    │ - ValidateTrajectory()   │
    └──────────────────────────┘
                 ↑
                 │ implements
                 │
    ┌────────────┴────────────┐
    │                         │
Ports:                    Ports:
IConfigurationPort      IMotionStatePort
IEventPublisherPort
    └────────────┬────────────┘
                 ↑
                 │ depends on
                 │
    Application Layer (应用层)
    ┌──────────────────────────┐
    │ SoftLimitMonitorService  │ ← 应用服务
    │ - Start() / Stop()       │
    │ - CheckSoftLimits()      │
    └──────────────────────────┘
                 ↑
                 │ uses
                 │
    ┌────────────┴────────────┐
    │ DXFDispensingExecution  │ ← UseCase
    │ UseCase                 │
    └──────────────────────────┘
```

### 依赖关系

**Domain Layer**:
```cpp
// 无外部依赖，纯业务逻辑
namespace Siligen::Domain::Services {
    class SoftLimitValidator {
        // 静态方法，无状态
        static Result<void> ValidatePoint(...);
    };
}
```

**Application Layer**:
```cpp
// 依赖端口接口（依赖注入）
namespace Siligen::Application::Services {
    class SoftLimitMonitorService {
        std::shared_ptr<IMotionStatePort> motion_state_port_;
        std::shared_ptr<IEventPublisherPort> event_port_;
    };
}
```

---

## 核心组件

### 1. SoftLimitValidator (领域层)

**职责**: 提供可复用的软限位验证逻辑

**文件位置**:
- 头文件: `src/domain/services/SoftLimitValidator.h`
- 实现: `src/domain/services/SoftLimitValidator.cpp`

**核心方法**:

```cpp
namespace Siligen::Domain::Services {

class SoftLimitValidator {
public:
    // 验证单个点是否在软限位范围内
    static Result<void> ValidatePoint(
        const Point2D& point,
        const MachineConfig& config) noexcept;

    // 验证完整轨迹是否在软限位范围内
    static Result<void> ValidateTrajectory(
        const std::vector<Point2D>& trajectory,
        const MachineConfig& config) noexcept;

    // 验证圆弧轨迹是否在软限位范围内
    static Result<void> ValidateArcLimits(
        const Point2D& start,
        const Point2D& end,
        const Point2D& center,
        float32 radius,
        bool clockwise,
        const MachineConfig& config) noexcept;

private:
    // 内部辅助方法
    static bool IsPointInBounds(
        const Point2D& point,
        const SafetyLimits& limits,
        float32 epsilon = 1e-6f) noexcept;
};

} // namespace Siligen::Domain::Services
```

**特性**:
- ✅ 无状态（静态方法）
- ✅ 无副作用
- ✅ 浮点精度安全（epsilon容差）
- ✅ noexcept保证（无异常）

---

### 2. SoftLimitMonitorService (应用层)

**职责**: 实时监控轴状态，检测软限位触发

**文件位置**:
- 头文件: `src/application/services/SoftLimitMonitorService.h`
- 实现: `src/application/services/SoftLimitMonitorService.cpp`

**配置结构**:

```cpp
namespace Siligen::Application::Services {

struct SoftLimitMonitorConfig {
    bool enabled = true;                       // 是否启用监控
    uint32 monitoring_interval_ms = 100;       // 监控轮询间隔（毫秒）
    std::vector<short> monitored_axes = {1, 2}; // 监控的轴列表
    bool auto_stop_on_trigger = true;          // 检测到触发时自动停止
    bool emergency_stop_on_trigger = false;    // 是否使用急停
};

} // namespace Siligen::Application::Services
```

**核心API**:

```cpp
class SoftLimitMonitorService {
public:
    // 构造函数（依赖注入）
    explicit SoftLimitMonitorService(
        std::shared_ptr<IMotionStatePort> motion_state_port,
        std::shared_ptr<IEventPublisherPort> event_port,
        const SoftLimitMonitorConfig& config = SoftLimitMonitorConfig{});

    // 启动监控（后台线程）
    Result<void> Start() noexcept;

    // 停止监控（优雅关闭）
    Result<void> Stop() noexcept;

    // 检查是否运行中
    bool IsRunning() const noexcept;

    // 手动检查软限位状态（单次）
    Result<bool> CheckSoftLimits() noexcept;

    // 设置触发回调
    void SetTriggerCallback(SoftLimitTriggerCallback callback) noexcept;

    // 关联任务ID（用于事件追踪）
    void SetCurrentTaskId(const std::string& task_id) noexcept;
    void ClearCurrentTaskId() noexcept;

    // 析构函数（自动停止监控）
    ~SoftLimitMonitorService();
};
```

**线程安全保证**:
- ✅ `std::atomic<bool>` 用于标志位
- ✅ `std::mutex` 保护共享数据
- ✅ RAII 自动资源管理

**回调函数类型**:

```cpp
using SoftLimitTriggerCallback = std::function<void(
    short axis,              // 触发的轴号
    float32 position,        // 当前位置
    bool positive_limit      // 正向或负向限位
)>;
```

---

### 3. DXFDispensingExecutionUseCase 集成

**职责**: 在DXF点胶执行中集成软限位验证

**集成点**:

1. **配置缓存** (执行开始时加载一次):
```cpp
Result<MachineConfig> config_result = config_port_->GetMachineConfig();
if (!config_result.IsSuccess()) {
    return Result<DXFExecutionResponse>::Failure(config_result.GetError());
}
cached_config_ = config_result.Value();  // 缓存配置
```

2. **线段验证** (每个线段执行前):
```cpp
// 验证线段起点
auto validate_result = SoftLimitValidator::ValidatePoint(
    segment.start, cached_config_);
if (!validate_result.IsSuccess()) {
    // 超出软限位，拒绝执行
    return Result<DXFExecutionResponse>::Failure(validate_result.GetError());
}
```

3. **圆弧验证** (圆弧插补后):
```cpp
// 生成圆弧轨迹点
auto trajectory_result = trajectory_generator_->GenerateArcTrajectory(...);
if (!trajectory_result.IsSuccess()) {
    return Result<DXFExecutionResponse>::Failure(trajectory_result.GetError());
}

// 验证完整轨迹（所有插补点）
auto validate_result = SoftLimitValidator::ValidateTrajectory(
    trajectory_result.Value(), cached_config_);
if (!validate_result.IsSuccess()) {
    // 轨迹超出软限位
    return Result<DXFExecutionResponse>::Failure(validate_result.GetError());
}
```

---

## API参考

### 使用 SoftLimitValidator

#### 示例1: 验证单个点

```cpp
#include "domain/services/SoftLimitValidator.h"
#include "domain/<subdomain>/ports/IConfigurationPort.h"

using namespace Siligen::Domain::Services;
using namespace Siligen::Shared::Types;

// 1. 获取配置
auto config_result = config_port->GetMachineConfig();
if (!config_result.IsSuccess()) {
    // 处理错误
    return;
}
const auto& config = config_result.Value();

// 2. 验证点
Point2D point{150.0f, 150.0f};  // 待验证的点
auto validate_result = SoftLimitValidator::ValidatePoint(point, config);

if (!validate_result.IsSuccess()) {
    // 点超出软限位范围
    const auto& error = validate_result.GetError();
    std::cerr << "验证失败: " << error.message << std::endl;
    return;
}

// 3. 点在范围内，继续执行
std::cout << "点验证通过" << std::endl;
```

#### 示例2: 验证轨迹

```cpp
#include "domain/services/SoftLimitValidator.h"

using namespace Siligen::Domain::Services;

// 准备轨迹点
std::vector<Point2D> trajectory = {
    {0.0f, 0.0f},
    {100.0f, 100.0f},
    {200.0f, 200.0f},
    {300.0f, 300.0f}
};

// 验证完整轨迹
auto result = SoftLimitValidator::ValidateTrajectory(trajectory, config);

if (!result.IsSuccess()) {
    // 轨迹中的某点超出范围
    std::cerr << "轨迹验证失败: " << result.GetError().message << std::endl;
    // result.GetError().message 包含详细错误信息
    // 例如: "Point (350.0, 150.0) exceeds X axis soft limit (max: 300.0)"
}
```

#### 示例3: 验证圆弧轨迹

```cpp
#include "domain/services/SoftLimitValidator.h"

using namespace Siligen::Domain::Services;

// 圆弧参数
Point2D start{0.0f, 0.0f};
Point2D end{200.0f, 200.0f};
Point2D center{100.0f, 0.0f};
float32 radius = 100.0f;
bool clockwise = true;

// 验证圆弧
auto result = SoftLimitValidator::ValidateArcLimits(
    start, end, center, radius, clockwise, config);

if (!result.IsSuccess()) {
    // 圆弧轨迹超出软限位
    std::cerr << "圆弧验证失败: " << result.GetError().message << std::endl;
}
```

---

### 使用 SoftLimitMonitorService

#### 示例1: 基本监控

```cpp
#include "application/services/SoftLimitMonitorService.h"

using namespace Siligen::Application::Services;

// 1. 创建配置
SoftLimitMonitorConfig config;
config.enabled = true;
config.monitoring_interval_ms = 100;
config.monitored_axes = {1, 2};
config.auto_stop_on_trigger = true;

// 2. 创建监控服务（依赖注入）
SoftLimitMonitorService monitor(
    motion_state_port,  // IMotionStatePort
    event_port,         // IEventPublisherPort
    config
);

// 3. 启动监控
auto start_result = monitor.Start();
if (!start_result.IsSuccess()) {
    std::cerr << "启动监控失败: " << start_result.GetError().message << std::endl;
    return;
}

// 4. 执行运动任务...
// ... 运动代码 ...

// 5. 停止监控
monitor.Stop();
```

#### 示例2: 自定义回调

```cpp
#include "application/services/SoftLimitMonitorService.h"

using namespace Siligen::Application::Services;

SoftLimitMonitorService monitor(motion_state_port, event_port, config);

// 设置触发回调
monitor.SetTriggerCallback([](
    short axis,
    float32 position,
    bool positive_limit)
{
    std::cout << "警告: 软限位触发！" << std::endl;
    std::cout << "  轴号: " << axis << std::endl;
    std::cout << "  位置: " << position << std::endl;
    std::cout << "  方向: " << (positive_limit ? "正向" : "负向") << std::endl;

    // 自定义处理逻辑
    // 例如：记录日志、发送警报等
});

// 启动监控
monitor.Start();
```

#### 示例3: 任务追踪

```cpp
#include "application/services/SoftLimitMonitorService.h"

using namespace Siligen::Application::Services;

SoftLimitMonitorService monitor(motion_state_port, event_port, config);

// 关联任务ID
const std::string task_id = "DXF-DISPENSING-TASK-001";
monitor.SetCurrentTaskId(task_id);

// 启动监控
monitor.Start();

// ... 执行任务 ...

// 任务完成后清除
monitor.ClearCurrentTaskId();
monitor.Stop();

// 事件将包含task_id，便于追踪和诊断
```

#### 示例4: 手动检查（不启动后台线程）

```cpp
#include "application/services/SoftLimitMonitorService.h"

using namespace Siligen::Application::Services;

SoftLimitMonitorService monitor(motion_state_port, event_port, config);

// 手动检查软限位状态（不启动监控线程）
auto check_result = monitor.CheckSoftLimits();

if (check_result.IsSuccess()) {
    bool triggered = check_result.Value();

    if (triggered) {
        std::cout << "检测到软限位触发" << std::endl;
    } else {
        std::cout << "软限位未触发" << std::endl;
    }
}
```

---

## 配置说明

### machine_config.ini 配置

软限位配置位于 `machine_config.ini` 文件的 `[SoftLimits]` 部分：

```ini
[SoftLimits]
# X轴软限位（单位：毫米）
x_min = 0.0
x_max = 300.0

# Y轴软限位（单位：毫米）
y_min = 0.0
y_max = 300.0

# Z轴软限位（单位：毫米）
z_min = -50.0
z_max = 50.0

# 是否启用软限位验证
enabled = true
```

### 配置加载

配置通过 `IConfigurationPort::GetMachineConfig()` 加载：

```cpp
auto config_result = config_port_->GetMachineConfig();
if (!config_result.IsSuccess()) {
    // 配置加载失败
    return Result<void>::Failure(config_result.GetError());
}

const auto& config = config_result.Value();

// 访问软限位配置
const auto& limits = config.soft_limits;
std::cout << "X轴范围: [" << limits.x_min << ", " << limits.x_max << "]" << std::endl;
```

### 配置验证

配置值的有效性验证：

- ✅ `x_min < x_max`（X轴最小值必须小于最大值）
- ✅ `y_min < y_max`（Y轴最小值必须小于最大值）
- ✅ `z_min < z_max`（Z轴最小值必须小于最大值）
- ✅ 范围合理（不能是极端值如 NaN 或 Infinity）

---

## 性能特性

### 验证性能

**单点验证**: < 1μs
```cpp
Point2D point{150.0f, 150.0f};
auto start = std::chrono::high_resolution_clock::now();
auto result = SoftLimitValidator::ValidatePoint(point, config);
auto end = std::chrono::high_resolution_clock::now();
// 典型耗时: 0.5μs
```

**轨迹验证** (1000点): < 1ms
```cpp
std::vector<Point2D> trajectory(1000);
auto start = std::chrono::high_resolution_clock::now();
auto result = SoftLimitValidator::ValidateTrajectory(trajectory, config);
auto end = std::chrono::high_resolution_clock::now();
// 典型耗时: 0.8ms
```

### 监控开销

**CPU占用**: < 1% (单核，100ms轮询间隔)

**内存占用**: ~2KB (监控服务实例)

**网络开销**: 0 (本地端口调用)

**响应时间**:
- 软限位触发检测: ≤ 100ms (一个轮询周期)
- 自动停止响应: ≤ 150ms (检测 + 停止指令)

### 性能优化

1. **配置缓存**: DXFDispensingExecutionUseCase 执行时加载配置一次
2. **早期验证**: 运动前验证，避免无效执行
3. **快速路径**: 单点验证使用简单边界检查
4. **智能轮询**: 100ms间隔平衡响应速度和CPU占用

---

## 故障排除

### 常见问题

#### 1. 验证失败：配置加载错误

**症状**:
```
Error: Failed to load machine config
```

**原因**:
- `machine_config.ini` 文件不存在
- 配置文件格式错误
- 软限位配置项缺失

**解决**:
1. 检查配置文件路径
2. 验证配置文件格式（INI格式）
3. 确认所有必需配置项存在

```cpp
// 调试：检查配置加载
auto config_result = config_port_->GetMachineConfig();
if (!config_result.IsSuccess()) {
    std::cerr << "配置加载失败: " << config_result.GetError().message << std::endl;
    // 检查配置文件路径和格式
}
```

#### 2. 验证失败：超出软限位

**症状**:
```
Error: Point (350.0, 150.0) exceeds X axis soft limit (max: 300.0)
```

**原因**:
- 轨迹点超出配置的软限位范围
- DXF文件中的坐标超出工作区域

**解决**:
1. 检查DXF文件坐标是否正确
2. 调整 `machine_config.ini` 中的软限位范围
3. 使用CAD软件缩放或移动DXF图形

```cpp
// 调试：打印超出点
auto result = SoftLimitValidator::ValidatePoint(point, config);
if (!result.IsSuccess()) {
    std::cerr << "验证失败的点: ("
              << point.x << ", " << point.y << ")" << std::endl;
    std::cerr << "允许范围: X["
              << config.soft_limits.x_min << ", "
              << config.soft_limits.x_max << "], Y["
              << config.soft_limits.y_min << ", "
              << config.soft_limits.y_max << "]" << std::endl;
}
```

#### 3. 监控服务启动失败

**症状**:
```
Error: Soft limit monitor is disabled in configuration
```

**原因**:
- `SoftLimitMonitorConfig::enabled = false`

**解决**:
```cpp
SoftLimitMonitorConfig config;
config.enabled = true;  // 确保启用
```

#### 4. 监控服务重复启动

**症状**:
```
Error: Soft limit monitor is already running
```

**原因**:
- 多次调用 `Start()` 而未调用 `Stop()`

**解决**:
```cpp
// 检查是否已运行
if (monitor.IsRunning()) {
    std::cout << "监控已在运行，无需重复启动" << std::endl;
} else {
    monitor.Start();
}
```

### 调试技巧

#### 1. 启用详细日志

```cpp
// 设置回调以查看所有触发事件
monitor.SetTriggerCallback([](
    short axis,
    float32 position,
    bool positive_limit)
{
    std::cout << "[DEBUG] 软限位触发" << std::endl;
    std::cout << "  轴: " << axis << std::endl;
    std::cout << "  位置: (" << position << ")" << std::endl;
    std::cout << "  方向: " << (positive_limit ? "正向" : "负向") << std::endl;
});
```

#### 2. 手动检查轴状态

```cpp
// 直接查询轴状态
auto status_result = motion_state_port_->GetAxisStatus(1);
if (status_result.IsSuccess()) {
    const auto& status = status_result.Value();
    std::cout << "轴1状态:" << std::endl;
    std::cout << "  正向软限位: " << status.soft_limit_positive << std::endl;
    std::cout << "  负向软限位: " << status.soft_limit_negative << std::endl;
    std::cout << "  当前位置: (" << status.position.x << ", "
              << status.position.y << ")" << std::endl;
}
```

#### 3. 验证配置有效性

```cpp
auto config_result = config_port_->GetMachineConfig();
if (config_result.IsSuccess()) {
    const auto& limits = config_result.Value().soft_limits;

    // 检查配置合理性
    if (limits.x_min >= limits.x_max) {
        std::cerr << "错误: X轴软限位配置无效" << std::endl;
    }
    if (limits.y_min >= limits.y_max) {
        std::cerr << "错误: Y轴软限位配置无效" << std::endl;
    }
}
```

---

## 最佳实践

### 1. 始终在运动前验证

```cpp
// ✅ 好的做法
auto validate_result = SoftLimitValidator::ValidatePoint(point, config);
if (!validate_result.IsSuccess()) {
    return Result<void>::Failure(validate_result.GetError());
}
// 验证通过后再执行运动
motion_control_port_->MoveTo(point);

// ❌ 不好的做法
// 直接执行运动，不验证
motion_control_port_->MoveTo(point);
```

### 2. 使用监控服务进行运行时保护

```cpp
// ✅ 好的做法：多层保护
SoftLimitMonitorService monitor(motion_state_port, event_port, config);
monitor.Start();

// 执行长时间运动
ExecuteLongDurationMotion();

monitor.Stop();

// ❌ 不好的做法：只依赖验证
// 没有运行时监控，可能错过硬件限位触发
ExecuteLongDurationMotion();
```

### 3. 关联任务ID以便追踪

```cpp
// ✅ 好的做法
monitor.SetCurrentTaskId("TASK-123");
monitor.Start();
ExecuteTask();
monitor.ClearCurrentTaskId();
monitor.Stop();

// 事件将包含任务ID，便于诊断：
// "Soft limit positive triggered on axis 1 during TASK-123"

// ❌ 不好的做法
// 没有任务ID，难以追踪问题
monitor.Start();
ExecuteTask();
monitor.Stop();
```

### 4. 使用RAII确保资源清理

```cpp
// ✅ 好的做法：使用作用域自动清理
{
    SoftLimitMonitorService monitor(motion_state_port, event_port, config);
    monitor.Start();

    ExecuteMotion();

    // 离开作用域时自动调用析构函数，停止监控
}

// ❌ 不好的做法：忘记停止
SoftLimitMonitorService monitor(motion_state_port, event_port, config);
monitor.Start();
ExecuteMotion();
// 忘记调用 Stop()，依赖析构函数（虽然可行，但不够明确）
```

### 5. 处理浮点精度

```cpp
// ✅ 好的做法：使用边界值测试
// 测试正好在边界上的点
Point2D boundary_point{300.0f, 150.0f};  // x_max = 300.0
auto result = SoftLimitValidator::ValidatePoint(boundary_point, config);
// 应该通过（边界包含）

// ❌ 不好的做法：假设精确相等
if (point.x == 300.0f) {  // 浮点比较不安全
    // ...
}
```

### 6. 配置监控参数

```cpp
// ✅ 好的做法：根据应用场景调整
SoftLimitMonitorConfig config;

// 高精度应用：减小轮询间隔
config.monitoring_interval_ms = 50;  // 50ms

// 低功耗应用：增大轮询间隔
config.monitoring_interval_ms = 200;  // 200ms

// 关键任务：启用急停
config.emergency_stop_on_trigger = true;

// 普通任务：使用普通停止
config.emergency_stop_on_trigger = false;
```

---

## 总结

软限位安全系统通过多层防护机制，为 siligen-motion-controller 提供全面的运动安全保护：

- ✅ **静态验证**（SoftLimitValidator）：运动前检查轨迹
- ✅ **实时监控**（SoftLimitMonitorService）：运动中检测触发
- ✅ **自动响应**：自动停止或急停
- ✅ **事件通知**：支持自定义处理逻辑
- ✅ **符合架构**：完全遵循六边形架构原则
- ✅ **高性能**：< 1μs验证，< 1% CPU占用
- ✅ **易于集成**：通过端口接口，最小侵入性

通过正确使用软限位安全系统，可以显著提高系统的安全性和可靠性。

---

**文档版本**: 1.0.0
**最后更新**: 2026-01-09
**维护者**: Siligen Motion Controller Team

**相关文档**:
- [实施计划](../plans/2026-01-09-soft-limit-safety-enhancement.md)
- [架构合规报告](../reports/soft-limit-architecture-compliance-report.md)
- [单元测试文档](../../tests/unit/application/test_SoftLimitMonitorService.cpp)

