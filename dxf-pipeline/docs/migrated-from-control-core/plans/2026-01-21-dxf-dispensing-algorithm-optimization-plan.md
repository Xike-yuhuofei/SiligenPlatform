# DXF点胶路径规划算法优化方案

## 文档信息

| 项目 | 值 |
|------|-----|
| 创建日期 | 2026-01-21 |
| 状态 | DRAFT |
| 作者 | Claude AI |
| 审核人 | 待定 |

---

## 一、背景与目标

### 1.1 当前状态

基于专业点胶机工程师视角的代码审计，当前DXF点胶路径规划算法综合评分为 **90/100**，已符合行业基本最佳实践。

### 1.2 优化目标

| 优化阶段 | 目标 | 预期收益 |
|----------|------|----------|
| 短期 | 速度规划升级 | 运动平滑度提升30% |
| 中期 | 硬件触发升级 | 触发精度提升10倍 |
| 长期 | 路径算法升级 | 空移距离减少15% |

---

## 二、短期优化方案（1-2周）

### 2.1 7段S曲线速度规划

**优先级**: P0 - 高

**问题描述**:
当前实现使用3段梯形速度曲线（加速-匀速-减速），缺少Jerk控制，高速运动时可能产生振动。

**当前实现位置**:
- `src/domain/trajectory/domain-services/MotionPlanner.cpp`

**目标架构**:

```
┌─────────────────────────────────────────────────────────────┐
│                    Shared Layer                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ TrajectoryConfig (已有max_jerk字段)                  │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Domain Layer                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ IVelocityProfilePort (新增端口)                      │    │
│  │   - GenerateProfile(distance, config) -> Profile    │    │
│  │   - GetProfileType() -> ProfileType                 │    │
│  └─────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ VelocityProfileService (新增领域服务)                │    │
│  │   - 7段S曲线计算逻辑                                 │    │
│  │   - 短距离自动降级为5段/3段                          │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                 Infrastructure Layer                         │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ SevenSegmentSCurveAdapter (新增适配器)               │    │
│  │   - 实现IVelocityProfilePort                        │    │
│  │   - 封装7段S曲线数学计算                             │    │
│  └─────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ MotionPlanner (修改)                                 │    │
│  │   - 注入IVelocityProfilePort                        │    │
│  │   - 替换GenerateSShapeVelocity调用                  │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

**实施步骤**:

| 步骤 | 文件 | 操作 | 规则约束 |
|------|------|------|----------|
| 1 | `src/domain/motion/ports/IVelocityProfilePort.h` | 新建 | PORT_NAMING_CONVENTION |
| 2 | `src/domain/motion/domain-services/VelocityProfileService.h/cpp` | 新建 | DOMAIN_PUBLIC_API_NOEXCEPT |
| 3 | `src/infrastructure/adapters/motion/velocity/SevenSegmentSCurveAdapter.h/cpp` | 新建 | ADAPTER_SINGLE_PORT |
| 4 | `src/domain/trajectory/domain-services/MotionPlanner.h/cpp` | 修改 | HEXAGONAL_PORT_ACCESS |
| 5 | `src/application/container/DIContainer.cpp` | 修改 | 注册新适配器 |

**7段S曲线算法规格**:

```
阶段1: 加加速段 (Jerk = +J_max)
阶段2: 匀加速段 (Jerk = 0, Acc = A_max)
阶段3: 减加速段 (Jerk = -J_max)
阶段4: 匀速段   (Jerk = 0, Acc = 0, Vel = V_max)
阶段5: 加减速段 (Jerk = -J_max)
阶段6: 匀减速段 (Jerk = 0, Acc = -A_max)
阶段7: 减减速段 (Jerk = +J_max)
```

**规则合规性检查**:

- [x] DOMAIN_NO_DYNAMIC_MEMORY: 使用固定大小数组存储7段参数
- [x] DOMAIN_PUBLIC_API_NOEXCEPT: 所有公共方法标记noexcept
- [x] HEXAGONAL_DEPENDENCY_DIRECTION: Domain不依赖Infrastructure
- [x] INDUSTRIAL_REALTIME_SAFETY: 无动态分配，有界执行时间

**验证标准**:

```bash
# 架构验证
scripts/analysis/arch-validate.ps1

