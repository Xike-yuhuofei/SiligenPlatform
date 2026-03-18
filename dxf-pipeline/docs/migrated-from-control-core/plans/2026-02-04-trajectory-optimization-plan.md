# 轨迹轴向不单调约束移除 - 工程级优化方案

**文档版本**: 1.0  
**创建日期**: 2026-02-04  
**负责人**: AI Assistant  
**状态**: 草案  

## 1. 项目概述

### 1.1 项目背景
当前系统对DXF轨迹规划设置了"轨迹轴向不单调"的安全约束，限制了复杂几何路径的点胶能力。经技术评估，硬件（博派运动控制卡）具备处理非单调轨迹的能力，约束为上层软件安全策略。

### 1.2 优化目标
移除轨迹轴向不单调约束，确保系统在以下三个方面达到工业级标准：
- **运动控制稳定性**: 复杂轨迹下的平稳运动
- **点胶质量一致性**: 维持胶线均匀性和路径精度  
- **系统安全可靠性**: 建立替代的安全约束机制

### 1.3 技术可行性
- ✅ 硬件支持: 博派运动控制卡具备高级插补功能
- ✅ 算法基础: 现有轨迹规划框架支持复杂路径处理
- ⚠️ 需要优化: 速度规划、点胶补偿、安全监控等关键模块

## 2. 技术实现方案

### 2.1 精确速度规划系统

#### 核心算法
```cpp
class EngineeringVelocityPlanner {
private:
    const double a_lat_max = 2.0;    // 最大离心加速度(m/s²)
    const double a_axial_max = 5.0;  // 最大轴向加速度(m/s²)  
    const double j_max = 50.0;       // 最大加加速度(m/s³)
    
public:
    double calculateSafeVelocity(const TrajectoryPoint& point) {
        // 多约束速度计算
        double v_centrifugal = sqrt(a_lat_max / abs(point.curvature));
        double v_axial = a_axial_max / max(0.1, abs(point.tangential_acceleration));
        double v_jerk = j_max / max(0.1, abs(point.jerk));
        
        return min({v_centrifugal, v_axial, v_jerk, system_max_speed});
    }
};
```

#### 尖点处理策略
- **大角度高速转角(>120°, >50mm/s)**: 停机再加速
- **中等角度(60°-120°)**: 插入过渡圆弧(R=2mm)  
- **小角度(<60°)**: S曲线连续过渡

### 2.2 弧长参数化与曲率平滑

#### 轨迹预处理
- **弧长均匀采样**: 确保运动参数均匀分布
- **样条曲率平滑**: 5点中心差分 + 高斯滤波，避免离散噪声
- **实时参数化**: 支持动态轨迹调整

### 2.3 精确点胶动态补偿

#### 系统辨识模型
```cpp
class DynamicGlueCompensator {
private:
    double time_constant = 50.0;    // 一阶惯性时间常数(ms)
    double pure_delay = 20.0;       // 纯延迟时间(ms)  
    double system_gain = 1.0;       // 系统增益
    
public:
    double computeCompensatedFlow(double target_speed, double current_flow, double dt_ms) {
        // 一阶滞后 + 纯延迟模型
        double delayed_speed = applyTimeDelay(target_speed, pure_delay);
        double flow_demand = system_gain * delayed_speed;
        double alpha = 1 - exp(-dt_ms / time_constant);
        return current_flow + (flow_demand - current_flow) * alpha;
    }
};
```

#### 转角特殊处理
- **尖锐转角**: 提前15ms开始出胶，延后10ms停止
- **平缓转角**: 提前8ms开始出胶，延后5ms停止

### 2.4 实时安全监控系统

#### 多层级安全约束
```cpp
class RealTimeSafetySystem {
private:
    // 硬约束阈值（不可逾越）
    const struct HardConstraints {
        double max_speed = 500.0;       // mm/s
        double max_acceleration = 1000.0; // mm/s²
        double max_jerk = 50000.0;      // mm/s³
        double max_torque = 80.0;       // %额定扭矩
        double max_position_error = 0.1; // mm
    } hard_limits;
    
public:
    SafetyStatus checkHardConstraints(const MotionState& state) {
        if (state.actual_speed > hard_limits.max_speed) 
            return SAFETY_CRITICAL;
        if (abs(state.acceleration) > hard_limits.max_acceleration)
            return SAFETY_CRITICAL;
        return SAFETY_NORMAL;
    }
};
```

#### 三级响应策略
1. **告警级(SAFETY_WARNING)**: 渐进降速30%，100ms过渡
2. **临界级(SAFETY_CRITICAL)**: 紧急停机
3. **正常级(SAFETY_NORMAL)**: 维持操作

## 3. 实施路线图

### 第一阶段：基础框架建设（3周）
**目标**: 建立精确的数学模型和实时控制框架

