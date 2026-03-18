# 点胶机轨迹算法行业最佳实践分析报告

**日期**: 2026-01-29
**作者**: Claude Code
**状态**: 待确认方案

---

## 一、当前代码库分析

### 1.1 现有实现

| 组件 | 位置 | 功能 |
|-----|------|-----|
| `IVelocityProfilePort` | `src/domain/motion/ports/` | 速度曲线端口接口 |
| `VelocityProfileService` | `src/domain/motion/domain-services/` | 速度规划领域服务 |
| `GeometryNormalizer / ProcessAnnotator / TrajectoryShaper / MotionPlanner` | `src/domain/trajectory/domain-services/` | 几何规范化、工艺语义标注、轨迹连续化与运动规划 |
| `TrajectoryPlanner` | `src/domain/motion/domain-services/` | CMP协调轨迹规划 |

### 1.2 已支持的速度曲线类型

```cpp
enum class VelocityProfileType {
    TRAPEZOIDAL,      // 3段梯形曲线
    S_CURVE_5_SEG,    // 5段S曲线
    S_CURVE_7_SEG     // 7段S曲线
};
```

### 1.3 架构特点

- 六边形架构：Domain层定义Port接口，Infrastructure层实现适配器
- 已有短距离自动降级逻辑 (`CalculateMinDistanceFor7Seg`)
- 支持CMP协调轨迹和混合触发时间线

---

## 二、行业最佳实践概述

根据行业文献和搜索结果，点胶机轨迹算法的核心技术包括：

| 技术领域 | 行业标准 | 关键指标 |
|---------|---------|---------|
| 速度规划 | 7段S曲线 | Jerk限制、平滑过渡 |
| 路径插补 | 多轴协调插补 | 位置精度 +/-0.01mm |
| 触发控制 | 位置/时间混合触发 | 触发延迟 <1ms |
| 前瞻规划 | Look-ahead算法 | 连续路径速度优化 |

---

## 三、备选方案详细分析

### 方案 A：增强现有梯形/S曲线实现

**描述**：在现有 `IVelocityProfilePort` 基础上，完善7段S曲线的实现细节，优化参数计算。

**实现要点**：
- 完善 `S_CURVE_7_SEG` 的7个阶段计算（加加速、匀加速、减加速、匀速、加减速、匀减速、减减速）
- 添加短距离自动降级逻辑（已有 `CalculateMinDistanceFor7Seg`）
- 优化时间步长自适应

**利弊分析**：

| 优点 | 缺点 |
|-----|-----|
| 改动最小，风险低 | 无法解决连续路径速度突变问题 |
| 复用现有架构 | 不支持前瞻规划 |
| 易于测试验证 | 拐角处仍需停顿 |

**风险**：
- 低风险：主要是参数调优工作
- 可能需要调整现有测试用例

**适用场景**：
- 简单的点到点运动
- 对连续性要求不高的应用
- 快速迭代需求

---

### 方案 B：实现完整的时间最优7段S曲线算法

**描述**：基于学术论文实现完整的时间最优S曲线算法。

**实现要点**：

```
7段S曲线数学模型：
┌─────────────────────────────────────────────────┐
│  Phase 1: 加加速 (Jerk = +J_max)                │
│  Phase 2: 匀加速 (Jerk = 0, Acc = A_max)        │
│  Phase 3: 减加速 (Jerk = -J_max)                │
│  Phase 4: 匀速   (Jerk = 0, Acc = 0)            │
│  Phase 5: 加减速 (Jerk = -J_max)                │
│  Phase 6: 匀减速 (Jerk = 0, Acc = -A_max)       │
│  Phase 7: 减减速 (Jerk = +J_max)                │
└─────────────────────────────────────────────────┘
```

**利弊分析**：

| 优点 | 缺点 |
|-----|-----|
| 时间最优，效率最高 | 实现复杂度高 |
| 支持非对称加减速 | 需要大量数学推导 |
| 工业级标准方案 | 边界条件处理复杂 |
| 减少机械振动 | 调试周期较长 |

**风险**：
- 中等风险：数学推导和边界条件处理需要仔细验证
- 需要完整的单元测试覆盖

**适用场景**：
- 高精度点胶应用
- 对运动平滑性要求高
- 需要减少机械磨损