# 单元测试
ctest -R VelocityProfile

# 性能测试
# 7段S曲线计算时间 < 1ms (1000点轨迹)
```

---

### 2.2 Jerk参数配置化

**优先级**: P1 - 中高

**问题描述**:
`TrajectoryConfig.max_jerk` 字段已存在但未被使用。

**当前状态**:
- `src/shared/types/TrajectoryTypes.h:24` 已定义 `max_jerk = 5000.0f`
- `MotionPlanner` 速度规划逻辑未使用此参数

**实施步骤**:

| 步骤 | 文件 | 操作 |
|------|------|------|
| 1 | `src/domain/trajectory/domain-services/MotionPlanner.cpp` | 修改速度规划使用config.max_jerk |
| 2 | `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.h` | 添加JERK常量 |

---

## 三、中期优化方案（2-4周）

### 3.1 CMP硬件位置触发

**优先级**: P0 - 高

**问题描述**:
当前MVP使用定时触发（100ms间隔），精度受限于软件定时器。CMP硬件触发可实现微秒级精度。

**当前实现位置**:
- `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.h:50` - 定时触发常量
- `src/domain/dispensing/ports/ITriggerControllerPort.h` - 已定义CMP接口

**目标架构**:

```
┌─────────────────────────────────────────────────────────────┐
│                    Domain Layer                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ ITriggerControllerPort (已有)                        │    │
│  │   - SetContinuousTrigger() ← 使用此方法              │    │
│  └─────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ CMPTriggerService (已有)                             │    │
│  │   - 添加 ConfigureForSegment() 方法                  │    │
│  │   - 添加 CalculateOptimalTriggerAxis() 方法         │    │
│  └─────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ ArcTriggerPointCalculator (已有)                     │    │
│  │   - 圆弧触发点计算已实现                             │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                 Application Layer                            │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ DXFDispensingExecutionUseCase (修改)                 │    │
│  │   - 添加 use_hardware_trigger 配置                   │    │
│  │   - ExecuteSingleLineSegment: 调用CMP配置           │    │
│  │   - ExecuteSingleArcSegment: 调用圆弧触发计算       │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

**实施步骤**:

| 步骤 | 文件 | 操作 | 规则约束 |
|------|------|------|----------|
| 1 | `src/domain/dispensing/domain-services/CMPTriggerService.h/cpp` | 添加ConfigureForSegment方法 | DOMAIN_PUBLIC_API_NOEXCEPT |
| 2 | `src/application/usecases/dispensing/dxf/DXFDispensingMVPRequest` | 添加use_hardware_trigger字段 | APPLICATION_REQUEST_VALIDATION |
| 3 | `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.cpp` | 修改ExecuteSingleLineSegment | INDUSTRIAL_CMP_TEST_SAFETY |
| 4 | `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.cpp` | 修改ExecuteSingleArcSegment | INDUSTRIAL_CMP_TEST_SAFETY |

**触发模式选择逻辑**:

```
IF use_hardware_trigger AND hardware_supports_cmp THEN
    IF segment.type == LINE THEN
        使用 SetContinuousTrigger(axis, start, end, interval)
    ELSE IF segment.type == ARC THEN
        使用 ArcTriggerPointCalculator.Calculate()
        使用 SetRangeTrigger() 或多点触发
    END IF
ELSE
    使用现有定时触发方案
END IF
```

**规则合规性检查**:

- [x] INDUSTRIAL_CMP_TEST_SAFETY: 验证位置范围、输出极性、卡连接
- [x] INDUSTRIAL_VALVE_SAFETY: 跟踪阀门状态、错误时关闭
- [x] HEXAGONAL_PORT_ACCESS: 通过ITriggerControllerPort访问

**验证标准**:

```bash
# CMP触发测试
ctest -R CMPTrigger

# 硬件集成测试（需要实际硬件）
# 触发位置误差 < 0.01mm
```

---

### 3.2 大型DXF文件空间索引

**优先级**: P2 - 中

**问题描述**:
当前最近邻算法为O(n²)，对于超过500段的DXF文件性能下降明显。

