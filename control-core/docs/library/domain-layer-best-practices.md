# Domain 层最佳实践方案
# 六边形架构 + 点胶机业务流程

**创建时间**: 2026-01-16
**项目**: Backend_CPP (点胶机运动控制系统)
**架构**: 六边形架构 (Hexagonal Architecture) + DDD
**语言**: C++17

---

## 执行摘要

本报告通过 Web Search 和 Context7 对六边形架构和 DDD 最佳实践进行了深入研究，结合点胶机业务流程特点，为 `src/domain` 目录结构提出系统化的改进方案。

### 核心发现

1. **当前结构偏技术导向**: 现有目录结构以技术组件（ports, services, models）为主，缺少业务领域边界
2. **缺乏聚合根和值对象明确区分**: models 目录混杂了类型定义和领域模型
3. **缺少显式的业务子域划分**: 点胶机业务包含运动控制、点胶工艺、安全监控等多个子域，未清晰分离
4. **端口设计优秀**: 当前的 ports 设计符合六边形架构原则，依赖反转清晰

### 补充规范（2026-02-04）

针对“胶路建压/稳压”流程，新增并固化以下架构规范（必须遵守）：

1. **规则唯一来源**: 建压/稳压时序与规则必须位于 Domain 层，不允许在 Application 或 Infrastructure 复写或复制。
2. **统一入口**: 使用 `SupplyStabilizationPolicy` 解析稳压时间；业务流程通过
   `PurgeDispenserProcess` / `DispensingProcessService` 触发与等待。
3. **配置来源**: 稳压时间从 `IConfigurationPort::GetDispensingConfig().supply_stabilization_ms` 获取。
4. **覆盖规则**: 允许用例通过请求传入覆盖值，但必须由领域层校验（范围 0-5000ms，0 表示使用配置默认值）。
5. **职责边界**: Application 只编排流程与调用端口；Infrastructure 只负责阀门 I/O 与适配，不包含时序或业务判断。

---

### 补充规范（点动统一入口，2026-02-04）

针对“点动（Jog）”功能，新增并固化以下架构规范（必须遵守）：

1. **规则唯一来源**: 点动参数校验、互锁判定与限位规则必须位于 Domain 层。
2. **统一入口**: 统一由 `Domain::Motion::DomainServices::JogController` 处理点动规则与流程。
3. **职责边界**: Application 层仅编排流程；Adapters 仅负责协议/参数映射与硬件调用。
4. **端口职责**: `IJogControlPort` 仅承载硬件动作与参数下发，不包含业务校验。

---

## 一、六边形架构 Domain 层核心原则

### 1.1 架构隔离原则

