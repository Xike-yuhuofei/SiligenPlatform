# Ruckig 集成技术可行性报告

**日期**: 2026-01-29
**阶段**: 阶段 0 - 原型验证
**状态**: 完成
**作者**: Claude Code

---

## 一、执行摘要

### 结论：技术可行，建议实施

| 评估项 | 结果 | 说明 |
|-------|------|------|
| **C++ 版本兼容性** | 通过 | Ruckig 0.9.2 支持 C++17，与项目兼容 |
| **编译器支持** | 通过 | Clang 19.1.0 完全支持 |
| **CMake 配置** | 通过 | 已配置 add_subdirectory，库会被编译 |
| **编译验证** | 通过 | ruckig.dll (145KB) + ruckig.lib 生成成功 |
| **代码集成** | 待完成 | 需要编写适配器并链接 ruckig 目标 |
| **架构兼容性** | 通过 | 可通过适配器模式无缝集成 |
| **功能满足度** | 通过 | 核心功能满足"曲线胶点均匀化"需求 |

---

## 二、调研发现

### 2.1 Ruckig 库现状

| 项目 | 发现 |
|-----|------|
| **库版本** | 0.9.2 (位于 `third_party/ruckig/`) |
| **C++ 标准** | C++17 (`target_compile_features(ruckig PUBLIC cxx_std_17)`) |
| **许可证** | MIT (商业友好) |
| **最新版本** | 0.15.5 (需要 C++20) |
| **CMake 状态** | 已配置 `add_subdirectory`，库会被编译 |
| **代码使用** | 无 (src/ 中没有任何文件引用 ruckig) |
| **链接状态** | 无 (没有目标 `target_link_libraries` ruckig) |

### 2.2 关键 API

```cpp
// 核心类
ruckig::Ruckig<DOFs>           // 轨迹生成器
ruckig::InputParameter<DOFs>   // 输入参数
ruckig::OutputParameter<DOFs>  // 输出参数
ruckig::Trajectory<DOFs>       // 轨迹对象

// 关键方法
otg.calculate(input, trajectory)           // 离线计算
trajectory.at_time(t, pos, vel, acc)       // 任意时刻采样
trajectory.get_duration()                  // 获取总时长
```

### 2.3 编译环境

| 项目 | 值 |
|-----|-----|
| **编译器** | Clang-CL 19.1.0 |
| **链接器** | LLD |
| **C++ 标准** | C++17 |
| **CMake** | >= 3.16 |

---

## 三、架构集成方案

### 3.1 六边形架构适配

```
┌─────────────────────────────────────────────────────────────┐
│  Domain Layer (不修改)                                       │
│  └── IVelocityProfilePort                                   │
│      └── GenerateProfile(distance, config) → VelocityProfile│
├─────────────────────────────────────────────────────────────┤
│  Infrastructure Layer (新增适配器)                           │
│  └── RuckigVelocityProfileAdapter                           │
│      └── 封装 ruckig::Ruckig<1> 调用                        │
├─────────────────────────────────────────────────────────────┤
│  Third-party (已存在)                                        │
│  └── third_party/ruckig/                                    │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 文件结构

```
src/infrastructure/adapters/motion/velocity/
├── SevenSegmentSCurveAdapter.h      # 现有实现
├── SevenSegmentSCurveAdapter.cpp
├── RuckigVelocityProfileAdapter.h   # 新增
└── RuckigVelocityProfileAdapter.cpp # 新增
```

### 3.3 工厂注册

```cpp
// InfrastructureAdapterFactory.h
std::shared_ptr<IVelocityProfilePort> CreateRuckigAdapter();

// 或通过配置选择
// machine_config.ini
// [VelocityProfile]
// provider = ruckig  # 或 seven_segment
```

---

## 四、解决"曲线胶点不均匀"问题

### 4.1 问题根因

```
等时间间隔触发:
时间:  t0 ─── t1 ─── t2 ─── t3 ─── t4
速度:  低     高     高     高     低
位置:  ├─┤   ├────┤ ├────┤ ├────┤ ├─┤
       短     长     长     长     短

结果: 加减速段胶点密集，匀速段胶点稀疏
```

### 4.2 解决方案

```
等距离间隔触发:
位置:  p0 ─── p1 ─── p2 ─── p3 ─── p4  (均匀)
时间:  t0 ─ t1 ──── t2 ──── t3 ─ t4    (非均匀)
       短   长      长      长   短

算法: 使用 trajectory.at_time() 反向计算
      对于目标位置 p_i，二分搜索找到对应时间 t_i
```

### 4.3 实现要点

```cpp
// 等距触发点计算
std::vector<double> CalculateEquidistantTriggerTimes(
    const ruckig::Trajectory<1>& trajectory,
    double total_distance,
    int num_points) {

    std::vector<double> trigger_times;
    double interval = total_distance / num_points;
    double duration = trajectory.get_duration();

    for (int i = 0; i <= num_points; ++i) {
        double target_pos = i * interval;
        // 二分搜索找到 position(t) ≈ target_pos 的时间 t
        double t = BinarySearchTime(trajectory, target_pos, 0, duration);
        trigger_times.push_back(t);
    }

    return trigger_times;
}
```

---

## 五、风险评估

| 风险 | 等级 | 缓解措施 |
|-----|------|---------|
| Ruckig 版本较旧 | 低 | 核心功能完备，暂不升级 |
| 与 MultiCard CMP 兼容性 | 中 | 需要实机验证 |
| 二分搜索精度 | 低 | 设置合理的收敛阈值 |
| 性能开销 | 低 | 离线计算，不影响实时性 |

---

## 六、下一步行动

### 阶段 1 任务清单

1. **创建 RuckigVelocityProfileAdapter**
   - 实现 `IVelocityProfilePort` 接口
   - 封装 Ruckig 调用
   - **链接 ruckig 库** (`target_link_libraries(... ruckig::ruckig)`)

2. **更新 CMakeLists.txt**
   - 在适配器目标中添加 ruckig 依赖
   - 添加 include 路径

3. **实现等距触发点计算**
   - 添加 `CalculateEquidistantTriggerTimes()` 方法
   - 集成到 `TrajectoryInterpolator`

4. **更新工厂类**
   - 在 `InfrastructureAdapterFactory` 中注册新适配器

5. **编写单元测试**
   - 验证轨迹生成正确性
   - 验证触发点均匀性

### 预估工作量

| 任务 | 工作量 |
|-----|--------|
| 适配器实现 | 2-3 天 |
| 等距触发算法 | 1-2 天 |
| 单元测试 | 1-2 天 |
| 集成测试 | 1 天 |
| **总计** | **5-8 天** |

---

## 七、参考资料

- Ruckig 源码: `third_party/ruckig/`
- 现有适配器: `src/infrastructure/adapters/motion/velocity/SevenSegmentSCurveAdapter.h`
- Domain Port: `src/domain/motion/ports/IVelocityProfilePort.h`
- 工厂类: `src/infrastructure/factories/InfrastructureAdapterFactory.h`

---

**报告结束**