**目标架构**:

```
┌─────────────────────────────────────────────────────────────┐
│                    Domain Layer                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ ISpatialIndexPort (新增端口)                         │    │
│  │   - Build(segments) -> void                         │    │
│  │   - FindNearest(point, k) -> vector<SegmentRef>     │    │
│  │   - FindInRadius(point, radius) -> vector<SegmentRef>│   │
│  └─────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ PathOptimizationStrategy (修改)                      │    │
│  │   - 注入ISpatialIndexPort                           │    │
│  │   - 段数>阈值时使用空间索引                          │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                 Infrastructure Layer                         │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ RTreeSpatialIndexAdapter (新增适配器)                │    │
│  │   - 实现ISpatialIndexPort                           │    │
│  │   - 使用boost::geometry::index::rtree              │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

**实施步骤**:

| 步骤 | 文件 | 操作 |
|------|------|------|
| 1 | `src/domain/planning/ports/ISpatialIndexPort.h` | 新建 |
| 2 | `src/infrastructure/adapters/planning/spatial/RTreeSpatialIndexAdapter.h/cpp` | 新建 |
| 3 | `src/domain/dispensing/domain-services/PathOptimizationStrategy.h/cpp` | 修改 |
| 4 | `CMakeLists.txt` | 添加Boost.Geometry依赖 |

**性能目标**:

| 段数量 | 当前耗时 | 优化后耗时 |
|--------|----------|------------|
| 100 | 10ms | 10ms |
| 500 | 250ms | 50ms |
| 1000 | 1000ms | 100ms |
| 5000 | 25000ms | 500ms |

---

## 四、长期优化方案（1-2月）

### 4.1 LKH路径优化算法

**优先级**: P3 - 低

**问题描述**:
当前2-Opt算法对于复杂路径的优化效果有限。LKH（Lin-Kernighan-Helsgaun）算法可进一步减少空移距离。

**目标架构**:

```
┌─────────────────────────────────────────────────────────────┐
│                    Domain Layer                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ IPathOptimizerPort (新增端口)                        │    │
│  │   - Optimize(segments, config) -> OptimizedPath     │    │
│  │   - GetAlgorithmType() -> AlgorithmType             │    │
│  └─────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ PathOptimizationConfig (新增配置)                    │    │
│  │   - algorithm: NEAREST_NEIGHBOR | TWO_OPT | LKH     │    │
│  │   - max_iterations: int                             │    │
│  │   - time_limit_ms: int                              │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                 Infrastructure Layer                         │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ NearestNeighborAdapter (重构现有代码)                │    │
│  │   - 实现IPathOptimizerPort                          │    │
│  └─────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ TwoOptAdapter (重构现有代码)                         │    │
│  │   - 实现IPathOptimizerPort                          │    │
│  └─────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ LKHAdapter (新增适配器)                              │    │
│  │   - 实现IPathOptimizerPort                          │    │
│  │   - 集成LKH-3库或自实现                             │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

**实施步骤**:

| 步骤 | 文件 | 操作 |
|------|------|------|
| 1 | `src/domain/planning/ports/IPathOptimizerPort.h` | 新建 |
| 2 | `src/domain/planning/PathOptimizationConfig.h` | 新建 |
| 3 | `src/infrastructure/adapters/planning/optimizer/NearestNeighborAdapter.h/cpp` | 重构 |
| 4 | `src/infrastructure/adapters/planning/optimizer/TwoOptAdapter.h/cpp` | 重构 |
| 5 | `src/infrastructure/adapters/planning/optimizer/LKHAdapter.h/cpp` | 新建 |
| 6 | `src/domain/dispensing/domain-services/PathOptimizationStrategy.h/cpp` | 修改为使用端口 |

**LKH算法特点**:

- 时间复杂度: O(n^2.2) 平均情况
- 空间复杂度: O(n)
- 优化效果: 比2-Opt提升10-15%

---

### 4.2 胶水粘度自适应参数

**优先级**: P3 - 低

**问题描述**:
当前速度和触发参数为硬编码，不同粘度胶水需要不同参数。

**目标架构**:

