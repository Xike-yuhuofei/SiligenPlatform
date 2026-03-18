# 2026-01-29 点胶轨迹算法优化实施方案 (修订版)

**日期**: 2026-01-29
**状态**: 已确认
**相关文档**: 
- `docs/_archive/2026-01/analysis/2026-01-29-dispensing-trajectory-algorithm-best-practices.md`

---

## 1. 方案背景与决策

为了解决点胶过程中拐角堆胶（"狗骨头"效应）和路径运行不连续的问题，项目组对原有方案进行了多轮评估与苏格拉底式批判审查。

**最终决策**：采用 **"几何领域核心 + Ruckig 外部引擎"** 的混合架构。
**核心理念**：将几何处理（让路径变平滑）作为核心业务逻辑置于 Domain 层，将动力学规划（让运动变平滑）委托给工业级开源库 Ruckig 并置于 Infrastructure 层。

---

## 2. 关键架构修正 (Socratic Review Outcomes)

基于苏格拉底式提问的审查，本方案做出了以下关键架构修正：

### 2.1 修正点一：几何算法归属权
*   **原认知**：在 Infrastructure 层处理路径平滑。
*   **新认知**：**几何学与硬件无关**。路径平滑、去噪、圆弧生成属于纯数学逻辑，是点胶工艺的核心业务，必须位于 **Domain 层**。
*   **决策**：在 `src/domain/motion/algorithms/` 下实现几何算法，保持纯 C++ 实现，不依赖任何第三方硬件库。

### 2.2 修正点二：Ruckig 的角色定位
*   **原认知**：直接将圆弧喂给 Ruckig。
*   **新认知**：Ruckig 是**在线轨迹生成器 (OTG)**，擅长处理离散状态（Waypoints）间的动力学约束，但不具备 CAD 级的几何解析能力。
*   **决策**：增加 **"离散化 (Discretization)"** 步骤。Domain 层负责将平滑后的圆弧路径离散化为高密度的路点流（PVT Points），再输入给 Ruckig 进行时间参数化。

### 2.3 修正点三：微小线段处理
*   **原认知**：直接对 DXF 线段进行圆弧倒角。
*   **新认知**：DXF 中存在的微小噪点线段（如 0.01mm）会导致倒角算法失效或机器震动。
*   **决策**：引入 **`PathRegularizer` (路径规范化)** 服务，先进行合并共线、剔除噪点，再进行平滑。

---

## 3. 详细架构设计 (Hexagonal)

### 3.1 Domain Layer (核心层)
位置：`src/domain/motion/`

*   **`services/PathRegularizer`**:
    *   **职责**: 预处理原始线段。
    *   **逻辑**: 合并共线线段（角度差 < 0.1度）；剔除微小线段（长度 < 阈值，如 0.1mm）。
*   **`algorithms/CornerBlender`**:
    *   **职责**: 纯几何计算。
    *   **逻辑**: 输入两条直线，计算最大允许过渡圆弧半径，输出包含圆弧的新路径结构。
*   **`algorithms/PathDiscretizer`**:
    *   **职责**: 将几何路径（含圆弧）转换为路点序列。
    *   **逻辑**: 按固定弦误差或弧长步长，将圆弧解析为一系列 (x, y) 坐标点。

### 3.2 Infrastructure Layer (适配层)
位置：`src/infrastructure/adapters/planning/`

*   **`ruckig/RuckigTrajectoryAdapter`**:
    *   **职责**: 适配 Ruckig 库。
    *   **逻辑**: 
        1.  接收 Domain 层的路点序列。
        2.  配置 Ruckig `InputParameter` (MaxVel, MaxAccel, MaxJerk)。
        3.  执行 `ruckig.update()` 生成实时控制指令。

### 3.3 Third Party (外部依赖)
位置：`third_party/`

*   **`ruckig`**: 引入 Ruckig C++ 源码 (MIT License)。

---

## 4. 实施路线图 (Roadmap)

### 阶段一：基础设施搭建 (Infrastructure Setup)
*   **任务 1.1**: 下载 Ruckig 源码 (v0.10+ 或兼容 C++17 的版本) 到 `third_party/ruckig`。
*   **任务 1.2**: 配置 CMake 构建系统，确保 Ruckig 能在 MSVC/Clang-CL 下编译通过。
*   **任务 1.3**: 创建 `RuckigTrajectoryAdapter` 原型，跑通简单的 Point-to-Point 单元测试。

### 阶段二：领域核心算法 (Domain Logic)
*   **任务 2.1**: 实现 `PathRegularizer`，编写测试用例（针对噪点数据）。
*   **任务 2.2**: 实现 `CornerBlender`，重点验证几何计算的正确性（输出数据可视化检查）。
*   **任务 2.3**: 实现 `PathDiscretizer`。

### 阶段三：整合与验证 (Integration)
*   **任务 3.1**: 修改 `DispensingUseCase`，串联 `Regularizer -> Blender -> Discretizer -> RuckigAdapter`。
*   **任务 3.2**: 实机/仿真测试。
    *   **验证指标**: 拐角处速度 > 0；无肉眼可见停顿；轨迹无过冲。

---

## 5. 风险管理

| 风险点 | 应对策略 |
| :--- | :--- |
| **Ruckig 性能开销** | Ruckig 计算量相对较大。若在控制周期 (1ms) 内无法完成计算，需将其放入独立的高优先级线程，或增加控制周期。 |
| **圆弧离散化精度** | 离散化点数过少导致多边形效应，过多导致 Ruckig 队列溢出。需通过 `ChordError` 参数动态平衡点密度。 |
| **DXF 脏数据** | 依赖 `PathRegularizer` 的鲁棒性。需要收集实际生产中的“垃圾 DXF”进行压力测试。 |