根据 [The Ultimate Guide to Mastering Hexagonal Architecture](https://scalastic.io/en/hexagonal-architecture-domain/) 和 [Domain-Driven Hexagon](https://github.com/Sairyss/domain-driven-hexagon):

```
核心隔离规则:
- Domain 层是内核层，不依赖任何外部层
- Domain 层定义 Ports (接口)，Infrastructure 层实现 Adapters
- Domain 层只依赖 Shared 层的类型和工具
- 禁止 Domain 层包含任何技术实现细节
```

**依赖方向**:
```
外部世界 → Adapters → Ports → Domain Services → Entities/Value Objects
```

### 1.2 互锁规则单一来源规范（强制）

- `Safety::DomainServices::InterlockPolicy` 是互锁规则唯一入口与规则来源
- 应用层与基础设施层不得复写互锁判定逻辑，仅负责采集、转发与动作执行
- 互锁信号必须通过端口提供（`IInterlockSignalPort`），Domain 不直接依赖硬件
- 诊断适配器保持硬件直读/直判，不纳入互锁统一入口

### 1.3 Domain 层组件构成

根据 [Microsoft DDD 指南](https://learn.microsoft.com/en-us/dotnet/architecture/microservices/microservice-ddd-cqrs-patterns/ddd-oriented-microservice) 和 [ABP Framework DDD 文档](https://abp.io/docs/latest/framework/architecture/domain-driven-design/domain-layer):

| 组件类型 | 职责 | 特征 |
|---------|------|------|
| **Entities (实体)** | 有唯一标识的业务对象 | 可变、有生命周期、有标识 |
| **Value Objects (值对象)** | 无标识的描述性对象 | 不可变、可替换、由属性值定义 |
| **Aggregates (聚合)** | 一组相关实体和值对象的集合 | 有聚合根、保证一致性边界 |
| **Domain Services (领域服务)** | 不属于实体的业务逻辑 | 无状态、跨实体操作 |
| **Domain Events (领域事件)** | 业务状态变化通知 | 不可变、异步解耦 |
| **Ports (端口接口)** | 与外部世界的契约 | 纯虚接口、依赖反转 |

---

## 二、当前 Domain 层结构分析

### 2.1 现有目录结构

```
src/domain/
├── configuration/          # 配置类 (MachineConfig, CMPPulseConfig, etc.)
├── events/                 # 领域事件 (未实际使用)
├── geometry/               # 几何类型 (GeometryTypes.h)
├── models/                 # 领域模型 (混杂了类型和实体)
│   ├── DispenserModel.h   # 点胶机聚合根 ✓
│   ├── ConfigTypes.h      # 配置类型定义
│   ├── MotionTypes.h      # 运动类型定义
│   └── ...                # 其他类型定义 (17个文件)
├── motion/                 # 运动控制算法 (插值、轨迹等)
│   └── state/             # 状态机
├── ports/                  # 端口接口 ✓
├── services/               # 领域服务
│   ├── CMPService.h       # CMP触发服务 ✓
│   ├── MotionService.h    # 运动服务
│   └── ...
├── system/                 # 系统诊断
├── triggering/             # 触发控制
└── README.md
```

### 2.2 问题诊断

#### 问题 1: 技术视角 vs 业务视角

**现状**: 目录结构反映技术分类（models, services, ports）
**问题**: 难以理解业务边界，新成员无法快速定位业务逻辑
**影响**: 跨子域的功能难以隔离，容易产生循环依赖

#### 问题 2: models 目录职责不清

**现状**: models 目录包含：
- 1 个聚合根 (DispenserModel.h)
- 17 个类型定义文件 (ConfigTypes.h, MotionTypes.h, etc.)

**问题**:
- 类型定义应该在 `shared/types`
- 缺少值对象的明确标识
- 缺少聚合的明确边界

#### 问题 3: motion 目录包含算法实现

**现状**: motion 目录有大量插值算法、轨迹计算
**问题**: 这些是 Domain Services 的职责，但未清晰归类
**影响**: 算法复用困难，业务规则与算法逻辑混杂

#### 问题 4: 缺少业务子域划分

点胶机业务包含多个子域:
- **运动控制子域**: 轴控制、回零、点动、插补、轨迹规划与执行
- **点胶工艺子域**: 点胶过程、胶路建压、工艺参数、触发控制、阀门协调、工艺结果
- **安全监控子域**: 限位检测、安全互锁、急停、诊断
- **设备运行子域**: 运行模式（自动运行/手动/暂停）、任务状态、标定流程（规则统一入口：DispenserModel）
- **配方子域**: 配方定义、版本管理、配方生效
- **配置管理子域**: 机器参数、工艺配置

**当前结构未体现这些边界**

---

## 三、最佳实践方案

### 3.1 推荐的目录结构

```
src/domain/
│
├── README.md                           # 领域层总览
├── CLAUDE.md                           # 领域层约束 (已有)
├── PhysicalDependencyRules.md          # 物理依赖规则 (已有)
│
├── shared/                             # 领域内共享
│   ├── value-objects/                 # 共享值对象
│   │   ├── Point2D.h                  # 2D坐标 (值对象)
│   │   ├── Velocity.h                 # 速度值对象
│   │   └── AxisPosition.h             # 轴位置值对象
│   ├── domain-events/                 # 领域事件基类
│   │   ├── DomainEventBase.h
│   │   └── EventPublisher.h
│   └── specifications/                # 领域规约模式
│       └── SafetySpecification.h
│
├── motion-control/                     # 运动控制子域 ⭐
│   ├── README.md                      # 子域说明
│   ├── entities/
│   │   ├── Axis.h                     # 轴实体
│   │   └── Trajectory.h               # 轨迹实体
│   ├── value-objects/
│   │   ├── MotionParameters.h         # 运动参数值对象
│   │   ├── InterpolationMode.h        # 插值模式枚举
│   │   └── MotionState.h              # 运动状态值对象
│   ├── aggregates/
│   │   └── MotionController.h         # 运动控制器聚合根
│   ├── domain-services/
│   │   ├── TrajectoryPlanner.h        # 轨迹规划服务
│   │   ├── InterpolationService.h     # 插值服务
│   │   │   ├── LinearInterpolator.h   # 线性插值
│   │   │   ├── ArcInterpolator.h      # 圆弧插值
│   │   │   └── SplineInterpolator.h   # 样条插值
│   │   └── BufferManager.h            # 缓冲管理服务
│   ├── events/
│   │   ├── AxisMovedEvent.h
│   │   └── TrajectoryCompletedEvent.h
│   └── ports/
│       ├── IPositionControlPort.h     # 位置控制端口
│       ├── IMotionStatePort.h         # 运动状态端口
│       └── IJogControlPort.h          # 点动控制端口
│
├── dispensing/                         # 点胶工艺子域 ⭐
│   ├── README.md
│   ├── entities/
│   │   ├── DispensingPath.h           # 点胶路径实体
│   │   └── Valve.h                    # 阀门实体
│   ├── value-objects/
│   │   ├── DispensingParameters.h     # 点胶参数值对象
│   │   ├── TriggerPoint.h             # 触发点值对象
│   │   └── FlowRate.h                 # 流量值对象
│   ├── aggregates/
│   │   └── DispensingTask.h           # 点胶任务聚合根
│   ├── domain-services/
│   │   ├── DispensingProcessService.h # 点胶过程统一入口
│   │   ├── DispensingController.h     # 触发计算与策略选择
│   │   ├── CMPTriggerService.h        # CMP触发服务
│   │   ├── ValveCoordinationService.h # 阀门协调服务
│   │   └── PathOptimizer.h            # 路径优化服务
│   ├── events/
│   │   ├── DispensingStartedEvent.h
│   │   ├── TriggerFiredEvent.h
│   │   └── DispensingCompletedEvent.h
│   └── ports/
│       ├── ITriggerControllerPort.h   # 触发控制端口
│       └── IValvePort.h               # 阀门控制端口
│
├── machine/                            # 点胶机设备子域 ⭐
│   ├── README.md
│   ├── aggregates/
│   │   └── DispenserModel.h           # 点胶机聚合根（自动运行/运行模式规则唯一来源）
│   ├── value-objects/                 # 值对象（按需扩展）
│   │   └── MachineConfiguration.h     # 机器配置值对象
│   ├── domain-services/
│   │   └── TaskQueueManager.h         # 任务队列管理（按需）
│   ├── events/
│   │   ├── MachineEvent.h
│   │   └── TaskQueuedEvent.h
│   └── ports/
│       ├── IMotionConnectionPort.h    # 运动连接端口
│       └── IHardwareTestPort.h        # 硬件测试端口
│
├── safety/                             # 安全监控子域 ⭐
│   ├── README.md
│   ├── value-objects/
│   │   ├── SafetyLimit.h              # 安全限位值对象
│   │   ├── EmergencyState.h           # 紧急状态值对象
│   │   └── InterLock.h                # 互锁值对象
│   ├── domain-services/
│   │   ├── SoftLimitValidator.h       # 软限位验证 (已有)
│   │   ├── InterlockMonitor.h         # 互锁监控
│   │   └── EmergencyStopHandler.h     # 紧急停止处理
│   ├── events/
│   │   ├── SafetyViolationEvent.h
│   │   └── EmergencyStopEvent.h
│   └── ports/
│       └── ISafetyMonitorPort.h       # 安全监控端口
│
├── diagnostics/                        # 诊断子域 ⭐
│   ├── README.md
│   ├── entities/
│   │   └── DiagnosticReport.h
│   ├── value-objects/
│   │   ├── HealthStatus.h
│   │   └── PerformanceMetrics.h
│   ├── domain-services/
│   │   ├── DiagnosticSystem.h         # 诊断系统 (已有)
│   │   └── StatusMonitor.h            # 状态监控 (已有)
│   └── ports/
│       └── IDiagnosticsPort.h
│
└── configuration/                      # 配置管理子域 ⭐
    ├── README.md
    ├── entities/
    │   └── ConfigurationProfile.h     # 配置档案实体
    ├── value-objects/
    │   ├── CMPConfiguration.h         # CMP配置值对象
    │   ├── InterpolationConfig.h      # 插值配置值对象
    │   └── ValveTimingConfig.h        # 阀门时序配置值对象
    ├── domain-services/
    │   ├── ConfigValidator.h          # 配置验证服务
    │   └── ConfigurationMigrator.h    # 配置迁移服务
    └── ports/
        ├── IConfigurationPort.h       # 配置端口
        └── IFileStoragePort.h         # 文件存储端口
```

### 3.2 设计原则说明

#### 原则 1: 按业务子域组织 (Bounded Context)

**理论依据**: [DDD Bounded Context Pattern](https://martinfowler.com/bliki/BoundedContext.html)

每个子域有清晰的边界:
- **motion-control**: 负责底层运动算法和轨迹执行
- **dispensing**: 负责点胶工艺逻辑和触发协调
- **machine**: 负责设备整体状态和任务编排
- **safety**: 负责安全规则和监控
- **diagnostics**: 负责诊断和性能监控
- **configuration**: 负责配置管理和验证

#### 原则 2: 明确实体、值对象、聚合的边界

**实体 (Entities)**:
- 有唯一标识 (ID)
- 有生命周期
- 可变
- 例: `Axis`, `Trajectory`, `DispensingPath`, `DispensingMachine`

**值对象 (Value Objects)**:
- 无标识
- 不可变
- 通过属性定义相等性
- 例: `Point2D`, `Velocity`, `MotionParameters`, `TriggerPoint`

**聚合根 (Aggregate Roots)**:
- 是实体的特殊形式
- 是一致性边界
- 外部只能通过聚合根访问内部实体
- 例: `DispensingMachine`, `MotionController`, `DispensingTask`

#### 原则 3: Domain Services 封装跨实体业务逻辑

**理论依据**: [Domain Services Pattern](https://dev.to/ielgohary/domain-driven-design-entities-value-objects-and-services-chapter-51-22cm)

Domain Services 特征:
- 无状态
- 操作跨越多个实体或值对象
- 包含不属于单个实体的业务逻辑
- 例: `TrajectoryPlanner`, `CMPTriggerService`, `PathOptimizer`

#### 原则 4: 每个子域独立端口

**理论依据**: [Interface Segregation Principle](https://tsh.io/blog/hexagonal-architecture)

- 每个子域定义自己需要的端口
- 避免"胖接口"
- 便于测试和替换实现

---

## 四、点胶机业务领域建模

### 4.1 核心聚合根识别

#### 聚合根 1: DispensingMachine (点胶机)

**职责**: 点胶机整体状态管理、任务编排
**一致性边界**: 机器状态、任务队列、配置
**不变式**:
- 未初始化状态不能执行任务
- 错误状态必须清除后才能继续
- 任务队列最多 100 个

**重构自**: `src/domain/models/DispenserModel.h`

```cpp
namespace Siligen::Domain::Machine {

class DispensingMachine {
public:
    // 聚合根标识
    // 获取聚合根标识（实现与类型按实际需要扩展）
    std::string GetId() const;

    // 状态管理
    Result<void> Initialize();
    Result<void> Shutdown();
    // 获取设备状态（实现与类型按实际需要扩展）
    std::string GetState() const;

    // 任务管理
    Result<void> QueueTask(DispensingTask task);
    Result<void> StartNextTask();
    Result<void> PauseCurrentTask();
    Result<void> CancelTask(TaskId id);

    // 配置管理
    Result<void> ApplyConfiguration(MachineConfiguration config);

private:
    std::string id_;
    std::string state_;
    std::vector<DispensingTask> task_queue_;
    MachineConfiguration config_;
};

} // namespace Siligen::Domain::Machine
```

#### 聚合根 2: MotionController (运动控制器)

**职责**: 多轴运动协调、轨迹执行
**一致性边界**: 轴状态、当前轨迹、插值参数
**不变式**:
- 轴必须回零后才能运动
- 轨迹执行中不能改变插值模式
- 缓冲区不能溢出

```cpp
namespace Siligen::Domain::MotionControl {

class MotionController {
public:
    // 轴管理
    Result<void> AddAxis(AxisId id, AxisParameters params);
    Result<AxisState> GetAxisState(AxisId id) const;

    // 轨迹执行
    Result<void> ExecuteTrajectory(Trajectory trajectory);
    Result<void> StopMotion();
    Result<void> PauseMotion();

    // 插值控制
    Result<void> SetInterpolationMode(InterpolationMode mode);

private:
    std::map<AxisId, Axis> axes_;
    std::unique_ptr<Trajectory> current_trajectory_;
    InterpolationMode interpolation_mode_;
};

} // namespace Siligen::Domain::MotionControl
```

#### 聚合根 3: DispensingTask (点胶任务)

**职责**: 单次点胶任务的完整生命周期
**一致性边界**: 路径、触发点、阀门参数
**不变式**:
- 路径不能为空
- 触发点必须在路径范围内
- 阀门开关时序合法

```cpp
namespace Siligen::Domain::Dispensing {

class DispensingTask {
public:
    TaskId GetId() const;

    // 路径管理
    Result<void> SetPath(DispensingPath path);
    const DispensingPath& GetPath() const;

    // 触发点管理
    Result<void> AddTriggerPoint(TriggerPoint point);
    Result<void> OptimizeTriggerPoints();

    // 执行控制
    Result<void> Start();
    Result<void> Pause();
    Result<void> Resume();
    Result<void> Complete();

    // 进度查询
    float GetProgress() const;
    bool IsCompleted() const;

private:
    TaskId id_;
    DispensingPath path_;
    std::vector<TriggerPoint> trigger_points_;
    TaskState state_;
};

} // namespace Siligen::Domain::Dispensing
```

### 4.2 关键值对象设计

#### 值对象示例 1: Point2D (2D坐标)

**职责**: 表示2D平面坐标
**不变式**: 坐标在有效范围内
**不可变性**: 一旦创建不能修改

```cpp
namespace Siligen::Domain::Shared {

class Point2D {
public:
    static Result<Point2D> Create(double x, double y);

    double X() const { return x_; }
    double Y() const { return y_; }

    // 值对象相等性
    bool operator==(const Point2D& other) const {
        return x_ == other.x_ && y_ == other.y_;
    }

    // 距离计算
    double DistanceTo(const Point2D& other) const;

private:
    Point2D(double x, double y) : x_(x), y_(y) {}
    double x_;
    double y_;
};

} // namespace Siligen::Domain::Shared
```

#### 值对象示例 2: TriggerPoint (触发点)

**职责**: 封装触发点参数
**不变式**: 脉冲宽度和位置合法
**不可变性**: 一旦创建不能修改

```cpp
namespace Siligen::Domain::Dispensing {

class TriggerPoint {
public:
    static Result<TriggerPoint> Create(
        int32_t position,
        int32_t pulse_width_us,
        int32_t delay_us = 0
    );

    int32_t Position() const { return position_; }
    int32_t PulseWidth() const { return pulse_width_us_; }
    int32_t Delay() const { return delay_us_; }

    bool operator==(const TriggerPoint& other) const {
        return position_ == other.position_ &&
               pulse_width_us_ == other.pulse_width_us_ &&
               delay_us_ == other.delay_us_;
    }

private:
    TriggerPoint(int32_t position, int32_t pulse_width_us, int32_t delay_us)
        : position_(position)
        , pulse_width_us_(pulse_width_us)
        , delay_us_(delay_us) {}

    int32_t position_;
    int32_t pulse_width_us_;
    int32_t delay_us_;
};

} // namespace Siligen::Domain::Dispensing
```

### 4.3 Domain Services 划分

#### 服务 1: TrajectoryPlanner (轨迹规划服务)

**职责**: 根据路径生成轨迹
**输入**: 路径点、运动参数
**输出**: 可执行轨迹

```cpp
namespace Siligen::Domain::MotionControl {

class TrajectoryPlanner {
public:
    Result<Trajectory> PlanTrajectory(
        const std::vector<Point2D>& path,
        const MotionParameters& params
    );

    Result<void> ValidateTrajectory(const Trajectory& trajectory);

private:
    std::shared_ptr<IInterpolationPort> interpolation_port_;
};

} // namespace Siligen::Domain::MotionControl
```

#### 服务 2: CMPTriggerService (CMP触发服务)

**职责**: 管理CMP触发配置和执行
**输入**: 触发点列表、触发模式
**输出**: 触发配置

```cpp
namespace Siligen::Domain::Dispensing {

class CMPTriggerService {
public:
    Result<void> ConfigureTriggers(
        const std::vector<TriggerPoint>& points,
        CMPTriggerMode mode
    );

    Result<void> EnableTriggers();
    Result<void> DisableTriggers();

    Result<std::vector<TriggerPoint>> GenerateTriggersFromPath(
        const DispensingPath& path,
        double interval_mm
    );

private:
    std::shared_ptr<ITriggerControllerPort> trigger_port_;
};

} // namespace Siligen::Domain::Dispensing
```

---

## 五、迁移策略

### 5.1 渐进式重构路线图

#### 阶段 1: 创建新目录结构 (1-2天)

**目标**: 创建新的子域目录，不影响现有代码

**步骤**:
1. 创建子域目录结构 (motion-control, dispensing, machine, safety, diagnostics, configuration)
2. 为每个子域创建 README.md，说明职责边界
3. 创建 entities/, value-objects/, aggregates/, domain-services/, events/, ports/ 子目录

**验证**: 编译通过，现有功能不受影响

#### 阶段 2: 迁移端口接口 (2-3天)

**目标**: 将 ports/ 目录的接口分配到各子域

**步骤**:
1. 分析现有端口的职责归属
2. 将端口移动到对应子域的 ports/ 目录
3. 更新 include 路径
4. 更新 CMakeLists.txt

**示例迁移**:
```
src/domain/<subdomain>/ports/IPositionControlPort.h
→ src/domain/motion-control/ports/IPositionControlPort.h

src/domain/dispensing/ports/ITriggerControllerPort.h
→ src/domain/dispensing/ports/ITriggerControllerPort.h

src/domain/motion/ports/IHomingPort.h
→ src/domain/motion-control/ports/IHomingPort.h
```

**验证**: 编译通过，端口依赖正确

#### 阶段 3: 重构聚合根 (3-4天)

**目标**: 重构 DispenserModel 为 DispensingMachine 聚合根

**步骤**:
1. 创建 `src/domain/machine/aggregates/DispensingMachine.h`
2. 提取 DispenserModel 中的状态管理逻辑
3. 提取任务队列管理逻辑
4. 保留原 DispenserModel 作为适配层 (Facade Pattern)
5. 逐步迁移使用方

**验证**: 单元测试通过，行为一致

#### 阶段 4: 提取值对象 (2-3天)

**目标**: 将类型定义重构为值对象

**步骤**:
1. 识别 models/ 目录中可以作为值对象的类型
2. 重构为不可变值对象
3. 添加验证逻辑
4. 添加工厂方法 (Create)

**示例重构**:
```cpp
// Before: ConfigTypes.h
struct CMPConfiguration {
    int32_t position;
    int32_t pulse_width;
};

// After: dispensing/value-objects/TriggerPoint.h
class TriggerPoint {
public:
    static Result<TriggerPoint> Create(int32_t position, int32_t pulse_width);
    // ... (不可变接口)
};
```

**验证**: 值对象不可变，验证逻辑正确

#### 阶段 5: 迁移领域服务 (3-4天)

**目标**: 将 services/ 和 motion/ 中的服务分配到子域

**步骤**:
1. 分析现有服务的职责归属
2. 将服务移动到对应子域的 domain-services/ 目录
3. 重构服务依赖，使用子域内的端口
4. 更新命名空间

**示例迁移**:
```
src/domain/services/CMPService.h
→ src/domain/dispensing/domain-services/CMPTriggerService.h

src/domain/motion/TrajectoryGenerator.h
→ src/domain/motion-control/domain-services/TrajectoryPlanner.h

src/domain/motion/LinearInterpolator.h
→ src/domain/motion-control/domain-services/interpolation/LinearInterpolator.h
```

**验证**: 服务职责清晰，依赖正确

#### 阶段 6: 清理和文档 (1-2天)

**目标**: 清理旧目录，完善文档

**步骤**:
1. 删除旧的 models/, services/, motion/ 目录
2. 更新 domain/README.md
3. 为每个子域创建详细 README.md
4. 更新 CLAUDE.md 约束
5. 更新 CMakeLists.txt

**验证**: 所有测试通过，文档完整

### 5.2 迁移优先级

| 优先级 | 子域 | 原因 |
|--------|------|------|
| **P0** | machine | 聚合根重构，影响面大 |
| **P1** | motion-control | 运动控制是核心功能 |
| **P1** | dispensing | 点胶工艺是核心业务 |
| **P2** | safety | 安全功能独立性强 |
| **P3** | diagnostics | 诊断功能独立性强 |
| **P3** | configuration | 配置管理独立性强 |

### 5.3 风险控制

#### 风险 1: 编译依赖破坏

**缓解措施**:
- 使用符号链接保留旧路径（临时）
- 使用 namespace alias 保留旧命名空间
- 分阶段迁移，每阶段验证编译

#### 风险 2: 运行时行为变化

**缓解措施**:
- 保留原有类作为 Facade
- 增加集成测试覆盖
- 使用 Adapter Pattern 渐进替换

#### 风险 3: 团队适应成本

**缓解措施**:
- 提供迁移文档和示例
- 创建代码模板和生成器
- 进行代码审查和知识分享

---

## 六、命名空间设计

### 6.1 推荐命名空间结构

```cpp
namespace Siligen {
namespace Domain {

    // 共享命名空间
    namespace Shared {
        namespace ValueObjects { ... }
        namespace Events { ... }
        namespace Specifications { ... }
    }

    // 子域命名空间
    namespace MotionControl {
        namespace Entities { ... }
        namespace ValueObjects { ... }
        namespace Aggregates { ... }
        namespace DomainServices { ... }
        namespace Events { ... }
        namespace Ports { ... }
    }

    namespace Dispensing {
        namespace Entities { ... }
        namespace ValueObjects { ... }
        namespace Aggregates { ... }
        namespace DomainServices { ... }
        namespace Events { ... }
        namespace Ports { ... }
    }

    namespace Machine {
        namespace Aggregates { ... }
        namespace ValueObjects { ... }
        namespace DomainServices { ... }
        namespace Events { ... }
        namespace Ports { ... }
    }

    namespace Safety {
        namespace ValueObjects { ... }
        namespace DomainServices { ... }
        namespace Events { ... }
        namespace Ports { ... }
    }

    namespace Diagnostics {
        namespace Entities { ... }
        namespace ValueObjects { ... }
        namespace DomainServices { ... }
        namespace Ports { ... }
    }

    namespace Configuration {
        namespace Entities { ... }
        namespace ValueObjects { ... }
        namespace DomainServices { ... }
        namespace Ports { ... }
    }

} // namespace Domain
} // namespace Siligen
```

### 6.2 命名约定

| 组件类型 | 命名约定 | 示例 |
|---------|---------|------|
| 实体 | 名词，单数 | `Axis`, `Trajectory` |
| 值对象 | 名词，单数 | `Point2D`, `Velocity` |
| 聚合根 | 名词，单数 | `DispensingMachine`, `MotionController` |
| 领域服务 | 动词+er 或 名词+Service | `TrajectoryPlanner`, `CMPTriggerService` |
| 领域事件 | 名词+过去式+Event | `AxisMovedEvent`, `DispensingCompletedEvent` |
| 端口接口 | I+名词+Port | `IPositionControlPort` |

---

## 七、CMakeLists.txt 结构

### 7.1 推荐的 CMake 结构

```cmake
# src/domain/CMakeLists.txt

# Domain 层总库
add_library(domain_core INTERFACE)

# 共享组件
add_subdirectory(shared)

# 子域库
add_subdirectory(motion-control)
add_subdirectory(dispensing)
add_subdirectory(machine)
add_subdirectory(safety)
add_subdirectory(diagnostics)
add_subdirectory(configuration)

# Domain 层依赖
target_link_libraries(domain_core INTERFACE
    shared_types
    domain_shared
    domain_motion_control
    domain_dispensing
    domain_machine
    domain_safety
    domain_diagnostics
    domain_configuration
)
```

### 7.2 子域 CMakeLists.txt 示例

```cmake
# src/domain/motion-control/CMakeLists.txt

# 运动控制子域库
add_library(domain_motion_control STATIC
    # Entities
    entities/Axis.cpp
    entities/Trajectory.cpp

    # Aggregates
    aggregates/MotionController.cpp

    # Domain Services
    domain-services/TrajectoryPlanner.cpp
    domain-services/interpolation/LinearInterpolator.cpp
    domain-services/interpolation/ArcInterpolator.cpp
    domain-services/interpolation/SplineInterpolator.cpp
    domain-services/BufferManager.cpp
)

target_include_directories(domain_motion_control PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(domain_motion_control PUBLIC
    shared_types
    domain_shared
)

# 单元测试
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

---

## 八、最佳实践总结

### 8.1 Domain 层黄金法则

1. **依赖反转**: Domain 层不依赖任何外部层，只依赖 Shared 层
2. **不可变值对象**: 值对象一旦创建不可修改
3. **聚合一致性边界**: 聚合内修改保证原子性和一致性
4. **无状态领域服务**: 领域服务不保存状态，只封装业务逻辑
5. **端口接口分离**: 每个子域定义自己的端口，避免胖接口
6. **Result<T> 错误处理**: 所有可能失败的操作返回 Result<T>
7. **公共 API noexcept**: Domain 公共 API 必须声明 `noexcept`，禁止抛异常
8. **领域事件解耦**: 使用领域事件实现子域间解耦通信
9. **共享类型轻量**: `shared/types` 不直接依赖重型几何/算法库；第三方几何注册与适配统一放在 `shared/Geometry` 适配层

### 8.2 点胶机特定最佳实践

1. **运动控制和点胶工艺分离**: motion-control 负责底层运动，dispensing 负责工艺逻辑
2. **安全规则集中管理**: safety 子域统一管理限位、互锁、紧急停止
3. **配置验证前置**: configuration 子域在应用配置前进行完整验证
4. **状态机清晰**: machine 子域的状态转换规则显式定义在 DispenserModel
5. **触发点值对象化**: 触发点作为不可变值对象，便于验证和缓存
6. **标定流程统一入口**: 标定规则与状态集中在 `Domain::Machine::DomainServices::CalibrationProcess`，应用层仅编排，设备/结果通过端口接入
7. **配方生效统一入口**: 配方生效规则集中在 `Domain::Recipes::DomainServices::RecipeActivationService`，应用层仅编排，审计通过端口写入
7. **点胶流程统一入口**: 点胶执行流程统一由 `Domain::Dispensing::DomainServices::DispensingProcessService` 负责，应用层不得重复流程规则
8. **触发算法统一实现**: 触发点/规划位置触发计算统一由 `DispensingController` 完成，禁止基础设施层重复实现；定时触发仅用于阀门单独控制（调试链路），不参与 DXF 执行
9. **执行参数值对象化**: 运行参数/执行计划使用 `DispensingExecutionTypes` 统一建模，用例仅做请求映射
10. **观察能力边界清晰**: 采样/监控通过 `IDispensingExecutionObserver` 扩展，不得在应用层插入业务规则
11. **插补规则统一入口**: 插补策略、参数校验与插补程序生成必须在 Motion 子域领域服务完成；基础设施仅做硬件 API 调用与错误映射

---

## 九、参考资源

### 9.1 六边形架构参考

- [The Ultimate Guide to Mastering Hexagonal Architecture](https://scalastic.io/en/hexagonal-architecture-domain/)
- [GitHub - Sairyss/domain-driven-hexagon](https://github.com/Sairyss/domain-driven-hexagon)
- [Hexagonal Architecture, DDD, and Spring | Baeldung](https://www.baeldung.com/hexagonal-architecture-ddd-spring)
- [Hexagonal Architecture – What Is It? Why Use It?](https://www.happycoders.eu/software-craftsmanship/hexagonal-architecture/)
- [Hexagonal architecture pattern - AWS Prescriptive Guidance](https://docs.aws.amazon.com/prescriptive-guidance/latest/cloud-design-patterns/hexagonal-architecture.html)

### 9.2 DDD 参考

- [Designing a DDD-oriented microservice - Microsoft Learn](https://learn.microsoft.com/en-us/dotnet/architecture/microservices/microservice-ddd-cqrs-patterns/ddd-oriented-microservice)
- [Domain-Driven Design (DDD) - GeeksforGeeks](https://www.geeksforgeeks.org/system-design/domain-driven-design-ddd/)
- [Best Practice - An Introduction To Domain-Driven Design](https://learn.microsoft.com/en-us/archive/msdn-magazine/2009/february/best-practice-an-introduction-to-domain-driven-design)
- [Domain Driven Design | ABP.IO Documentation](https://abp.io/docs/latest/framework/architecture/domain-driven-design/domain-layer)
- [Domain-Driven Design: Entities, Value Objects and Services](https://dev.to/ielgohary/domain-driven-design-entities-value-objects-and-services-chapter-51-22cm)

### 9.3 运动控制领域参考

- [Enabling high-precision dispensing with advanced motion control | Machine Design](https://www.machinedesign.com/archive/article/21826627/enabling-high-precision-dispensing-with-advanced-motion-control)
- [Motion Scenarios: Dispensing | Machine Design](https://www.machinedesign.com/markets/article/21826768/motion-scenarios-dispensing)
- [Comprehensive Guide to Motion Controller Programming](https://go.aerotech.com/in-motion/comprehensive-guide-to-motion-controller-programming)

---

## 十、下一步行动

### 10.1 立即行动

1. **评审本方案**: 与团队讨论子域划分是否合理
2. **选择试点子域**: 建议从 `dispensing` 子域开始试点
3. **创建 POC**: 为 `DispensingTask` 聚合根创建 POC 实现

### 10.2 中期目标

1. **完成迁移**: 按阶段完成所有子域迁移
2. **单元测试覆盖**: 每个聚合根和领域服务达到 80% 覆盖率
3. **文档完善**: 每个子域有完整的 README 和示例代码

### 10.3 长期目标

1. **领域事件系统**: 实现完整的领域事件发布订阅机制
2. **CQRS 模式**: 引入命令查询职责分离模式
3. **事件溯源**: 为关键聚合引入事件溯源机制

---

**文档版本**: v1.0
**最后更新**: 2026-02-04
**作者**: Claude Code + Web Search + Context7
**审核状态**: 待审核