```
┌─────────────────────────────────────────────────────────────┐
│                    Domain Layer                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ GlueProfile (新增值对象)                             │    │
│  │   - viscosity_cps: float32                          │    │
│  │   - recommended_velocity: float32                   │    │
│  │   - recommended_trigger_interval: float32           │    │
│  │   - temperature_compensation: bool                  │    │
│  └─────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ GlueParameterService (新增领域服务)                  │    │
│  │   - CalculateOptimalVelocity(profile) -> float32    │    │
│  │   - CalculateTriggerInterval(profile) -> float32    │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

**实施步骤**:

| 步骤 | 文件 | 操作 |
|------|------|------|
| 1 | `src/domain/dispensing/value-objects/GlueProfile.h` | 新建 |
| 2 | `src/domain/dispensing/domain-services/GlueParameterService.h/cpp` | 新建 |
| 3 | `src/application/usecases/dispensing/dxf/DXFDispensingMVPRequest` | 添加glue_profile字段 |
| 4 | `config/glue_profiles.json` | 新建预设配置 |

---

## 五、实施优先级矩阵

```
                    影响度
                    高 │ ┌─────────────┐ ┌─────────────┐
                      │ │ 7段S曲线    │ │ CMP硬件触发 │
                      │ │ (短期P0)    │ │ (中期P0)    │
                      │ └─────────────┘ └─────────────┘
                      │
                      │ ┌─────────────┐ ┌─────────────┐
                      │ │ Jerk配置化  │ │ 空间索引    │
                      │ │ (短期P1)    │ │ (中期P2)    │
                      │ └─────────────┘ └─────────────┘
                      │
                    低 │ ┌─────────────┐ ┌─────────────┐
                      │ │ LKH算法     │ │ 粘度自适应  │
                      │ │ (长期P3)    │ │ (长期P3)    │
                      │ └─────────────┘ └─────────────┘
                      └───────────────────────────────────
                        低                            高
                                    紧迫度
```

---

## 六、风险评估

| 风险项 | 可能性 | 影响 | 缓解措施 |
|--------|--------|------|----------|
| 7段S曲线计算性能不足 | 低 | 高 | 预计算查表、SIMD优化 |
| CMP硬件兼容性问题 | 中 | 高 | 保留定时触发回退方案 |
| LKH算法集成复杂 | 高 | 低 | 可选功能，不影响核心流程 |
| 空间索引内存占用 | 低 | 中 | 仅大文件启用，可配置阈值 |

---

## 七、验收标准

### 7.1 短期优化验收

- [ ] 7段S曲线单元测试通过
- [ ] 速度曲线连续性验证（无速度突变）
- [ ] 加速度曲线连续性验证（无加速度突变）
- [ ] 性能测试：1000点轨迹计算 < 1ms

### 7.2 中期优化验收

- [ ] CMP触发集成测试通过
- [ ] 触发位置误差 < 0.01mm
- [ ] 空间索引性能测试：1000段 < 100ms
- [ ] 向后兼容：定时触发仍可用

### 7.3 长期优化验收

- [ ] LKH算法单元测试通过
- [ ] 路径优化效果：比2-Opt提升 > 10%
- [ ] 胶水配置文件加载测试通过

---

## 八、附录

### A. 相关规则文件

- `.claude/rules/HEXAGONAL.md` - 六边形架构规则
- `.claude/rules/DOMAIN.md` - 领域层规则
- `.claude/rules/INDUSTRIAL.md` - 工业安全规则
- `.claude/rules/PORTS_ADAPTERS.md` - 端口适配器规则
- `.claude/rules/REALTIME.md` - 实时性规则

### B. 相关代码文件

- `src/domain/dispensing/domain-services/PathOptimizationStrategy.h/cpp`
- `src/domain/trajectory/domain-services/MotionPlanner.h/cpp`
- `src/domain/dispensing/domain-services/ArcTriggerPointCalculator.h/cpp`
- `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.h/cpp`

### C. 参考资料

- 7段S曲线算法: "Trajectory Planning for Automatic Machines and Robots" - Biagiotti & Melchiorri
- LKH算法: http://webhotel4.ruc.dk/~keld/research/LKH-3/
- R-tree空间索引: Boost.Geometry文档