| 任务 | 时间 | 负责人 | 交付物 | 验收标准 |
|------|------|--------|--------|----------|
| 弧长参数化算法 | 1周 | 开发团队 | ArcLengthParameterization类 | 支持均匀弧长采样 |
| 多约束速度规划 | 1周 | 算法团队 | EngineeringVelocityPlanner类 | 通过极端工况测试 |
| 点胶动态模型标定 | 1周 | 工艺团队 | 系统辨识工具 | 模型误差<5% |

### 第二阶段：实时控制集成（2周）  
**目标**: 集成到现有系统并建立实时监控

| 任务 | 时间 | 负责人 | 交付物 | 验收标准 |
|------|------|--------|--------|----------|
| C++实时线程重构 | 1周 | 系统团队 | 实时控制模块 | 控制周期≤1ms |
| 安全监控系统集成 | 1周 | 安全团队 | RealTimeSafetySystem类 | 响应延迟≤1ms |

### 第三阶段：工艺优化验证（2周）
**目标**: 参数调优和工艺质量验证

| 任务 | 时间 | 负责人 | 交付物 | 验收标准 |
|------|------|--------|--------|----------|
| 极端工况测试 | 1周 | 测试团队 | 测试报告 | 无系统崩溃 |
| 工艺质量验证 | 1周 | 质量团队 | 质量评估报告 | 胶线均匀性CV≤5% |

## 4. 关键技术指标（KPI）

### 4.1 运动控制性能指标
- **速度平滑度**: 加加速度≤50,000 mm/s³
- **路径精度**: 跟随误差≤0.1mm
- **稳定性**: 无机械共振和过冲现象

### 4.2 点胶质量指标  
- **胶线均匀性**: 宽度变异系数≤5%
- **转角质量**: 无堆胶、断胶现象
- **路径一致性**: 实际路径与设计路径偏差≤0.2mm

### 4.3 系统安全指标
- **响应时间**: 安全监控延迟≤1ms
- **故障处理**: 异常情况下安全停机时间≤10ms
- **可靠性**: 连续运行无故障时间≥1000小时

## 5. 风险控制策略

### 5.1 技术风险控制
1. **渐进式实施**: 分模块替换，保留原有算法备份
2. **充分测试**: 建立完整测试用例库，覆盖极端工况
3. **参数可调**: 所有关键参数外部可配置，便于现场调优

### 5.2 安全风险控制  
1. **多重保护**: 硬件限位 + 软件约束 + 实时监控
2. **紧急处理**: 三级响应机制，确保异常安全停机
3. **操作培训**: 为操作人员提供新安全操作规程培训

### 5.3 回滚策略
- 保留原有轴向单调性检查作为可选配置项
- 建立A/B测试框架，可快速切换算法版本
- 完整日志记录和故障诊断工具

## 6. 资源需求

### 6.1 人力资源
- **算法工程师**: 2人（速度规划、点胶补偿）
- **系统工程师**: 1人（实时控制集成）
- **测试工程师**: 1人（极端工况测试）
- **工艺工程师**: 1人（质量验证）

### 6.2 设备资源
- **测试平台**: 博派运动控制卡 + 点胶设备
- **测量仪器**: 高速相机、激光测距仪、流量计
- **开发环境**: Visual Studio, CMake, 版本控制系统

### 6.3 时间资源
- **总工期**: 7周
- **关键路径**: 基础框架建设(3周) → 实时控制集成(2周) → 工艺验证(2周)

## 7. 预期效益

### 7.1 技术效益
- **工艺能力提升**: 支持更复杂几何路径点胶
- **质量一致性**: 提高点胶工艺稳定性和重复性  
- **系统智能化**: 实现自适应控制和实时优化

### 7.2 经济效益
- **生产效率**: 减少因路径限制导致的生产中断
- **质量成本**: 降低不良品率和返工成本
- **维护成本**: 通过预防性维护减少设备故障

## 8. 后续工作建议

### 8.1 短期优化（3-6个月）
- 机器学习优化运动参数
- 自适应工艺参数调整
- 云端数据分析和预测维护

### 8.2 长期规划（6-12个月）
- 多设备协同控制
- 智能路径优化算法
- 数字孪生系统集成

## 附录

### A. 参考文献
1. 博派运动控制卡技术文档V7.3
2. 实时运动控制算法研究
3. 点胶工艺质量控制标准

### B. 相关代码文件
- `src/application/usecases/dispensing/dxf/DXFDispensingPlanner.cpp`
- `src/infrastructure/adapters/motion/MultiCardAdapter.cpp`
- `src/domain/trajectory/domain-services/PathRegularizer.cpp`

### C. 测试用例清单
1. 极端曲率轨迹测试
2. 尖锐转角处理测试  
3. 高速方向变化测试
4. 长时间运行稳定性测试

---

**文档修订记录**

| 版本 | 日期 | 修改内容 | 修改人 |
|------|------|----------|--------|
| 1.0 | 2026-02-04 | 初始版本创建 | AI Assistant |