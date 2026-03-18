# DXF 轨迹算法审计报告 (Technical Audit)

**日期**: 2026-02-01  
**对象**: `src/domain/trajectory/domain-services` 下的核心算法实现  
**目的**: 修正 `dxf-dispensing-flow-analysis.md` 中的描述误差，明确当前实现的数学边界与技术债务。

---

## 1. 核心发现摘要

经过对源代码 (`MotionPlanner.cpp`, `GeometryNormalizer.cpp`) 的深度核查，确认当前系统在算法严谨性上存在以下局限：

| 模块 | 原文档描述 | 代码实际实现 | 差异等级 |
| :--- | :--- | :--- | :--- |
| **MotionPlanner** | S-Curve (S形速度规划) | **梯形规划 (Trapezoidal)** | 🔴 **严重** |
| **Jerk Control** | Jerk 受限 | **默认后验检查**（可选 Ruckig 严格限制） | 🔴 **严重** |
| **Geometry** | 仿射变换/旋转 | **仅缩放** (Uniform Scale) | 🟡 中等 |
| **Time Integration** | 时间积分 | 解析积分 + 低速回退估算（保留旧式积分差异统计） | 🟡 中等 |

---

## 2. 详细审计结果

### 2.1 速度规划器 (MotionPlanner)
*   **实际算法**: 双向扫描法 (Forward/Backward Pass)。
*   **数学公式**: 使用 $V_{next} = \sqrt{V_{curr}^2 + 2 \cdot A_{max} \cdot \Delta S}$。这是经典的**恒定加速度**公式，对应**梯形速度曲线**。
*   **关于 S-Curve**: 虽然代码预留了 `VelocityProfileService` 接口，但在主路径中，双向扫描逻辑会**覆盖**任何平滑的速度分布，强制将其“削”成梯形以满足路径上的硬性速度限制。
*   **Jerk 处理**: 默认 `max_jerk` 仅用于生成轨迹点时的统计 (`report.max_jerk_observed`)，规划过程不做严格约束；可选启用 Ruckig 严格约束（`enforce_jerk_limit`）。

### 2.2 几何处理 (GeometryNormalizer)
*   **功能边界**: 当前代码仅实现了 `primitive * scale`。
*   **缺失功能**:
    *   没有实现旋转 (Rotation)。
    *   没有处理 DXF 的 UCS (用户坐标系)。
    *   没有平移逻辑 (Translation) —— 依赖外部或默认原点。

### 2.3 样条拟合 (SplineApproximation)
*   **误差模型**: 使用 `ComputeCircumradius` (三点外接圆) 估算局部曲率半径 $R$，并基于 $Step = \sqrt{8 \cdot R \cdot \epsilon}$ 计算步长。
*   **风险**: 在曲率极小或共线点附近，半径 $R \to \infty$，误差步长会退化为 0 并回落到固定/自适应步长；未见递归分割，严格误差上界依赖配置与退化路径。

### 2.4 数值稳定性
*   **积分风险**: 使用解析式 `dt_step = 2*ds_step/(v0+v1)`，当 `v0+v1` 接近 0 时改用基于 `amax` 的回退估算；同时统计与旧式积分的差异。低速密集区域仍需关注累计误差。

---

## 3. 修正后的时序图注解

针对 `dxf-dispensing-flow-analysis.md` 中的 Step 4 (Motion Planner)，应理解为：

```mermaid
    Note right of Motion: [修正] 实际算法: 梯形速度规划 (Trapezoidal)<br/>1. 虽有 S-Curve 接口，但被双向扫描逻辑覆盖。<br/>2. Jerk 约束未生效，仅做统计。<br/>3. 加速度在起停时刻存在跳变风险。
```

## 4. 改进建议 (Roadmap)

1.  **短期 (Fix Documentation)**: 在所有对外文档中，将 "S-Curve Planning" 更正为 "Trapezoidal Planning with Corner Constraints"。
2.  **中期 (Algorithm Enhancement)**:
    *   引入真正的 **Jerk-Limited Profiler** (如 7-segment S-curve)。
    *   重构双向扫描逻辑，使其支持加速度的平滑过渡，而不仅仅是速度截断。
3.  **长期 (Numerical Stability)**: 引入 Ruckig 或类似成熟库替换手写的积分逻辑，解决除零风险和时间累积误差。

---
**审计人**: Gemini CLI (基于源代码静态分析)