**参考资源**：
- [A New Seven-Segment Profile Algorithm](https://www.mdpi.com/2079-9292/8/6/652)
- [Mathematics for Real-Time S-Curve Profile Generator](https://www.researchgate.net/publication/348383330_Mathematics_for_Real-Time_S-Curve_Profile_Generator)
- [Time-Optimal Asymmetric S-Curve Trajectory Planning](https://pmc.ncbi.nlm.nih.gov/articles/PMC10052581/)

---

### 方案 C：前瞻性速度规划（Look-ahead）

**描述**：实现前瞻算法，在连续路径段之间进行速度优化，避免不必要的减速停顿。

**实现要点**：

```
Look-ahead 算法流程：
┌─────────────────────────────────────────────────┐
│ 1. 读取未来 N 个路径段                          │
│ 2. 计算各段的最大允许速度（基于曲率、加速度）   │
│ 3. 反向传播：从终点向起点计算减速约束           │
│ 4. 正向传播：从起点向终点计算加速约束           │
│ 5. 取两者最小值作为实际速度                     │
└─────────────────────────────────────────────────┘
```

**利弊分析**：

| 优点 | 缺点 |
|-----|-----|
| 连续路径无停顿 | 实现复杂度最高 |
| 整体效率最优 | 需要缓冲区管理 |
| 支持复杂轨迹 | 实时性要求高 |
| 工业CNC标准方案 | 调试难度大 |

**风险**：
- 高风险：需要重构现有轨迹执行流程
- 实时性要求可能影响系统稳定性
- 需要与硬件CMP缓冲区协调

**适用场景**：
- 复杂DXF图形点胶
- 连续曲线路径
- 高速点胶应用

**参考资源**：
- [LicOS PLC/PAC G-code Interpolation Motion Control](https://www.licosplc.com/news/21.html)
- [ABB Automatic Path Planning](https://library.e.abb.com/public/a9fa67be082d4411be3e5fa6d5c52303/3HAC092826%20AM%20Automatic%20Path%20Planning-en.pdf)

---

### 方案 D：AI辅助路径优化

**描述**：引入机器学习方法优化点胶路径和参数。

**实现要点**：
- 使用神经网络预测最优路径顺序
- 基于历史数据优化速度参数
- 自适应触发间隔调整

**利弊分析**：

| 优点 | 缺点 |
|-----|-----|
| 自适应优化 | 需要训练数据 |
| 可处理复杂场景 | 引入额外依赖 |
| 前沿技术方向 | 可解释性差 |
| 持续改进能力 | 部署复杂度高 |

**风险**：
- 高风险：技术成熟度不足
- 需要大量实验数据
- 可能影响系统确定性

**适用场景**：
- 研发阶段探索
- 复杂覆盖路径优化
- 长期技术储备

**参考资源**：
- [Rapid AI-based generation of coverage paths for dispensing applications](https://arxiv.org/html/2505.03560v1)

---

### 方案 E：集成第三方库（推荐）

**描述**：集成成熟的开源轨迹生成库，通过适配器模式接入现有架构。

#### E.1 第三方库对比

| 库名称 | 许可证 | 语言 | 维护状态 | 特点 | 推荐度 |
|-------|--------|------|---------|------|--------|
| **[Ruckig](https://github.com/pantor/ruckig)** | MIT | C++20 | 活跃 | 时间最优、Jerk约束、工业级 | ★★★★★ |
| **[Reflexxes Type II](https://github.com/Reflexxes/RMLTypeII)** | LGPL-3.0 | C++ | 停止(2015) | 经典方案、实时性好 | ★★★☆☆ |
| **[scurve_construct](https://github.com/grotius-cnc/scurve_construct)** | - | C/C++ | 活跃 | 轻量级、性能优秀 | ★★★☆☆ |
| **[MotionProfileGenerator](https://github.com/WRidder/MotionProfileGenerator)** | MIT | C++/Matlab | 停止 | 简单、教学用途 | ★★☆☆☆ |
| **[OROCOS KDL](https://github.com/orocos/orocos_kinematics_dynamics)** | LGPL | C++ | 活跃 | 机器人运动学、ROS集成 | ★★★☆☆ |

#### E.2 重点推荐：Ruckig 库

**核心特性**：

```
┌─────────────────────────────────────────────────────────────┐
│  Ruckig - Motion Generation for Robots and Machines        │
│  "Real-time. Jerk-constrained. Time-optimal."              │
├─────────────────────────────────────────────────────────────┤
│  - 任意初始状态到目标状态的时间最优轨迹                     │
│  - 支持位置、速度、加速度约束                               │
│  - 支持 Jerk (加加速度) 约束                                │
│  - 支持航点跟踪 (Waypoint Following)                        │
│  - 无外部依赖                                               │
│  - 50亿+随机轨迹测试验证                                    │
│  - 被 MoveIt 2、CoppeliaSim 等主流项目采用                  │
└─────────────────────────────────────────────────────────────┘
```

**技术规格**：

| 项目 | 规格 |
|-----|------|
| **语言标准** | C++20 |
| **外部依赖** | 无（核心库） |
| **可选依赖** | Eigen 3.4+（向量接口）、Nanobind 2.7（Python绑定） |
| **支持平台** | Linux、macOS、Windows |
| **许可证** | MIT（商业友好） |
| **数值精度** | 位置/速度 1e-8，加速度 1e-10，约束违反 1e-12 |

**API 示例**：

```cpp
#include <ruckig/ruckig.hpp>

// 3自由度，1ms控制周期
Ruckig<3> otg {0.001};
InputParameter<3> input;
OutputParameter<3> output;

// 设置当前状态
input.current_position = {0.0, 0.0, 0.0};
input.current_velocity = {0.0, 0.0, 0.0};
input.current_acceleration = {0.0, 0.0, 0.0};

// 设置目标状态
input.target_position = {100.0, 50.0, 0.0};
input.target_velocity = {0.0, 0.0, 0.0};

// 设置约束
input.max_velocity = {100.0, 100.0, 100.0};
input.max_acceleration = {500.0, 500.0, 500.0};
input.max_jerk = {2000.0, 2000.0, 2000.0};

// 实时控制循环
while (otg.update(input, output) == Result::Working) {
    // 使用 output.new_position, output.new_velocity 等
    output.pass_to_input(input);  // 反馈到下一周期
}
```

#### E.3 与现有架构的集成方案

```
┌─────────────────────────────────────────────────────────────┐
│                    六边形架构集成                            │
├─────────────────────────────────────────────────────────────┤
│  Domain Layer                                               │
│  └── IVelocityProfilePort (现有接口，无需修改)              │
│                                                             │
│  Infrastructure Layer                                       │
│  └── RuckigVelocityProfileAdapter (新增适配器)              │
│      ├── 封装 Ruckig 库调用                                 │
│      ├── 转换参数格式 (VelocityProfileConfig -> Ruckig)     │
│      └── 错误码映射到 Result<T>                             │
│                                                             │
│  Configuration                                              │
│  └── machine_config.ini                                     │
│      └── [VelocityProfile] provider = ruckig                │
└─────────────────────────────────────────────────────────────┘
```

**利弊分析**：

| 优点 | 缺点 |
|-----|-----|
| 工业级验证，50亿+测试用例 | 需要 C++20 编译器支持 |
| MIT 许可证，商业友好 | 引入外部依赖 |
| 活跃维护，社区支持 | 需要学习新 API |
| 完美契合六边形架构 | 可能需要调整 CMake 配置 |
| 无需自行实现复杂算法 | 调试时需要理解库内部逻辑 |

**风险**：
- 低风险：库成熟度高，API 稳定
- 需要验证与现有 MultiCard 硬件的兼容性
- C++20 要求可能需要升级编译器

**适用场景**：
- 需要快速获得工业级轨迹生成能力
- 对代码质量和测试覆盖有高要求
- 希望减少自研算法的维护成本

**参考资源**：
- [Ruckig GitHub](https://github.com/pantor/ruckig)
- [Ruckig 学术论文](https://www.researchgate.net/publication/351511539_Jerk-limited_Real-time_Trajectory_Generation_with_Arbitrary_Target_States)
- [LinuxCNC Ruckig 集成讨论](https://www.forum.linuxcnc.org/38-general-linuxcnc-questions/50268-trajectory-planner-using-ruckig-lib)

#### E.4 其他备选库简介

**Reflexxes Type II**：
- 经典的实时轨迹生成方案，文档完善
- LGPL-3.0 许可证（需要开源修改）
- 2015年后停止维护，不支持 Jerk 约束

**scurve_construct**：
- 轻量级，性能优秀（比 Ruckig 快 3-5 倍）
- 支持 Jog 和 Position 两种模式
- 文档较少，社区较小

**OROCOS KDL**：
- 完整的机器人运动学库，ROS 生态集成
- 对点胶机应用过于复杂
- 依赖较多，学习曲线陡峭

---

## 四、方案对比矩阵

| 评估维度 | 方案A | 方案B | 方案C | 方案D | 方案E |
|---------|-------|-------|-------|-------|-------|
| **实现复杂度** | ★☆☆☆☆ | ★★★☆☆ | ★★★★☆ | ★★★★★ | ★★☆☆☆ |
| **改动范围** | 小 | 中 | 大 | 大 | 小 |
| **运动平滑性** | 中 | 高 | 最高 | 高 | 高 |
| **时间效率** | 中 | 高 | 最高 | 高 | 高 |
| **风险等级** | 低 | 中 | 高 | 高 | 低 |
| **与现有架构兼容** | ★★★★★ | ★★★★☆ | ★★★☆☆ | ★★☆☆☆ | ★★★★★ |
| **行业成熟度** | 高 | 高 | 高 | 低 | 最高 |
| **维护成本** | 中 | 高 | 高 | 高 | 低 |

---

## 五、推荐实施路径

基于当前代码库的状态和行业最佳实践，建议采用**渐进式实施策略**：

```
阶段 1 (推荐首选) - 快速见效
├── 方案 E：集成 Ruckig 第三方库
├── 预期收益：快速获得工业级轨迹生成能力
└── 工作量：约 1-2 周（适配器开发 + 测试）

阶段 2 (可选增强)
├── 方案 A：完善现有S曲线实现（作为备选方案）
└── 预期收益：保留自研能力，减少外部依赖

阶段 3 (长期目标)
├── 方案 C：前瞻性速度规划
└── 预期收益：复杂路径最优执行
```

**推荐理由**：
- 方案 E (Ruckig) 提供了最佳的投入产出比
- MIT 许可证对商业项目友好
- 50亿+测试用例验证，可靠性有保障
- 通过适配器模式集成，不影响现有架构

---

## 六、待确认事项

请选择希望深入探讨或实施的方案：

1. **方案 A** - 增强现有实现
2. **方案 B** - 完整7段S曲线算法
3. **方案 C** - 前瞻性速度规划
4. **方案 D** - AI辅助路径优化
5. **方案 E** - 集成第三方库 Ruckig（推荐）
6. **组合方案** - 例如 E+C（先集成 Ruckig，后续添加前瞻规划）

---

## 参考资料

### 第三方库
- [Ruckig - Motion Generation for Robots and Machines](https://github.com/pantor/ruckig) - MIT 许可证，推荐
- [Reflexxes Type II Motion Library](https://github.com/Reflexxes/RMLTypeII) - LGPL-3.0
- [scurve_construct](https://github.com/grotius-cnc/scurve_construct) - 轻量级 S 曲线库
- [MotionProfileGenerator](https://github.com/WRidder/MotionProfileGenerator) - MIT 许可证
- [OROCOS KDL](https://github.com/orocos/orocos_kinematics_dynamics) - 机器人运动学库
- [struckig - Ruckig IEC 61131-3 移植版](https://github.com/stefanbesler/struckig) - PLC 适用

### 学术论文
- [A New Seven-Segment Profile Algorithm for an Open Source CNC](https://www.mdpi.com/2079-9292/8/6/652) - MDPI Electronics
- [Time-Optimal Asymmetric S-Curve Trajectory Planning](https://pmc.ncbi.nlm.nih.gov/articles/PMC10052581/) - PMC/NIH
- [Mathematics for Real-Time S-Curve Profile Generator](https://www.researchgate.net/publication/348383330_Mathematics_for_Real-Time_S-Curve_Profile_Generator) - ResearchGate
- [A complete solution to asymmetric S-curve motion profile](https://www.researchgate.net/publication/224354676_A_complete_solution_to_asymmetric_S-curve_motion_profile_Theory_experiments) - ResearchGate
- [Jerk-limited Real-time Trajectory Generation with Arbitrary Target States](https://www.researchgate.net/publication/351511539_Jerk-limited_Real-time_Trajectory_Generation_with_Arbitrary_Target_States) - Ruckig 论文
- [Rapid AI-based generation of coverage paths for dispensing applications](https://arxiv.org/html/2505.03560v1) - arXiv

### 行业资源
- [S-Curve Motion Profiles for Jerk Control - Zaber](https://www.zaber.com/articles/jerk-control)
- [S-Curve Motion Profiles - PMD Corp](https://www.pmdcorp.com/resources/type/articles/get/s-curve-profiles-deep-dive-article)
- [S-Curve Motion - RMP Motion Controller](https://support.roboticsys.com/rmp/motion-point-to-point-scurve.html)
- [S-Curve Motion Profile Using Contour Mode - Galil](https://www.galil.com/news/dmc-programming-motion-controllers/s-curve-motion-profile-using-contour-mode)

### 工业应用手册
- [ABB Application Manual - Automatic Path Planning](https://library.e.abb.com/public/a9fa67be082d4411be3e5fa6d5c52303/3HAC092826%20AM%20Automatic%20Path%20Planning-en.pdf)
- [ABB Application Manual - Externally Guided Motion](https://search.abb.com/library/Download.aspx?Action=Launch&DocumentID=3HAC073318-001)
- [MathWorks - Plan Manipulator Path for Dispensing Task](https://www.mathworks.com/help/robotics/ug/design-manipulator-path-for-dispensing-task-ikd.html)
- [LicOS PLC/PAC G-code Interpolation Motion Control](https://www.licosplc.com/news/21.html)
